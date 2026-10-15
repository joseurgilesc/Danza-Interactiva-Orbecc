# Tasks: FluorescentNet Scene

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | 1400–1600 |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Suggested split | PR 1 → PR 2 → PR 3 |
| Delivery strategy | ask-on-risk |
| Chain strategy | feature-branch-chain |

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: feature-branch-chain
400-line budget risk: High

### Suggested Work Units

| Unit | Goal | Likely PR | Notes |
|------|------|-----------|-------|
| 1 | Skeleton, types, params, FREE orbit physics | PR 1 | Base = feature/fluorescent-net. ~450 changed lines. |
| 2 | ANCHORED net physics, transition, edge mgmt, rendering | PR 2 | Base = PR 1 branch. ~450 changed lines. |
| 3 | ofApp wiring, preset JSON, polish, build verify | PR 3 | Base = PR 2 branch. ~160 changed lines. |

## Phase 1: Skeleton + Params + FREE Physics (PR 1)

- [x] 1.1 Create `src/scenes/FluorescentNetScene.h`: `ParticlePhase` enum (FREE, BLENDING, ANCHORED), `NetParticle` struct (pos, vel, color, phase, jointId, phaseOffset, life, originalPos, noiseOffset, transitionTimer, blendAlpha), `FluorescentEdge` struct (a, b, restLength, active), class decl with 28 parameters, factory `CreateFluorescentNetScene()`
- [x] 1.2 Create `src/scenes/FluorescentNetScene.cpp`: constructor, `getId()` → `"fluorescentNet"`, `setup()` (initialize pool, spatial grid), `activate()`/`deactivate()` stubs, `getName()` → `"Fluorescent Net"`
- [x] 1.3 Register 28 parameters via `registry_` with `fluorescentNet.` prefix: orbit (8), net (5), noise/wind (5), transition (2), topology (2), visual (5), pool (1) — all ranges and defaults per design.md
- [x] 1.4 Implement FREE orbital physics: per-frame `InteractionPoint` lookup, `resolveCenter()` with wave `A*sin(w*t - q*dist + phase)` + swirl `S*cos(ws*t - qs*dist + phase)`, center attraction `k*(target-pos)`, damping `beta`, `maxSpeed` clamp. Update `ParticlePhase::FREE` pool segment.
- [x] 1.5 Implement FREE rendering pass: filled circles, palette color, alpha = `255 * blendAlpha`, size = `particleSize * (1.0 - transitionTimer * 0.5)`, `ofEnableAlphaBlending()`
- [x] 1.6 Implement particle spawn/deactivation: spawn per active interaction point per frame (respect `maxParticles` cap), deactivate stale FREE particles when joint inactive > 3s

## Phase 2: ANCHORED Physics + Transition + Rendering (PR 2)

- [x] 2.1 Implement ANCHORED physics: gravity (`vel.y += gravity * dt`), wind (`sin(time * windFreq + windBias) * windStrength * dt`), Perlin drift (`ofSignedNoise(pos*noiseScale+time) * noiseStrength * dt`), damping (`vel *= damping`), return force (`(originalPos - pos) * returnForce * dt`)
- [x] 2.2 Implement spring relaxation: `springIterations` passes over `edges_`, compute `delta = posB - posA`, `diff = |delta| - restLength`, correction `posA += delta * diff * springStiffness * 0.5`, `posB` inversely
- [x] 2.3 Implement transition FSM: `transitionTimer += dt * transitionRate`, enter BLENDING at ≥1.0, blend physics (lerp FREE/ANCHORED velocities) over `blendPeriod`, finalize to ANCHORED — lock `originalPos`, clamp velocity to `maxAnchorSpeed`
- [x] 2.4 Implement edge management: on anchor finalize, neighbor search within `connectionRadius` (brute-force under 500 nodes, spatial hash above), create edges to ≤`maxConnectionsPerNode` nearest ANCHORED neighbors, per-frame dissolution when dist > `connectionRadius * 1.5`
- [x] 2.5 Implement rendering passes 2–4: ANCHORED edges (alpha-faded lines per distance deviation), ANCHORED node dots (`nodeRadius`, `nodeAlpha`), debug joint markers (green dots at active points)

## Phase 3: Integration + Preset + Polish (PR 3)

- [x] 3.1 Wire `src/ofApp.cpp`: add include, `modular::CreateFluorescentNetScene()`, `sceneEngine.loadScene(scene->getId(), scene)`, `presetManager_->loadPresets("fluorescentNet", "config/presets/fluorescentNet.json")`
- [x] 3.2 Create `config/presets/fluorescentNet.json`: sceneId `"fluorescentNet"`, 3 presets — "germinal" (300 particles, slow transition, small net radius), "creciente" (800 particles, medium), "maduro" (2000 particles, fast transition, large net)
- [x] 3.3 Edge-case hardening: cap velocity to `maxAnchorSpeed` on anchor event, enforce `maxAnchoredNodes` cap in transition logic, enforce `maxConnectionsPerNode` during edge creation, handle zero active joints gracefully (drift FREE particles to center)
- [x] 3.4 Build verification: `make` from project root, confirm zero errors and warnings
