# Proposal: Enjambre Organico — Single Cohesive Swarm Scene

## Intent

Current interactive scenes (e.g. PinturaFluorescente) spawn particle groups per interaction point. The choreography needs ONE cohesive swarm that travels slowly and organically through the dancer's body, joint to joint. The archived FlujoArticularScene attempted similar flow with per-particle targeting and burned 18 fix commits on jitter, self-bounce, and pacing — this change builds a new scene from scratch with a structurally different approach.

## Scope

### In Scope
- New `EnjambreOrganicoScene` (scene id `enjambreOrganico`), 2 new files: `src/scenes/EnjambreOrganicoScene.h/.cpp`.
- Leader-follower swarm: single anchor state machine (dwell → travel via smoothstep, seconds per segment) + spring-damper particles + layered Perlin orbital noise.
- Anchor max-speed clamp against tracking teleports.
- Explicit single-joint (dwell + breathe) and zero-joint (ease to screen center) states.
- PinturaFluorescente-style fluorescent palettes, dark background, debug-markers mode.
- All motion/timing params exposed via ParameterRegistry (`travelDuration`, `dwellDuration`, `swarmRadius`, `noiseAmount`, `noiseSpeed`, `springStiffness`, `particleCount`).
- Registration in `src/ofApp.cpp` (~3 lines).

### Out of Scope
- Restoring archived FlujoArticularScene (lessons inform design only).
- Modifying PinturaFluorescenteScene, refactoring ofApp casts, new rendering systems.
- Boids separation pass / spatial grid (defer unless swarm reads as a blob).

## Capabilities

### New Capabilities
- `enjambre-organico-scene`: swarm anchor state machine, particle following dynamics, joint path traversal, edge-case states, exposed parameters, rendering and debug markers.

### Modified Capabilities
- None.

## Approach

Exploration Approach 1 (leader-follower): one invisible anchor traverses active joints in canonical order; particles never pick targets — each follows the anchor via critically-damped spring plus per-particle noise offset. Slowness is first-class (durations in seconds). Fixed persistent particle population → no respawn pop. Framerate-independent (`* dt`). Copy FlujoArticular's `activate()` semantics (preserve interaction points, decay 0.08).

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/scenes/EnjambreOrganicoScene.h/.cpp` | New | Scene implementation |
| `src/ofApp.cpp` | Modified | Factory + `loadScene` registration (~3 lines) |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Organic feel needs subjective tuning | High | Expose all motion params in panel from day one |
| Joint teleports lurch anchor | Med | Max-speed clamp; verify with real Kinect input |
| 1/0-joint edge states regress | Med | Designed-in states; manual test checklist in verify |
| PR exceeds 400-line review budget | High | Plan chained PRs: core scene → noise/visual polish |

## Rollback Plan

Remove the 2 new files and the ~3 registration lines in `ofApp.cpp`; no shared code touched, so revert is a single clean commit.

## Dependencies

- Live Kinect/MediaPipe OSC input for motion tuning (mock adapter for build verification).

## Success Criteria

- [ ] `make` builds clean; scene selectable as `enjambreOrganico` without colliding with `particleSwarm`.
- [ ] One cohesive swarm visibly travels joint-to-joint at seconds-per-segment pacing.
- [ ] Single-joint and zero-joint states behave as specified (no jitter/bounce).
- [ ] All listed params adjustable live via registry panel; debug markers toggle works.

## Proposal question round

Assumptions needing user confirmation (proceeding with these defaults):
1. Visual style: soft filled circles, ONE strong visual (no trails+glow+bubbles stacking) — confirm trails vs accumulate mode preference at design time.
2. Default pacing ~3 s travel / ~1.5 s dwell is "much slower" enough.
3. Beat/bar hooks unused in v1 (pure continuous motion).
