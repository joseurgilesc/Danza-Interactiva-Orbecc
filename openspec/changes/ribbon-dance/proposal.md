# Proposal: RibbonDanceScene — Fluorescent Ribbon Dance Scene

## Intent

Create `RibbonDanceScene`: elegant fluorescent ribbon trails that respond to interaction points and joint velocity. Each active interaction point emits a continuous ribbon rendered with `ofMesh` + `OF_PRIMITIVE_TRIANGLE_STRIP`, spawining dust/spark particles. Velocity modulates width, brightness, undulation, and particle emission. Ribbons fade gracefully when their source point disappears.

Problem: existing scenes (PinturaFluorescente, FlujoArticular) use particle-only visuals — no continuous ribbon/trail rendering exists in the codebase.

## Scope

### In Scope
- `RibbonDanceScene` (`.h` + `.cpp`): new scene, inherits `IBaseScene`
- `RibbonTrail` class: per-interaction-point ribbon with point history, variable width, alpha fade, gravity, wave undulation, velocity influence
- `RibbonParticle` class + pool: dust/spark particles emitted along ribbons, oldest-eviction cap
- `ofMesh` + `OF_PRIMITIVE_TRIANGLE_STRIP` rendering (first use in codebase)
- Velocity smoothing (moving average/damping) from `InteractionPoint::velocity`
- Jump protection: configurable max distance threshold, starts new ribbon segment
- Skeleton-loss handling: existing ribbons fade, new ribbons spawn on reconnect
- Color from global palettes (same `getColorFromPalette()` pattern as PinturaFluorescenteScene)
- Parameter exposure: ~20 configurable parameters (ribbon geometry, particle behavior, velocity influence)
- Registration in `SceneEngine` / `ofApp.cpp`

### Out of Scope
- Modifying existing scenes or their physics
- New rendering pipeline (2D only, same additive/alpha blending as PinturaFluorescente)
- Serialization of ribbon state
- 3D ribbons or depth-aware rendering

## Capabilities

### New Capabilities
- `ribbon-dance-scene`: continuous ribbon trails responding to multi-point interaction with velocity-modulated visuals

### Modified Capabilities
None — no existing spec-level behavior changes.

## Approach

**Independent scene** based on PinturaFluorescenteScene pattern:
- `RibbonTrail` and `RibbonParticle` embedded in header (following project convention)
- `std::map<std::string, RibbonTrail>` keyed by interaction point ID
- `std::vector<RibbonParticle>` particle pool with hard cap + oldest-eviction
- Ribbon mesh rebuilt each frame from trail point history — width tapers toward tail, alpha fades, gravity pulls older points down
- Wave undulation: sinusoidal displacement offset based on time + velocity magnitude
- Particle emission triggered by: high velocity, sharp curves, new ribbon segment
- Velocity smoothed via exponential moving average, exposed as parameter
- `setupPalettes()` + `getColorFromPalette()` reuses existing 5-palette system

## Dependencies

- `InteractionPoint` (existente, `src/scenes/InteractionPoint.h`)
- `IBaseScene` / `BaseScene.h` (existente)
- `ParameterRegistry` con 5 paletas globales (existente)
- `SceneEngine::loadScene()` (existente)

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/scenes/RibbonDanceScene.h` | New | RibbonTrail + RibbonParticle structs, RibbonDanceScene class (~150 lines) |
| `src/scenes/RibbonDanceScene.cpp` | New | Full implementation (~1000 lines) |
| `src/ofApp.cpp` | Modified | Add `#include`, factory call, `loadScene()` (~5 lines) |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| No `ofMesh`/`OF_PRIMITIVE_TRIANGLE_STRIP` precedent in codebase | Medium | Standard OF API, well-documented; isolated in draw method |
| Velocity smoothing needs empirical tuning | Low | Expose damping factor as parameter; sensible defaults (0.96) |
| Particle count may impact FPS with multiple ribbons | Medium | Hard cap + oldest-eviction; conservative defaults (600-1200) |
| Jump protection false positives/negatives | Low | Configurable threshold; default 150px covers most cases |

## Rollback Plan

Delete `src/scenes/RibbonDanceScene.h` and `src/scenes/RibbonDanceScene.cpp`. Remove the 5-line registration block from `src/ofApp.cpp`. No impact on existing scenes.

## Success Criteria

- [ ] Scene compiles and activates via `SceneEngine`
- [ ] Ribbons render as continuous triangle strips with variable width and alpha fade
- [ ] Multiple simultaneous ribbons from multiple interaction points
- [ ] Velocity visibly modulates width, brightness, undulation, and particle emission
- [ ] Skeleton loss triggers graceful fade; reconnect spawns fresh ribbons
- [ ] Jump protection prevents discontinuous ribbon segments
- [ ] Build successful (`make`) without new warnings
- [ ] Scene responds to `activate()`/`deactivate()` without leaks
