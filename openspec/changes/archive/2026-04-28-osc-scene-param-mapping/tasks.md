# Tasks: OSC-to-Scene-Parameter Mapping

## Phase 1: Foundation (data structures + engine core)

- [x] 1.1 **Create `src/mapping/OscMappingEngine.h`** — `OscFrequencyMode` + `LearnState` enums, `OscMapping` struct (13 fields), engine class declaration (public/private API per design §Class Design). `S`
- [x] 1.2 **Create `src/mapping/OscMappingEngine.cpp`** — CRUD: `addMapping()` with duplicate rejection, `removeMapping()`, `getMappings()`, `findMapping()`, `generateId()`, `findDuplicate()`. `M`

## Phase 2: Engine Logic (processing, scaling, frequency)

- [x] 2.1 **Implement `processOscMessage()`** — match `oscAddress`, resolve `targetParameterId` via `SceneEngine::getScene()` + `setParameter()`; skip inactive scenes silently (R-3 scenarios). `M`
- [x] 2.2 **Implement `scaleValue()`** — linear interp `[minInput,maxInput]→[paramMin,paramMax]`; invert when `minInput>maxInput` (R-4: 0→1, 0→127 MIDI, inverted ranges). `S`
- [x] 2.3 **Implement `shouldDispatch()` + `update()` frame counter** — EVERY_FRAME always true, ON_CHANGE `|Δ|>0.001f`, THROTTLED `frameCounter%throttleFrames==0` (R-5). `S`

## Phase 3: Learning Mode (single-slot FSM)

- [x] 3.1 **Implement learning FSM methods** — `startLearning()` (reject if already LEARNING), `cancelLearning()`, `getLearnState()`, `getLearningTarget()`, `getLearnTimeRemaining()`. `S`
- [x] 3.2 **Integrate in `processOscMessage()` + `update()`** — if LEARNING, auto-create mapping from next OSC message; 10s timeout in `update()` resets to IDLE with `[warning]` log (R-2). `S`

## Phase 4: Persistence (JSON I/O via nlohmann/json)

- [x] 4.1 **Implement `saveToJson()`** — serialize all mappings (exclude `lastValue`/`frameCounter`) to `config/osc-mappings.json` as JSON array (R-6 round-trip). `M`
- [x] 4.2 **Implement `loadFromJson()`** — deserialize array; handle missing file (`[info]`), corrupt JSON (`[error]`), restore all fields (R-6 missing/corrupt scenarios). `M`

## Phase 5: OSCAdapter Integration (callback mechanism)

- [x] 5.1 **Modify `src/adapters/OSCAdapter.h`** — add `using OscMessageCallback = std::function<void(const std::string&, float)>;` setter `setOscMessageCallback()`, and `oscMessageCallback_` private member. `S`
- [x] 5.2 **Modify `src/adapters/OSCAdapter.cpp::update()`** — after the `} else { ... }` block (line 187) and BEFORE `processed++` (line 189), insert: `if (oscMessageCallback_) oscMessageCallback_(addr, msg.getNumArgs()>0?msg.getArgAsFloat(0):0.0f);` — fires for ALL messages after built-in routing (R-3 fader coexistence). `S`

## Phase 6: UI Panel (OscMappingPanel IUIModule)

- [x] 6.1 **Create `src/ui/OscMappingPanel.h`** — `IUIModule` subclass: toolbar/table/learning-indicator methods, `OscMappingEngine*` + `SceneEngine*` members, filter buf, inline edit bufs. `S`
- [x] 6.2 **Create `src/ui/OscMappingPanel.cpp`** — ImGui 7-column table (Address|Target|Min|Max|Freq|Enabled|MAP), MAP disables when learning + shows "Listening…", toolbar (Add/Remove/Save/Load) (R-7). `L`

## Phase 7: Dock Layout (UIManager changes)

- [x] 7.1 **Modify `src/ui/PanelIds.h`** — add `static const char* kOscMapping = "OSC Mappings";`. `S`
- [x] 7.2 **Modify `src/ui/UIManager.h`** — add `ImGuiID oscMappingDockId_;` after `inputDockId_`. `S`
- [x] 7.3 **Modify `src/ui/UIManager.cpp::buildDefaultDockLayout()`** — after `mappingDockId_ = dockMainId` (line 204), split: `oscMappingDockId_ = DockBuilderSplitNode(mappingDockId_, Right, 0.45f, nullptr, &mappingDockId_);` then dock `kOscMapping` to `oscMappingDockId_`. `S`

## Phase 8: Application Wiring (ofApp integration)

- [x] 8.1 **Modify `src/ofApp.h`** — `#include "mapping/OscMappingEngine.h"`, add `oscMappingEngine` (unique_ptr) member (no value member: engine lacks default ctor). `S`
- [x] 8.2 **Modify `src/ofApp.cpp`** — 6 insertions (R-8 full lifecycle): (a) include OscMappingPanel.h; (b) create engine after uiManager.setup(); (c) create OscMappingPanel + addModule after inputPorts; (d) setOscMessageCallback lambda + loadFromJson after startListening() success; (e) engine.update(delta) after oscAdapter update; (f) saveToJson(...) before deactivateAll() in exit(). `M`

## Phase 9: Build & Manual Verification

- [x] 9.1 **Run `make`** from project root — zero errors, zero warnings (only pre-existing TARGET_OS_VISION warnings + sceneEngine_ unused in OscMappingPanel). `S`
- [x] 9.2 **Manual smoke test** — launch app, verify OSC Mappings panel in new dock, add mapping, send OSC → param changes, test learning mode + persistence round-trip (R-9 edge cases). `M`
