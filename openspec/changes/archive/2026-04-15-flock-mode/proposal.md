# Proposal: Flock Mode (Bandada de Pájaros)

## Intent

Agregar un modo toggle activado con la tecla `B` donde las partículas se comportan como una bandada de pájaros (boids) con comportamiento autónomo: separación, alineación y cohesión.

## Scope

### In Scope
- [ ] Toggle con tecla `B` para activar/desactivar flock mode
- [ ] Agregar fuerzas de boids a Particle::update() cuando flock activo
- [ ] Separación: evitar colisiones con otras partículas
- [ ] Alineación: matching de velocidad con vecinos
- [ ] Cohesión: moverse hacia el centro del grupo

### Out of Scope
- Optimización espacial (quadtree) - puede ficar lento con muchas partículas
- Render especial para modo flock
- Múltiples bandadas

## Approach

1. Agregar `flockMode` bool en ofApp
2. keyPressed detecta 'B' para toggle
3. En Particle::update() calcular 3 fuerzas cuando flock activo
4. Combinar con fuerzas existentes (attractor, center, etc.)

## Constants

| Constant | Value | Description |
|----------|-------|-------------|
| FLOCK_SEPARATION | 0.5f | Fuerza de separación |
| FLOCK_ALIGNMENT | 0.3f | Fuerza de alineación |
| FLOCK_COHESION | 0.2f | Fuerza de cohesión |
| FLOCK_RADIUS | 50.0f | Radio de vecindad |

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/ofApp.h` | Modified | Add flockMode + boid constants |
| `src/ofApp.cpp` | Modified | Toggle handler + fuerzas en update |

## Success Criteria

- [ ] Presionar `B` activa modo flock
- [ ] Partículas se agrupan y mueven como bandada
- [ ] Presionar `B` de nuevo desactiva
- [ ] `make` compila sin errores