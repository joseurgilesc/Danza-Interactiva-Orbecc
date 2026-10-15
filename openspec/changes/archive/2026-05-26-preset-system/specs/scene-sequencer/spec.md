# scene-sequencer Specification

## Purpose
Ordered cue sequence combining preset application with scene transitions. Loaded from JSON; triggered by OSC, MIDI, or UI.

## Requirements

### Requirement: Sequence Loading
SceneSequencer MUST load `config/sequence.json` containing an array of N cues. Each cue SHALL have: `cue` (int), `sceneId`, `presetName`, `morphDuration` (float). No hardcoded cue limit.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Load valid file | `sequence.json` with 5 cues | Sequencer initializes | All 5 cues loaded in order |
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
