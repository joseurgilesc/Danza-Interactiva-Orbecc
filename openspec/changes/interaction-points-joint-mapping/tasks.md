# Tasks: Multi-Point Joint-to-Scene Interaction Mapping

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~300 |
| 400-line budget risk | Low |
| Chained PRs recommended | No |
| Suggested split | Single PR |
| Delivery strategy | ask-on-risk |
| Chain strategy | pending |

Decision needed before apply: No
Chained PRs recommended: No
Chain strategy: pending
400-line budget risk: Low

### Suggested Work Units

| Unit | Goal | Likely PR | Notes |
|------|------|-----------|-------|
| 1 | Full feature | PR 1 | Single PR, ~300 lines, 10 commits |

## Phase 1: Foundation (no deps)

- [x] **T-001** Create `src/scenes/InteractionPoint.h`. Struct with position, velocity, confidence, decayRate (default 0.3), active, sourceName, lastUpdateTime. Methods: `update(float dt)` (linear decay, trajectory), `setTarget(normalizedPos, conf, screenW, screenH)` (coord conversion + velocity calc). Static constexpr `DEFAULT_DECAY_RATE=0.3f`, `MAX_POINTS_PER_SCENE=8`. Accept: unit-testable standalone header, `active` defaults to false.

- [x] **T-002** Add `VEC2_XY` to `KinectTransformType` enum in `src/mapping/KinectMappingTypes.h`. Add `KinectOutputMode` enum `{ SCALAR, VEC2_XY }`. Add `outputMode` field to `KinectMapping` struct (default `SCALAR`). Accept: compiles with existing code, `saveToJson`/`loadFromJson` persist and restore `outputMode`.

- [x] **T-003** Register `global.useMouseInput` (BOOL, default true) in `SceneEngine::registerGlobalParameters()` (`src/engine/SceneEngine.cpp`). Add `setUseMouseInput(bool)` and `isMouseInputEnabled()` to `SceneEngine` class in `src/engine/SceneEngine.h`. Accept: parameter visible in ParamPanel, `isMouseInputEnabled()` returns true by default.

## Phase 2: Core Implementation (depends on Phase 1)

- [x] **T-004** In `src/scenes/FurryScene.h/.cpp`: Replace `glm::vec2 interactionPoint_` with `std::map<string, InteractionPoint> interactionPoints_`. Implement `setParameter(glm::vec2)` — routes to `interactionPoints_[paramId].setTarget()`. In `update()`, iterate active points instead of `ofGetMouse()`. Keep `mouseMoved` handler but have it only set interactionPoint_ (fallback). `#include "InteractionPoint.h"` in `BaseScene.h`. Accept: FurryScene compiles, `setParameter("hand_left", vec2(320,240))` creates active point, `update()` uses it.

- [x] **T-005** In `src/scenes/AnemoneFieldScene.h/.cpp`: Same pattern as T-004 — replace `interactionPoint_` with `interactionPoints_` map, implement `setParameter(glm::vec2)`, iterate active points in `update()` replacing `ofGetMouse()`. Accept: compiles, multi-point deflection works per fiber.

- [x] **T-006** In `src/scenes/PinturaFluorescenteScene.h/.cpp`: Replace `attractorPoint_` usage with `interactionPoints_` map average. Implement `setParameter(glm::vec2)`. In `update()`, compute attractor center as average of all active interaction points. For spawn position, use first active point (or center if none). Accept: compiles, spawn follows first interaction point, particles flow toward averaged center.

- [x] **T-007** In `src/mapping/KinectMappingEngine.cpp` `processJointData()`: Add VEC2_XY branch — when `mapping.outputMode == VEC2_XY`, extract joint (position.x, position.y), convert to pixels: `x * ofGetWidth()`, `y * ofGetHeight()`, dispatch via `scene->setParameter(localParamId, glm::vec2(pixelX, pixelY))`. Skip condition/range/scaling/smoothing for VEC2_XY. Accept: creating a VEC2_XY mapping dispatches vec2 to scene, existing float mappings unchanged.

## Phase 3: Input & UI (depends on Phase 1, 2)

- [x] **T-008** In `src/ofApp.h`, add `void mouseMoved(int x, int y) override;`. In `src/ofApp.cpp`, implement: forward to `uiManager.mouseMoved(x, y)` and `inputRouter->mouseMoved(x, y)`. Accept: `mouseMoved` events reach both UI and InputRouter (verified via log or debugger).

- [x] **T-009** In `src/ui/KinectPanel.h`: add `int editOutputMode_` field. In `src/ui/KinectPanel.cpp` `drawMappingEditorRight()`: add outputMode dropdown (`SCALAR`/`VEC2_XY`). When VEC2_XY: hide condition/transform/range fields. In `drawStatusBar()`: add `ImGui::Checkbox("Mouse Input", &mouseInputEnabled)`, reading/writing `global.useMouseInput` via ParameterRegistry. Accept: checkbox toggles, persists across scene changes, VEC2_XY selection hides scalar fields.

## Phase 4: Integration (depends on all)

- [x] **T-010** In `SceneEngine::mouseMoved()` (`src/engine/SceneEngine.cpp`): after forwarding to scene, if `isMouseInputEnabled()`: call `activeScene->setParameter("mouse", glm::vec2(x, y))`. Remove `ofGetMouse()` calls from all 3 scene update() loops (already done in T-004/5/6). In scene `mouseMoved` handlers: keep forwarding but replace `interactionPoint_` assignment with no-op (deprecated). Accept: with useMouseInput ON, moving mouse populates `"mouse"` interaction point; with OFF, no mouse point created. All 3 scenes use interactionPoints_ only.

- [x] **T-011** Manual validation: run with MediaPipe bridge, create VEC2_XY mapping (e.g., hand_right → furry.hand_right). Verify fibers react to hand position. Toggle useMouseInput OFF, verify mouse has no effect. Stop tracking, verify decay fade-out (~3.3s). Verify existing float mappings still work. Accept: all 6 success criteria from proposal pass.
