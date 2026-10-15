# midi-cue-routing Specification

## Purpose
Route MIDI Program Change messages to scene sequencer cue triggers. Support MIDI Learn mode for assigning PC values to cues.

## Requirements

### Requirement: MIDI PC to Cue Mapping
MidiPortsStore or adapter MUST translate MIDI Program Change (0–127) to cue number (PC N → cue N+1) and call `SceneSequencer::triggerCue()`.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| PC 0 → cue 1 | Sequence loaded | MIDI PC 0 arrives | `triggerCue(1)` called |
| PC 5 → cue 6 | Sequence loaded | MIDI PC 5 arrives | `triggerCue(6)` called |
| PC 127 → cue 128 | Sequence with 128 cues loaded | MIDI PC 127 arrives | `triggerCue(128)` called |

### Requirement: Enable/Disable Toggle
MIDI cue routing MUST be toggleable. Disabled routing SHALL ignore all MIDI PC messages for cue purposes.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Disabled | Routing disabled | MIDI PC 3 arrives | No cue triggered |
| Enabled | Routing enabled | MIDI PC 3 arrives | `triggerCue(4)` called |

### Requirement: Error Handling
PC values mapping to cues beyond loaded cue count SHALL be silently ignored (no error, no crash).

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Out of range | 10 cues loaded | MIDI PC 15 arrives (→ cue 16) | Silently ignored |

### Requirement: MIDI Learn Gate
`ofApp::newMidiMessage()` MUST check a learn callback before normal MIDI routing. If callback is set and returns `true` (message consumed), normal routing SHALL be skipped. If unset or returns `false`, normal routing proceeds unchanged.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Learn mode active, PC consumed | Learn callback is registered | MIDI PC 10 arrives | Callback called; callback returns true; normal routing skipped |
| Learn mode inactive, routing proceeds | No learn callback registered | MIDI PC 10 arrives | Normal routing calls `triggerCue(11)` as usual |
| Non-PC message during learn | Learn callback is active | MIDI Note On arrives (not PC) | Callback called; returns false (only PC consumed); normal routing proceeds for Note On |

### Requirement: MIDI Learn Callback Registration
`ofApp` MUST expose a method to set the learn callback. CueEditorPanel SHALL register its callback on first Learn button click and unregister it on capture. At most one learn callback MAY exist at a time.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Register callback on Learn click | No callback registered | User clicks "Learn" on a cue in CueEditorPanel | Learn callback registered with `ofApp` |
| Unregister callback on capture | Learn callback is registered and active | MIDI PC message arrives and is captured | Callback captures PC value; unregisters itself; learn mode deactivated |

### Requirement: MIDI PC Capture and Assignment
The learn callback MUST capture the next incoming Program Change message, extract its program number (0–127), and assign it as `midiPC` on the target cue via `SceneSequencer::updateCue()`.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Capture PC for cue | Cue 3 is target of active learn mode, midiPC is -1 | MIDI PC 42 message arrives | Callback captures PC=42; calls `updateCue(3, CueDef{..., midiPC:42})`; unregisters |
| Capture at boundary | Cue 5 is target of active learn mode | MIDI PC 0 arrives | Captured midiPC = 0; cue updated |

### Requirement: Learn Mode Toggle
CueEditorPanel MUST provide a "Cancel Learn" action while learn mode is active. Panel SHALL automatically deactivate learn mode after one capture.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Cancel learn manually | Learn mode active, "Learning..." shown on button | User clicks "Cancel Learn" or presses Escape | Learn callback unregistered; button returns to "Learn"; cue unchanged |
| Auto-deactivate after capture | Learn mode active for cue 2 | MIDI PC is captured | Learn mode deactivated automatically; no manual cancel needed |
