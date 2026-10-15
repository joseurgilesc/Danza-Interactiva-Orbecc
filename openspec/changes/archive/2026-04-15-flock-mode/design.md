# Design: Flock Mode

## Technical Approach

Agregar lógica de boids en Particle::update() cuando flockMode está activo. Por cada frame, cada partícula calcula fuerzas de separación, alineación y cohesión con sus vecinas.

## Constants

| Constant | Value | Description |
|----------|-------|-------------|
| FLOCK_SEPARATION | 0.8f | Fuerza de separación |
| FLOCK_ALIGNMENT | 0.5f | Fuerza de alineación |
| FLOCK_COHESION | 0.3f | Fuerza de cohesión |
| FLOCK_RADIUS | 50.0f | Radio de vecindad |

## Implementation

- **Performance**: O(n²) - cada partícula checkea todas las demás. Aceptable para <5000 partículas.
- **GPU**: No necesario por ahora

## File Changes

| File | Action |
|------|--------|
| src/ofApp.h | Add flockMode, boid constants, flock flag in update |
| src/ofApp.cpp | Add 'B' toggle, compute boid forces |

## Open Questions

None