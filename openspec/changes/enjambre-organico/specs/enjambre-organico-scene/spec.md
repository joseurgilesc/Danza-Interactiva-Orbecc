# Enjambre Organico Scene Specification

## Purpose

A single cohesive swarm travels slowly joint-to-joint across active Kinect InteractionPoints: one leader anchor followed by spring-damper particles with Perlin orbital noise.

## Requirements

### Requirement: Multi-Joint Travel

With 2+ active joints, the anchor MUST traverse them in canonical order (non-canonical points appended), one smoothstep segment at a time, cycling after the last.

#### Scenario: Anchor travels joint-to-joint

- GIVEN three active joints
- WHEN dwell at joint A completes
- THEN the anchor travels to the next active joint over `travelDuration` seconds
- AND all particles follow as one cohesive group

#### Scenario: Cyclic traversal

- GIVEN the anchor dwelling at the last active joint
- WHEN dwell completes
- THEN it targets the first active joint in the cycle

### Requirement: Slow Pacing as First-Class Parameter

`travelDuration` and `dwellDuration` MUST be seconds-based and directly bound anchor pacing. All motion MUST be framerate-independent (`dt`-scaled).

#### Scenario: Pacing follows parameters

- GIVEN `travelDuration` = 3.0 and `dwellDuration` = 1.5
- WHEN the anchor traverses a segment
- THEN it takes ~3.0 s plus ~1.5 s dwell, regardless of framerate

### Requirement: Single-Joint Dwell

With exactly one active joint, the anchor MUST dwell there without advancing; particles MUST "breathe" in place via noise offsets, with no oscillation or self-bounce.

#### Scenario: One joint active

- GIVEN exactly one active joint
- WHEN the scene updates over several seconds
- THEN the anchor stays put and the swarm wobbles around it without jitter

### Requirement: Zero-Joint Fallback

With no active joints, the anchor MUST ease toward screen center at clamped speed; the swarm keeps breathing until joints reappear.

#### Scenario: All joints lost

- GIVEN no active joints
- WHEN the scene updates
- THEN the anchor eases to screen center and particles remain visible and animated

### Requirement: Dynamic Joint Appearance and Disappearance

The joint sequence MUST be rebuilt each frame. If the anchor's target disappears mid-travel, it MUST retarget the next active joint smoothly. `activate()` MUST preserve interaction points; `deactivate()` MUST clear them; confidence decay SHALL be 0.08.

#### Scenario: Target joint drops mid-travel

- GIVEN the anchor traveling toward joint B
- WHEN joint B becomes inactive
- THEN the anchor retargets the next active joint without a positional jump

#### Scenario: New joint appears

- GIVEN the anchor cycling two joints
- WHEN a third joint becomes active
- THEN it joins the sequence in canonical order at the next segment selection

### Requirement: Anchor Max-Speed Clamp

Anchor displacement per frame MUST be clamped to a maximum speed so tracking teleports cannot lurch the swarm.

#### Scenario: Joint teleports

- GIVEN a tracked joint jumping a large distance in one frame
- WHEN the anchor follows
- THEN its movement stays within the clamp and the swarm transitions smoothly

### Requirement: Parameter Registration

On `activate()` the scene MUST register float parameters in the global ParameterRegistry under the `enjambreOrganico.` prefix and unregister them on `deactivate()`: `travelDuration` (0.5–10 s, default 3.0), `dwellDuration` (0–10 s, default 1.5), `swarmRadius`, `noiseAmount`, `noiseSpeed`, `springStiffness`, `particleCount`. Saved values MUST be read on activate; live changes MUST apply without scene reload.

#### Scenario: Live parameter tuning

- GIVEN the scene active
- WHEN `travelDuration` changes via the registry panel
- THEN the next segment uses the new duration

### Requirement: Scene Registration

The scene MUST be registered in `ofApp` with SceneEngine under distinct id `enjambreOrganico`, not colliding with `particleSwarm`.

#### Scenario: Scene selectable

- GIVEN the application started
- WHEN scene `enjambreOrganico` is loaded
- THEN it activates and `particleSwarm` remains independently selectable

### Requirement: Debug Markers Mode

A debug-markers toggle MUST render active joint and anchor positions; when disabled, no debug visuals SHALL be drawn.

#### Scenario: Toggle debug markers

- GIVEN the scene running with debug markers enabled
- WHEN the toggle is disabled
- THEN joint and anchor markers disappear while the swarm keeps rendering
