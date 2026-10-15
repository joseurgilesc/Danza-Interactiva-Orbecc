## Exploration: MIDI Cue Editor UI

### Current State

**End-to-end sequencer flow (ofApp → MIDI → SceneSequencer → PresetManager → SceneEngine)**

1. **Data source**: `config/sequence.json` — a `{"cues": [...]}` JSON array with 11 cues mapping MIDI PC 0–10 to scene+preset combinations.
2. **Loading** (`src/ofApp.cpp:60-61`): `SceneSequencer` loads the JSON at startup, parsing each entry into a `CueDef` struct (`cue`, `sceneId`, `presetName`, `morphDuration`).
3. **MIDI routing** (`src/ofApp.cpp:325-339`): `ofxMidiIn` listener in `newMidiMessage()` translates Program Change `pitch` → cue number (`pitch + 1`), then calls `triggerCue()`. Gated by `cueRoutingEnabled_`. Keyboard shortcuts (Shift+1–9, `[`, `]`) provide fallback.
4. **SceneSequencer** (`src/sequencer/`) — READ-ONLY API:
   - `loadSequence(path)` — parses JSON into `std::vector<CueDef> cues_`
   - `triggerCue(cueNum)` — validates scene exists in SceneEngine → validates preset exists in PresetManager → applies preset with morph → transitions scene
   - `triggerNext()/triggerPrev()` — sequential navigation with boundary clamping
   - Accessors: `getCueCount()`, `getCue(index)`, `getCurrentCueIndex()`, `hasSequence()`
   - **No mutation methods**: cannot add, remove, update, reorder, or save cues at runtime
5. **PresetManager** (`src/presets/`) — loads `config/presets/<sceneId>.json`, provides `applyPreset(scene, name, duration)` with morph lerping. Has `addOrUpdatePreset()`, `deletePreset()`, `captureCurrentParams()`, `savePresets()` — so it already supports CRUD.
6. **Comparing with PresetManager**: PresetManager has both a read API and a write API (`addOrUpdatePreset`, `deletePreset`, `savePresets`, `captureCurrentParams`). SceneSequencer has only a read API. The UI pattern to follow is PresetPanel — it shows data, lets users create/update/delete, and calls save.

**Current UI (`SequencerPanel` — `src/ui/SequencerPanel.cpp`)**
- ImGui table: columns Cue#, Scene, Preset, Morph (4 columns, scrollable)
- Prev/Next buttons, current cue index display
- Row highlight for current cue (green background)
- Row click triggers cue
- **Fully read-only** — no inline editing, no Add/Remove/Save buttons
- Implements `IUIModule`, registered in `UIManager::buildDefaultDockLayout()` docked in bottom strip (tabs with Transport)
- Panel ID: "Sequencer" (`panel_ids::kSequencer`)

### Affected Areas

| File | Role | Change needed |
|---|---|---|
| `src/sequencer/SceneSequencer.h` | Cue model + API | Add mutation methods (`addCue`, `removeCue`, `updateCue`, `saveSequence`) |
| `src/sequencer/SceneSequencer.cpp` | Implementation | Implement CRUD operations and JSON write-back |
| `src/ui/SequencerPanel.cpp` | Current read-only UI | Either convert to editable OR leave as-is and create new panel |
| `src/ui/PanelIds.h` | Panel ID constants | Possibly add `kCueEditor` if separate panel |
| `src/ofApp.cpp` | MIDI routing + wiring | Add MIDI learn mode routing; pass SceneEngine* to editor panel |
| `src/ofApp.h` | Member declarations | Possibly add `cueLearnMode_` flag or editor panel unique_ptr |
| `config/sequence.json` | Data file | Read/write target (file already exists with 11 cues) |
| `openspec/specs/scene-sequencer/spec.md` | Spec | Add mutation API requirements |
| `openspec/specs/sequencer-ui/spec.md` | Spec | Add editing UI requirements |
| `openspec/specs/midi-cue-routing/spec.md` | Spec | Add MIDI learn mode requirement |
| `src/ui/UIManager.cpp` | Dock layout | Add new panel to dock if separate panel |
| `src/input/MidiPortsStore.h` | Already has `learning` field | Could be leveraged for port-level learn state (currently unused for cues) |

### Approaches

1. **Extend SequencerPanel (make it editable)**
   - Convert existing table rows to editable: sceneId combo, presetName combo, morphDuration drag float, cue number input
   - Add "Add Cue" button at bottom, "Delete" button per row, "Save" button
   - Toggle between "Perform" and "Edit" modes within the same panel
   - MIDI Learn: "Learn" button per row that captures next MIDI PC
   - Pros: Minimal file changes (1 panel file), shares dock space, consistent UX
   - Cons: Modal UX (Perform vs Edit toggle), clutters performance view with edit controls, violates single-responsibility — the panel becomes large and complex
   - Effort: Medium

2. **New `CueEditorPanel` (separate from SequencerPanel)**
   - `SequencerPanel` stays as the clean performance view (read-only, Prev/Next, row-click trigger)
   - New `CueEditorPanel` with full editing: table with editable fields, Add/Remove/Save buttons, MIDI Learn per row
   - Both panels share the same `SceneSequencer*` reference
   - Docked in a different area (e.g., tabbed with Presets, or bottom-right with Sequencer)
   - Pros: Clean separation of concerns, performance mode unaffected, each panel stays focused, follows existing pattern (PresetPanel is separate from ScenePanel)
   - Cons: More files (+2 .h/.cpp), need to coordinate state between two panels (editor changes reflected in read-only view)
   - Effort: Medium

3. **Unified panel with conditional rendering (Edit/Perform toggle)**
   - Single panel draws different UI based on a mode toggle button
   - Perform mode: current read-only table
   - Edit mode: editable table with CRUD controls
   - Pros: Single panel, one dock slot
   - Cons: Complex draw() logic, all state lives in one class, harder to test each mode independently
   - Effort: Medium-High

### Recommendation

**Approach 2: New `CueEditorPanel` (separate panel).**

Rationale:
- Follows the existing architecture pattern — the codebase already separates concerns: `ScenePanel` shows scenes, `PresetPanel` manages presets, `SequencerPanel` shows cue table. Adding editing as a separate panel is consistent.
- The `PresetPanel` is the exact UI pattern to mirror: it has combo selectors, input fields, Apply/Update/Delete buttons, and calls `savePresets()`. `CueEditorPanel` would do the same with `saveSequence()`.
- `SequencerPanel` is already useful in its current form during performances (actor sees what cue is active, can click to jump). Don't break that.
- `SceneSequencer` needs mutation methods regardless of which UI approach — that's the core work. The UI is a thin layer on top. Approach 2 adds ~2 files but the total implementation is well-scoped.
- `SceneSequencer` can notify observers (or the editor panel can poll `getCueCount()` every frame) — state coordination is minimal since ImGui redraws every frame anyway.

### MIDI Learn Approach

Current MIDI flow in `ofApp::newMidiMessage()`:
```cpp
if (cueRoutingEnabled_ && sceneSequencer_ && sceneSequencer_->hasSequence()) {
    if (message.status == MIDI_PROGRAM_CHANGE) {
        int cueNum = message.pitch + 1;
        sceneSequencer_->triggerCue(cueNum);
    }
}
```

**Proposed MIDI Learn flow**:
1. `CueEditorPanel` has a "Learn" button per cue row. Clicking it sets a `learnTarget_` (pointer/index to the cue being edited) and toggles `learnActive_ = true`.
2. `ofApp` gains a `cueLearnCallback_` or a flag `cueLearnMode_`. When the editor panel activates learn, it registers itself with ofApp (or sets a flag).
3. In `newMidiMessage()`, before the normal routing path, check `cueLearnMode_`:
   ```cpp
   if (cueLearnMode_ && cueEditorPanel_) {
       cueEditorPanel_->onMidiLearn(message);
       return; // Don't trigger cue
   }
   ```
4. `CueEditorPanel::onMidiLearn()` captures `message.pitch + 1` as the cue number, assigns it to the target cue's `cue` field, and deactivates learn mode.
5. The panel shows visual feedback: "Learning..." text, button highlight, or a popup.

**What messages to learn from**: At minimum, Program Change (0–127 → cue 1–128). Could extend later to Note On (note number → cue) or CC. Start with PC only — matches existing routing.

**Alternative**: Add a `setMidiLearnCallback(std::function<void(ofxMidiMessage&)>)` to `ofApp` that the editor panel sets. Cleaner than a raw pointer/flag.

### Risks

- **Cue number conflicts**: Two cues with the same `cue` number after editing. `SceneSequencer::findCueIndex()` returns first match — subsequent duplicates are ignored. Editor must validate uniqueness before save.
- **Preset deleted after cue references it**: Already handled gracefully — `triggerCue()` logs warning and proceeds without preset. Editor should show warning icon next to orphan cues.
- **Scene not loaded**: `triggerCue()` validates and rejects. Editor scene combo should only list scenes from `SceneEngine::getLoadedScenes()`.
- **MIDI port not available**: Learn mode should be disabled/ungrabbable when no MIDI port is open. The editor can check `midiIn_.getNumInPorts() > 0` via a method or flag from ofApp.
- **Sequence file missing on save**: `saveSequence()` should create the file. Existing `loadSequence()` handles missing file gracefully.
- **External JSON edit race**: If user edits `sequence.json` in a text editor while app is running, `saveSequence()` would overwrite their changes. Acceptable — this is an edge case for a live performance tool.
- **Save format consistency**: Must preserve `{"cues": [...]}` wrapper format. The JSON write must match what `loadSequence()` expects to read.
- **Empty preset lists**: Some scenes may have no presets. Preset combo should include "(none)" option and the cue's `presetName` clears to `""`.

### Ready for Proposal

Yes — the exploration is complete. The recommendation is clear (new CueEditorPanel + SceneSequencer mutation API), the touch points are identified, and the risks are catalogued. No blocking unknowns.

Next phase: **sdd-propose** — define scope, approach, and rollback plan.
