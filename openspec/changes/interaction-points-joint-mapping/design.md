# Design: Multi-Point Joint-to-Scene Interaction Mapping

## Technical Approach

Replace direct `ofGetMouse()` calls in scene `update()` loops with a per-scene `map<string, InteractionPoint>` driven by two sources: (1) Kinect joints via new `VEC2_XY` dispatch in KinectMappingEngine, and (2) mouse via fixed `mouseMoved` routing when `global.useMouseInput` is ON. Interaction points auto-decay on tracking loss via configurable linear confidence reduction. Cap at 8 points per scene.

## Architecture Decisions

| Decision | Option | Tradeoff | Chosen |
|----------|--------|----------|--------|
| **Normalized→Pixel conversion** | A: KinectMappingEngine | Engine polluted with screen concerns | ❌ |
| | B: Scene::setParameter(vec2) | Scene must know coordinate space | ❌ |
| | C: InteractionPoint::setTarget | Conversion at owning struct; single responsibility | ✅ |
| | **Rationale**: InteractionPoint owns `position` (pixels) — it should own the conversion. Scene passes `(normalized, confidence, screenW, screenH)` and the point is self-contained. |
| **InteractionPoint placement** | Inline in BaseScene.h | Tight coupling to interface header | ❌ |
| | Separate InteractionPoint.h | Cleaner separation; independent unit | ✅ |
| | **Rationale**: InteractionPoint is a standalone data structure with its own behavior. A separate header `src/scenes/InteractionPoint.h` allows independent testing and avoids bloating BaseScene.h. Included by `BaseScene.h`. |
| **useMouseInput storage** | Field in SceneEngine | Requires custom getter/setter | ❌ |
| | Global param in ParameterRegistry | Consistent with bpm/timeScale pattern; auto-visible in UI | ✅ |
| | **Rationale**: Global parameters are the established pattern. `global.useMouseInput` registered in `SceneEngine::registerGlobalParameters()` alongside BPM, timeScale, etc. ParamPanel, KinectPanel, and scenes all read it via registry. |

## Data Flow

```
MediaPipePoseAdapter ──→ joint callback ──→ KinectMappingEngine::processJointData
                                                  │
                          ┌───────────────────────┼────────────────────────────┐
                          │ [SCALAR]              │ [VEC2_XY]                  │
                          │ scene->setParameter(   │ scene->setParameter(       │
                          │   paramId, float)      │   paramId, vec2(x, y))    │
                          │ [existing]             │ [new]                     │
                          └───────────────────────┼────────────────────────────┘
                                                  │
                          ofApp::mouseMoved(x,y) ──┤
                          → InputRouter            │
                          → SceneEngine            │
                          → if useMouseInput:      │
                              scene->setParameter(  │
                                "mouse", vec2(x,y)) │
                                                  │
                    Scene::setParameter(glm::vec2) │
                         │                        │
                         ▼                        │
            InteractionPoint::setTarget(          │
              normalized, confidence, screenW, H) │
                         │                        │
                    ┌────▼────┐                   │
                    │ convert │                   │
                    │→ pixels │                   │
                    └────┬────┘                   │
                         │                        │
                    ┌────▼────┐                   │
                    │velocity │ = (new-old)/dt    │
                    │position │                   │
                    └─────────┘                   │
                                                  │
Scene::update():                                 │
  for each active point:                         │
    point.update(dt)  // decay + trajectory       │
    use point.position (pixels) for effect        │
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/scenes/InteractionPoint.h` | **Create** | Struct with position, velocity, confidence, decay, active, source. `setTarget()` converts normalized→pixels. `update()` applies decay + trajectory. |
| `src/scenes/BaseScene.h` | Modify | `#include "InteractionPoint.h"` |
| `src/scenes/FurryScene.h` | Modify | Replace `glm::vec2 interactionPoint_` with `std::map<std::string, InteractionPoint> interactionPoints_`. Add helpers: `addPoint()`, `getActivePoints()`. |
| `src/scenes/FurryScene.cpp` | Modify | Implement `setParameter(glm::vec2)` — routes to InteractionPoint map. `update()` iterates `getActivePoints()` instead of `ofGetMouse()`. |
| `src/scenes/AnemoneFieldScene.h` | Modify | Same pattern as FurryScene. |
| `src/scenes/AnemoneFieldScene.cpp` | Modify | Same. |
| `src/scenes/PinturaFluorescenteScene.h` | Modify | Add `interactionPoints_` map for multi-point attractors. Replace `attractorPoint_` usage. |
| `src/scenes/PinturaFluorescenteScene.cpp` | Modify | `update()` averages active interaction points for attractor center. |
| `src/mapping/KinectMappingTypes.h` | Modify | Add `outputMode` field to `KinectMapping` (enum: `SCALAR`, `VEC2_XY`). |
| `src/mapping/KinectMappingEngine.cpp` | Modify | In `processJointData()`: when `outputMode == VEC2_XY`, parse joint name (skip axis), extract `(position.x, position.y)` from `Vector3D`, dispatch via `scene->setParameter(paramId, glm::vec2(x, y))`. |
| `src/engine/SceneEngine.cpp` | Modify | Register `global.useMouseInput` (BOOL, default true) in `registerGlobalParameters()`. |
| `src/ofApp.h` | Modify | Add `void mouseMoved(int x, int y) override;` |
| `src/ofApp.cpp` | Modify | Implement `mouseMoved`: route through `uiManager` + `inputRouter`. |
| `src/ui/KinectPanel.h` | Modify | Add `int editOutputMode_` field. |
| `src/ui/KinectPanel.cpp` | Modify | Mapping editor: add `outputMode` dropdown (SCALAR/VEC2_XY). When VEC2_XY: hide axis fields. Add `useMouseInput` checkbox in status bar. |

## Interfaces

### InteractionPoint (new)

```cpp
struct InteractionPoint {
    glm::vec2 position;         // current position in PIXELS
    glm::vec2 velocity;         // pixels/frame for trajectory
    float confidence;           // 0.0-1.0 from joint source
    float decayRate;            // configurable (default 0.3, ~3.3s fade)
    bool active;                // receiving data?
    std::string sourceName;     // e.g. "hand_right", "mouse"
    float lastUpdateTime;       // for time-based decay

    static constexpr float DEFAULT_DECAY_RATE = 0.3f;
    static constexpr int MAX_POINTS_PER_SCENE = 8;

    void update(float deltaTime);
    void setTarget(glm::vec2 normalizedPos, float conf,
                   float screenW, float screenH);
};
```

### KinectMapping addition

```cpp
enum class KinectOutputMode : uint8_t { SCALAR, VEC2_XY };

// Add to KinectMapping struct:
KinectOutputMode outputMode = KinectOutputMode::SCALAR;
```

## Decay Behavior

Linear decay: `confidence -= decayRate * deltaTime`. While `confidence > 0 && !active`, `position += velocity * deltaTime`. When `confidence <= 0`, point removed from active set.

Default `decayRate = 0.3` (~3.3s fade). Rates: `0.0` (instant vanish), `0.1` (~10s), `0.5` (~2s).

## Thread Safety

All scene state and interaction points are on the main OF thread. `processJointData` is called from adapter callbacks within `ofApp::update()`, also main thread. SceneEngine uses `std::mutex` already. **No additional locks needed.**

## Testing Strategy

| Layer | What | Approach |
|-------|------|----------|
| Unit | `InteractionPoint::setTarget` conversion | Feed normalized (0.5, 0.5) with 1920×1080 → expect (960, 540) |
| Unit | `InteractionPoint::update` decay | Set confidence=1.0, decayRate=0.3, dt=1.0 → confidence=0.7 |
| Integration | VEC2_XY dispatch → scene | Create mapping with outputMode=VEC2_XY, verify scene receives vec2 |
| Integration | mouseMoved → scene when useMouseInput ON | Move mouse, verify "mouse" interaction point updated |
| Integration | mouseMoved blocked when useMouseInput OFF | Verify scene update() ignores mouse when toggle is off |
| E2E | Manual: run with MediaPipe bridge, verify fibers react to hands | |

## Migration / Rollout

No migration required. Existing float-based Kinect mappings continue working (outputMode defaults to SCALAR). Visual behavior changes are additive: scenes still respond to mouse when `useMouseInput=true` (default), and joint-driven interaction points are opt-in via new VEC2_XY mappings.

Rollback per section: revert mouseMoved in ofApp.h, remove VEC2_XY mappings, restore single `interactionPoint_` and `ofGetMouse()` in scenes.

## Open Questions

- None. All architectural decisions resolved.
