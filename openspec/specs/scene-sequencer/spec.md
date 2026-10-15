# scene-sequencer Specification

## Purpose
Ordered cue sequence combining preset application with scene transitions. Loaded from JSON; triggered by OSC, MIDI, or UI. Supports mutation (add/remove/update/reorder) and persistence (save).

## Requirements

### Requirement: Sequence Loading
SceneSequencer MUST load `config/sequence.json` containing an array of N cues. Each cue SHALL have: `cue` (int), `sceneId`, `presetName`, `morphDuration` (float), `midiPC` (int, optional, defaults to -1 = unassigned). No hardcoded cue limit.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Load valid file | `sequence.json` with 5 cues | Sequencer initializes | All 5 cues loaded in order; midiPC parsed when present |
| Load file with midiPC | `sequence.json` with a cue having `midiPC: 42` | Sequencer initializes | CueDef.midiPC = 42 |
| Load file without midiPC | `sequence.json` with a cue missing `midiPC` | Sequencer initializes | CueDef.midiPC = -1 (unassigned) |
| Missing file | No `sequence.json` | Initialize | Warning logged; empty cue list; sequencer disabled |
| Malformed JSON | Syntax error in file | Initialize | Error logged; empty cue list |

### Requirement: Cue Triggering
`triggerCue(n)` MUST (a) apply preset via PresetManager with cue's morphDuration, then (b) transition scene via `SceneEngine::transitionTo(sceneId)`. If presetName is empty, SHALL skip preset apply and only transition.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Full cue | Cue 1: `sceneId="pinturaFluorescente", presetName="full", morphDuration=2.0` | `triggerCue(1)` | Preset applied with 2s morph; scene transitions to pinturaFluorescente |
| No preset | Cue 3: `presetName=""` | `triggerCue(3)` | Only scene transition; no preset applied |
| Zero duration | Cue 2: `morphDuration=0` | `triggerCue(2)` | Preset applied instantly; scene transitions |

### Requirement: Current Cue Tracking
Sequencer SHALL track last triggered cue index. Initial state (no cue triggered) MUST report -1.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| First trigger | Current = -1 | `triggerCue(2)` | Current = 2 |
| Sequential advance | Current = 2 | `triggerCue(3)` | Current = 3 |

### Requirement: Error Handling
Out-of-range cue, unknown sceneId, or unknown presetName SHALL log errors and reject the trigger.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Cue out of range | 3 cues loaded (1-3) | `triggerCue(99)` | Error logged; no action |
| Unknown scene | Cue 4: `sceneId="nonexistent"` | `triggerCue(4)` | Error logged; trigger rejected |
| Unknown preset | Cue 2: `presetName="badPreset"` | `triggerCue(2)` | Warning logged; scene still transitions |

### Requirement: Cue Mutation API
SceneSequencer MUST provide `addCue(CueDef)`, `removeCue(int cueNumber)`, and `updateCue(int cueNumber, CueDef)`. `addCue` SHALL append to the cue list. `removeCue` SHALL remove the cue matching the given number. `updateCue` SHALL replace the cue with the given number.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Add cue | Empty cue list | `addCue({cue:1, sceneId:"s1", presetName:"p1", morphDuration:2.0, midiPC:0})` | Cue list has 1 entry with cue number 1 and midiPC 0 |
| Remove existing cue | Cue list has cue numbers [1, 2, 3] | `removeCue(2)` | Cue list has cue numbers [1, 3] |
| Remove nonexistent cue | Cue list has cue number 1 only | `removeCue(99)` | Error logged; no change to cue list |
| Update existing cue | Cue 1 has `sceneId="s1"` | `updateCue(1, CueDef{cue:1, sceneId:"s2", ...})` | Cue 1 sceneId is now "s2" |

### Requirement: Sequence Persistence
`saveSequence(path)` MUST serialize the current cue list to `path` as `{"cues": [...]}` JSON. Format SHALL be identical to the format `loadSequence` reads, ensuring round-trip fidelity.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Save and reload | 3 cues in memory with midiPC values | `saveSequence("config/sequence.json")` then `loadSequence("config/sequence.json")` | All 3 cues reload with identical data including midiPC |
| Save empty sequence | Empty cue list | `saveSequence("sequence.json")` | File contains `{"cues": []}` |

### Requirement: Cue Number Uniqueness
On `addCue` and `updateCue`, SceneSequencer MUST reject operations that would create duplicate cue numbers within the same sequence. Validation SHALL happen before mutation.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Duplicate on add | Cue list has cue number 1 | `addCue({cue:1, ...})` | Error logged; cue not added |
| Duplicate on update | Cue list has cue numbers [1, 2] | `updateCue(1, CueDef{cue:2, ...})` | Error logged; cue 1 unchanged |
| Same number on update (self) | Cue list has cue numbers [1, 2] | `updateCue(1, CueDef{cue:1, sceneId:"newScene", ...})` | Cue 1 updated successfully (not rejected as duplicate) |

### Requirement: Cue Reordering
`reorderCues(fromIndex, toIndex)` SHALL move the cue at `fromIndex` (0-based position) to `toIndex`. Indices out of range SHALL be rejected with an error log.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Move cue up | 3 cues: [A, B, C] at indices 0,1,2 | `reorderCues(2, 0)` | Cue order becomes [C, A, B] |
| Move same index | 3 cues at indices 0,1,2 | `reorderCues(1, 1)` | No change; no error |
| Index out of range | 3 cues | `reorderCues(0, 99)` | Error logged; no reorder performed |
