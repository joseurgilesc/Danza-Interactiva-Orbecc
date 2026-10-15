# Design: attractor-click

## Technical Approach

Agregar atracción adicional en Particle::update() cuando el attractor está activo.

## Architecture Decisions

### Decision: Attractor Implementation

**Choice**: State in ofApp, force in Particle
**Alternatives**: 
- Attractor como objeto (rejected: overkill)
- Force constant value (rejected: no flexibility)

**Rationale**: Simple state + physics integration

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/ofApp.h` | Modified | Add attractorPoint, attractorActive |
| `src/ofApp.cpp` | Modified | mousePressed/Released handlers, pass attractor to update |

## Constants

| Constant | Value | Description |
|----------|-------|-------------|
| ATTRACTOR_STRENGTH | 0.15f | Fuerza del attractor (suave) |

## Open Questions

None