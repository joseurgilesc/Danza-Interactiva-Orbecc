# MockKinectAdapter Specification

## Purpose

Deterministic IKinectAdapter implementation for development and testing without physical Kinect hardware.

## Requirements

### Requirement: Full Interface Compliance

MUST implement all IKinectAdapter methods. `initialize()` MUST always return `true`. `getConnectionState()` MUST return CONNECTED after `initialize()`.

#### Scenario: Always succeeds
- GIVEN MockKinectAdapter is constructed
- WHEN `initialize()` is called
- THEN returns `true`; `getConnectionState()` returns CONNECTED

### Requirement: Deterministic Joint Data

MUST produce a single `TrackedBody` with `bodyId=1` and all 18 joints at default center position (all `Vector3D` values 0.5) when activated.

#### Scenario: Default body
- GIVEN mock adapter is initialized and `update()` called
- WHEN `getAllTrackedBodies()` is queried
- THEN one body with `bodyId=1`, `isTracked=true`, `confidence=1.0`, and all 18 joints at (0.5, 0.5, 0.5)

### Requirement: Simulation Modes

SHALL support a `setSimulationMode(SimMode)` method with modes: `IDLE` (static center), `WALKING` (sinusoidal oscillation on y axis), `WAVING` (periodic right-hand movement on x/y axes). Mode changes SHALL take effect on next `update()` cycle.

#### Scenario: Walking simulation
- GIVEN mode is set to WALKING
- WHEN `update()` is called repeatedly
- THEN joint positions oscillate with sinusoidal pattern on y-axis between 0.3–0.7

#### Scenario: Waving simulation
- GIVEN mode is set to WAVING
- WHEN `update()` is called repeatedly
- THEN HAND_RIGHT position oscillates on x (0.3–0.7) and y (0.4–0.8); other joints remain stable

### Requirement: Programmatic Joint Setting

SHALL provide `setMockJointPosition(JointType type, Vector3D pos)` allowing tests to set specific joint positions. After setting, the joint MUST retain that position until explicitly changed or simulation mode overrides it.

#### Scenario: Test-driven joint position
- GIVEN mock is in IDLE mode
- WHEN `setMockJointPosition(ELBOW_RIGHT, {0.8, 0.2, 0.5})` is called
- THEN next `getAllTrackedBodies()` returns body with ELBOW_RIGHT at (0.8, 0.2, 0.5)

### Requirement: Callback Invocation

MUST invoke registered callbacks each frame after `update()` with deterministic mock data, matching real adapter behavior.

#### Scenario: Joint callback fires
- GIVEN a joint callback is registered
- WHEN `update()` is called
- THEN callback is invoked 18 times (once per joint) with mock data
