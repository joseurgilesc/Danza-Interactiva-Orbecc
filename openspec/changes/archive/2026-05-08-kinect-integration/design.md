# Design: Kinect Integration

## Technical Approach

Exact structural mirror of the OSC pattern: `IKinectAdapter` (abstract, ≈`OSCAdapter`), `KinectMappingEngine` (CRUD+dispatch, ≈`OscMappingEngine`), `KinectPanel` (IUIModule, ≈`OscMappingPanel`). Key difference: Kinect produces multi-dimensional skeleton data → needs abstract interface for mockability, 5 callbacks instead of 1, and condition-based dispatch (ABOVE_THRESHOLD, GESTURE, etc.) instead of unconditional OSC forwarding. All Kinect SDK dependencies (OpenNI2, NiTE2) confined to `OpenNIKinectAdapter.cpp` only — headers and all other files remain SDK-free.

## Architecture Decisions

| Decision | Choice | Rejected | Rationale |
|----------|--------|----------|-----------|
| **Kinect SDK** | ofxOpenNI + OpenNI2 + NiTE2 | ofxKinectV2 (only Kinect v2, not v1 Xbox 360), ofxAzureKinect (discontinued hardware, no macOS), Nuitrack (commercial license, $299/yr) | ofxOpenNI is the only addon compatible with Kinect v1 (target hardware), is free (Apache 2.0), and has macOS+OF community history. Spec requires v1. |
| **Data delivery** | Callback-based via 5 `std::function` setters | Polling-only API, observer pattern, signal/slots | Matches existing OSC callback pattern (`setOscMessageCallback`). Polling is also supported via `getAllTrackedBodies()` for rendering use. Callbacks fire from adapter's `update()`, keeping all Kinect processing on the main thread — no mutex needed. |
| **Mapping engine** | Separate `KinectMappingEngine` class | Extending `OscMappingEngine` with virtual dispatch, or merging into one `MappingEngine` | OSC mappings are unconditional passthrough; Kinect mappings need 5 condition types (ABOVE/BELOW_THRESHOLD, IN_RANGE, GESTURE, CONTINUOUS) and multi-axis joint sources (`hand_right.position.y`). Merging would create a god class with dual dispatch logic. Separate engines keep each simple. |
| **Coordinate space** | Normalize to 0.0–1.0 screen-proportional | Keep raw depth-space meters (e.g., 640×480, 0–4000mm) | Different Kinect models have different resolutions and depth ranges. Normalized space makes mappings, skeleton rendering, and mock data resolution-independent. Spec §kinect-types R-4 mandates this. |
| **Persistence file** | `config/kinect-mappings.json` | Reusing `config/osc-mappings.json` with a `"source": "kinect"` field | Separate files prevent schema collisions (different fields: `jointSource` vs `oscAddress`, `condition` vs `frequencyMode`). Each engine owns its file, same pattern as `parameters.json` vs `osc-mappings.json`. |
| **SkeletonView** | Separate renderer class (`SkeletonView`) | Inline rendering inside `KinectPanel` or `ofApp::draw()` | Skeleton needs independent visibility toggle (`setVisible`), confidence color-coding logic, and bone connection table — embedding in ofApp bloats the main class. Separate class = single responsibility. Panel's mini-preview delegates to same drawing logic (no duplication). |
| **MockKinectAdapter** | Separate concrete class implementing IKinectAdapter | `#ifdef MOCK_KINECT` compile-time macro swapping `OpenNIKinectAdapter` internals | Mock must compile WITHOUT OpenNI headers to verify SDK isolation (spec §kinect-adapter R-7). Compile-time macro would require OpenNI includes in all translation units. Separate class = real interface compliance verification. Also enables runtime mock/hardware switching. |

## Component Design

### 1. KinectTypes.h — Shared Data Types

```cpp
// src/adapters/kinect/KinectTypes.h
// ZERO SDK DEPENDENCY — compiles with C++17 standard library only

namespace modular {

enum class JointType : uint8_t {
    HEAD, NECK, SHOULDER_LEFT, SHOULDER_RIGHT,
    ELBOW_LEFT, ELBOW_RIGHT, WRIST_LEFT, WRIST_RIGHT,
    HAND_LEFT, HAND_RIGHT,
    HIP_LEFT, HIP_RIGHT, KNEE_LEFT, KNEE_RIGHT,
    ANKLE_LEFT, ANKLE_RIGHT,
    SPINE_BASE, SPINE_MID,
    COUNT  // = 18
};

struct Vector3D { float x = 0.5f, y = 0.5f, z = 0.5f; };
struct Joint {
    JointType type;
    Vector3D position, velocity;
    float confidence = 0.0f;
};
struct TrackedBody {
    int bodyId = 0;
    bool isTracked = false;
    float confidence = 0.0f;
    Joint joints[static_cast<int>(JointType::COUNT)];
};

enum class HandState { OPEN, CLOSED, POINTING, UNKNOWN };
enum class ConnectionState { DISCONNECTED, CONNECTING, CONNECTED, ERROR };

} // namespace modular
```

**Joint-to-bone map** (17 bone connections):
```
{HEAD,NECK}, {NECK,SPINE_MID}, {SPINE_MID,SPINE_BASE},
{SHOULDER_LEFT,SHOULDER_RIGHT},  // clavicle
{SHOULDER_LEFT,ELBOW_LEFT}, {ELBOW_LEFT,WRIST_LEFT}, {WRIST_LEFT,HAND_LEFT},
{SHOULDER_RIGHT,ELBOW_RIGHT}, {ELBOW_RIGHT,WRIST_RIGHT}, {WRIST_RIGHT,HAND_RIGHT},
{HIP_LEFT,HIP_RIGHT},  // pelvic line at spine_base level (visual only)
{SPINE_BASE,HIP_LEFT}, {HIP_LEFT,KNEE_LEFT}, {KNEE_LEFT,ANKLE_LEFT},
{SPINE_BASE,HIP_RIGHT}, {HIP_RIGHT,KNEE_RIGHT}, {KNEE_RIGHT,ANKLE_RIGHT}
```

### 2. IKinectAdapter.h — Abstract Interface

```cpp
// src/adapters/kinect/IKinectAdapter.h
// ZERO SDK DEPENDENCY — only includes KinectTypes.h + <functional>

namespace modular {

enum class GestureType { WAVE, SWIPE_LEFT, SWIPE_RIGHT, SWIPE_UP, SWIPE_DOWN, CLICK, GRAB, RELEASE };
struct Zone { int col, row; bool active; float intensity; };

class IKinectAdapter {
public:
    virtual ~IKinectAdapter() = default;

    // Lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual void update() = 0;
    virtual ConnectionState getConnectionState() const = 0;

    // Callback registration (one std::function per data channel)
    using JointCallback  = std::function<void(int bodyId, JointType type, const Joint& data)>;
    using BodyCallback   = std::function<void(const TrackedBody& body)>;
    using HandCallback   = std::function<void(int bodyId, HandState state)>;
    using GestureCallback = std::function<void(GestureType gesture, float confidence)>;
    using ZoneCallback   = std::function<void(const Zone& zone)>;

    virtual void setJointCallback(JointCallback cb) = 0;
    virtual void setBodyCallback(BodyCallback cb) = 0;
    virtual void setHandCallback(HandCallback cb) = 0;
    virtual void setGestureCallback(GestureCallback cb) = 0;
    virtual void setZoneCallback(ZoneCallback cb) = 0;

    // Polling access (for SkeletonView rendering, bypasses callbacks)
    virtual std::vector<TrackedBody> getAllTrackedBodies() const = 0;
    virtual Vector3D getRawJointPosition(int bodyId, JointType type) const = 0;

    // Configuration
    virtual void setSmoothing(float factor) = 0;
    virtual void setConfidenceThreshold(float min) = 0;
    virtual void setZoneGrid(int cols, int rows) = 0;
};

} // namespace modular
```

### 3. OpenNIKinectAdapter — Hardware Implementation

```
src/adapters/kinect/OpenNIKinectAdapter.h
src/adapters/kinect/OpenNIKinectAdapter.cpp
```

- **Members**: `ofxOpenNI kinect` device handle, `nite::UserTracker* userTracker_`, callback `std::function`s, `ConnectionState state_`, smoothing factor, confidence threshold, frame timing for 30fps cap, `std::vector<TrackedBody> trackedBodies_` (frame cache).
- **initialize()**: open Kinect device via `ofxOpenNI`, create NiTE user tracker, set skeleton smoothing, set `state_ = CONNECTING` → `CONNECTED`.
- **update()**: cap to 30fps via delta accumulator. If threshold met: `userTracker_->readFrame()`, iterate visible users, call `userTracker_->getSkeleton(userId)` → populate `TrackedBody` struct via `KinectNormalizer` (depth-space meters → 0.0–1.0). Invoke all 5 callbacks. Cache bodies for polling.
- **shutdown()**: `userTracker_->stopTracking()`, close device, `state_ = DISCONNECTED`.
- **Frame cap**: `frameAccumulator_ += deltaTime; if (accumulator < 1.0f/30.0f) return;` → process, reset accumulator. Smoothing via EMA: `pos = raw * (1-alpha) + prev * alpha` where alpha = smoothing factor.
- Only file that `#include`s `<ofxOpenNI.h>` and `<NiTE.h>`. All SDK types contained within `.cpp`.

### 4. MockKinectAdapter — Testing Implementation

```
src/adapters/kinect/MockKinectAdapter.h
src/adapters/kinect/MockKinectAdapter.cpp
```

- **Members**: `TrackedBody mockBody_` (1 body, bodyId=1, all joints at 0.5,0.5,0.5), callback `std::function`s, `SimMode simMode_`, simulation timer, `bool initialized_`.
- `enum class SimMode { IDLE, WALKING, WAVING }`
- **update()**: advance timer, apply sinusoidal displacement based on `simMode_`:
  - IDLE: all joints at (0.5, 0.5, 0.5)
  - WALKING: y oscillates `0.5 + 0.2 * sin(t * 2.0)` for all joints
  - WAVING: HAND_RIGHT oscillates x: `0.5 + 0.2 * sin(t * 3.0)`, y: `0.5 + 0.2 * cos(t * 3.0)`
- Invoke all 5 registered callbacks with mock data each frame.
- `setMockJointPosition(JointType, Vector3D)` overrides a joint's position (test-driven). Mode override: if simulation mode is active, per-joint settings are ignored (mode wins).
- ZERO external dependencies — no OpenNI headers needed to compile.

### 5. KinectMappingEngine — Joint-to-Parameter Dispatch

```
src/mapping/KinectMappingEngine.h
src/mapping/KinectMappingEngine.cpp
src/mapping/KinectMappingTypes.h   // KinectCondition, KinectMapping struct
```

**KinectMappingTypes.h:**

```cpp
enum class KinectCondition : uint8_t {
    ABOVE_THRESHOLD,  // value > threshold → dispatch maxOutput
    BELOW_THRESHOLD,  // value < threshold → dispatch minOutput
    IN_RANGE,         // minRange ≤ value ≤ maxRange → dispatch ofMap'd value
    GESTURE,          // gesture name matches → dispatch maxOutput once
    CONTINUOUS        // always dispatch, ofMap input→output range
};

enum class KinectTransformType : uint8_t { LINEAR, EXPONENTIAL, LOGARITHMIC, INVERT };

struct KinectMapping {
    std::string mappingId;           // "kinect-map-N"
    std::string jointSource;         // e.g. "hand_right.position.y"
    std::string targetParameterId;   // "sceneId.localParamId"
    KinectCondition condition;
    float threshold = 0.5f;
    float minRange = 0.0f;
    float maxRange = 1.0f;
    float minInput = 0.0f;
    float maxInput = 1.0f;
    float minOutput = 0.0f;
    float maxOutput = 1.0f;
    KinectTransformType transformType = KinectTransformType::LINEAR;
    bool enabled = true;
    float smoothing = 0.0f;
    // Runtime only (NOT persisted):
    float lastValue = 0.0f;
};
```

**KinectMappingEngine.h** — mirrors `OscMappingEngine` structure:

```cpp
class KinectMappingEngine {
public:
    explicit KinectMappingEngine(SceneEngine* sceneEngine);

    // CRUD
    std::string addMapping(const KinectMapping& mapping);  // rejects duplicates on (jointSource, target)
    bool removeMapping(const std::string& mappingId);
    std::vector<KinectMapping>& getMappings();
    const std::vector<KinectMapping>& getMappings() const;
    KinectMapping* findMapping(const std::string& mappingId);

    // Dispatch — called from IKinectAdapter::JointCallback
    void processJointData(int bodyId, JointType jointType, const Joint& joint);

    // Per-frame smoothing advance
    void update(float deltaTime);

    // Persistence (same pattern as OscMappingEngine)
    bool saveToJson(const std::string& filepath) const;
    bool loadFromJson(const std::string& filepath);

private:
    std::string generateId();       // "kinect-map-" + ++idCounter_
    KinectMapping* findDuplicate(const std::string& jointSrc, const std::string& targetParam) const;
    float evaluateCondition(const KinectMapping& mapping, float value);
    float applyTransform(float t, KinectTransformType type);
    void dispatchToScene(const KinectMapping& mapping, float outputValue);

    SceneEngine* sceneEngine_;
    std::vector<KinectMapping> mappings_;
    int idCounter_ = 0;
};
```

**`processJointData()` logic:**
1. Build source key: `jointTypeName(jointType) + ".position." + axis` where axis is extracted from jointSource string (x/y/z or "velocity.x", etc.).
2. For each mapping where `jointSource` matches and `enabled`:
   - Extract scalar value from joint's Vector3D based on axis.
   - Evaluate condition: ABOVE_THRESHOLD → if `value > threshold` output = `maxOutput`; BELOW_THRESHOLD → if `value < threshold` output = `minOutput`; IN_RANGE → if in range, `ofMap(value, minRange, maxRange, minOutput, maxOutput)`; GESTURE → match gesture name, dispatch `maxOutput` once; CONTINUOUS → always `ofMap(value, minInput, maxInput, minOutput, maxOutput)`.
   - Apply transform (LINEAR/EXPONENTIAL/LOGARITHMIC/INVERT) to output value.
   - Apply smoothing: `smoothed = lastValue + (output - lastValue) * (1.0 - smoothing)`.
   - Resolve `targetParameterId` (split on first dot → sceneId, localParamId), call `sceneEngine->getScene(sceneId)->setParameter(localParamId, smoothed)`.

**Duplicate rejection**: `(jointSource, targetParameterId)` pair — identical to `OscMappingEngine`'s `(oscAddress, targetParameterId)` logic.

### 6. SkeletonView — Visual Renderer

```
src/input/sensors/SkeletonView.h
src/input/sensors/SkeletonView.cpp
```

```cpp
class SkeletonView {
public:
    void draw(const std::vector<TrackedBody>& bodies);
    void setVisible(bool visible);
    bool isVisible() const;

private:
    void drawJoint(const Joint& j, const Vector3D& pos, float viewportW, float viewportH);
    void drawBone(const Vector3D& from, const Vector3D& to, float confAvg, float vpW, float vpH);
    ofColor confidenceColor(float confidence);  // green >0.7, yellow 0.3-0.7, red <0.3

    bool visible_ = true;
};
```

**`draw()` implementation**: obtains viewport size from `ofGetWidth()/ofGetHeight()`. For each tracked body: draw joints as `ofDrawCircle` (radius 3–12px proportional to confidence). Draw 17 bone connections as `ofDrawLine` colored by average confidence of the two joints. Draw "Tracking: ON" / "Tracking: LOST" text overlay at top-left.

**Coordinate system**: normalized (x: 0.0 left → 1.0 right, y: 0.0 top → 1.0 bottom). Mirror horizontally: `screenX = (1.0 - normalizedX) * viewportWidth` — because Kinect sees a mirror image and we want natural left-right mapping.

### 7. KinectPanel — ImGui Configuration UI

```
src/ui/KinectPanel.h
src/ui/KinectPanel.cpp
```

```cpp
class KinectPanel : public IUIModule {
public:
    KinectPanel(IKinectAdapter* adapter, KinectMappingEngine* mapEngine, SceneEngine* sceneEngine);

    void setup() override {}
    void update(float deltaTime) override;
    void draw() override;
    void setVisible(bool visible) override;
    bool isVisible() const override;
    std::string getPanelId() const override { return panel_ids::kKinect; }

private:
    void drawStatusBar();          // Connection state indicator + Connect/Disconnect
    void drawBodyTable();          // Tracked bodies list, selectable
    void drawJointDetail();        // 18 joints with X/Y/Z/confidence for selected body
    void drawMappingEditor();      // Add/remove mapping, dropdowns, sliders
    void drawMappingList();        // Current mappings with enable toggles
    void drawSkeletonPreview();    // Mini skeleton viewport (delegates to SkeletonView logic)

    IKinectAdapter* adapter_;
    KinectMappingEngine* mapEngine_;
    SceneEngine* sceneEngine_;
    bool visible_ = true;
    int selectedBodyIndex_ = 0;
    // Mapping editor state
    char jointSourceBuf_[128];
    char targetParamBuf_[128];
    int selectedCondition_ = 0;
    int selectedTransform_ = 0;
};
```

**Sections (top to bottom):**
1. **Status bar**: colored circle (green/yellow/red) + state text + Connect/Disconnect buttons
2. **Tracked bodies table**: columns: Body ID, Confidence, Status → select row
3. **Joint detail**: 18-row table for selected body with X, Y, Z, confidence (color-coded like SkeletonView)
4. **Mapping editor**: joint source dropdown (jointType.axis), target param dropdown (sceneId.paramId), condition combo, threshold slider, transform combo, Add/Remove buttons, Save/Load buttons
5. **Mapping list**: current mappings with enable checkbox, delete button per row
6. **Skeleton mini-preview**: small viewport rendering tracked skeleton (reuses SkeletonView bone table)

**Contextual control state**: when `adapter_->getConnectionState() != CONNECTED`, mapping editor, body table, and skeleton preview are disabled/grayed out.

### 8. ofApp Integration

**ofApp.h additions:**
```cpp
#include "adapters/kinect/IKinectAdapter.h"
#include "mapping/KinectMappingEngine.h"
#include "input/sensors/SkeletonView.h"
#include "ui/KinectPanel.h"

std::unique_ptr<modular::IKinectAdapter> kinectAdapter_;
std::unique_ptr<modular::KinectMappingEngine> kinectMappingEngine_;
std::unique_ptr<modular::SkeletonView> skeletonView_;
```

**ofApp.cpp `setup()`** — insert after OSC adapter initialization, before `=== SETUP COMPLETE ===`:
1. Create `kinectMappingEngine_ = std::make_unique<KinectMappingEngine>(&sceneEngine)`
2. Create `kinectAdapter_ = std::make_unique<OpenNIKinectAdapter>()` (or `MockKinectAdapter` — toggle via `#ifdef` or runtime flag)
3. Wire callbacks:
   ```cpp
   kinectAdapter_->setJointCallback(
       [this](int bodyId, JointType type, const Joint& j) {
           kinectMappingEngine_->processJointData(bodyId, type, j);
       });
   ```
4. `kinectAdapter_->initialize()` — log result
5. Load saved mappings: `kinectMappingEngine_->loadFromJson("config/kinect-mappings.json")`
6. Create `skeletonView_ = std::make_unique<SkeletonView>()`
7. Create `auto kinectPanel = std::make_unique<KinectPanel>(kinectAdapter_.get(), kinectMappingEngine_.get(), &sceneEngine)` → `uiManager.addModule(std::move(kinectPanel))`

**ofApp.cpp `update()`:**
```cpp
if (kinectAdapter_) kinectAdapter_->update();
if (kinectMappingEngine_) kinectMappingEngine_->update(delta);
// ... then sceneEngine.update() ...
```

**ofApp.cpp `draw()`:**
```cpp
sceneEngine.draw();
if (skeletonView_ && kinectAdapter_) {
    skeletonView_->draw(kinectAdapter_->getAllTrackedBodies());
}
uiManager.draw();
```

**ofApp.cpp `exit()`:**
```cpp
if (kinectMappingEngine_) {
    kinectMappingEngine_->saveToJson("config/kinect-mappings.json");
}
if (kinectAdapter_) {
    kinectAdapter_->shutdown();
}
```

## Data Flow

```
┌──────────────┐    ofxOpenNI/NiTE2     ┌──────────────────────┐
│ Kinect v1    │ ──── depth skeleton ──→ │ OpenNIKinectAdapter  │
│ (hardware)   │      raw coords         │ ::update()           │
└──────────────┘                         │  → normalize 0-1     │
                                         │  → populate bodies[] │
                                         │  → invoke callbacks  │
                                         └──────┬───────────────┘
                                                │
                          ┌─────────────────────┼─────────────────────┐
                          │ JointCallback        │ BodyCallback         │ getAllTrackedBodies()
                          ▼                      ▼                      ▼
                ┌──────────────────┐   ┌──────────────┐   ┌──────────────────┐
                │ KinectMappingEng │   │  (logging/   │   │   SkeletonView   │
                │ ::processJointD  │   │   monitoring) │   │   ::draw()       │
                │                  │   │              │   │   → ofDrawCircle │
                │ 1. match source  │   └──────────────┘   │   → ofDrawLine   │
                │ 2. eval condition│                      └──────────────────┘
                │ 3. apply xform   │
                │ 4. smooth        │
                │ 5. dispatch ─────┼──→ scene->setParameter(id, value)
                └──────────────────┘          │
                                              ▼
                                    ┌─────────────────┐
                                    │  IBaseScene     │
                                    │  (never knows   │
                                    │   Kinect exists) │
                                    └─────────────────┘
```

## UI Layout

New dock in existing ImGui dockspace: `kinectDockId_` splits from right side of the center area, sharing column with `paramsDockId_`.

```
+------------------+----------------+------------+----------+
| OSC Mappings     | Scenes         | Params     | Kinect   |
| (left 40%)       | (18%)          | (28%)      | (new)    |
+------------------+----------------+------------+----------+
| Transport | Mapping (MIDI) | Input Ports                    |
+-----------+----------------+--------------------------------+
```

**UIManager modifications:**
- `PanelIds.h`: add `static const char* kKinect = "Kinect";`
- `UIManager.h`: add `ImGuiID kinectDockId_;` member
- `UIManager.cpp` `buildDefaultDockLayout()`: split `paramDockId_` (rightmost) — params takes 60% width, Kinect takes 40% (as a new rightmost dock). `ImGui::DockBuilderDockWindow(panel_ids::kKinect, kinectDockId_);`

## File Organization

| File | Action | Description |
|------|--------|-------------|
| `src/adapters/kinect/KinectTypes.h` | **Create** | JointType, TrackedBody, Joint, Vector3D, HandState, ConnectionState — zero SDK dependency |
| `src/adapters/kinect/IKinectAdapter.h` | **Create** | Abstract interface: lifecycle, 5 callbacks, polling, config — zero SDK dependency |
| `src/adapters/kinect/OpenNIKinectAdapter.h` | **Create** | Concrete ofxOpenNI adapter header |
| `src/adapters/kinect/OpenNIKinectAdapter.cpp` | **Create** | ONLY file that includes `<ofxOpenNI.h>` and `<NiTE.h>`. init/update/shutdown, coordinate normalization, 30fps cap, callback invocation |
| `src/adapters/kinect/MockKinectAdapter.h` | **Create** | Mock adapter header with SimMode enum + setMockJointPosition() |
| `src/adapters/kinect/MockKinectAdapter.cpp` | **Create** | Deterministic body simulation (IDLE/WALKING/WAVING) |
| `src/mapping/KinectMappingTypes.h` | **Create** | KinectCondition enum, KinectTransformType enum, KinectMapping struct |
| `src/mapping/KinectMappingEngine.h` | **Create** | Engine header: CRUD, dispatch, persistence |
| `src/mapping/KinectMappingEngine.cpp` | **Create** | condition evaluation, transform application, smoothing, JSON I/O |
| `src/input/sensors/SkeletonView.h` | **Create** | Skeleton renderer header |
| `src/input/sensors/SkeletonView.cpp` | **Create** | ofDrawCircle/ofDrawLine rendering, confidence color coding, horizontal mirroring |
| `src/ui/KinectPanel.h` | **Create** | IUIModule header: status, bodies, joints, mapping editor, skeleton preview |
| `src/ui/KinectPanel.cpp` | **Create** | 6-section ImGui panel implementation |
| `src/ui/PanelIds.h` | **Modify** | Add `kKinect = "Kinect"` |
| `src/ui/UIManager.h` | **Modify** | Add `kinectDockId_` member |
| `src/ui/UIManager.cpp` | **Modify** | Split params dock for Kinect dock, dock `kKinect` window |
| `src/ofApp.h` | **Modify** | Add 3 unique_ptr members: kinectAdapter_, kinectMappingEngine_, skeletonView_ |
| `src/ofApp.cpp` | **Modify** | 4 insertion points: setup() instantiation+wiring+panel, update() adapter+engine calls, draw() skeleton render, exit() save+shutdown |
| `addons.make` | **Modify** | Add `ofxOpenNI` line |

## Bone Connection Map

```cpp
static const std::pair<JointType, JointType> kKinectBoneConnections[17] = {
    {JointType::HEAD,            JointType::NECK},
    {JointType::NECK,            JointType::SPINE_MID},
    {JointType::SPINE_MID,       JointType::SPINE_BASE},
    {JointType::SHOULDER_LEFT,   JointType::SHOULDER_RIGHT},   // clavicle
    {JointType::SHOULDER_LEFT,   JointType::ELBOW_LEFT},
    {JointType::ELBOW_LEFT,      JointType::WRIST_LEFT},
    {JointType::WRIST_LEFT,      JointType::HAND_LEFT},
    {JointType::SHOULDER_RIGHT,  JointType::ELBOW_RIGHT},
    {JointType::ELBOW_RIGHT,     JointType::WRIST_RIGHT},
    {JointType::WRIST_RIGHT,     JointType::HAND_RIGHT},
    {JointType::HIP_LEFT,        JointType::HIP_RIGHT},        // pelvic line
    {JointType::SPINE_BASE,      JointType::HIP_LEFT},
    {JointType::HIP_LEFT,        JointType::KNEE_LEFT},
    {JointType::KNEE_LEFT,       JointType::ANKLE_LEFT},
    {JointType::SPINE_BASE,      JointType::HIP_RIGHT},
    {JointType::HIP_RIGHT,       JointType::KNEE_RIGHT},
    {JointType::KNEE_RIGHT,      JointType::ANKLE_RIGHT}
};
```

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Compile isolation | `KinectTypes.h` and `IKinectAdapter.h` compile without OpenNI | Separate translation unit test in build |
| Unit | `KinectMappingEngine::evaluateCondition()` all 5 conditions | Hardcoded input→output pairs per spec scenarios |
| Unit | `KinectMappingEngine::smoothing` interpolation | Feed two consecutive values, assert lerp between them |
| Unit | MockKinectAdapter determinism | `getAllTrackedBodies()` in IDLE mode → all joints at (0.5, 0.5, 0.5) |
| Unit | `SkeletonView::confidenceColor()` thresholds | Test boundary values: 0.29→red, 0.3→yellow, 0.7→green, 0.71→green |
| Integration | JointCallback → KinectMappingEngine → scene setParameter | Wire MockKinectAdapter callback to engine, assert scene receives value |
| Build | Full project compilation | `make` — zero errors, zero warnings |
| Manual | SkeletonView renders in viewport | Run with MockKinectAdapter, verify circles + bones visible |

## Open Questions

- [ ] `ofxOpenNI` compatibility with OF 0.12.1 on macOS — needs verification before full implementation. If it fails to compile, Nuitrack (paid) is the fallback with a `NuitrackKinectAdapter` conforming to the same IKinectAdapter.
- [ ] Kinect v1 requires proprietary USB/power adapter — document in installation docs.
- [ ] Whether `KinectPanel` skeleton mini-preview should use a separate `SkeletonView` instance or inline drawing with shared bone table — design currently assumes delegated shared logic.
