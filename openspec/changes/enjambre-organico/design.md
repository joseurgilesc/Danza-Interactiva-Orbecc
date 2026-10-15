# Design: Enjambre Organico — Single Cohesive Swarm Scene

## Technical Approach

Leader-follower (exploration Approach 1). One invisible **swarm anchor** owns ALL travel decisions via a two-state machine (`DWELLING` / `TRAVELING`); particles never pick targets — each follows `anchor + noiseOffset` through a spring-damper. This structurally eliminates FlujoArticular's per-particle-targeting bug family (jitter, self-bounce, pacing fights). O(n) per frame, fixed pre-allocated pool, all forces scaled by `dt`.

## Architecture Decisions

| Decision | Choice | Alternatives rejected | Rationale |
|---|---|---|---|
| Travel ownership | Single anchor state machine | Per-particle targeting (FlujoArticular) | 18-commit pain map: distributed advance decisions cause jitter/self-bounce; one traveler = one pacing knob |
| Travel easing | Time-parametric smoothstep `t∈[0,1]` over `travelDuration` seconds, from captured start pos to live joint pos | Velocity-based steering | Slowness becomes first-class (seconds per segment); steering speeds drift with forces |
| Cohesion | Critically-damped spring to `anchor + offset` | Boids seek+separation | O(n) vs O(n²); springs to a shared anchor cannot oscillate as a group; boids deferred unless swarm reads as blob |
| Organic motion | 2-octave Perlin orbital offset per particle (slow drift + fast shimmer), evolves continuously | Random walk / wander force | Noise offsets are positional, not forces → no energy injection, no respawn pop, framerate-independent |
| Teleport defense | Clamp anchor displacement to `maxAnchorSpeed * dt` AFTER state-machine output | Input filtering on joints | Single choke point covers travel retarget, dwell tracking, and edge-state transitions uniformly |
| Particle lifecycle | Fixed persistent population (no kill/respawn) | Pool acquire/release per spawn | FlujoArticular commits 010632b→b827bae: respawn pops; persistent particles + smooth noise = zero pops |
| Rendering v1 | Soft filled circles, additive blend, fluorescent palettes, dark bg, ONE visual | Trails + glow + bubbles | Lesson 7a086cf: one strong visual; accumulate/trails deferred to polish slice |
| Activation semantics | `activate()` preserves `interactionPoints_`, decay 0.08; `deactivate()` clears + unregisters params | Clear on activate | Lesson e3df953: joints accumulate while scene is loaded-but-inactive |

## Data Flow

    OSC joints ──setParameter(id, vec2)──→ interactionPoints_ map (decay 0.08)
                                                  │
                                  buildJointSequence() each frame
                          (CANONICAL_JOINTS active-first, then extras e.g. "mouse")
                                                  │
                                          SwarmAnchor FSM
                                                  │ clamped position
                                  particles[i]: spring(anchor+noise(i,t)) ──→ draw()

Anchor state machine (per `update(dt)`):

    DWELLING(joint i) ── dwellTimer ≥ dwellDuration AND ≥2 active joints ──→ TRAVELING(i→next)
        │ tracks live joint pos + idle breathe noise
    TRAVELING ── t ≥ 1 ──→ DWELLING(next)
        │ pos = mix(startPos, liveTargetPos, smoothstep(t))   // target tracks moving joint
        │ target joint drops mid-travel → retarget to next active joint, keep t (clamp absorbs jump)
        │ only 1 joint remains → finish travel to it, then DWELL indefinitely (breathe, never advance)
        │ 0 joints → target = screen center, state DWELLING at center, swarm slowly fades alpha to ~40%
    0→N joints reappear → normal travel to first active joint (clamped, no teleport)

## File Changes

| File | Action | Description |
|---|---|---|
| `src/scenes/EnjambreOrganicoScene.h` | Create | Scene class, `SwarmAnchor`, `SwarmParticle`, constants, factory decl |
| `src/scenes/EnjambreOrganicoScene.cpp` | Create | IBaseScene impl, FSM, physics, rendering, param registration |
| `src/ofApp.cpp` | Modify | ~3 lines after line 64: `CreateEnjambreOrganicoScene()` + `sceneEngine.loadScene(...)` (no dynamic_cast hooks needed) |

No Makefile changes (src auto-globbed). No SceneEngine changes.

## Interfaces / Contracts

Follow `PinturaFluorescenteScene.h` boilerplate exactly (full IBaseScene override set, `parameters_` map, register on `activate()` / unregister on `deactivate()`).

```cpp
struct SwarmAnchor {
    enum class State { Dwelling, Traveling };
    State state; glm::vec2 pos, travelStart;
    int currentJointIdx, targetJointIdx;   // indices into frame-built sequence
    std::string currentJointId, targetJointId; // ids survive sequence reshuffle
    float travelT, dwellTimer;
};
struct SwarmParticle {            // plain struct, std::vector pre-sized, no pool churn
    glm::vec2 pos, vel; float noiseSeed, size; ofFloatColor color;
};
```

Parameters (registry prefix `enjambreOrganico.`):

| Param | Default | Range | Notes |
|---|---|---|---|
| `travelDuration` | 3.0 s | 0.5–10 | slowness knob #1 |
| `dwellDuration` | 1.5 s | 0–8 | slowness knob #2 |
| `swarmRadius` | 80 px | 10–300 | offset distribution radius |
| `noiseAmount` | 30 px | 0–150 | orbital wobble amplitude |
| `noiseSpeed` | 0.3 | 0.01–2 | noise time rate |
| `springStiffness` | 4.0 | 0.5–20 | damping derived: `2*sqrt(k)` (critical) |
| `particleCount` | 1500 | 100–3000 | resize on change, O(n) safe |
| `maxAnchorSpeed` | 600 px/s | 100–3000 | teleport clamp |
| `particleSize` | 4 px | 1–12 | base circle size |

Debug markers (toggle like PinturaFluorescente's `setDebugMarkers`): anchor cross + state label, target joint ring, active joint dots, segment line.

## Testing Strategy

| Layer | What | Approach |
|---|---|---|
| Build | Compiles clean | `make` (project verification standard) |
| Manual | Travel pacing, 1-joint breathe, 0-joint center fade, teleport clamp, mid-travel joint drop | Checklist in verify phase; mouse as synthetic joint + debug markers; real Kinect for tuning |
| Live tuning | Organic feel | All params adjustable via registry panel |

No automated tests (`strict_tdd: false`).

## Migration / Rollout

No migration. Rollback = delete 2 files + revert ~3 ofApp.cpp lines. Chained PRs planned (proposal risk): PR1 core scene + FSM + debug markers, PR2 noise/visual polish.

## Open Questions

- [ ] Trails vs accumulate mode preference (proposal Q1) — v1 ships clean-frame circles; decide at polish slice.
- [ ] Default pacing 3 s / 1.5 s "slow enough" — confirm with dancer during live tuning.
