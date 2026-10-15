# Tasks: MIDI Cue Editor

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~350 |
| 400-line budget risk | Medium |
| Chained PRs recommended | No |
| Suggested split | Single PR |
| Delivery strategy | ask-on-risk |
| Chain strategy | pending |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: pending
400-line budget risk: Medium

### Suggested Work Units

| Unit | Goal | Likely PR | Notes |
|------|------|-----------|-------|
| 1 | SceneSequencer mutation API | PR 1 | Backend only; independently verifiable via save→load round-trip |
| 2 | CueEditorPanel UI + dock | PR 1 | Depends on unit 1 |
| 3 | ofApp wiring + MIDI Learn | PR 1 | Integration; single PR since total under 400 lines |

## Phase 1: Backend — SceneSequencer Mutation API

- [x] 1.1 Add `int midiPC = -1` to `CueDef` struct in `src/sequencer/SceneSequencer.h`; parse `midiPC` in `loadSequence` with default -1 when absent
  > Covers: scene-sequencer MODIFIED "Sequence Loading" (Scenarios: Load with midiPC, Load without midiPC)
- [x] 1.2 Add private `isDuplicateCueNumber(int cueNumber, int excludeIdx = -1)` helper; implement `addCue(CueDef) → bool`, `removeCue(int) → bool`, `updateCue(int, CueDef) → bool` in `src/sequencer/SceneSequencer.cpp`
  > Covers: "Cue Mutation API" (Add/Remove/Update scenarios), "Cue Number Uniqueness" (Duplicate on add/update, self-update allowed)
- [x] 1.3 Implement `reorderCues(fromIndex, toIndex) → bool` (rejects out-of-range indices with error log); implement `saveSequence(path) → bool` serializing cues_[] as `{"cues": [...]}` via `ofSaveJson`
  > Covers: "Sequence Persistence" (Save/reload round-trip, Empty save), "Cue Reordering" (Move up, Same index, Out of range)

## Phase 2: UI — CueEditorPanel + Dock Registration

- [x] 2.1 Add `kCueEditor = "Cue Editor"` to `src/ui/PanelIds.h`; add `cueEditorDockId_` member to `src/ui/UIManager.h`; dock CueEditor at `dockRightId` (tabs with Presets/Params) in `src/ui/UIManager.cpp`
  > Covers: sequencer-ui ADDED "CueEditorPanel Lifecycle" (infrastructure)
- [x] 2.2 Create `src/ui/CueEditorPanel.h` — `IUIModule` with constructor taking `SceneSequencer*`, `SceneEngine*`, `PresetManager*`, `ofxMidiIn*`, and `registerLearn` callback; declare member vars and `draw()`/`setVisible()`/`isVisible()`/`getPanelId()`
  > Covers: sequencer-ui ADDED "CueEditorPanel Lifecycle" (Toggle scenarios)
- [x] 2.3 Create `src/ui/CueEditorPanel.cpp` — `draw()` with `ImGui::Begin`/`End`, empty-state "No cues defined" + Add button, ImGui table skeleton matching `SequencerPanel` pattern
  > Covers: "CueEditorPanel Lifecycle" (Empty state, With cues)
- [x] 2.4 Implement editable columns: `InputInt` for Cue#, `Combo` for Scene (from `SceneEngine::getLoadedScenes()`), `Combo` for Preset (from `PresetManager::getPresets(sceneId)` + "(none)"), `DragFloat` for Morph, `InputInt` for MIDI PC
  > Covers: "Cue Table Editing" (Scene/preset dropdowns, Edit fields inline)
- [x] 2.5 Implement Add Cue button (defaults: cue=maxExisting+1, first scene, "(none)", 0.0, -1), Save button calling `SceneSequencer::saveSequence` with success/error feedback, Delete button per row with ImGui confirmation popup
  > Covers: "Add Cue Button", "Save Button", "Delete Confirmation" (All scenarios)
- [x] 2.6 Implement MIDI Learn per row: "Learn" button toggles → registers callback via `registerLearn` lambda, shows "Learning..." while active, captures next PC → calls `updateCue` → unregisters; disable button when `midiIn_->getNumInPorts() == 0`
  > Covers: "MIDI Learn Button" (Learn captures PC, No MIDI ports), midi-cue-routing "MIDI Learn Gate"/"Callback Registration"/"PC Capture and Assignment"/"Learn Mode Toggle"
- [x] 2.7 Implement validation rendering: duplicate cue# → red row bg + red inline text, orphan preset → yellow (!) warning icon (check `PresetManager::getPresets`); duplicate validation blocks save via error popup
  > Covers: "Validation Feedback" (Duplicate and orphan feedback)

## Phase 3: Integration — ofApp Wiring + MIDI Learn Gate

- [x] 3.1 Add `cueEditorPanel_` (unique_ptr), `midiLearnCallback_` (std::function<bool(ofxMidiMessage&)>), and `setMidiLearnCallback()` to `src/ofApp.h`
  > Covers: midi-cue-routing "MIDI Learn Gate" (interface)
- [x] 3.2 Include `CueEditorPanel.h` in `src/ofApp.cpp`; instantiate panel after SequencerPanel with `sceneSequencer_.get()`, `&sceneEngine`, `presetManager_.get()`, `&midiIn_`, and a lambda for `setMidiLearnCallback`; register with `uiManager.addModule()`
  > Covers: midi-cue-routing "MIDI Learn Callback Registration" (Register/Unregister)
- [x] 3.3 Add MIDI Learn gate in `newMidiMessage()`: if `midiLearnCallback_` is set, call it; if returns true (consumed), skip normal PC→cue routing
  > Covers: "MIDI Learn Gate" (Learn active → consumed, Learn inactive → routing, Non-PC during learn)

## Phase 4: Verification

- [x] 4.1 Build with `make` — verified no compilation errors or regressions; build succeeds cleanly
  > Covers: All success criteria from proposal
