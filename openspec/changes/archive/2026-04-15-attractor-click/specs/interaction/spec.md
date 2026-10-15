# Delta for Interaction

## ADDED Requirements

### Requirement: Click Attractor

The system SHALL create a temporary attraction point at mouse position when user clicks.

#### Scenario: Activate attractor

- GIVEN application running with particles
- WHEN user presses mouse button
- THEN attractor point activates at mouse position
- AND particles feel a gentle pull toward that point

#### Scenario: Deactivate attractor

- GIVEN attractor is active
- WHEN user releases mouse button
- THEN attractor deactivates
- AND particles return to normal behavior

### Requirement: Attractor Force

The system SHOULD use a weak attraction coefficient to avoid clustering.

- GIVEN attractor is active
- WHEN particle is attracted
- THEN force magnitude MUST be smaller than center attraction

## Verification Scenarios

### Build Verification

- GIVEN modified source files
- WHEN `make` executes
- THEN compilation succeeds without errors

### Functional Verification

- GIVEN app running
- WHEN user clicks anywhere
- THEN particles move toward click point
- AND when released, particles return to normal movement