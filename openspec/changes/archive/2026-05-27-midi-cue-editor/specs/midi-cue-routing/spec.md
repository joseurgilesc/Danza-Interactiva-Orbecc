# Delta for midi-cue-routing

## ADDED Requirements

### Requirement: MIDI Learn Gate

`ofApp::newMidiMessage()` MUST check a learn callback before normal MIDI routing. If callback is set and returns `true` (message consumed), normal routing SHALL be skipped. If unset or returns `false`, normal routing proceeds unchanged.

#### Scenario: Learn mode active, PC consumed

- GIVEN Learn callback is registered
- WHEN MIDI PC 10 arrives
- THEN Callback called; callback returns true; normal routing skipped

#### Scenario: Learn mode inactive, routing proceeds

- GIVEN No learn callback registered
- WHEN MIDI PC 10 arrives
- THEN Normal routing calls `triggerCue(11)` as usual

#### Scenario: Non-PC message during learn

- GIVEN Learn callback is active
- WHEN MIDI Note On arrives (not PC)
- THEN Callback called; returns false (only PC consumed); normal routing proceeds for Note On

### Requirement: MIDI Learn Callback Registration

`ofApp` MUST expose a method to set the learn callback. CueEditorPanel SHALL register its callback on first Learn button click and unregister it on capture. At most one learn callback MAY exist at a time.

#### Scenario: Register callback on Learn click

- GIVEN No callback registered
- WHEN User clicks "Learn" on a cue in CueEditorPanel
- THEN Learn callback registered with `ofApp`

#### Scenario: Unregister callback on capture

- GIVEN Learn callback is registered and active
- WHEN MIDI PC message arrives and is captured
- THEN Callback captures PC value; unregisters itself; learn mode deactivated

### Requirement: MIDI PC Capture and Assignment

The learn callback MUST capture the next incoming Program Change message, extract its program number (0–127), and assign it as `midiPC` on the target cue via `SceneSequencer::updateCue()`.

#### Scenario: Capture PC for cue

- GIVEN Cue 3 is target of active learn mode, midiPC is -1
- WHEN MIDI PC 42 message arrives
- THEN Callback captures PC=42; calls `updateCue(3, CueDef{..., midiPC:42})`; unregisters

#### Scenario: Capture at boundary

- GIVEN Cue 5 is target of active learn mode
- WHEN MIDI PC 0 arrives
- THEN Captured midiPC = 0; cue updated

### Requirement: Learn Mode Toggle

CueEditorPanel MUST provide a "Cancel Learn" action while learn mode is active. Panel SHALL automatically deactivate learn mode after one capture.

#### Scenario: Cancel learn manually

- GIVEN Learn mode active, "Learning..." shown on button
- WHEN User clicks "Cancel Learn" or presses Escape
- THEN Learn callback unregistered; button returns to "Learn"; cue unchanged

#### Scenario: Auto-deactivate after capture

- GIVEN Learn mode active for cue 2
- WHEN MIDI PC is captured
- THEN Learn mode deactivated automatically; no manual cancel needed
