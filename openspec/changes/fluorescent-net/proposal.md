# Proposal: FluorescentNet Scene

## Intent

New scene where fluorescent particles start by freely orbiting Kinect joints (PinturaFluorescente style) and **gradually transition** into becoming nodes of a connected organic mesh (OrganicNet style). Each particle has two states: FREE (orbiting its joint) and ANCHORED (fixed as a net node, connected by spring edges). A `transitionRate` parameter controls how fast particles anchor. Fluorescent palettes carry through both states. The net grows organically from active joints outward.

## Scope

### In Scope
- `FluorescentNetScene` (`.h` + `.cpp`): new self-contained scene inheriting `IBaseScene`
- Unified particle pool: each particle has `FREE` / `ANCHORED` state + transition logic
- FREE physics: orbital mechanics adapted from PinturaFluorescente (center attraction, wave oscillation, damping)
- ANCHORED physics: spring-constraint mesh adapted from OrganicNet (gravity, wind, noise drift, iterative relaxation)
- Dynamic topology: edges created/destroyed as particles anchor/de-anchor
- Reuse `InteractionPoint` as source for FREE orbit centers
- Reuse 5 fluorescent palettes from PinturaFluorescente
- Rendering: FREE particles = fluorescent dots; ANCHORED particles = net lines + nodes; blend both
- 28 parameters registered with `fluorescentNet.` prefix
- SceneEngine registration + factory `CreateFluorescentNetScene()`
- ofApp wiring + preset loading

### Out of Scope
- Modifying PinturaFluorescenteScene, OrganicNetScene, or EnjambreOrganicoScene
- Trail/ribbon rendering (net lines suffice)
- Flock mode
- External file configuration beyond standard presets

## Approach

**Unified pool with dual physics**: single `std::vector<NetParticle>` where each particle stores phase (`FREE` or `ANCHORED`), position, velocity, joint assignment, and a transition timer. Per-frame: FREE particles run orbital physics toward their joint; ANCHORED particles run net physics (gravity + wind + noise + spring relaxation). Rendering: single draw pass with `ofMesh` for lines + circles.

**Transition FSM**: when `transitionTimer` > 0, particle blinks between FREE/ANCHORED behavior for a smooth blend period. After blend completes, particle stays ANCHORED permanently (unless deactivated). Transition is one-way: FREE → ANCHORED.

**Edge management**: after anchoring, check spatial neighbors within `connectionRadius`. Create spring edges between nearby ANCHORED nodes. Max `maxConnectionsPerNode` (default 6). Edges dissolve if nodes are too far apart (> `connectionRadius * 1.5`).

**Performance**: target 500-2000 particles at 60fps. Spatial hash for neighbor queries when > 500 ANCHORED nodes. Spring relaxation: `springIterations` passes (default 3). Brute-force neighbor search acceptable below 500 nodes.

## Dependencies

- `InteractionPoint` (existing)
- `IBaseScene` / `BaseScene.h` (existing)
- `ParameterRegistry` (existing)
- 5 fluorescent palettes from PinturaFluorescente (copy, no dependency)
- No dependency on OrganicNetScene or PinturaFluorescenteScene

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/scenes/FluorescentNetScene.h` | New | Structs + class declaration (~200 lines) |
| `src/scenes/FluorescentNetScene.cpp` | New | Implementation (~1200 lines) |
| `src/ofApp.cpp` | Modified | Include, load scene, loadPresets (~8 lines) |
| `config/presets/fluorescentNet.json` | New | Default preset file |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Dynamic topology causes velocity impulse spikes | Medium | Cap velocity at `maxAnchorSpeed` on anchor event |
| Visual pop on FREE→ANCHORED transition | Medium | Blend alpha over `blendPeriod` (0.5s) |
| Edge explosion with many nodes | Low | Hard-cap `maxConnectionsPerNode=6`, `maxAnchoredNodes` param |
| IBaseScene boilerplate bloat | High | ~300 lines of stubs — accepted, match project pattern |
| PR exceeds 400-line budget | High | Chained PRs: core physics → net rendering → polish |

## Success Criteria

- [ ] Scene compiles and activates via SceneEngine
- [ ] FREE particles orbit joints with fluorescent colors
- [ ] ANCHORED particles form connected mesh with spring edges
- [ ] Transition FREE→ANCHORED is smooth (no visual pop)
- [ ] Build exits zero (`make`)
