# OSC-to-Scene-Parameter Mapping Specification

## Purpose

Receive OSC from Ableton Live (M4L Connection Kit: OSC Send, OSC MIDI Send) and map to scene parameters. Existing OSCAdapter routing (fader/toggle/push/bpm/play/stop) MUST continue unaffected.

## Requirements

### R-1: Mapping Storage
The OscMappingEngine SHALL store/retrieve/delete `OscMapping` entries. Each entry MUST have: `oscAddress`, `targetParameterId`, `minInput`, `maxInput`, `frequencyMode`, `throttleFrames`, `enabled`. Uniqueness SHALL be enforced on `oscAddress + targetParameterId`.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Create | No mappings | Add `/XPos`→`pintura.k`, defaults | `getMappings()` returns one entry |
| Duplicate | `/Vel`→`anemone.speed` exists | Add same pair again | Rejected; warning logged |
| Delete | `map-3` exists | `removeMapping("map-3")` | Entry gone from `getMappings()` |

---

### R-2: Learning Mode
The engine SHALL support single-slot learning: activate for a target, next OSC message auto-assigns address, timeout after 10 s. Only one slot active at a time.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Learn address | Learning for `furry.density` | `/XPosition` 0.75 arrives | Mapping created; learning exits; UI signals success |
| Timeout | Learning for `bloom.radius` | 10 s elapse, no message | Cancel; `[warning]` logged; UI returns to idle |
| Double activation | Learning for `sceneA.p1` active | UI activates for `sceneB.p2` | Second rejected; first continues |

---

### R-3: OSC Message Processing
OSCAdapter MUST invoke `std::function<void(string,float)>` callback for every message AFTER built-in routing. Engine SHALL first check `/preset` and `/cue` addresses — if matched, route to PresetManager/SceneSequencer and skip mapping dispatch. Otherwise, proceed with existing mapping match and scaled dispatch to active scene via `SceneEngine::setParameter()`.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Dispatch | `pintura` active; `/XPos`→`pintura.k` mapped | `/XPos` 0.42 arrives | Engine matches, scales, calls `setParameter("k", scaled)` |
| /preset bypasses mapping | `/preset furry full` arrives | Engine processes | PresetManager invoked; mapping matcher NOT invoked |
| /cue bypasses mapping | `/cue 3` arrives | Engine processes | SceneSequencer invoked; mapping matcher NOT invoked |
| Inactive scene | `anemone` NOT active; `/Y`→`anemone.wave` mapped | `/Y` arrives | Silently skipped (no crash, no log) |
| Fader coexistence | `osc.json` prefix `rig`; `/rig/fader/2` mapped | `/rig/fader/2` 0.8 arrives | Fader handler runs AND engine dispatches |

---

### R-4: Value Scaling
Input values SHALL map from `[minInput, maxInput]` to parameter's `[min, max]` via linear interpolation. If `minInput > maxInput`, range SHALL invert.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Standard 0→1 | min=0, max=1; param [0,5] | OSC value 0.4 | Output 2.0 |
| MIDI 0→127 | min=0, max=127; param [0,1] | OSC MIDI value 63 | Output ≈0.496 |
| Inverted range | min=1, max=0; param [0,100] | OSC value 0.2 | Output 80.0 |

---

### R-5: Frequency Modes
Each mapping SHALL have an independent `OscFrequencyMode`: `EVERY_FRAME`, `ON_CHANGE` (`|Δ| > 0.001f`), or `THROTTLED` (≤1 dispatch per N frames, set via `throttleFrames`).

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Mode behavior | 3 mappings (all modes, THROTTLED N=5) | Same value 0.5 for 10 frames | EVERY_FRAME: 10×, ON_CHANGE: 1×, THROTTLED: 2× |

---

### R-6: JSON Persistence
Mappings SHALL save to `config/osc-mappings.json` on exit + explicit save. SHALL load on startup after scene registration.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Round-trip | 3 mappings saved | App restarts | All 3 restored identically |
| Missing file | File absent at startup | App initializes | `[info]` logged; empty set |
| Corrupt JSON | Invalid JSON in file | Load attempted | `[error]` logged; empty set |

---

### R-7: OscMappingPanel UI
Panel SHALL implement `IUIModule`, display a table (`OSC Address | Target | Min | Max | Freq | Enabled | [MAP]`), and provide Add/Remove/Save/Load buttons. SHALL be dockable in UIManager.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Display | One mapping exists | Panel draws | Table shows row; MAP button visible but inactive |
| MAP click | Row for `furry.density`; no learning active | Click [MAP] | Learning starts; button shows "Listening..." highlighted; other [MAP]s disabled |

---

### R-8: Application Wiring
OscMappingEngine SHALL be a member of `ofApp`. OscMappingPanel SHALL register with `UIManager`. Callback SHALL connect after OSCAdapter `startListening()`. Load after scenes registered; save on `ofApp::exit()`.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Full lifecycle | App launches normally | `setup()` completes | Engine instantiated; panel visible; callback connected; saved mappings loaded |

---

### R-9: Edge Cases & Error Handling
The system SHALL handle errors gracefully — no crashes, no corrupt state.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Unmapped address | No mapping for `/Unknown` | Message arrives | Silently ignored |
| Parameter missing | `/Addr`→`bad.param` mapped, param absent | Message arrives | `[warning]` logged; continue |
| Adapter not listening | OSCAdapter status `NOT_LISTENING` | Learning activated | Error feedback; auto-cancel |
| Ambiguous param name | Two scenes have param `speed`; mapping uses `speed` | Dispatch to active scene | Resolves via active scene's fully qualified ID; no ambiguity |

---

### R-10: Preset and Cue Address Routing
OscMappingEngine MUST process `/preset` and `/cue` addresses BEFORE existing mapping dispatch. These addresses SHALL NOT match stored OscMapping entries.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| /preset route | Scene loaded, preset "lite" exists | `/preset pinturaFluorescente lite` | `PresetManager::applyPreset("lite")` called; mapping dispatch skipped |
| /preset with duration | Scene loaded | `/preset pinturaFluorescente full 3.0` | `applyPreset("full", 3.0f)` called |
| /cue route | Sequencer loaded | `/cue 5` | `SceneSequencer::triggerCue(5)` called |

---

### R-11: Configurable Prefix
The address prefix SHALL be configurable (default: `/preset`, `/cue`). When customized, the original default addresses SHALL be ignored.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Default prefix | Config uses defaults | `/cue 2` arrives | Routed to SceneSequencer |
| Custom prefix | Config sets prefix to `/show/cue` | `/show/cue 2` arrives | Routed; `/cue 2` ignored |

---

### R-12: Error Handling for Preset/Cue Addresses
Invalid scene, preset name, or cue number in OSC messages SHALL log errors without crashing.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Unknown scene | No "ghost" scene loaded | `/preset ghost lite` | Error logged; no crash |
| Unknown preset | pinturaFluorescente loaded | `/preset pinturaFluorescente badPreset` | Error logged; no crash |
| Bad cue number | 5 cues loaded | `/cue 99` | Error logged; no crash |
