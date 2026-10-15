# sequencer-ui Specification

## Purpose
ImGui panel displaying the cue sequence table with trigger buttons and sequential navigation.

## Requirements

### Requirement: Panel Lifecycle
SequencerPanel MUST be toggleable via UIManager. When sequence is empty, panel SHALL show "No sequence loaded."

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Toggle on | Panel hidden | User toggles via UIManager | Panel renders cue table |
| No sequence | `sequence.json` missing | Panel renders | "No sequence loaded" message displayed |

### Requirement: Cue Table Display
Panel MUST render a table with columns: Cue#, Scene, Preset, Duration, Status. Current cue row SHALL be highlighted.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| 5 cues loaded | Sequence has 5 cues | Panel renders | All 5 rows displayed with correct data |
| Current cue = 2 | Cue 2 was last triggered | Panel renders | Row 2 highlighted; others default style |

### Requirement: Cue Triggering
Each row SHALL have a "Trigger" button calling `SceneSequencer::triggerCue(n)`.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Click trigger | Cue 3 row visible | Click "Trigger" | `triggerCue(3)` called |

### Requirement: Sequential Navigation
Panel SHALL provide "Prev" and "Next" buttons. Next triggers current+1; Prev triggers current-1. Navigation SHALL clamp or wrap at boundaries without crash.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Next advance | Current cue = 2, 5 cues total | Click "Next" | `triggerCue(3)` called |
| Next at boundary | Current cue = 5 (last) | Click "Next" | Clamps at 5 or wraps to 1; no crash |
| Prev at start | Current cue = 1 | Click "Prev" | Clamps at 1 or wraps to 5; no crash |
