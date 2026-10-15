# Apply Progress: midi-cue-editor

**Status**: success  
**Mode**: Standard (strict_tdd: false, no test runner)  
**Delivery**: Single PR | 623 lines added (estimated 350, slight overrun)

## Completed Tasks (14/14)

### Phase 1: SceneSequencer Mutation API ✅
- [x] 1.1 Added `int midiPC = -1` to CueDef, parsed in loadSequence
- [x] 1.2 Implemented `addCue`, `removeCue`, `updateCue` with `isDuplicateCueNumber` helper
- [x] 1.3 Implemented `saveSequence` (ofSaveJson) and `reorderCues(fromIndex, toIndex)` with currentCueIndex tracking

### Phase 2: CueEditorPanel UI ✅
- [x] 2.1 Added kCueEditor panel ID, cueEditorDockId_, docked at dockRightId (tabbed with Presets/Params)
- [x] 2.2 Created CueEditorPanel.h — IUIModule with SceneSequencer/SceneEngine/PresetManager/ofxMidiIn/registerLearn
- [x] 2.3 Created CueEditorPanel.cpp — ImGui table skeleton, empty-state "No cues defined" + Add button
- [x] 2.4 Editable columns: InputInt (Cue#), Combo (Scene), Combo (Preset), DragFloat (Morph), InputInt (MIDI PC)
- [x] 2.5 Add Cue (auto-number maxExisting+1), Save (validates duplicates first, error popup), Delete (confirmation popup)
- [x] 2.6 MIDI Learn: "Learning..." button, captures next PC → updateCue → unregister, disabled when no MIDI ports
- [x] 2.7 Validation: red row bg on duplicate cue#, yellow (!) on orphan preset ref

### Phase 3: ofApp Integration ✅
- [x] 3.1 Added cueEditorPanel_, midiLearnCallback_, setMidiLearnCallback() to ofApp.h
- [x] 3.2 Instantiated CueEditorPanel after SequencerPanel, registered with UIManager via registerLearn lambda
- [x] 3.3 MIDI Learn gate in newMidiMessage: check callback → call → if consumed, skip normal routing

### Phase 4: Verification ✅
- [x] 4.1 Build passes with `make -j6` — zero errors, zero new warnings on macOS (Apple clang 14.0.0)

## Files Changed

| File | Action | Lines |
|------|--------|-------|
| `src/sequencer/SceneSequencer.h` | Modified | +27 (midiPC field, 5 mutation method declarations, private helper) |
| `src/sequencer/SceneSequencer.cpp` | Modified | +141 (midiPC parsing, 6 method implementations) |
| `src/ui/CueEditorPanel.h` | Created | +56 (IUIModule subclass with 7 members + helpers) |
| `src/ui/CueEditorPanel.cpp` | Created | +366 (full ImGui table, edit, validate, Learn, Save, Delete) |
| `src/ui/PanelIds.h` | Modified | +1 (kCueEditor) |
| `src/ui/UIManager.h` | Modified | +1 (cueEditorDockId_) |
| `src/ui/UIManager.cpp` | Modified | +5 (-1 net: constructor init, dock assignment, registration, default-hide) |
| `src/ofApp.h` | Modified | +7 (include, cueEditorPanel_, midiLearnCallback_, setMidiLearnCallback) |
| `src/ofApp.cpp` | Modified | +20 (include, instantiation+registration, Learn gate in newMidiMessage) |
| `openspec/changes/midi-cue-editor/tasks.md` | Updated | 14/14 tasks marked [x] |

**Total: 623 insertions, 1 deletion across 9 files (4 commits)**

## Deviations from Design

1. **Duplicate validation**: The design says "inline red text" for duplicate errors. Implementation uses red row background tint + the actual InputInt widget still shows the duplicate value. This is more visible than inline text alone.
2. **Orphan preset warning**: Uses yellow "(!)" text instead of unicode ⚠ emoji (portability concern with ImGui font atlas).
3. **reorderCues**: Spec says "Indices out of range SHALL be rejected with an error log" — implemented rejection (not clamping as tasks.md originally suggested).
4. **Save path**: Uses `"config/sequence.json"` (relative to working directory, which is `bin/data/` at runtime). This matches the existing `loadSequence("config/sequence.json")` path used in ofApp.cpp.
5. **Estimated vs actual lines**: Estimated ~350, actual 623. The CueEditorPanel.cpp ImGui implementation (366 lines) was larger than forecast due to the 7-column table with validation, combos, Learn, and delete confirmation logic.

## Issues Found

None — implementation compiles cleanly and follows existing patterns exactly. The only pre-existing warning (`TARGET_OS_VISION`) is unrelated to this change.

## Commits

```
fcb257f feat(sequencer): add cue mutation API
0699133 feat(ui): add CueEditorPanel
a7a19bc feat(ofApp): wire CueEditorPanel + MIDI Learn gate
33be91e chore(sdd): mark all midi-cue-editor tasks complete
```

## Next Recommended

sdd-verify — manual smoke test with MIDI controller, or sdd-archive if satisfied.
