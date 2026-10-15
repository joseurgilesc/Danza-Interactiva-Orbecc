# Delta for scene-sequencer

## MODIFIED Requirements

### Requirement: Sequence Loading

SceneSequencer MUST load `config/sequence.json` containing an array of N cues. Each cue SHALL have: `cue` (int), `sceneId`, `presetName`, `morphDuration` (float), `midiPC` (int, optional). `midiPC` defaults to -1 when absent, meaning unassigned. No hardcoded cue limit.
(Previously: CueDef only had `cue`, `sceneId`, `presetName`, `morphDuration` — no `midiPC` field.)

#### Scenario: Load valid file with midiPC

- GIVEN `sequence.json` with a cue having `midiPC: 42`
- WHEN Sequencer initializes
- THEN CueDef.midiPC = 42

#### Scenario: Load file without midiPC field

- GIVEN `sequence.json` with a cue missing `midiPC`
- WHEN Sequencer initializes
- THEN CueDef.midiPC = -1 (unassigned)

#### Scenario: Missing file

- GIVEN No `sequence.json`
- WHEN Initialize
- THEN Warning logged; empty cue list; sequencer disabled

#### Scenario: Malformed JSON

- GIVEN Syntax error in file
- WHEN Initialize
- THEN Error logged; empty cue list

## ADDED Requirements

### Requirement: Cue Mutation API

SceneSequencer MUST provide `addCue(CueDef)`, `removeCue(int cueNumber)`, and `updateCue(int cueNumber, CueDef)`. `addCue` SHALL append to the cue list. `removeCue` SHALL remove the cue matching the given number. `updateCue` SHALL replace the cue with the given number.

#### Scenario: Add cue

- GIVEN Empty cue list
- WHEN `addCue({cue:1, sceneId:"s1", presetName:"p1", morphDuration:2.0, midiPC:0})`
- THEN Cue list has 1 entry with cue number 1 and midiPC 0

#### Scenario: Remove existing cue

- GIVEN Cue list has cue numbers [1, 2, 3]
- WHEN `removeCue(2)`
- THEN Cue list has cue numbers [1, 3]

#### Scenario: Remove nonexistent cue

- GIVEN Cue list has cue number 1 only
- WHEN `removeCue(99)`
- THEN Error logged; no change to cue list

#### Scenario: Update existing cue

- GIVEN Cue 1 has `sceneId="s1"`
- WHEN `updateCue(1, CueDef{cue:1, sceneId:"s2", ...})`
- THEN Cue 1 sceneId is now "s2"

### Requirement: Sequence Persistence

`saveSequence(path)` MUST serialize the current cue list to `path` as `{"cues": [...]}` JSON. Format SHALL be identical to the format `loadSequence` reads, ensuring round-trip fidelity.

#### Scenario: Save and reload

- GIVEN 3 cues in memory with midiPC values
- WHEN `saveSequence("config/sequence.json")` then `loadSequence("config/sequence.json")`
- THEN All 3 cues reload with identical data including midiPC

#### Scenario: Save empty sequence

- GIVEN Empty cue list
- WHEN `saveSequence("sequence.json")`
- THEN File contains `{"cues": []}`

### Requirement: Cue Number Uniqueness

On `addCue` and `updateCue`, SceneSequencer MUST reject operations that would create duplicate cue numbers within the same sequence. Validation SHALL happen before mutation.

#### Scenario: Duplicate on add

- GIVEN Cue list has cue number 1
- WHEN `addCue({cue:1, ...})`
- THEN Error logged; cue not added

#### Scenario: Duplicate on update

- GIVEN Cue list has cue numbers [1, 2]
- WHEN `updateCue(1, CueDef{cue:2, ...})`
- THEN Error logged; cue 1 unchanged

#### Scenario: Same number on update (self)

- GIVEN Cue list has cue numbers [1, 2]
- WHEN `updateCue(1, CueDef{cue:1, sceneId:"newScene", ...})`
- THEN Cue 1 updated successfully (not rejected as duplicate)

### Requirement: Cue Reordering

`reorderCues(fromIndex, toIndex)` SHALL move the cue at `fromIndex` (0-based position) to `toIndex`. Indices out of range SHALL be rejected with an error log.

#### Scenario: Move cue up

- GIVEN 3 cues: [A, B, C] at indices 0,1,2
- WHEN `reorderCues(2, 0)`
- THEN Cue order becomes [C, A, B]

#### Scenario: Move same index

- GIVEN 3 cues at indices 0,1,2
- WHEN `reorderCues(1, 1)`
- THEN No change; no error

#### Scenario: Index out of range

- GIVEN 3 cues
- WHEN `reorderCues(0, 99)`
- THEN Error logged; no reorder performed
