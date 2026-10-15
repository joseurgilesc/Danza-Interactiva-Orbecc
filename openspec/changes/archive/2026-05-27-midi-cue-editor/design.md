# Design: MIDI Cue Editor

## Technical Approach

Add mutation methods to `SceneSequencer`, a new `CueEditorPanel` (`IUIModule`), and a MIDI Learn gate in `ofApp::newMidiMessage()`. The existing `SequencerPanel` stays read-only. The editor panel tabs with Presets in the right dock column.

## Architecture Decisions

### Decision: New Panel vs Extending SequencerPanel

| Option | Tradeoff | Decision |
|--------|----------|----------|
| Extend SequencerPanel | Mixed read/edit mode in one panel; breaks single-responsibility | **Reject** |
| New CueEditorPanel | Clean separation; edit panel can be hidden during performance | **Choose** |

**Rationale**: Performance mode needs read-only visibility. Editing during performance is dangerous. Separate panels let users hide the editor during shows.

### Decision: MIDI Learn Callback vs Direct ofApp Integration

| Option | Tradeoff | Decision |
|--------|----------|----------|
| `ofApp` exposes `setMidiLearnCallback(std::function<bool(ofxMidiMessage&)>)` | Loose coupling; panel owns its learn logic | **Choose** |
| CueEditorPanel calls `ofApp::getMidiIn()` directly | Tight coupling; breaks IUIModule pattern | Reject |

**Rationale**: Panel stays testable (`SceneSequencer*` + callback). ofApp is the integration point — that is its job. The callback unregisters itself after one capture.

### Decision: mutation return types

| Method | Returns | Rationale |
|--------|---------|-----------|
| `addCue(CueDef)` | `bool` | Fails on duplicate cue number |
| `removeCue(int)` | `bool` | Fails if cue not found |
| `updateCue(int, CueDef)` | `bool` | Fails if target or new cue number conflicts |
| `saveSequence(path)` | `bool` | Fails on file write error |
| `reorderCues(from, to)` | `bool` | Fails on out-of-range indices |

Error logging happens inside each method via `ofLogError`. UI uses return value for inline feedback.

## Data Flow

### Edit → Save flow
```
CueEditorPanel::draw()          SceneSequencer              filesystem
  │ edit fields in memory         │                           │
  │── updateCue(n, def) ────────→│ mutate cues_[]            │
  │   (per-field, debounced)      │ validate uniqueness       │
  │                               │ return bool               │
  │                               │                           │
  │── Save button ───────────────→│ saveSequence(path) ──────→│ write cues_[] as JSON
  │                               │ ofSaveJson(path, json)    │
```

### MIDI Learn flow
```
ofxMidiIn                        ofApp::newMidiMessage()     CueEditorPanel
  │ incoming PC 42                 │                           │
  │───────────────────────────────→│ 1. check learnCallback_   │
  │                                │    if set → call          │
  │                                │──────────────────────────→│ capture midiPC=42
  │                                │←─ return true ───────────│→ updateCue(cue, def)
  │                                │ 2. skip normal routing    │→ unregister callback
  │   (Learn inactive)             │                           │
  │── incoming PC 10 ─────────────→│ 1. learnCallback_ null    │
  │                                │ 2. normal routing:        │
  │                                │    triggerCue(11)         │
```

### Validation flow
```
UI edit → check duplicate (in-memory scan of cues_[]) → show inline red text
         → check preset exists (PresetManager::getPresets) → show warning icon
         → Save validates again → return error if duplicate
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/sequencer/SceneSequencer.h` | Modify | Add `midiPC` to CueDef; declare addCue/removeCue/updateCue/saveSequence/reorderCues; add private `isDuplicateCueNumber(int, int excludeIdx)` |
| `src/sequencer/SceneSequencer.cpp` | Modify | Implement 5 mutation methods; serialize with ofSaveJson matching loadSequence format; append `midiPC` to loadSequence parsing |
| `src/ui/CueEditorPanel.h` | **Create** | New IUIModule: editable cue table, Learn/MIDI callbacks, validation rendering |
| `src/ui/CueEditorPanel.cpp` | **Create** | ImGui table: InputInt(cue#), Combo(scene), Combo(preset), DragFloat(morph), InputInt(midiPC), Learn/Delete/Save/Add buttons |
| `src/ui/PanelIds.h` | Modify | Add `kCueEditor = "Cue Editor"` |
| `src/ui/UIManager.h` | Modify | Add `cueEditorDockId_` member |
| `src/ui/UIManager.cpp` | Modify | Set `cueEditorDockId_ = dockRightId` (tabs with Presets/Params); dock CueEditorPanel window |
| `src/ofApp.h` | Modify | Add `cueEditorPanel_` unique_ptr; add `midiLearnCallback_` (std::function); add `setMidiLearnCallback()` method |
| `src/ofApp.cpp` | Modify | Include CueEditorPanel.h; instantiate+register panel (after PresetPanel); gate newMidiMessage with callback check; clear callback on exit |

## Interfaces / Contracts

```cpp
// CueDef — modified (src/sequencer/SceneSequencer.h)
struct CueDef {
    int cue;
    std::string sceneId;
    std::string presetName;
    float morphDuration;
    int midiPC = -1;  // NEW: -1 = unassigned, 0-127 = assigned
};

// SceneSequencer — new public methods
bool addCue(const CueDef& cue);
bool removeCue(int cueNumber);
bool updateCue(int cueNumber, const CueDef& newDef);
bool saveSequence(const std::string& path) const;
bool reorderCues(int fromIndex, int toIndex);

// ofApp — new member + method
std::function<bool(ofxMidiMessage&)> midiLearnCallback_;
void setMidiLearnCallback(std::function<bool(ofxMidiMessage&)> cb);

// CueEditorPanel constructor
CueEditorPanel(SceneSequencer* seq, SceneEngine* engine,
               PresetManager* presets, ofxMidiIn* midiIn,
               std::function<void(std::function<bool(ofxMidiMessage&)>)> registerLearn);
```

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Unit | SceneSequencer mutation + uniqueness | Test in code via temporary `main` or compile-time assertions |
| Integration | Save → Load round-trip with midiPC | Write test sequence, reload, assert equality |
| Manual | UI table rendering, Learn capture | Visual verification with MIDI controller |

Note: project has no unit test framework (`strict_tdd: false, test_runner: null`). Correctness is verified via build (`make`) and manual testing.

## Error Handling Strategy

| Error | Detection | Response |
|-------|-----------|----------|
| Duplicate cue number | `isDuplicateCueNumber()` scans cues_[] before insert | Return false + `ofLogError`; UI shows red inline text |
| Orphan preset ref | `PresetManager::getPresets()` check per row in draw() | Warning icon (⚠️), non-blocking |
| Missing sequence.json | `loadSequence` already handles — log warning | Empty cue list; Add button still available |
| MIDI port unavailable | `midiIn->getNumInPorts() == 0` | Learn button disabled with tooltip |
| Save write error | `ofFile` fails | Return false + `ofLogError`; UI shows error popup |

## Integration Points

- **Dock layout**: `cueEditorDockId_ = dockRightId` — tabs with Presets and Parameters in right column (existing pattern: `presetsDockId_ = dockRightId`)
- **Sequence path**: `"config/sequence.json"` — same path used by `loadSequence`
- **ofApp wiring**: Instantiate after PresetPanel (needs `presetManager_`, `sceneEngine*`, `sceneSequencer_`, `&midiIn_`); `midiLearnCallback_` initialized to `nullptr`
- **Load-time**: `loadSequence` already parses `midiPC` field (added to existing loop); defaults to -1 when absent

## Migration / Rollout

No migration required. New fields are backward-compatible: `midiPC` defaults to -1, existing sequence.json files without the field load correctly. `saveSequence` writes the new field — regenerated files are fully compatible.
