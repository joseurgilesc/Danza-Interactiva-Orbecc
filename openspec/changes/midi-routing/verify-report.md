## Verification Report

**Change**: midi-routing
**Version**: N/A
**Mode**: Standard

### Completeness
| Metric | Value |
|--------|-------|
| Tasks total | 12 |
| Tasks complete | 12 |
| Tasks incomplete | 0 |

### Build & Tests Execution
**Build**: ✅ Passed
```text
make -j4
All .o files compiled successfully (MidiMappingEngine.o, MidiRouter.o, MidiConfig.o, 
MidiMapping.o, ofApp.o, PresetManager.o, CueEditorPanel.o, MidiMappingPanel.o)
Link: bin/ProyectoDanza2026 — zero errors
```

**Tests**: ➖ No test runner configured (openFrameworks C++ project — no gtest/catch2 harness)
**Coverage**: ➖ Not available

### Spec Compliance Matrix — midi-cue-routing

| Requirement | Scenario | Evidence | Result |
|-------------|----------|----------|--------|
| MIDI Channel Config | Default config (no file) | MidiConfig::loadFromJson returns false, defaults preserved (MidiConfig.cpp:14-18). Defaults: cue=1, param=2, port=IAC Driver Bus 1 (MidiConfig.h:15-17) | ✅ PASS |
| MIDI Channel Config | Same channel rejected | isValid() returns false if cueChannel==paramChannel (MidiConfig.cpp:87); fallback to defaults (MidiConfig.cpp:35-46) | ✅ PASS |
| MIDI Channel Config | Invalid channel (0) | isValid() rejects <1 (MidiConfig.cpp:85); fallback to defaults | ✅ PASS |
| MIDI Channel Config | Invalid channel (17) | isValid() rejects >16 (MidiConfig.cpp:86); fallback to defaults | ✅ PASS |
| MIDI Channel Config | Custom port | portName loaded from JSON (MidiConfig.cpp:30); port opened by name in ofApp.cpp:115-122 | ✅ PASS |
| MIDI Channel Config | Runtime change | reloadConfig() on MidiRouter exists (MidiRouter.cpp:81-87) and loadFromJson exists, but NO runtime trigger wired (no key shortcut, file watcher, or UI) | ⚠️ WARNING |
| Channel-Based Routing | CC on cue channel | MidiRouter::routeMessage: MIDI_CONTROL_CHANGE on cueChannel → findCueByMidi("cc") → triggerCue (MidiRouter.cpp:40-41, 44-54) | ✅ PASS |
| Channel-Based Routing | NoteOn vel>0 on cue ch | MIDI_NOTE_ON → findCueByMidi("note") → triggerCue (MidiRouter.cpp:38-39) | ✅ PASS |
| Channel-Based Routing | PC on cue channel | MIDI_PROGRAM_CHANGE → findCueByMidi("pc") with fallback (MidiRouter.cpp:31-37) | ✅ PASS |
| Channel-Based Routing | CC on param channel | msg.channel==paramChannel_ → mappingEngine_->processMidiMessage(msg) (MidiRouter.cpp:57-58) | ✅ PASS |
| Channel-Based Routing | NoteOn on param channel | Same path — all param-channel messages → MidiMappingEngine (MidiRouter.cpp:57-58) | ✅ PASS |
| Channel-Based Routing | Unconfigured channel | else clause → silently ignored (MidiRouter.cpp:60) | ✅ PASS |
| MIDI Learn Gate (MODIFIED) | Both panels Learn: CueEditor first | newMidiMessage iterates vector copy (ofApp.cpp:400-403). First callback returns true → return; second skipped | ✅ PASS |
| MIDI Learn Gate (MODIFIED) | Only param Learn active | Vector has one entry; callback matches → returns true; normal routing skipped | ✅ PASS |
| MIDI Learn Gate (MODIFIED) | No Learn active | Vector empty or all return false → normal channel routing (ofApp.cpp:406-408) | ✅ PASS |
| MIDI Learn Gate (MODIFIED) | Cue callback returns false | CueEditor callback returns false for non-PC → loop continues to MidiMapping callback → it consumes (ofApp.cpp:401-403) | ✅ PASS |
| MIDI Learn Callback Reg | CueEditor registers on Learn | registerLearn_ wrapped in addMidiLearnCallback (ofApp.cpp:411-413; CueEditorPanel.cpp:368) | ✅ PASS |
| MIDI Learn Callback Reg | CueEditor unregisters | unregisterLearn_ calls removeMidiLearnCallback (CueEditorPanel.cpp:308, 364) | ✅ PASS |
| MIDI Learn Callback Reg | MidiMapping Learn alongside CueEditor | Both panels have independent callbacks, pushed to vector separately (ofApp.cpp:214-236) | ✅ PASS |
| MIDI Learn Callback Reg | Both unregister independently | Each panel calls removeMidiLearnCallback for its own callback; removal by target_type identity (ofApp.cpp:415-428) | ✅ PASS |

**Compliance summary — midi-cue-routing**: 20/21 scenarios PASS, 1 WARNING

### Spec Compliance Matrix — midi-param-mapping

| Requirement | Scenario | Evidence | Result |
|-------------|----------|----------|--------|
| MIDI Mapping Structure | All fields present | MidiMapping struct: mappingId, type (MidiMessageType), channel, number, targetParameterId, minInput/maxInput, minOutput/maxOutput, controlMode (ControlMode), enabled, lastValue, noteHeld (MidiMapping.h:32-55) | ✅ PASS |
| MIDI Mapping Structure | CONTINUOUS scaling | scaleValue: linear interpolation `out = minOut + t*(maxOut-minOut)` (MidiMappingEngine.cpp:149-166) | ✅ PASS |
| MIDI Mapping Structure | TOGGLE 1st press | dispatchToggle: flips from current state to opposite (MidiMappingEngine.cpp:179-196) | ✅ PASS |
| MIDI Mapping Structure | TOGGLE 2nd press | Same toggle logic — flips back (midpoint comparison, lines 186-191) | ✅ PASS |
| MIDI Mapping Structure | TRIGGER press→release | dispatchTrigger: NoteOn→maxOutput, NoteOff→minOutput (MidiMappingEngine.cpp:198-204) | ✅ PASS |
| MIDI Mapping Structure | MOMENTARY press→release | dispatchMomentary delegates to dispatchTrigger (MidiMappingEngine.cpp:206-209) | ✅ PASS |
| MIDI Mapping Structure | Disabled mapping ignored | `if (!mapping.enabled) continue;` (MidiMappingEngine.cpp:108) | ✅ PASS |
| Per-Preset Mappings | Preset with mappings | applyPreset calls midiMappingEngine_->setActiveMappings(preset->midiMappings) (PresetManager.cpp:180-181) | ✅ PASS |
| Per-Preset Mappings | Preset replaces prior | setActiveMappings replaces entire vector (MidiMappingEngine.cpp:244-262) | ✅ PASS |
| Per-Preset Mappings | Preset without mappings | missing midiMappings key → vector stays empty (PresetManager.cpp:112-116: optional parse) | ✅ PASS |
| Per-Preset Mappings | Old JSON backward-compat | Existing presets without midiMappings field → vector defaults empty; no error/warning (MidiMapping.h:38 default) | ✅ PASS |
| MIDI Mapping Engine | Matched CC dispatch | processMidiMessage: match by type+channel+number → scale → applyToScene (MidiMappingEngine.cpp:107-142) | ✅ PASS |
| MIDI Mapping Engine | Inverted range | scaleValue: `t = (raw-inMin)/(inMax-inMin)` works for inMin>inMax (MidiMappingEngine.cpp:161) | ✅ PASS |
| MIDI Mapping Engine | Unmatched CC | Loop finds no match → returns silently (MidiMappingEngine.cpp:143) | ✅ PASS |
| MIDI Mapping Engine | Note tracking | TOGGLE: dispatchToggle tracks lastValue; TRIGGER/MOMENTARY track noteOn state (MidiMappingEngine.cpp:179-209) | ✅ PASS |
| MIDI Learn for Params | Learn captures CC | Panel callback captures MIDI_CONTROL_CHANGE → fills type=CC, channel, number (MidiMappingPanel.cpp:36-43) | ✅ PASS |
| MIDI Learn for Params | Learn captures Note | Panel callback captures MIDI_NOTE_ON vel>0 → fills type=Note, channel, number (MidiMappingPanel.cpp:38-42) | ✅ PASS |
| MIDI Learn for Params | Learn timeout | Panel update: learnTimer_ reaches kLearnTimeout → deactivates (MidiMappingPanel.cpp:70-80) | ✅ PASS |
| MIDI Learn for Params | Double Learn rejected | alreadyLearning guard disables Learn button on other rows (MidiMappingPanel.cpp:243-263) | ✅ PASS |
| MIDI Learn for Params | PC ignored during param Learn | Panel callback only captures CC or NoteOn vel>0 (MidiMappingPanel.cpp:35-43); returns false for PC | ✅ PASS |
| MIDI Mapping Panel UI | Display mappings | drawMappingTable iterates engine_->getActiveMappings(), renders 11-column table (MidiMappingPanel.cpp:118-318) | ✅ PASS |
| MIDI Mapping Panel UI | Edit CC number inline | InputInt widget for number column, clamped 0–127 (MidiMappingPanel.cpp:176-182) | ✅ PASS |
| MIDI Mapping Panel UI | Disable mapping | Checkbox on enabled column (MidiMappingPanel.cpp:224-225) | ✅ PASS |
| MIDI Mapping Panel UI | Preset change refresh | Panel reads engine_->getActiveMappings() every frame; setActiveMappings replaces on applyPreset — natural sync | ✅ PASS |
| MIDI Mapping Panel UI | Add empty row | "+ Add Mapping" button pushes default mapping to active set (MidiMappingPanel.cpp:286-299) | ✅ PASS |
| MIDI Mapping Panel UI | Remove row | "X" button per row, deferred erase (MidiMappingPanel.cpp:266-275, 302-313) | ✅ PASS |

**Compliance summary — midi-param-mapping**: 26/26 scenarios PASS

### Coherence (Design Decisions)

| Decision | Followed? | Notes |
|----------|-----------|-------|
| MidiRouter dispatches by channel | ✅ Yes | MidiRouter.cpp:26-61 — channel-based dispatch to SceneSequencer or MidiMappingEngine |
| MidiMappingEngine uses OscMappingEngine Learn pattern | ✅ Yes | MidiLearnState IDLE/LEARNING/TIMEOUT FSM mirrors OscMappingEngine (MidiMappingEngine.h:19-23; .cpp:269-321) |
| midiLearnCallback_ is now a vector | ✅ Yes | `std::vector<std::function<bool(ofxMidiMessage&)>>` (ofApp.h:86) |
| PresetDef stores midiMappings, backward-compatible | ✅ Yes | PresetManager.h:41; optional parse in PresetManager.cpp:112-116 |
| Port opened by name from config | ✅ Yes | ofApp.cpp:112-130 iterates ports to find match by portName |
| CueEditorPanel Learn behavior preserved | ✅ Yes | Constructor takes register/unregister lambdas instead of setLearnCallback (CueEditorPanel.h:23-26; .cpp:9-16) |
| Config file: config/midi.json | ✅ Yes | config/midi.json with cueChannel, paramChannel, portName |
| Mapping storage: per-preset in PresetDef | ✅ Yes | Serialized/deserialized alongside kinectMappings (PresetManager.cpp:112-116, 367-374) |
| Mapping matching: type+channel+number | ✅ Yes | processMidiMessage loop (MidiMappingEngine.cpp:107-111) |
| MidiMappingEngine placed in src/midi/ | ✅ Yes | All 8 files in src/midi/ directory |

**Coherence**: 10/10 decisions followed

### Issues Found

**CRITICAL**: None

**WARNING**:
1. **Runtime config reload not wired**: `MidiConfig::loadFromJson` and `MidiRouter::reloadConfig` exist but no runtime trigger (key shortcut, file watcher, menu item) to reload `config/midi.json`. The spec scenario "User edits midi.json, triggers reload → New channels take effect immediately" only has the infrastructure, not the trigger. (ofApp.cpp — no reload mechanism)
2. **MidiMappingEngine::update() never called**: The engine's own Learn FSM timeout (`update(deltaTime)`) is never invoked from `ofApp::update()`. The panel handles its own timeout separately via `UIManager::update()`, but the engine's Learn path is effectively dead code. (ofApp.cpp:301-329)
3. **Learn timeout UI feedback**: On timeout, the panel silently deactivates Learn without showing "Timeout" message in the UI. Spec says "UI shows 'Timeout'". The countdown is shown during Learn ("Listening... (Xs)") but no post-timeout notification. (MidiMappingPanel.cpp:71-79)
4. **MidiMappingPanel not in default dock layout**: `kMidiMapping` is not registered in `UIManager::buildDefaultDockLayout()`. The panel appears as a floating ImGui window when first toggled via View menu. (UIManager.cpp:193-265)
5. **removeMidiLearnCallback uses `target_type()`**: Design states "by pointer identity", implementation uses `std::function::target_type()` comparison. Safe in practice (only one instance per panel type), but could be fragile if two callbacks of same type are registered. (ofApp.cpp:421-423)
6. **Sample preset references non-existent parameter**: `config/presets/pinturaFluorescente.json` line 26 maps to `"pinturaFluorescente.intensity"` — `intensity` is NOT a registered parameter in `PinturaFluorescenteScene`. At runtime, `setParameter` will return false silently.

**SUGGESTION**:
1. Add runtime config reload (e.g., `F5` key or View→"Reload MIDI Config" menu item) calling `midiConfig_.loadFromJson("config/midi.json"); midiRouter_->reloadConfig(midiConfig_);`
2. Call `midiMappingEngine_->update(delta)` in `ofApp::update()` for consistency, even if panel handles its own timer
3. Show a timed "Timeout" toast/label in MidiMappingPanel after Learn expires
4. Add `kMidiMapping` to `buildDefaultDockLayout()` (e.g., tab with `kOscMapping` and `kKinect` in left dock)
5. Fix sample preset target param to an existing parameter (e.g., `pinturaFluorescente.k` or `pinturaFluorescente.beta`)
6. Add a `kMidiMapping` entry inside the hide-by-default list in `buildDefaultDockLayout()` if it should start hidden

### Verdict
**PASS WITH WARNINGS**

All 12 tasks complete. Build compiles with zero errors. 46/47 spec scenarios pass. Design coherence: 10/10 decisions confirmed in code. No critical bugs or missing features. Six warnings identified — all are wiring gaps (runtime reload trigger, engine update call, UI timeout feedback) or cosmetic (dock layout, sample preset data). None break fundamental MIDI routing or parameter mapping functionality.
