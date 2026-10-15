# Proposal: MIDI Cue Editor

## Intent

Users hand-edit `config/sequence.json` to define cue scenes, presets, and MIDI assignments — no runtime editing exists. This change adds a GUI panel for creating, editing, deleting, and reordering cues at runtime, plus MIDI Learn for assignments. The performance `SequencerPanel` stays untouched.

## Scope

### In Scope

- **SceneSequencer mutation API**: `addCue`, `removeCue`, `updateCue`, `saveSequence`, `reorderCues`
- **CueEditorPanel** (new `IUIModule`): editable table with scene/preset combos, morph slider, manual MIDI PC input, per-row Delete, global Add/Save, MIDI Learn button per cue
- **MIDI Learn intercept**: `newMidiMessage()` checks learn mode before normal routing; assigns captured PC to target cue
- **Dock layout**: panel tabs with Presets (right column), visible by default

### Out of Scope

- OSC-based cue editing
- Cue auto-play / crossfade / triggering during edit
- Modifying `SequencerPanel` (stays read-only)
- Changing existing MIDI routing logic (only inserting a learn gate before it)

## Capabilities

### New Capabilities

None — all changes extend existing capabilities.

### Modified Capabilities

- **scene-sequencer**: adding mutation methods (add/remove/update/save/reorder)
- **sequencer-ui**: adding CueEditorPanel as new editable view alongside read-only SequencerPanel
- **midi-cue-routing**: adding MIDI Learn mode that intercepts PC messages before cue routing

## Approach

**Backend**: Add `addCue(CueDef)`, `removeCue(int cueNumber)`, `updateCue(int cueNumber, CueDef)`, `saveSequence(path)`, `moveCue(from, to)` to `SceneSequencer`. `saveSequence` writes `{"cues": [...]}` JSON matching `loadSequence` format. Maintain cue number uniqueness on add/update.

**UI**: New `CueEditorPanel` follows `PresetPanel` pattern — takes `SceneSequencer*` + `SceneEngine*`, implements `IUIModule`. Table with editable fields: ImGui `InputInt` for cue number, `Combo` for scene (from `SceneEngine::getLoadedScenes()`), `Combo` for preset (from `PresetManager::getPresets(sceneId)` plus "(none)"), `DragFloat` for morph duration. "Learn" button per row toggles MIDI learn via callback registered with `ofApp`.

**MIDI Learn**: `ofApp` exposes `setMidiLearnCallback(std::function<bool(ofxMidiMessage&)>)`. `newMidiMessage()` calls it first; if callback returns `true` (consumed), skip normal routing. Editor registers on first Learn click and unregisters on capture.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/sequencer/SceneSequencer.h/.cpp` | Modified | Add 5 mutation methods + JSON serialization |
| `src/ui/CueEditorPanel.h/.cpp` | **New** | Editable cue table panel (IUIModule) |
| `src/ui/PanelIds.h` | Modified | Add `kCueEditor` constant |
| `src/ui/UIManager.h/.cpp` | Modified | Add `cueEditorDockId_` member, dock with Presets |
| `src/ofApp.h` | Modified | Add `cueEditorPanel_` unique_ptr, MIDI learn callback member |
| `src/ofApp.cpp` | Modified | Wire CueEditorPanel, add learn gate in `newMidiMessage()` |
| `config/sequence.json` | Read/write | Target for `saveSequence()` |
| `openspec/specs/scene-sequencer/spec.md` | Delta spec | Mutation requirements |
| `openspec/specs/sequencer-ui/spec.md` | Delta spec | Editor panel requirements |
| `openspec/specs/midi-cue-routing/spec.md` | Delta spec | MIDI Learn intercept requirements |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Cue number duplicates after edit | Medium | Validate uniqueness on save; show inline error |
| Orphan preset references (deleted after cue created) | Low | Show warning icon; triggerCue already handles gracefully |
| External JSON edit race | Low | Acceptable for live tool; save warns if file mtime changed |
| MIDI port absent during Learn | Low | Disable Learn button when `midiIn_.getNumInPorts() == 0` |
| Save format mismatch with loadSequence | Medium | Single JSON serializer used for both; validate round-trip in tests |

## Rollback Plan

1. Remove `#include "ui/CueEditorPanel.h"` and panel registration from `ofApp.cpp`
2. Remove MIDI learn callback member from `ofApp.h` and gate from `newMidiMessage()`
3. Commentary-out: `CueEditorPanel.h/.cpp`, `kCueEditor` constant, dock ID member
4. SceneSequencer mutation methods are additive — no existing API changes, so they don't need rollback (they'll just be unused)
5. Rebuild with `make`

## Dependencies

- `SceneEngine::getLoadedScenes()` — already exists
- `PresetManager::getPresets(sceneId)` — already exists
- `ofxMidiIn` — already wired in `ofApp`
- `UIManager::addModule()` / dock builder — already in place

## Success Criteria

- [ ] Add a cue via GUI → appears in `config/sequence.json` after Save
- [ ] Edit scene/preset/morph per cue → save → reload persists changes
- [ ] MIDI Learn: click Learn, send PC 42 → cue number updates to 43
- [ ] Delete a cue → save → cue removed from JSON and table
- [ ] SequencerPanel still works — reads updated cues after Save
- [ ] Normal MIDI routing still works when Learn is inactive
- [ ] Build passes with `make`
