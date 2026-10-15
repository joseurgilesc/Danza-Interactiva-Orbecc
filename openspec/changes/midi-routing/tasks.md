# Tasks: MIDI Routing — Separación Cues vs Parámetros por Preset

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~900–1000 (6 new files, 4 modified) |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Delivery strategy | ask-on-risk |
| Chain strategy | pending |
| Decision needed before apply | Yes |

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: pending
400-line budget risk: High

### Suggested Work Units (Chained PRs)

| Unit | Goal | Base | ~LOC |
|------|------|------|------|
| 1 | Foundation types, config, router, engine (no behavior change) | feature/midi-routing | ~270 |
| 2 | Refactor callback → vector, PresetDef extension | PR 1 branch | ~120 |
| 3 | ofApp wiring: instantiate engines, load config, auto-open port | PR 2 branch | ~100 |
| 4 | MidiMappingPanel UI + sample preset data | PR 3 branch | ~380 |

Each PR autonomous with build verification. No runtime impact until PR 3.

---

## Phase 1: Foundation (no existing code modified)

- [x] 1.1 Create `src/midi/MidiMappingEngine.h` — `ControlMode` enum, `MidiMapping` struct with JSON serialization helpers
- [x] 1.2 Create `src/midi/MidiRouter.h` — `MidiConfig` struct (load/save/validate json, defaults: ch1 cues, ch2 params, port "IAC Driver Bus 1")
- [x] 1.3 Create `src/midi/MidiRouter.cpp` — `MidiRouter::routeMessage()` channel dispatch to SceneSequencer/MidiMappingEngine, `setChannel()`, `reloadConfig()`
- [x] 1.4 Create `src/midi/MidiMappingEngine.cpp` — `processMidiMessage()` match+scale+dispatch, `setActiveMappings()`, `startLearning()`/`cancelLearning()`, Learn timeout

## Phase 2: Core Refactors

- [x] 2.1 Refactor `ofApp::midiLearnCallback_` single `function<…>` → `vector<function<…>>`; add `addMidiLearnCallback()`/`removeMidiLearnCallback()`; update `newMidiMessage()` Learn gate to iterate vector first-consumer-wins
- [x] 2.2 Update `CueEditorPanel` constructor: `registerLearn` now calls `addMidiLearnCallback`/`removeMidiLearnCallback` via stored function pointers
- [x] 2.3 Add `std::vector<MidiMapping> midiMappings` to `PresetDef`; extend `PresetManager` constructor to accept `MidiMappingEngine*`; serialize/deserialize `midiMappings` in `loadPresets()`/`savePresets()`; hook `setActiveMappings()` in `applyPreset()`

## Phase 3: Integration & Wiring

- [x] 3.1 Add `midiRouter_`, `midiMappingEngine_`, `MidiConfig` members to `ofApp`; instantiate in `setup()` after `sceneSequencer_`; load `config/midi.json` (auto-create defaults if missing); open `midiIn_` by `portName` not index 0
- [x] 3.2 Create `config/midi.json` with `{"cueChannel":1,"paramChannel":2,"portName":"IAC Driver Bus 1"}`
- [x] 3.3 Add `kMidiMapping = "MIDI Mappings"` to `PanelIds.h`

## Phase 4: UI Panel

- [x] 4.1 Create `src/ui/MidiMappingPanel.h/.cpp` — `IUIModule` ImGui table: Type, Channel, CC/Note#, Target, Min/Max In/Out, Mode, Enabled, Learn. Add/Remove/Edit rows. Learn captures next param-channel message; timeout after 10s. Refreshes via `OnPresetApplied` callback. Register in `ofApp::setup()` via `uiManager.addModule()`.
- [x] 4.2 Add sample `midiMappings` to one preset JSON (e.g., `pinturaFluorescente.json`) for manual testing

## Verification

Per task: compile, run, verify via spec scenarios. Build after every phase.
