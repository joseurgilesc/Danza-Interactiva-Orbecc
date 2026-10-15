# Tasks: Enjambre Organico — Single Cohesive Swarm Scene

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~600–750 total (PR1 ~420–520, PR2 ~180–250) |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Suggested split | PR 1 (core scene + FSM + debug markers) → PR 2 (noise/visual polish) |
| Delivery strategy | ask-always (orchestrator asks user before apply) |
| Chain strategy | pending |

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: pending
400-line budget risk: High

If PR1 trends past ~520 lines during apply, fallback split: PR1a anchor FSM + debug markers, PR1b particle physics.

### Suggested Work Units

| Unit | Goal | Likely PR | Notes |
|------|------|-----------|-------|
| 1 | Functional scene: anchor FSM, spring particles, basic noise, all 9 params, debug markers, minimal render | PR 1 | Base depends on chain strategy; verified via build + mouse-joint checklist |
| 2 | Organic polish: 2-octave noise, fluorescent palettes, additive blend, 0-joint alpha fade | PR 2 | Depends on PR 1; verified via build + visual checklist |

## Phase 1: Foundation (PR 1)

- [x] 1.1 Create `src/scenes/EnjambreOrganicoScene.h`: class skeleton following `PinturaFluorescenteScene.h` boilerplate (full IBaseScene overrides, `parameters_` map, factory decl), `SwarmAnchor` + `SwarmParticle` structs, constants per design.
- [x] 1.2 In `.h`/`.cpp`: declare/implement `buildJointSequence()` — rebuild each frame from `interactionPoints_`, CANONICAL_JOINTS order first, extras appended.

## Phase 2: Core FSM + Physics (PR 1)

- [x] 2.1 Create `src/scenes/EnjambreOrganicoScene.cpp`: anchor FSM — DWELLING (track live joint + dwellTimer) → TRAVELING (smoothstep over `travelDuration`, target tracks live joint) → DWELLING(next), cyclic.
- [x] 2.2 Implement edge states: 1 joint = dwell indefinitely (breathe); 0 joints = ease to screen center; mid-travel target drop = retarget next active joint keeping `t`; 0→N reappear = normal clamped travel.
- [x] 2.3 Apply `maxAnchorSpeed * dt` clamp as single choke point AFTER FSM output.
- [x] 2.4 Implement particle update: pre-sized `std::vector<SwarmParticle>`, critically-damped spring (`damping = 2*sqrt(k)`) toward `anchor + noiseOffset`, single-octave Perlin offset v1, all `dt`-scaled; resize on `particleCount` change.
- [x] 2.5 Implement minimal `draw()`: filled circles, flat color, dark background.

## Phase 3: Integration + Debug (PR 1)

- [x] 3.1 Implement `activate()`: register all 9 floats under `enjambreOrganico.` prefix (ranges/defaults per design table), read saved values, preserve `interactionPoints_`, decay 0.08; `deactivate()`: unregister + clear points.
- [x] 3.2 Implement debug markers toggle (`setDebugMarkers` pattern): anchor cross + state label, target joint ring, active joint dots, segment line; nothing drawn when off.
- [x] 3.3 Modify `src/ofApp.cpp` (~3 lines after line 64): `CreateEnjambreOrganicoScene()` + `sceneEngine.loadScene("enjambreOrganico", ...)` — distinct from `particleSwarm`.

## Phase 4: PR 1 Verification

- [x] 4.1 Build: `make -j$(sysctl -n hw.ncpu)` compiles clean.
- [ ] 4.2 Manual checklist (mouse as synthetic joint + debug markers): travel pacing ≈ `travelDuration`+`dwellDuration`; cyclic order; 1-joint breathe (no jitter/self-bounce); 0-joint center ease; teleport clamp; mid-travel joint drop retarget; live `travelDuration` tuning applies next segment; debug toggle off hides markers; `particleSwarm` still loads.

## Phase 5: Noise + Visual Polish (PR 2)

- [x] 5.1 Upgrade noise to 2-octave Perlin orbital offset (slow drift + fast shimmer), driven by `noiseAmount`/`noiseSpeed`, continuous evolution.
- [x] 5.2 Add fluorescent palette coloring, soft circles, additive blend, `particleSize` variation.
- [x] 5.3 Implement 0-joint slow alpha fade to ~40% and restore on joint reappear.
- [x] 5.4 Resolve open question: trails/accumulate mode vs clean frame (decide with user; default clean frame).

## Phase 6: PR 2 Verification

- [x] 6.1 Build: `make -j$(sysctl -n hw.ncpu)` compiles clean.
- [ ] 6.2 Manual checklist: organic feel via live registry tuning; 0-joint fade; no respawn pops; confirm 3 s / 1.5 s pacing with dancer on real Kinect.
