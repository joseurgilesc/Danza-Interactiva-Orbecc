# ribbon-dance-scene Specification

## Purpose

Fluorescent ribbon trails emitted from active interaction points, responding to joint velocity.
Ribbons render via `ofMesh` triangle strips with width/alpha taper, wave undulation, soft gravity,
and palette-colored particle bursts. Fade out gracefully on point deactivation or skeleton loss.

## Requirements

### Requirement: Ribbon Per Interaction Point

The system MUST create and track one RibbonTrail per active interaction point, keyed by point ID.
Each ribbon SHALL follow its source point position every frame while active.

#### Scenario: New interaction point activates
- GIVEN scene is active and no ribbon exists for pointId "A"
- WHEN interaction point "A" activates (confidence > 0)
- THEN a new RibbonTrail is created, linked to pointId "A"
- AND ribbon position updates each frame from point "A"

#### Scenario: Multiple simultaneous points
- GIVEN interaction points "A" and "B" are active
- WHEN both points update positions
- THEN two independent ribbons render simultaneously
- AND each ribbon tracks its own interaction point ID

---

### Requirement: Ribbon Fade-Out on Deactivation

When an interaction point disappears (confidence drops to 0), its ribbon MUST NOT cut instantly.
The ribbon SHALL enter fade-out: stop adding new trail points, fade alpha gradually, continue
gravity/decay on existing points. Ribbon SHALL be destroyed when alpha ≈ 0 or all points invisible.

#### Scenario: Interaction point deactivates
- GIVEN a ribbon with 50 trail points, alpha 1.0
- WHEN source interaction point confidence drops to 0
- THEN no new trail points are added
- AND trail alpha decreases each frame by fadeSpeed
- AND existing particles continue their natural lifecycle
- AND ribbon is removed when alpha ≤ 0.01

---

### Requirement: Velocity-Modulated Visuals

The system MUST smooth interaction-point velocity via exponential moving average (EMA).
Smoothed velocity SHALL modulate: ribbon width (higher = thicker), brightness, wave amplitude,
particle emission rate, and trail persistence. Transitions MUST interpolate smoothly — no abrupt changes.

#### Scenario: High velocity movement
- GIVEN smoothed velocity exceeds emission threshold
- WHEN ribbon updates for the frame
- THEN ribbon width increases toward maxWidth
- AND wave undulation amplitude scales proportionally
- AND particle burst is emitted at current ribbon position

#### Scenario: Low velocity / still
- GIVEN smoothed velocity near zero
- WHEN ribbon updates for the frame
- THEN ribbon stays at or near minWidth
- AND no particle bursts fire
- AND wave undulation is minimal

---

### Requirement: Palette Colors (Global)

Ribbon and particle colors MUST come exclusively from the global 5-palette system
(Rosa, Cian, Verde, Naranja, Amarillo). The system MUST NOT hardcode any `ofColor(r,g,b)`.
Active palette index SHALL be read from `registry.getParam("global.currentPalette")`.
Each ribbon SHALL pick its color via `getColorFromPalette(currentPalette_, variation)`.

#### Scenario: Palette color assignment
- GIVEN active palette is "Cian" (index 1)
- WHEN a new ribbon is created for pointId "C"
- THEN ribbon stroke color is derived from Cian palette with variation
- AND particle colors also derive from Cian palette

---

### Requirement: Triangle-Strip Ribbon Rendering

Ribbons MUST render via `ofMesh` + `OF_PRIMITIVE_TRIANGLE_STRIP`.
For each history point, the system SHALL compute perpendicular edges, width tapering
from `ribbonMaxWidth` at head to `ribbonMinWidth` at tail, alpha fading toward tail,
and sinusoidal wave displacement based on time and velocity magnitude.

#### Scenario: Single-frame ribbon draw
- GIVEN a ribbon with 30 history points
- WHEN `draw()` is called
- THEN an `ofMesh` triangle strip is built with 2 vertices per history point
- AND vertex colors fade from full alpha at head to transparent at tail
- AND wave offset displaces vertices perpendicular to ribbon direction
- AND alpha blending is enabled

---

### Requirement: Ribbon Soft Gravity

Older trail points SHALL experience downward gravity force, with strength increasing toward tail.
A damping factor MUST prevent oscillation or jitter. Visual behavior SHALL resemble lightweight fabric,
not a heavy rope.

#### Scenario: Gravity on aged trail points
- GIVEN trail points aged > 10 frames
- WHEN `update()` is called
- THEN older points shift downward proportional to gravity × pointAge
- AND damping reduces accumulated velocity each frame
- AND recent points (head) show minimal gravity displacement

---

### Requirement: Particle Emissions

The system MUST spawn particles along ribbons at trigger events: high velocity, sharp direction
changes (>30° in one frame), or new interaction-point activation. Particles SHALL have:
position near ribbon, own velocity, variable size (min–max), limited lifetime, alpha fade,
color from current palette. Hard cap SHALL be enforced (default 800) with oldest-eviction.

#### Scenario: Particle burst on velocity spike
- GIVEN smoothed velocity > `particleEmissionThreshold`
- WHEN ribbon update runs
- THEN `particlesPerBurst` particles spawn near ribbon head
- AND each particle gets randomized velocity, size, and lifetime within configured ranges

#### Scenario: Particle pool at capacity
- GIVEN particle count equals `particleMaxCount`
- WHEN a new burst tries to spawn
- THEN oldest particles are evicted first
- AND new particles are added within the cap

---

### Requirement: Skeleton Loss Handling

When Kinect skeleton is lost, the system MUST stop adding new points to all ribbons.
Existing ribbons SHALL continue fade-out naturally and particles continue their lifecycle.
When skeleton returns, the system SHALL spawn fresh ribbons from new interaction points
without drawing connecting lines to prior positions.

#### Scenario: Skeleton lost mid-scene
- GIVEN 3 active ribbons from 3 interaction points
- WHEN skeleton tracking is lost
- THEN all 3 ribbons stop receiving new trail points
- AND each ribbon fades out over `ribbonLife` seconds
- AND particles continue decaying naturally

#### Scenario: Skeleton regained after loss
- GIVEN all ribbons have faded out after skeleton loss
- WHEN skeleton tracking resumes with new interaction points
- THEN fresh ribbons spawn from new point positions
- AND no visual line connects old ribbon tails to new heads

---

### Requirement: Jump/Bounce Protection

If distance between consecutive interaction-point positions exceeds `ribbonMaxDistanceJump`,
the system SHALL soft-cut the ribbon (start a new segment). The threshold MUST be
configurable and SHOULD account for screen/window scale.

#### Scenario: Teleport-like position jump
- GIVEN `ribbonMaxDistanceJump` = 150px
- WHEN interaction point moves 200px between consecutive frames
- THEN the current ribbon segment is soft-cut (enters fade-out)
- AND a fresh ribbon segment begins at the new position

---

### Requirement: Configurable Parameters

All visual and physics behavior MUST be tunable via ParameterRegistry.

| Parameter | Default | Description |
|-----------|---------|-------------|
| `ribbon.maxPoints` | 200 | Max trail history points per ribbon |
| `ribbon.maxWidth` | 40.0 | Head width in pixels |
| `ribbon.minWidth` | 2.0 | Tail width in pixels |
| `ribbon.gravity` | 0.15 | Downward force per frame on aged points |
| `ribbon.damping` | 0.96 | Velocity decay per frame (EMA factor) |
| `ribbon.fadeSpeed` | 0.02 | Alpha decrease per frame during fade-out |
| `ribbon.life` | 3.0 | Seconds to full fade after source lost |
| `ribbon.velocityInfluence` | 0.5 | How much velocity affects width/undulation |
| `ribbon.waveAmount` | 8.0 | Max wave displacement in pixels |
| `ribbon.waveSpeed` | 2.0 | Wave oscillation frequency |
| `ribbon.maxDistanceJump` | 150 | Pixel threshold for jump protection |
| `particle.maxCount` | 800 | Hard cap, oldest-eviction |
| `particle.emissionThreshold` | 80 | Velocity (px/s) to trigger burst |
| `particle.perBurst` | 5 | Particles spawned per trigger |
| `particle.velocityInfluence` | 0.3 | Ribbon velocity contribution to particle |
| `particle.gravity` | 0.05 | Downward force on particles |
| `particle.damping` | 0.98 | Particle velocity decay per frame |
| `particle.lifeMin` | 0.5 | Minimum particle lifetime (seconds) |
| `particle.lifeMax` | 2.0 | Maximum particle lifetime (seconds) |
| `particle.sizeMin` | 1.0 | Minimum particle radius (pixels) |
| `particle.sizeMax` | 4.0 | Maximum particle radius (pixels) |

#### Scenario: Parameter modification at runtime
- GIVEN scene is active
- WHEN `ribbon.maxWidth` is changed via OSC/UI to 60.0
- THEN all active ribbons re-sample width at head to 60.0 on next frame
- AND no restart or scene reload is required

---

### Requirement: Scene Integration

`RibbonDanceScene` MUST implement `IBaseScene` (`activate`, `deactivate`, `update`, `draw`).
Registration in `ofApp.cpp` SHALL follow existing factory pattern.
The scene SHALL reuse the shared `InteractionPoint` system, palette system, and `ParameterRegistry`.

#### Scenario: Scene lifecycle
- GIVEN SceneEngine is initialized
- WHEN `loadScene("RibbonDanceScene")` is called
- THEN scene `activate()` is called, initializing ribbons and parameters
- WHEN `loadScene("PinturaFluorescenteScene")` is called
- THEN RibbonDanceScene `deactivate()` cleans up all ribbons and particles
- AND no memory leaks remain

> **Verification note**: Requirements involving visual output (rendering, alpha fade, undulation,
> particle aesthetics, gravity feel, color palette) require manual verification — the project has
> no automated visual test framework. Build verification via `make` MUST pass without warnings.
