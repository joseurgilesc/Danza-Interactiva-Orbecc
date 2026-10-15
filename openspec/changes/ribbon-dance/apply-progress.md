## Apply Progress: RibbonDanceScene — PR 3/3: Wiring + Verification (FINAL)

**Mode**: Standard  
**Build**: Pass (`make -j4` — compiled and linked clean, zero new warnings)  
**Batch**: 1 of 1 for PR 3  

### Completed Tasks (All Phases — Cumulative)

- [x] **1.1** Create `RibbonDanceScene.h` — RibbonTrail/RibbonParticle structs, class declaration, constexprs, factory
- [x] **1.2** Create `RibbonDanceScene.cpp` — stub implementations for all IBaseScene methods
- [x] **1.3** Palette system — 5-palette (Rosa, Cian, Verde, Naranja, Amarillo) with hue jitter
- [x] **2.1** `TrailPoint` struct — position (glm::vec2), velocity, age, width, alpha, intensity
- [x] **2.2** `RibbonTrail::setup()` — ring buffer allocation, head/count/active/fading reset
- [x] **2.3** `RibbonTrail::addPoint()` — EMA velocity smoothing, jump protection (soft-cut), ring-buffer write
- [x] **2.4** `RibbonTrail::update()` — gravity on aged points, velocity damping, position integration, fade-out alpha decrement, age increment
- [x] **2.5** `RibbonTrail::buildMesh()` — triangle strip OF_PRIMITIVE_TRIANGLE_STRIP; age-based normalizedAge; width = mix(maxWidth, minWidth, normAge) * intensity; alpha = (1-normAge) * ribbonAlpha; wave undulation; glm::vec3 vertices + ofFloatColor mesh colors
- [x] **2.6** `RibbonTrail::draw()`, `fadeOut()`, `isDead()` — mesh.draw(), fading flag, dead when alpha <= 0.01 && count == 0
- [x] **3.1** `RibbonParticle::setup()` — position, velocity, color, size, maxLife; life = maxLife; active = true
- [x] **3.2** `RibbonParticle::update()` — gravity, damping, position integration, life decrement, alpha = life/maxLife
- [x] **3.3** `RibbonParticle::draw()` — ofSetColor with palette-derived color + alpha; ofDrawCircle
- [x] **3.4** Pool management — `std::vector<RibbonParticle>` pre-allocated; `emitParticles()` finds inactive slots or overwrites oldest active; `particlesPerBurst` spawn with randomized vel/size/life
- [x] **4.1** Scene `update()` — IP decay, ribbon management per pointId, addPoint(), fade-out on inactive IPs, ribbon update + particle emission, dead ribbon cleanup, particle update
- [x] **4.2** Skeleton-loss handling — when no active IPs, all ribbons enter fade-out; on reconnect, fresh ribbons spawn
- [x] **4.3** Scene `draw()` — ofEnableAlphaBlending(); buildMesh() + draw() per ribbon; draw active particles; debug markers with velocity lines + ID labels
- [x] **5.1** `initializeParameterDefinitions()` — 21 params (12 ribbon + 9 particle); 3 INT params use `ParameterType::INT`, rest use `ParameterType::FLOAT`
- [x] **5.2** `RibbonParams` syncing — `syncParamsFromMap()` reads from parameters_ map; `setParameter()` directly syncs to `ribbonParams_` fields (bidirectional)
- [x] **5.3** `registerParameters()` / `unregisterParameters()` — ParameterRegistry::getGlobal() pattern
- [x] **5.4** `src/ofApp.cpp` — added `#include "scenes/RibbonDanceScene.h"` + `CreateRibbonDanceScene()` + `loadScene("ribbonDance")` (no auto-activate, matching FlujoArticularScene pattern)
- [x] **6.1** Build verification — `make -j4` passes clean, zero new warnings

### Files Modified (PR 3)

| File | Action | Description |
|------|--------|-------------|
| `src/ofApp.cpp` | Modified | Added `#include "scenes/RibbonDanceScene.h"` (line 7); registered scene via `CreateRibbonDanceScene()` + `loadScene()` (lines 42-43, after FlujoArticularScene) |
| `src/scenes/RibbonDanceScene.h` | Modified | Renamed `MAX_PARTICLES` → `RIBBON_MAX_PARTICLES` (line 19) to avoid collision with `PinturaFluorescenteScene::MAX_PARTICLES` |
| `openspec/changes/ribbon-dance/tasks.md` | Modified | Tasks 5.4, 6.1 marked complete |
| `openspec/changes/ribbon-dance/apply-progress.md` | Modified | PR 3 cumulative progress recorded |

### Deviations from Design

- **Constant collision fix**: Renamed `MAX_PARTICLES` → `RIBBON_MAX_PARTICLES` in ribbon scene. Both scenes live in `namespace modular` and the constant was unused in ribbon code (particle count is parameter-driven via `ribbonParams_.particleMaxCount`). No behavioral impact.

### Issues Found

- **MAX_PARTICLES collision** at compile time: PinturaFluorescenteScene.h defines `MAX_PARTICLES = 5000`, RibbonDanceScene.h also defined `MAX_PARTICLES = 800`. Both in `namespace modular`, causing redefinition error when both headers are included. Fixed by renaming ribbon's constant to `RIBBON_MAX_PARTICLES`.

### Remaining Tasks

- [ ] **6.2** Lifecycle test: scene activates/deactivates without leaks (manual)
- [ ] **6.3** Edge case: 0 IPs, 8 simultaneous IPs, jump > threshold (manual)
- [ ] **6.4** Visual smoke test: debugMarkers, ribbon taper, alpha fade, wave undulation, particle bursts (manual)

### PR Boundary

- **PR**: 3 of 3 — Wiring + Verification (FINAL)
- **Target**: PR 2 branch (chain strategy: stacked-to-main)
- **Scope**: ofApp.cpp registration, `RIBBON_MAX_PARTICLES` rename, build verification. Scene is fully integrated — appears in scene list (key 1-5), loads/activates via SceneEngine.
- **Status**: Implementation complete. Remaining tasks (6.2-6.4) are manual visual verification only.
