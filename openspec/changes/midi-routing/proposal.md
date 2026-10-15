# Proposal: MIDI Routing — Separación Cues vs Parámetros por Preset

## Intent

Separate the single MIDI entry point (`ofApp::newMidiMessage`) into two channel-isolated paths: channel 1 for sequencer cues (partially implemented, no channel filter) and channel 2+ for per-preset parameter mapping with MIDI Learn. Both channels configurable from UI or `config/midi-mappings.json`.

## Scope

### In Scope
- **MidiRouter**: channel dispatch — ch1 → cues, ch2+ → params
- **MidiMappingEngine**: MIDI CC/Note → `IBaseScene::setParameter()` with Learn
- **MidiMappingPanel**: ImGui mapping table per preset with Learn button
- **midiMappings** in `PresetDef`: storage + serialization (backward-compatible)
- **midiLearnCallbacks_ vector**: refactor single slot to multi-panel support
- **config/midi-mappings.json**: configurable channels (defaults: cue=1, param=2)

### Out of Scope
- OSC (OscMappingEngine/OscMappingPanel): untouched
- New MIDI ports: IAC Driver Bus 1 remains
- Standalone parameter mapping persistence: always per-preset

## Capabilities

### New Capabilities
- `midi-param-mapping`: per-preset MIDI→parameter mapping engine, ImGui panel with table + Learn, PresetDef persistence, channel-based dispatch via MidiRouter

### Modified Capabilities
- `midi-cue-routing`: single `midiLearnCallback_` → vector (first-consumer-wins); channel filter in `newMidiMessage()` separating ch1 (cues) from ch2+ (params); multi-panel Learn coexistence

## Approach

**MidiRouter** dispatches by channel in `newMidiMessage()`, reading channels from `config/midi-mappings.json`. Ch1 → `sceneSequencer_->findCueByMidi()`. Ch2+ → `midiMappingEngine_->processMidi()`.

**MidiMappingEngine** (follows OscMappingEngine pattern): stores `vector<MidiMapping>`, exposes `setActiveMappings()`/`clearActiveMappings()`, matches by channel+type+number → scale → `SceneEngine::setParameter()`. Learn: registers in shared callback vector, captures next message on param channel.

**MidiMappingPanel** (follows CueEditorPanel + OscMappingPanel): ImGui table (Channel, Type, CC/Note, Target, Range, Mode), MAP button for Learn, manual edit popup, Remove button.

**PresetDef** gains `vector<MidiMapping> midiMappings`. `applyPreset()` calls `midiMappingEngine_->setActiveMappings(preset.midiMappings)`. Existing JSONs without the field → empty vector.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/ofApp.cpp/.h` | MODIFY | Channel routing, vector callbacks, engine instantiation |
| `src/midi/MidiRouter.h` | CREATE | Configurable channel dispatch |
| `src/midi/MidiMappingEngine.h/.cpp` | CREATE | Mapping engine with Learn (~200 LOC) |
| `src/ui/MidiMappingPanel.h/.cpp` | CREATE | ImGui panel (~300 LOC) |
| `src/presets/PresetManager.h/.cpp` | MODIFY | `midiMappings` field, serialize, hook in applyPreset |
| `src/ui/CueEditorPanel.cpp` | MODIFY | Adapt to callback vector (~5 LOC change) |
| `src/ui/PanelIds.h` | MODIFY | Add `kMidiMapping` |
| `config/midi-mappings.json` | CREATE | Default channels and port |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| CueEditorPanel regression from callback refactor | Low | Only registration changes: `=` → `push_back`; iteration with first-consumer-wins |
| Existing preset JSONs without `midiMappings` | Low | Optional field; `ofJson` omits missing keys, parse defaults to empty |
| Channel 1 conflict between cue and param | Low | MidiRouter enforces exclusivity: ch1=cues, ch2+=params only |
| Simultaneous Learn mode collision | Low | First registered callback consumes; UI documents behavior |

## Rollback Plan

1. Revert `midiLearnCallbacks_` to single slot (CueEditorPanel code compatible with both)
2. Remove MidiRouter → restore unfiltered `newMidiMessage()`
3. Drop `midiMappings` from PresetDef and PresetManager
4. Delete new files: `src/midi/`, `src/ui/MidiMappingPanel.*`, `config/midi-mappings.json`
5. Each step is independent and preserves existing functionality

## Dependencies

None. `SceneEngine`, `PresetManager`, `IBaseScene::setParameter()`, `UIManager`, and `ofxMidi` already exist.

## Success Criteria

- [ ] MIDI PC/Note/CC on ch1 triggers correct cue without param channel interference
- [ ] MIDI CC on ch2 maps to active scene parameter with correctly scaled value
- [ ] MIDI Learn captures channel+type+number and assigns to selected mapping
- [ ] `applyPreset()` activates preset MIDI mappings without affecting cues
- [ ] Presets without `midiMappings` load without errors (backward-compatible)
- [ ] Missing `config/midi-mappings.json` → defaults (cue=1, param=2)
