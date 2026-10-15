# Design: OSC-to-Scene-Parameter Mapping

## Technical Approach

Add an `OscMappingEngine` that stores `OscMapping` entries and dispatches scaled OSC values to scene parameters via `SceneEngine`. A `std::function` callback on `OSCAdapter` fires AFTER existing routing for every message. An `OscMappingPanel` (new `IUIModule`) provides an ImGui table for CRUD + single-slot learning mode. Persistence via `config/osc-mappings.json`. No changes to `MappingLayer`, `ParameterBinding`, or scene code.

## Architecture Decisions

| Decision | Choice | Rejected | Rationale |
|----------|--------|----------|-----------|
| Engine placement | Separate class `OscMappingEngine` | Extending `MappingLayer` or `OSCAdapter` | `MappingLayer` uses `ParameterBinding` (MIDI-oriented, different struct). `OSCAdapter` is transport-only. New engine has frequency modes + learning FSM that neither existing class owns. |
| Adapter→Engine link | `std::function<void(string,float)>` callback setter on OSCAdapter | Inheritance, Observer pattern, or direct Engine* coupling | Callback is minimal surface — one `#include`, one setter, one line in `update()`. No interface, no virtual dispatch, no adapters needing to know about engines. |
| Callback fire timing | After built-in routing, for ALL messages (matched or unmatched) | Only for unmatched addresses | Allows dual dispatch (e.g., `/rig/fader/2` fires both fader handler AND engine mapping). R-3 scenario "Fader coexistence" requires this. |
| Target param resolution | Split `targetParameterId` on first dot → `sceneId`.`localParamId`, then `SceneEngine::getScene()` + `scene->setParameter()` | `ParameterRegistry` lookup or active-scene-only dispatch | Matches `ofApp.cpp` pattern (lines 53-69). Allows targeting inactive scenes (skipped silently per R-3). Direct scene access avoids registry round-trip. |
| Learning mode | Single-slot FSM (IDLE → LEARNING → timeout/assigned → IDLE) | Multi-slot queue | R-2 explicitly requires single-slot. Single-slot eliminates race conditions and is simpler to communicate in UI. |
| Dock placement | New `oscMappingDockId_` splitting from `mappingDockId_` right edge | Tabbed with existing Mapping panel, or floating window | Separate dock keeps SCENE-param mapping visually distinct from DEVICE-channel mapping. Natural layout: split the code's main center into two side-by-side panels. |
| JSON persistence | Independent file `config/osc-mappings.json` via nlohmann/json (bundled with OF) | Extending `ParameterRegistry` format or OF XML | Separate schema avoids coupling. nlohmann/json is header-only, already in OF toolchain. |

## Class Design — OscMappingEngine

```
src/mapping/OscMappingEngine.h
src/mapping/OscMappingEngine.cpp
```

### Data Structures

```cpp
enum class OscFrequencyMode { EVERY_FRAME, ON_CHANGE, THROTTLED };

enum class LearnState { IDLE, LEARNING, TIMEOUT };

struct OscMapping {
    std::string mappingId;
    std::string oscAddress;
    std::string targetParameterId;
    float minInput = 0.0f;
    float maxInput = 1.0f;
    OscFrequencyMode frequencyMode = OscFrequencyMode::EVERY_FRAME;
    int throttleFrames = 5;
    bool enabled = true;
    // Runtime (not persisted):
    float lastValue = 0.0f;
    int frameCounter = 0;
};
```

### Public API

```cpp
class OscMappingEngine {
public:
    explicit OscMappingEngine(SceneEngine* sceneEngine);

    // CRUD
    std::string addMapping(const OscMapping& mapping);          // returns mappingId, rejects duplicates
    bool removeMapping(const std::string& mappingId);
    const std::vector<OscMapping>& getMappings() const;
    OscMapping* findMapping(const std::string& mappingId);

    // Dispatch — called from OSCAdapter callback
    void processOscMessage(const std::string& oscAddress, float value);

    // Learning mode
    bool startLearning(const std::string& targetParameterId);   // returns false if already learning
    void cancelLearning();
    LearnState getLearnState() const;
    std::string getLearningTarget() const;                      // target param being learned
    float getLearnTimeRemaining() const;                        // seconds remaining

    // Persistence
    bool saveToJson(const std::string& filepath) const;
    bool loadFromJson(const std::string& filepath);

    // Called each frame to advance timeout counter
    void update(float deltaTime);
};
```

### Private Members

```cpp
private:
    OscMapping* findDuplicate(const std::string& oscAddr, const std::string& targetParam) const;
    float scaleValue(float input, const OscMapping& mapping) const;
    bool shouldDispatch(const OscMapping& mapping, float value);
    std::string generateId();

    SceneEngine* sceneEngine_;
    std::vector<OscMapping> mappings_;
    int idCounter_;

    // Learning state
    LearnState learnState_ = LearnState::IDLE;
    std::string learnTargetParamId_;
    float learnTimer_ = 0.0f;
    static constexpr float kLearnTimeout = 10.0f;
    static constexpr float kOnChangeEpsilon = 0.001f;
};
```

### Key Method Behavior

**`processOscMessage(addr, value)`**:
1. If `learnState_ == LEARNING`: create mapping with `oscAddress=addr`, `targetParameterId=learnTargetParamId_`, add to `mappings_`, reset to `IDLE`.
2. Else: iterate `mappings_`, match on `oscAddress`, skip if `!enabled`. Call `shouldDispatch()`. If yes: resolve `targetParameterId` → `sceneId.localParamId`, call `SceneEngine::getScene(sceneId)->setParameter(localParamId, scaledValue)`.

**`shouldDispatch(mapping, value)`**: switch on `frequencyMode`:
- `EVERY_FRAME`: always true.
- `ON_CHANGE`: `|value - mapping.lastValue| > 0.001f`.
- `THROTTLED`: `mapping.frameCounter % mapping.throttleFrames == 0`.

**`saveToJson(filepath)`**: iterate `mappings_`, serialize all fields EXCEPT `lastValue` and `frameCounter`. Write array of objects.

## Class Design — OscMappingPanel

```
src/ui/OscMappingPanel.h
src/ui/OscMappingPanel.cpp
```

```cpp
class OscMappingPanel : public IUIModule {
public:
    OscMappingPanel(OscMappingEngine* engine, SceneEngine* sceneEngine);

    void setup() override {}
    void update(float deltaTime) override;
    void draw() override;

    void setVisible(bool visible) override;
    bool isVisible() const override;
    std::string getPanelId() const override { return "OscMappingPanel"; }

private:
    void drawToolbar();
    void drawMappingTable();
    void drawLearningIndicator();

    OscMappingEngine* engine_;
    SceneEngine* sceneEngine_;
    bool visible_;
    ImGuiTextFilter filter_;

    // Inline editing state
    char newOscAddrBuf_[128];
    char newTargetParamBuf_[128];
};
```

### Table Layout (7 columns)

| OSC Address | Target Param | Min | Max | Freq Mode | Enabled | MAP |
|-------------|-------------|-----|-----|-----------|---------|-----|

- **OSC Address**: text input when selected, otherwise display
- **Target Param**: display only (set during learning or manual entry)
- **Min/Max**: `ImGui::DragFloat` with step 0.01, clamped ±1000
- **Freq Mode**: `ImGui::Combo` (EVERY_FRAME, ON_CHANGE, THROTTLED)
- **Enabled**: `ImGui::Checkbox`
- **MAP**: button that calls `engine_->startLearning(targetParamId)`. Disabled (greyed) when `learnState_ != IDLE`. Shows "Listening..." with spinner when that row is learning.

### Toolbar Buttons
- **+ Add**: creates empty mapping (user fills in addr + target manually)
- **Remove Selected**: delete highlighted row
- **Save**: `engine_->saveToJson("config/osc-mappings.json")`
- **Load**: `engine_->loadFromJson("config/osc-mappings.json")`

## OSCAdapter Modifications

### OSCAdapter.h — additions

```cpp
#include <functional>  // already included

// New public typedef + setter:
using OscMessageCallback = std::function<void(const std::string& address, float value)>;
void setOscMessageCallback(OscMessageCallback callback);

// New private member:
OscMessageCallback oscMessageCallback_;
```

### OSCAdapter.cpp — update() modification

After the existing `} else { ... log warning ... }` block (line 183-187), add BEFORE `processed++`:

```cpp
// Fire mapping engine callback for ALL messages
if (oscMessageCallback_) {
    oscMessageCallback_(addr, msg.getNumArgs() > 0 ? msg.getArgAsFloat(0) : 0.0f);
}
```

This fires for every message — matched OR unmatched — AFTER built-in routing.

## Sequence Diagrams

### Learning Mode Flow

```
User                OscMappingPanel        OscMappingEngine       OSCAdapter
 |                        |                      |                     |
 | click [MAP] "furry.d" |                      |                     |
 |---------------------->|                      |                     |
 |                       | startLearning()      |                     |
 |                       |--------------------->|                     |
 |                       |  learnState=LEARNING  |                     |
 |                       |  target="furry.d"     |                     |
 |                       |<---------------------|                     |
 |  [MAP]→"Listening..." |                      |                     |
 |                       |                      |                     |
 |                                            /XPosition 0.75 ----->|
 |                                                                    |
 |                                            callback("/XPos",0.75)  |
 |                                                                    |-->|
 |                                          processOscMessage(       |   |
 |                                            "/XPos", 0.75)         |   |
 |                                          |<------------------------   |
 |    learnState==LEARNING? YES              |                         |
 |    createMapping({oscAddr:"/XPos",        |                         |
 |      target:"furry.d", defaults})         |                         |
 |    learnState=IDLE                        |                         |
 |                                          |                         |
 |                       | next draw(): learnState==IDLE              |
 |                       |<-------------------------------------------|
 |  table refreshes,     |                                            |
 |  new row visible      |                                            |
```

### Message Dispatch Flow

```
Ableton/M4L           OSCAdapter             OscMappingEngine        SceneEngine   IBaseScene
    |                      |                        |                     |             |
    | /XPos 0.42 --------->|                        |                     |             |
    |                      | stripPrefix → "/XPos"  |                     |             |
    |                      | built-in routing       |                     |             |
    |                      | (fader/toggle/etc)     |                     |             |
    |                      |                        |                     |             |
    |                      | callback("/XPos",0.42) |                     |             |
    |                      |----------------------->|                     |             |
    |                      |                        | findMappings("/XPos")|             |
    |                      |                        | → [{pintura.k, ...}] |             |
    |                      |                        | for each enabled:     |             |
    |                      |                        |   shouldDispatch? YES |             |
    |                      |                        |   scaleValue(0.42)    |             |
    |                      |                        |   → 2.1              |             |
    |                      |                        |                       |             |
    |                      |                        | resolve "pintura.k"   |             |
    |                      |                        | → sceneId="pintura"   |             |
    |                      |                        | → localParamId="k"    |             |
    |                      |                        |                       |             |
    |                      |                        | getScene("pintura")   |             |
    |                      |                        |---------------------->|             |
    |                      |                        | ScenePtr              |             |
    |                      |                        |<----------------------|             |
    |                      |                        |                       |             |
    |                      |                        | setParameter("k",2.1) |             |
    |                      |                        |-------------------------------------->|
    |                      |                        |                       |             | apply to visuals
```

### Frequency Mode Gating

```
OSC value 0.5 arrives for 10 consecutive frames:

Frame  EVERY_FRAME    ON_CHANGE (Δ check)     THROTTLED (N=5)
  1    dispatch ✓     dispatch ✓ (|0.5-0|=0.5) dispatch ✓ (frameCounter=5→0%5==0)
  2    dispatch ✓     skip (|0.5-0.5|=0)       skip (frameCounter=6→6%5==1)
  3    dispatch ✓     skip                      skip (7%5==2)
  4    dispatch ✓     skip                      skip (8%5==3)
  5    dispatch ✓     skip                      skip (9%5==4)
  6    dispatch ✓     skip                      dispatch ✓ (10%5==0)
  7    dispatch ✓     skip                      skip (11%5==1)
  8    dispatch ✓     skip                      skip (12%5==2)
  9    dispatch ✓     skip                      skip (13%5==3)
 10    dispatch ✓     skip                      skip (14%5==4)

Result: 10x | 1x (or 2x if value changes on frame 6) | 2x
```

## Dock Layout Decision

The existing layout has 5 dock zones:

```
+------------------+---------------------------+------------+
| Scenes (24%)     |                           | Params (30%)|
| dockLeftId       |    Mapping (center)       | dockRightId |
|                  |    mappingDockId_         |             |
+------------------+                           +------------+
| Transport (28%)  |  Input Ports (35%)       |
| dockBottomId     |  inputDockId_            |
+------------------+---------------------------+
```

**Recommendation**: Split `mappingDockId_` horizontally into two zones — left for Mapping (MIDI) and right for OscMapping (OSC). This keeps both mapping panels visible at once, each in its own column.

Implementation: In `buildDefaultDockLayout()`, after assigning `mappingDockId_ = dockMainId`, split it:

```cpp
ImGuiID oscMappingDockId_ = ImGui::DockBuilderSplitNode(mappingDockId_, ImGuiDir_Right, 0.45f, nullptr, &mappingDockId_);
```

Then dock `kOscMapping` to `oscMappingDockId_`. Add `kOscMapping = "OSC Mappings"` to `PanelIds.h`.

## JSON Schema

`config/osc-mappings.json` — array of mapping objects:

```json
[
  {
    "mappingId": "map-001",
    "oscAddress": "/XPosition",
    "targetParameterId": "pinturaFluorescente.k",
    "minInput": 0.0,
    "maxInput": 1.0,
    "frequencyMode": 0,
    "throttleFrames": 5,
    "enabled": true
  },
  {
    "mappingId": "map-002",
    "oscAddress": "/Velocity1",
    "targetParameterId": "anemone.speed",
    "minInput": 0.0,
    "maxInput": 127.0,
    "frequencyMode": 1,
    "throttleFrames": 0,
    "enabled": false
  }
]
```

| JSON field | C++ field | Notes |
|-----------|-----------|-------|
| `mappingId` | `OscMapping::mappingId` | Unique string generated by engine |
| `oscAddress` | `oscAddress` | Full OSC address after prefix stripping |
| `targetParameterId` | `targetParameterId` | Format: `sceneId.paramName` |
| `minInput` | `minInput` | Float, default 0.0 |
| `maxInput` | `maxInput` | Float, default 1.0 |
| `frequencyMode` | `frequencyMode` | int: 0=EVERY_FRAME, 1=ON_CHANGE, 2=THROTTLED |
| `throttleFrames` | `throttleFrames` | int, default 5, used only if THROTTLED |
| `enabled` | `enabled` | bool |

`lastValue` and `frameCounter` are runtime-only — NEVER persisted.

## ofApp Wiring

### ofApp.h — additions

```cpp
#include "mapping/OscMappingEngine.h"
// OscMappingPanel forward-declared or included

modular::OscMappingEngine oscMappingEngine;
std::unique_ptr<modular::OscMappingPanel> oscMappingPanel_;
```

### ofApp.cpp setup() — order of operations

1. (existing) `sceneEngine.loadScene(...)` x4 + `activateScene`
2. (existing) `uiManager.setup()`
3. (existing) Load saved parameters from JSON
4. (existing) Add MIDI mappings + ports
5. (existing) Create all 5 UI panels
6. **NEW** — after `addModule(inputPortsPanel)`:
   ```cpp
   oscMappingPanel_ = std::make_unique<modular::OscMappingPanel>(&oscMappingEngine, &sceneEngine);
   uiManager.addModule(std::move(oscMappingPanel_));
   ```
7. (existing) `inputRouter = ...`
8. (existing) Initialize OSC adapter
9. **NEW** — after `oscAdapter_->startListening()` succeeds:
   ```cpp
   oscAdapter_->setOscMessageCallback(
       [this](const std::string& addr, float val) {
           oscMappingEngine.processOscMessage(addr, val);
       }
   );
   oscMappingEngine.loadFromJson("config/osc-mappings.json");
   ```

### ofApp.cpp update() — addition

Add after `oscAdapter_->update()`:
```cpp
oscMappingEngine.update(delta);
```

### ofApp.cpp exit() — addition

Add before `sceneEngine.deactivateAll()`:
```cpp
oscMappingEngine.saveToJson("config/osc-mappings.json");
```

## File Manifest

| File | Action | Description |
|------|--------|-------------|
| `src/mapping/OscMappingEngine.h` | **Create** | Header: `OscMapping`, `OscFrequencyMode`, `LearnState`, engine class |
| `src/mapping/OscMappingEngine.cpp` | **Create** | Implementation: CRUD, dispatch, scaling, learning FSM, JSON I/O |
| `src/ui/OscMappingPanel.h` | **Create** | Header: `IUIModule` subclass |
| `src/ui/OscMappingPanel.cpp` | **Create** | Implementation: ImGui table, MAP button, toolbar |
| `src/adapters/OSCAdapter.h` | **Modify** | Add `OscMessageCallback` typedef, `setOscMessageCallback()` setter, `oscMessageCallback_` member |
| `src/adapters/OSCAdapter.cpp` | **Modify** | Add callback invocation after routing (1 line in `update()`) |
| `src/ui/PanelIds.h` | **Modify** | Add `kOscMapping = "OSC Mappings"` |
| `src/ui/UIManager.h` | **Modify** | Add `oscMappingDockId_` member |
| `src/ui/UIManager.cpp` | **Modify** | Split `mappingDockId_` for new dock, dock `kOscMapping` window |
| `src/ofApp.h` | **Modify** | Add `oscMappingEngine` (value) + `oscMappingPanel_` (unique_ptr) members |
| `src/ofApp.cpp` | **Modify** | 4 insertions: panel creation, callback wiring, `update()`, `exit()` |

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Unit | `scaleValue()` math | Hardcoded input/output pairs (R-4 scenarios: 0→1, 0→127, inverted) |
| Unit | `shouldDispatch()` frequency gating | Simulate 10 frames per mode, assert dispatch count |
| Unit | `addMapping()` duplicate rejection | Add same pair twice, assert second returns empty string |
| Unit | Learning FSM transitions | `startLearning()` → IDLE→LEARNING; `processOscMessage()` → LEARNING→IDLE; timeout → IDLE with warning |
| Integration | OSCAdapter→Engine callback | Construct OSCAdapter, set callback, manually call `update()` with synthetic message, assert engine receives it |
| Build | Full compile verification | `make` from project root — zero errors, zero warnings |
| Manual | End-to-end Ableton→visual | Run app, send OSC from M4L, verify visual parameter changes |

## Open Questions

None. All decisions have clear rationale backed by existing codebase patterns and spec requirements.
