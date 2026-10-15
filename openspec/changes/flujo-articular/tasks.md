# Tasks: Flujo Articular Scene

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | 650–1000 |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Suggested split | PR 1: Header + boilerplate + ofApp (~453) → PR 2: Physics + rendering + params (~500) |
| Delivery strategy | ask-on-risk |
| Chain strategy | feature-branch-chain |

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: feature-branch-chain
400-line budget risk: High

### Suggested Work Units

| Unit | Goal | Likely PR | Notes |
|------|------|-----------|-------|
| 1 | Header, stubs, ofApp — compilable empty scene | PR 1 | ~453 lines, 2 new + 1 mod. Mechanical stubs, low cognitive load. |
| 2 | Physics + rendering + params — full scene | PR 2 | ~500 lines core logic. Depends on PR 1 for header/class. |

## Phase 1: Header + Structs — `FlujoArticularScene.h`

- [x] 1.1 Create header: `#pragma once`, includes (BaseScene.h, ofMain.h, glm, map, vector). Declare `FlowParticle` struct (pos, vel, color, alpha, size, life, maxLife, jointIdx, active) with `init()`.
- [x] 1.2 Declare `FlowParticlePool`: fixed array 5000 + per-slot active flag + `spawn()`/`recycle()` methods. Static `CANONICAL_JOINTS` constant: `[head, neck, shoulder_left, elbow_left, wrist_left, hand_left, shoulder_right, elbow_right, wrist_right, hand_right, spine_mid, spine_base, hip_left, knee_left, ankle_left, hip_right, knee_right, ankle_right]`.
- [x] 1.3 Class `FlujoArticularScene : public IBaseScene` — all 24 pure-virtual overrides. Private: FlowParticlePool, `std::map<std::string,InteractionPoint>`, joint sequence cache, 5 param shared_ptrs, state flags. Factory: `ScenePtr CreateFlujoArticularScene()`.

## Phase 2: IBaseScene Boilerplate — `FlujoArticularScene.cpp`

- [x] 2.1 Lifecycle: `setup()` alloc pool + params, `activate()` resets pool + interactionPoints_ + set active, `deactivate()` clears + unregisters, `pause()`/`resume()` toggle flag.
- [x] 2.2 Id + validation: `getId()` → `"flujoArticular"`, `getName()` → `"Flujo Articular"`, `isValid()` → true, `getInvalidReason()` → `""`, `getInteractionPointIds()` → canonical list.
- [x] 2.3 Param stubs: `getParameterIds()` returns 5 ids. `getParameter()`/`setParameter(float/vec2/vec3)` with `parameters_` map storage.
- [x] 2.4 All 16 remaining stubs no-op: `getSuggestedMappings`, `getDefaultPreset`, `getSceneMetadata`, `onBeat`, `onBar`, `unloadResources`, `reloadResources`, `keyPressed`, `keyReleased`, `mousePressed`, `mouseReleased`, `mouseDragged`, `mouseMoved`, `windowResized`.

## Phase 3: Flow Physics

- [x] 3.1 `setParameter("jointName", vec2)`: delegates to `interactionPoints_["jointName"].setTarget(normalizedPos, 1.0f, ofGetWidth(), ofGetHeight())`. Update applies decay on untouched points. ✅
- [x] 3.2 `buildJointSequence()`: filter `CANONICAL_JOINTS` to entries active+touched in interactionPoints_. Empty → fallback to screen center `(w/2, h/2)`. ✅
- [x] 3.3 `update()`: per active particle → seek `sequence[jointIdx].pos` with `flowSpeed`, steer damped by `inertiaDecay`. Advance `jointIdx` (wrap) when `distance < attractRadius`. `life -= dt`, deactivate expired. Spawn: per active joint, `spawnRate` particles at joint pos ± offset, random palette color, `jointIdx=index_in_seq`, `maxLife=particleLife`. ✅

## Phase 4: Rendering

- [x] 4.1 `draw()`: `ofMesh` `GL_POINTS` from active particles (pos, color, `alpha * life/maxLife`, `size * life/maxLife`). Glow second pass: larger point + lower alpha. Trail: ring buffer per particle (up to 30), fading line strip. Palette: 5 fluorescent colors from PinturaFluorescente. ✅

## Phase 5: Parameter Registration

- [x] 5.1 Register 5 params in `setup()`: `flujoArticular.flowSpeed` (0.1-5.0, 1.0), `flujoArticular.spawnRate` (1-20, 5), `flujoArticular.particleLifespan` (5-60, 20), `flujoArticular.trailLength` (0-30, 10), `flujoArticular.flowRadius` (10-200, 80). ✅
- [x] 5.2 `update()` reads current param values via `parameters_[id]->currentValue` at frame start. ✅

## Phase 6: Integration — `src/ofApp.cpp`

- [x] 6.1 Add `#include "scenes/FlujoArticularScene.h"`. Add `ScenePtr flujoScene = CreateFlujoArticularScene(); sceneEngine.loadScene(flujoScene->getId(), flujoScene);`. Do NOT auto-activate. ScenePanel auto-discovers via `getLoadedScenes()` — no UI change required.

## Phase 7: Build Verification

- [x] 7.1 `make` — zero new warnings, binary links. Confirm `"flujoArticular"` in ScenePanel dropdown. Verify activate/deactivate no crash via key switching (1-2-3 cycling).
