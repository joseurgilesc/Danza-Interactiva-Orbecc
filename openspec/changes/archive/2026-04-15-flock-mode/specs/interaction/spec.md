# Delta for Flock Mode

## ADDED Requirements

### Requirement: Flock Toggle

The system SHALL toggle flock mode when user presses `B` key.

#### Scenario: Activate flock

- GIVEN app running in normal mode
- WHEN user presses 'B'
- THEN flockMode becomes active
- AND particles apply boid forces

#### Scenario: Deactivate flock

- GIVEN flock mode active
- WHEN user presses 'B' again
- THEN flockMode becomes inactive
- AND particles return to normal behavior

### Requirement: Boid Forces

The system MUST implement three boid behaviors when flock mode is active:

- **Separation**: avoid crowding neighbors (steer away)
- **Alignment**: steer towards average heading of neighbors
- **Cohesion**: steer towards center of mass of neighbors

#### Scenario: Flock movement

- GIVEN flock mode active
- WHEN particles update
- THEN each particle considers neighbors within FLOCK_RADIUS
- AND applies separation, alignment, and cohesion forces

## Verification

- GIVEN press B
- THEN particles flock together
- AND when press B again, return to normal