# Delta for sequencer-ui

## ADDED Requirements

### Requirement: CueEditorPanel Lifecycle

CueEditorPanel MUST implement `IUIModule` and be toggleable via UIManager. Panel SHALL render an editable cue table when sequence is present, or "No cues defined" message when empty.

#### Scenario: Toggle on and empty state

- GIVEN CueEditorPanel registered in UIManager
- WHEN User toggles it visible; sequence has zero cues
- THEN "No cues defined" displayed; Add button visible

#### Scenario: Toggle on with cues

- GIVEN Sequence has cues
- WHEN Panel toggled visible
- THEN Editable table renders with all cue rows

### Requirement: Cue Table Editing

Panel MUST render editable columns: Cue#, Scene (dropdown), Preset (dropdown), Morph Duration (slider), MIDI PC (int). Each row SHALL have a Delete button. Scene dropdown populates from `SceneEngine::getLoadedScenes()`. Preset dropdown populates from `PresetManager::getPresets(sceneId)` plus "(none)".

#### Scenario: Scene and preset dropdowns

- GIVEN Loaded scenes ["s1","s2"], scene "s1" has presets ["p1","p2"]
- WHEN User opens scene dropdown on a cue, then opens preset dropdown
- THEN Scene shows "s1","s2"; preset shows "(none)","p1","p2"

#### Scenario: Edit fields inline

- GIVEN Cue row with morph=2.0, midiPC=-1
- WHEN User drags morph to 3.5 and types "42" in midiPC
- THEN morph=3.5, midiPC=42

#### Scenario: Delete cue

- GIVEN 3 cues loaded
- WHEN User clicks Delete on cue 2 and confirms
- THEN Cue removed; 2 cues remain

### Requirement: Add Cue Button

Panel SHALL provide a global "Add Cue" button that appends a new cue with defaults: cue = maxExisting+1, scene = first loaded, preset = "(none)", morph = 0.0, midiPC = -1.

#### Scenario: Add cue

- GIVEN Cue numbers [1,3], scene "s1" loaded
- WHEN User clicks "Add Cue"
- THEN New cue appears with cue=4, sceneId="s1", preset="(none)", morph=0.0, midiPC=-1

### Requirement: Save Button

Panel SHALL provide a global "Save" button calling `SceneSequencer::saveSequence()` with success/error feedback.

#### Scenario: Save

- GIVEN Cues edited but unsaved
- WHEN User clicks "Save"
- THEN `saveSequence()` called; success message shown, or error if write fails

### Requirement: Validation Feedback

Panel MUST show inline errors for duplicate cue numbers. Orphan preset references SHALL show a warning icon (non-blocking). Duplicate validation does NOT block save — it produces a save error.

#### Scenario: Duplicate and orphan feedback

- GIVEN User edits cue 2 number to existing 1; cue 3 has deleted preset "oldP"
- WHEN Validation runs
- THEN Red "Duplicate cue number" next to cue 2; warning icon next to cue 3 preset

### Requirement: MIDI Learn Button

Each cue row SHALL have a "Learn" button. On click, activates MIDI Learn for that cue (button shows "Learning..."). On next incoming PC, midiPC updates and learn deactivates. Button is disabled when `midiIn_.getNumInPorts() == 0`.

#### Scenario: Learn captures PC

- GIVEN Cue 2 midiPC=-1, MIDI ports available
- WHEN User clicks "Learn" on cue 2, sends MIDI PC 42
- THEN Cue 2 midiPC=42; button reverts to "Learn"; learn deactivated

#### Scenario: No MIDI ports

- GIVEN No MIDI input ports available
- WHEN Panel renders
- THEN Learn buttons disabled with tooltip

### Requirement: Delete Confirmation

Delete action MUST show a confirmation dialog before removing the cue.

#### Scenario: Delete confirmed or cancelled

- GIVEN User clicks Delete on cue 3
- WHEN Dialog shown; user clicks "Yes"
- THEN Cue removed
- WHEN User clicks "No" or dismisses
- THEN Cue unchanged
