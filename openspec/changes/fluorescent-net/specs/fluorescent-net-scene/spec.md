# FluorescentNetScene Specification

## Purpose

Particle scene where particles exist in two phases:
1. **FREE**: orbit active Kinect joints with fluorescent colors and wave physics (PinturaFluorescente-style)
2. **ANCHORED**: become nodes in a spring-constrained organic mesh (OrganicNet-style)

Particles transition FREE→ANCHORED at a rate controlled by `transitionRate`. The net grows organically from active joints outward. Fluorescent palette colors persist in both phases.

## Requirements

### R-1: Dual-phase particle pool
Particles SHALL support both FREE and ANCHORED states within a single pool, using a `phase` enum field.

#### Scenario: Particle spawns as FREE
- GIVEN an active interaction point at position (400, 300)
- WHEN a particle spawns
- THEN the particle's phase is FREE, it is assigned to that interaction point, and its color is set from the active fluorescent palette.

#### Scenario: Particle transitions to ANCHORED
- GIVEN a FREE particle that has existed for `transitionRate` seconds
- WHEN `transitionRate` timer expires
- THEN the particle enters blend period (0.5s), then becomes ANCHORED permanently.

### R-2: FREE orbital physics
FREE particles SHALL orbit their assigned interaction point using center attraction, sinusoidal wave oscillation, and velocity damping.

#### Scenario: Particle orbits active joint
- GIVEN a FREE particle assigned to joint "head.position" at (400, 300)
- WHEN the scene updates
- THEN the particle moves in an orbital pattern around (400, 300) with visible wave-like oscillation.

### R-3: ANCHORED net physics
ANCHORED particles SHALL participate in a spring-constrained mesh with gravity, wind, Perlin noise drift, and iterative relaxation.

#### Scenario: Anchored particles form connected mesh
- GIVEN 3 ANCHORED particles within `connectionRadius` of each other
- WHEN the scene updates
- THEN spring edges are created between them, and they maintain structure via constraint relaxation.

### R-4: Smooth FREE→ANCHORED transition
Particles transitioning from FREE to ANCHORED SHALL not produce visual popping.

#### Scenario: No velocity spike on anchor
- GIVEN a FREE particle with orbital velocity (50, -30)
- WHEN it transitions to ANCHORED
- THEN its velocity is clamped to `maxAnchorSpeed` and rendered alpha blends over 0.5s.

### R-5: Dynamic edge management
Edges between ANCHORED nodes SHALL be created and destroyed dynamically as particles anchor and move.

#### Scenario: New edge created
- GIVEN a newly ANCHORED particle at position P
- WHEN there are ANCHORED neighbors within `connectionRadius` of P
- THEN spring edges are created between P and up to `maxConnectionsPerNode` nearest neighbors.

#### Scenario: Edge dissolved on distance
- GIVEN an edge between two ANCHORED particles
- WHEN their distance exceeds `connectionRadius * 1.5`
- THEN the edge is removed.

### R-6: Fluorescent palette colors
Both FREE and ANCHORED particles SHALL use the 5 fluorescent palettes (Pink, Cyan, Green, Orange, Yellow) matching PinturaFluorescenteScene.

#### Scenario: FREE particles use fluorescent colors
- GIVEN the scene is active with palette index 0 (Pink)
- WHEN FREE particles render
- THEN each particle is drawn with a pink-fluorescent color.

### R-7: Net rendering
ANCHORED particles SHALL render as connected lines (edges) with alpha-based distance fading, plus subtle node dots.

#### Scenario: Net edges visible
- GIVEN 2 connected ANCHORED particles
- WHEN the scene draws
- THEN a line is rendered between them with alpha fading as distance deviates from rest length.

### R-8: Parameter registration
28 parameters SHALL be registered with `fluorescentNet.` prefix, all editable via ParamPanel.

#### Scenario: Parameters appear in panel
- GIVEN the scene is active
- WHEN the user opens ParamPanel with scene filter
- THEN all 28 fluorescentNet.* parameters are visible and editable.

### R-9: SceneEngine registration
The scene SHALL register in SceneEngine via factory function and ofApp.

#### Scenario: Scene selectable in panel
- GIVEN the app is running
- WHEN the user opens the ScenePanel dropdown
- THEN "Fluorescent Net" appears as a selectable scene.
