# sequencer-ui Specification

## Purpose
ImGui panels displaying the cue sequence. SequencerPanel provides read-only triggering and navigation for performance. CueEditorPanel provides editable cue management (add, edit, delete, reorder, MIDI Learn).

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

### Requirement: CueEditorPanel Lifecycle
CueEditorPanel MUST implement `IUIModule` and be toggleable via UIManager. Panel SHALL render an editable cue table when sequence is present, or "No cues defined" message when empty.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Toggle on and empty state | CueEditorPanel registered in UIManager | User toggles it visible; sequence has zero cues | "No cues defined" displayed; Add button visible |
| Toggle on with cues | Sequence has cues | Panel toggled visible | Editable table renders with all cue rows |

### Requirement: Cue Table Editing
Panel MUST render editable columns: Cue#, Scene (dropdown), Preset (dropdown), Morph Duration (slider), MIDI PC (int). Each row SHALL have a Delete button. Scene dropdown populates from `SceneEngine::getLoadedScenes()`. Preset dropdown populates from `PresetManager::getPresets(sceneId)` plus "(none)".

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Scene and preset dropdowns | Loaded scenes ["s1","s2"], scene "s1" has presets ["p1","p2"] | User opens scene dropdown on a cue, then opens preset dropdown | Scene shows "s1","s2"; preset shows "(none)","p1","p2" |
| Edit fields inline | Cue row with morph=2.0, midiPC=-1 | User drags morph to 3.5 and types "42" in midiPC | morph=3.5, midiPC=42 |
| Delete cue | 3 cues loaded | User clicks Delete on cue 2 and confirms | Cue removed; 2 cues remain |

### Requirement: Add Cue Button
Panel SHALL provide a global "Add Cue" button that appends a new cue with defaults: cue = maxExisting+1, scene = first loaded, preset = "(none)", morph = 0.0, midiPC = -1.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Add cue | Cue numbers [1,3], scene "s1" loaded | User clicks "Add Cue" | New cue appears with cue=4, sceneId="s1", preset="(none)", morph=0.0, midiPC=-1 |

### Requirement: Save Button
Panel SHALL provide a global "Save" button calling `SceneSequencer::saveSequence()` with success/error feedback.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Save | Cues edited but unsaved | User clicks "Save" | `saveSequence()` called; success message shown, or error if write fails |

### Requirement: Validation Feedback
Panel MUST show inline errors for duplicate cue numbers. Orphan preset references SHALL show a warning icon (non-blocking). Duplicate validation SHALL produce a save error.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Duplicate and orphan feedback | User edits cue 2 number to existing 1; cue 3 has deleted preset "oldP" | Validation runs | Red "Duplicate cue number" next to cue 2; warning icon next to cue 3 preset |

### Requirement: MIDI Learn Button
Each cue row SHALL have a "Learn" button. On click, activates MIDI Learn for that cue (button shows "Learning..."). On next incoming PC, midiPC updates and learn deactivates. Button is disabled when `midiIn_.getNumInPorts() == 0`.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Learn captures PC | Cue 2 midiPC=-1, MIDI ports available | User clicks "Learn" on cue 2, sends MIDI PC 42 | Cue 2 midiPC=42; button reverts to "Learn"; learn deactivated |
| No MIDI ports | No MIDI input ports available | Panel renders | Learn buttons disabled with tooltip |

### Requirement: Delete Confirmation
Delete action MUST show a confirmation dialog before removing the cue.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Delete confirmed or cancelled | User clicks Delete on cue 3 | Dialog shown; user clicks "Yes" | Cue removed |
| | | User clicks "No" or dismisses | Cue unchanged |
