# midi-cue-routing Specification

## Purpose
Route MIDI Program Change messages to scene sequencer cue triggers.

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
