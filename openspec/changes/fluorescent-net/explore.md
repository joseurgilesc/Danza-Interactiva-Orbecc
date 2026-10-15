# Exploration: FluorescentNet — Hybrid Scene (PinturaFluorescente + OrganicNet)

## Current State

The project has 7 working scenes, two of which are the direct ancestors of FluorescentNet:

### PinturaFluorescenteScene (`src/scenes/PinturaFluorescenteScene.h/.cpp`)
- **Particle system**: Fixed-size `ParticlePool` (default 5000 capacity, resizable). Particles have position, velocity, color, alpha, life, phase, and `pointId` (which `InteractionPoint` they orbit).
- **Physics**: Orbital wave physics — center attraction `k`, sinusoidal wave `A*sin(w*t - q*d + phase)`, swirl `S*cos(ws*t - qs*d + phase)`, damping `beta`, optional attractor. Each particle orbits its assigned interaction point via `resolveCenter()` per-frame. An alternative `flockMode` transitions to boid-style physics via `flockTransition` blend.
- **Rendering**: Simple filled circles (`ofDrawCircle`) with alpha fading based on remaining life. Trail particles with lower alpha and smaller size.
- **Palette system**: 5 fluorescent palettes (Pink, Cyan, Green, Orange, Yellow) using `ofFloatColor::fromHex`. Random color selection with variation per particle on spawn.
- **Interaction model**: `std::map<std::string, InteractionPoint>` populated via `setParameter(id, vec2)`. Each point has position, velocity, confidence, decay. Spawn happens per active point each frame.
- **Parameters (18)**: `k`, `A`, `w`, `q`, `beta`, `S`, `ws`, `qs`, `flockSpeed`, `flockWaveX`, `flockWaveY`, `flockCohesion`, `flockLife`, `flockTransition`, `maxParticles`, `spawnRate`, `trailEnabled`.

### OrganicNetScene (`src/scenes/OrganicNetScene.h/.cpp`)
- **Node grid**: Rectangular grid of `NetNode` structs (position, originalPos, velocity, noiseOffset). Built once with random offsets from perfect grid. Max dimensions 24×36 = 864 nodes.
- **Edges**: Horizontal, vertical, diagonal, anti-diagonal connections with `omitChance` randomization. Each `NetEdge` stores indices a/b and `restLength`.
- **Physics**: Per-frame: gravity (+Y), wind (sin oscillation), Perlin noise drift (`ofSignedNoise`), damping. Then spring constraints via iterative relaxation (`springIterations` passes of position correction). Joint lift: quadratic falloff from `InteractionPoint` positions, pushing nodes upward and outward. Gentle return force toward `originalPos`.
- **Rendering**: Organic noise-displaced lines (`drawOrganicLine` with multi-octave noise and endpoint tapering). Subtle node dots with separate alpha. Lines fade at stretched distances.
- **Parameters (22)**: `grid.rows`, `grid.cols`, `grid.spacing`, `grid.randomOffset`, `grid.omitChance`, `phys.gravity`, `phys.damping`, `phys.springStiffness`, `phys.springIterations`, `noise.strength`, `noise.scale`, `wind.strength`, `wind.frequency`, `wind.bias`, `lift.radius`, `lift.strength`, `return.force`, `line.segments`, `line.noiseAmount`, `line.alpha`, `line.width`, `node.radius`, `node.alpha`.

### EnjambreOrganicoScene (reference)
- **FSM per anchor**: `AnchorState::Dwelling` / `Traveling` with skeleton-path-based travel between joints. Demonstrates state-machine-per-entity pattern already used in the codebase.
- **Bone trail**: Particles distributed along bone segments via `boneTrailT` parameter.
- **Spring-to-anchor**: Particles follow anchors with spring stiffness + noise.

### Shared Infrastructure
- All scenes implement `IBaseScene` (`src/scenes/BaseScene.h`): `setup()`, `update(dt)`, `draw()`, activate/deactivate, parameter registration, interaction points.
- `InteractionPoint` (`src/scenes/InteractionPoint.h`): normalized → pixel coords, velocity tracking, confidence decay. Used identically by all scenes.
- `ParameterRegistry`: global singleton for cross-scene parameter persistence.
- Scene ID pattern: `pinturaFluorescente`, `organicNet`, `enjambreOrganico`.

## Affected Areas

- **New files to create**:
  - `src/scenes/FluorescentNetScene.h` — new scene class inheriting `IBaseScene`
  - `src/scenes/FluorescentNetScene.cpp` — full implementation
- **Files to modify**:
  - `src/scenes/BaseScene.h` — no changes needed (IBaseScene is sufficient)
  - `src/ofApp.cpp` / scene registration — add `CreateFluorescentNetScene` factory and register it
- **Existing files reused (no modification)**:
  - `src/scenes/InteractionPoint.h` — used as-is for joint data
  - `src/mapping/ParameterTypes.h` — `Parameter`, `ParameterScope`, `ParameterType` used as-is
  - `src/registry/ParameterRegistry.h` — used as-is for parameter registration

## Approaches

### Approach A: Separate Pools (FREE pool + ANCHORED vectors)

Keep two separate data structures:
- A `ParticlePool` for FREE particles (reuse `PinturaFluorescenteScene::ParticlePool` as-is or near-copy)
- `std::vector<NetNode>` + `std::vector<NetEdge>` for ANCHORED nodes (reuse `OrganicNetScene` pattern)

Transition: when a FREE particle anchors, destroy it from the pool and create a new entry in the node/edge vectors.

- **Pros**: Maximum code reuse from both ancestor scenes; clear separation; each pool uses its own well-tested physics.
- **Cons**: Transition involves explicit destroy-and-create (loses velocity continuity, complicates smooth visual transitions); two separate update loops; harder to implement graceful de-anchoring (ANCHORED → FREE); particles must be re-created with new identity.
- **Effort**: Medium

### Approach B: Unified Particle Pool with Phase Enum ✅ RECOMMENDED

Single `std::vector<FluorescentParticle>` with a `phase` field (`FREE | ANCHORED`). A separate `std::vector<NetEdge>` for connections between ANCHORED particles.

Transition: change `phase` from `FREE` to `ANCHORED`, assign `netNodeIndex`, add new edges to nearby ANCHORED particles. No data copying.

- **Pros**: Single source of truth; trivial state transition (just flip enum + add edges); velocity continuity preserved; de-anchoring is a reverse operation; particles retain their fluorescent color from FREE through ANCHORED states; rendering can branch on phase cleanly.
- **Cons**: Single update loop must branch physics per particle phase (but this is simple and performant); cannot directly reuse `ParticlePool` or `NetNode` structs — must create a new merged struct.
- **Effort**: Medium

### Approach C: Composition via Delegation Classes

Create a `FluorescentNetScene` that owns both a `PinturaFluorescenteEngine` and `OrganicNetEngine` internally, routing interaction points to both and managing transitions at the scene level.

- **Pros**: Extreme code reuse.
- **Cons**: Over-engineering for a single scene; the two engines would need cross-references (transition requires coordination); tight coupling defeats the purpose; significantly more code to maintain.
- **Effort**: High

## Recommendation

**Approach B — Unified Particle Pool with Phase Enum.**

Rationale:
1. The two ancestor scenes are well-understood and the physics for both states are straightforward (orbital waves + spring constraints). Branching on `phase` inside a single update loop is clean and performant.
2. Transition is the core novel feature — flipping an enum is zero-cost and preserves all particle data (position, velocity, color, size). This enables smooth visual transitions.
3. The codebase already uses FSM patterns (`AnchorState` in EnjambreOrganico, `flockMode` transition in PinturaFluorescente). This is consistent with existing conventions.
4. A single pool means single memory allocation, single rendering pass (with branching), and simpler lifecycle management.
5. The `edges_` vector is independent (only for ANCHORED particles), following OrganicNet's pattern exactly for spring constraint solving.

### Key Design Decisions

**Particle state machine:**
```cpp
enum class ParticlePhase { FREE, ANCHORED };
```
- `FREE`: standard orbital wave physics (inner radius forces, wave, swirl, damping). No spring constraints.
- `ANCHORED`: no orbital forces. Gravity + wind + noise drift + damping. Spring constraints to connected ANCHORED neighbors (via `edges_`). Joint lift applies.

**Transition process:**
1. Each frame, identify FREE particles within `jointAnchorRadius` of an active `InteractionPoint` as anchor candidates.
2. Apply stochastic selection: `transitionRate` particles per second are randomly selected from candidates.
3. For each selected particle:
   - Set `phase = ANCHORED`.
   - Record `netNodeIndex = nodesAnchoredSoFar++` (for edge indexing).
   - Search for nearest `maxConnectionsPerNode` ANCHORED particles within `connectionRadius` using spatial hash.
   - Create edges to those neighbors, computing `restLength` from current positions.
4. If an ANCHORED node has no active joint nearby for `anchorInactiveTimeout` seconds, it transitions back to FREE (de-anchor), removing its edges and restoring orbital physics.

**Net growth pattern:**
- The net grows organically from joints outward. Since particles spawn near joints, the first anchorings happen close to joint positions.
- As ANCHORED nodes accumulate near joints, new anchorings connect to them, growing the net radially.
- The OrganicNet-style `liftRadius` parameter pushes the net upward from joints, creating a "net lifted by the skeleton" visual.

**Spatial hash for neighbor search:**
- Cell size = `connectionRadius` (80px default).
- Grid maps cell key → list of ANCHORED node indices.
- Updated incrementally on each anchor/de-anchor operation.
- Avoids O(n²) neighbor search when anchoring a particle among 500+ existing nodes.

**Rendering:**
- FREE particles: identical to PinturaFluorescente drawing (filled circles, alpha fade).
- ANCHORED nodes: `drawOrganicLine` between connected pairs (from OrganicNet, with noise displacement and endpoint tapering). Subtle filled circles for nodes.
- Both layers in the same `draw()` call, FREE particles drawn first, then ANCHORED edges, then ANCHORED nodes on top.
- Alpha blending enabled globally.

**Parameters (28 total, merging both ancestors' parameter spaces):**

| Parameter | Range | Default | Source |
|-----------|-------|---------|--------|
| `transitionRate` | 0.1–20.0 | 3.0 | New |
| `maxAnchoredNodes` | 50–1500 | 500 | New |
| `connectionRadius` | 30–200 px | 80.0 | New |
| `maxConnectionsPerNode` | 1–8 | 4 | New |
| `jointAnchorRadius` | 30–300 px | 120.0 | New |
| `anchorInactiveTimeout` | 1.0–30.0 s | 8.0 | New |
| `k` | 0.0–2.5 | 0.12 | PinturaFluorescente |
| `A` | 0.0–100.0 | 15.0 | PinturaFluorescente |
| `w` | 0.1–5.0 | 0.15 | PinturaFluorescente |
| `q` | 0.0–0.5 | 0.08 | PinturaFluorescente |
| `beta` | 0.9–1.0 | 0.97 | PinturaFluorescente |
| `S` | 0.0–50.0 | 6.0 | PinturaFluorescente |
| `ws` | 0.0–5.0 | 0.5 | PinturaFluorescente |
| `qs` | 0.0–0.5 | 0.02 | PinturaFluorescente |
| `freeAlpha` | 0.1–1.0 | 0.9 | New |
| `maxParticles` | 500–5000 | 5000 | PinturaFluorescente |
| `spawnRate` | 1–20 | 6 | PinturaFluorescente |
| `springStiffness` | 0.02–0.4 | 0.15 | OrganicNet |
| `springIterations` | 1–8 | 3 | OrganicNet |
| `noiseStrength` | 0.1–2.0 | 0.8 | OrganicNet |
| `noiseScale` | 0.005–0.04 | 0.015 | OrganicNet |
| `windStrength` | 0.0–1.5 | 0.4 | OrganicNet |
| `windFrequency` | 0.1–1.5 | 0.3 | OrganicNet |
| `windBias` | -0.8–0.8 | 0.0 | OrganicNet |
| `liftRadius` | 40–250 px | 120.0 | OrganicNet |
| `liftStrength` | 0.2–3.5 | 1.5 | OrganicNet |
| `returnForce` | 0.0–0.03 | 0.008 | OrganicNet |
| `netLineAlpha` | 20–255 | 150 | OrganicNet (`line.alpha`) |
| `netLineWidth` | 0.3–4.0 | 1.0 | OrganicNet (`line.width`) |
| `nodeRadius` | 0.5–6.0 | 3.0 | OrganicNet |
| `nodeAlpha` | 0–255 | 120 | OrganicNet |
| `lineNoiseAmount` | 2–30 | 10.0 | OrganicNet |
| `lineSegments` | 4–20 | 10 | OrganicNet |

## Risks

1. **Dynamic topology stability**: Adding/removing nodes with spring edges mid-simulation can cause transient instabilities. Mitigation: when de-anchoring, apply a brief velocity impulse to disconnected neighbors to absorb slack. Increase `springIterations` during high-change frames.

2. **Spatial hash overhead**: Per-frame spatial hash updates on every anchor/de-anchor add bookkeeping. Mitigation: use a lazy-update spatial grid that only rebuilds affected cells. For 500 nodes, a brute-force O(n) search per anchor operation may actually be faster than hash maintenance — benchmark during implementation.

3. **Visual discontinuity on anchor**: A FREE particle snapping from orbital motion to spring-constrained net behavior may look jarring. Mitigation: add a brief transition blend (0.2–0.5s) where both physics contributions are mixed before fully anchoring.

4. **Parameter namespace collision**: Both ancestor scenes have parameters like `line.alpha` (OrganicNet) — must namespace clearly for FluorescentNet (`fluorescentNet.line.alpha`). Scene ID should be `fluorescentNet`.

5. **Edge explosion risk**: If `maxConnectionsPerNode` is too high and `connectionRadius` is large, the edge count grows O(n²) and spring solving becomes expensive. Mitigation: parameter guard that `maxEdges = maxAnchoredNodes * maxConnectionsPerNode / 2` and hard-cap edge creation.

6. **Trail particles**: PinturaFluorescente has trail particles. FluorescentNet likely does NOT need trails (free particles are already numerous, and trails would clash with net lines). Decision: omit trail pool from FluorescentNet.

## Ready for Proposal

**Yes.** The architecture is well-understood. Both ancestor scenes have been deeply analyzed. The unified-pool approach with phase-enum state machine is a clean extension of existing patterns. All parameter ranges, rendering strategies, and physics models are defined. The risks are manageable with the mitigations described.

Next phase: **sdd-propose** — formalize the intent, scope, approach, and rollback plan.
