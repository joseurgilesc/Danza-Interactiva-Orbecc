# Tasks: RibbonDanceScene

## Review Workload Forecast

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: pending
400-line budget risk: High

| Field | Value |
|-------|-------|
| Estimated changed lines | ~1085 (180 hdr + 900 cpp + 5 ofApp) |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Suggested split | 3 PRs (~300 + ~600 + ~185 lines) |
| Delivery strategy | ask-on-risk |

### Suggested Work Units

| Unit | Goal | Likely PR | Lines | Notes |
|------|------|--------|-------|-------|
| 1 | Scene skeleton: header + cpp stubs, compiles as no-op | PR 1 | ~300 | Targets `feature/ribbon-dance`; compiles, loads, draws nothing |
| 2 | Core: RibbonTrail, RibbonParticle, update/draw, 21 params | PR 2 | ~600 | Targets PR 1 branch; full visual behavior |
| 3 | Registration in ofApp.cpp + debug markers + polish | PR 3 | ~185 | Targets PR 2 branch; final integration |

## Phase 1: Foundation — Scene Skeleton

- [x] 1.1 Create `src/scenes/RibbonDanceScene.h` — Forward declares RibbonTrail/RibbonParticle (PR 2 defines); RibbonDanceScene class with all IBaseScene methods; constexpr `MAX_PARTICLES=800`, `DEFAULT_RIBBON_CAPACITY=200`, `VELOCITY_EMA_FACTOR=0.96f`; `CreateRibbonDanceScene()` factory
- [x] 1.2 Create `src/scenes/RibbonDanceScene.cpp` — stub implementations for all IBaseScene methods (`setup`, `update`, `draw`, `activate`, `deactivate`, `pause`, `resume`, input handlers, `getParameter`, `setParameter`, `getSuggestedMappings`, `getDefaultPreset`, `getSceneMetadata`, `onBeat`, `onBar`, `isValid`, `unloadResources`, `reloadResources`); `getId()` returns `"ribbonDance"`; factory function; `initializeParameterDefinitions()` with 5 essential params; `registerParameters()`/`unregisterParameters()` pattern
- [x] 1.3 Implement palette system: same 5-palette structure as PinturaFluorescenteScene (Rosa, Cian, Verde, Naranja, Amarillo); `getColorFromPalette(int idx, float variation)` returns `ofFloatColor` with ±0.15 hue jitter

## Phase 2: Core Components — RibbonTrail

- [x] 2.1 Implement `TrailPoint` struct: position, velocity, age, width, alpha, intensity fields
- [x] 2.2 Implement `RibbonTrail::setup()` — allocates ring buffer `std::vector<TrailPoint>` to capacity; resets head/count/active/fading
- [x] 2.3 Implement `RibbonTrail::addPoint()` — EMA velocity smoothing (`smoothed = factor*old + (1-factor)*rawSpeed`); jump protection (soft-cut if `dist > maxJumpThreshold`); ring-buffer write with wrap-around; `timeSinceLastUpdate` reset
- [x] 2.4 Implement `RibbonTrail::update()` — gravity on aged points (`ageFactor = min(1.0, age/3.0)`), velocity damping, position integration; fade-out alpha decrement; intensity from smoothed velocity; increment age per point
- [x] 2.5 Implement `RibbonTrail::buildMesh()` — triangle strip: iterate ring buffer from head, compute perpendicular edge per point, width lerp from maxWidth→minWidth using age-based normalizedAge (`pt.age / ribbonLife`), alpha = (1-normAge) * ribbonAlpha, wave offset `sin(time*waveSpeed + normAge*waveAmount + intensity*5) * (waveAmount*0.5)`, push left/right vertices + colors; set `OF_PRIMITIVE_TRIANGLE_STRIP`
- [x] 2.6 Implement `RibbonTrail::draw()` — call `mesh.draw()` with alpha blending enabled; `RibbonTrail::fadeOut()` sets `fading=true`; `RibbonTrail::isDead()` returns true when `alpha <= 0.01 && count == 0`

## Phase 3: Core Components — RibbonParticle + Pool

- [x] 3.1 Implement `RibbonParticle::setup()` — position, velocity, color, size, maxLife from params with randomization; `life = maxLife`; `active = true`
- [x] 3.2 Implement `RibbonParticle::update()` — gravity (`vel.y += gravity*dt`), damping (`vel *= damping`), position integration, life decrement (`life -= dt`), alpha = `life/maxLife`
- [x] 3.3 Implement `RibbonParticle::draw()` — `ofSetColor` with alpha from palette color; `ofDrawCircle(pos, size)` with alpha-scaled radius
- [x] 3.4 Implement pool management: `std::vector<RibbonParticle> particles_` pre-sized to `ribbonParams_.particleMaxCount`; `RibbonTrail::emitParticles()` searches pool for inactive slot or overwrites oldest active particle; `particlesPerBurst` spawn per trigger with randomized vel/size/life

## Phase 4: Integration — Scene Update Loop

- [x] 4.1 Implement `RibbonDanceScene::update()` — iterate `interactionPoints_`, for each active IP find/create `RibbonTrail` by `pointId`; call `addPoint()`, mark inactive IPs as `fading=true`; iterate ribbons calling `update()` + `emitParticles()` when `smoothedVelocity > emissionThreshold`; cleanup dead ribbons via `erase(dead)`; iterate particles calling `update()`
- [x] 4.2 Implement skeleton-loss handling: when all `interactionPoints_` disappear (Kinect lost), stop adding points; ribbons fade naturally; on reconnect, spawn fresh ribbons (no connect-line to old positions)
- [x] 4.3 Implement `RibbonDanceScene::draw()` — `ofEnableAlphaBlending()`; iterate ribbons calling `buildMesh()` + `draw()`; iterate active particles calling `draw()`; debug markers (IP circles + velocity lines + ID labels) when `debugMarkers_` enabled

## Phase 5: Parameters + Wiring

- [x] 5.1 Implement `initializeParameterDefinitions()` — register 21 params with `"ribbonDance."` prefix: 12 ribbon params + 9 particle params. Integer params (`ribbon.maxPoints`, `particle.maxCount`, `particle.perBurst`) use `ParameterType::INT`; all others use `ParameterType::FLOAT`.
- [x] 5.2 Implement `RibbonParams` struct syncing from `parameters_` map via `syncParamsFromMap()`; `setParameter()` bidirectionally forwards to both `parameters_` map and `RibbonParams` fields (matching PinturaFluorescenteScene pattern)
- [x] 5.3 Implement `registerParameters()` + `unregisterParameters()` via `ParameterRegistry::getGlobal()`
- [x] 5.4 Modify `src/ofApp.cpp`: add `#include "scenes/RibbonDanceScene.h"`; create scene via `CreateRibbonDanceScene()`; call `sceneEngine.loadScene(scene->getId(), scene)`

## Phase 6: Verification

- [x] 6.1 Build verification: `make` compiles without new warnings (target: `make -j4`)
- [ ] 6.2 Lifecycle test: scene activates/deactivates without leaks; switch scenes via keys (1-2-3-4) verifying ribbons + particles freed on deactivation
- [ ] 6.3 Edge case: 0 interaction points → no ribbons; 8 simultaneous IPs → 8 ribbons rendering; jump > 150px → soft-cut triggers new segment; fade-out completes in ~3s
- [ ] 6.4 Visual smoke test (manual): `debugMarkers_ = true` shows IP circles + velocity lines; ribbon width tapers head→tail; alpha fades; wave undulation visible; particle bursts on fast movement

## Requirement Coverage

| Requirement | Tasks |
|-------------|-------|
| REQ-1: Ribbon Per Interaction Point | 2.3, 4.1 |
| REQ-2: Ribbon Fade-Out on Deactivation | 2.4, 4.1 |
| REQ-3: Velocity-Modulated Visuals | 2.3, 2.4, 2.5, 3.4 |
| REQ-4: Palette Colors (Global) | 1.3, 2.1 |
| REQ-5: Triangle-Strip Ribbon Rendering | 2.5, 2.6 |
| REQ-6: Ribbon Soft Gravity | 2.4 |
| REQ-7: Particle Emissions | 3.1, 3.2, 3.3, 3.4 |
| REQ-8: Skeleton Loss Handling | 4.2 |
| REQ-9: Jump/Bounce Protection | 2.3 |
| REQ-10: Configurable Parameters | 5.1, 5.2, 5.3 |
| REQ-11: Scene Integration | 1.1, 1.2, 5.4, 6.1 |
