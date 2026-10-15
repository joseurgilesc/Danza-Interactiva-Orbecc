# Delta for pintura-fluorescente-params

## ADDED Requirements

### Requirement: maxParticles Parameter
PinturaFluorescenteScene MUST register `maxParticles` (INT, 500–5000, default 5000) via ParameterRegistry. Changing this parameter SHALL resize the particle pool, deferred to next frame.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Default behavior | Scene initializes | Parameter registered | maxParticles=5000 (matches current MAX_PARTICLES) |
| Reduce pool | maxParticles=5000 | Set to 2000 | Pool shrinks to 2000 next frame |
| Clamp below min | Value=300 passed | Internally | Clamped to 500 |
| Clamp above max | Value=6000 passed | Internally | Clamped to 5000 |

### Requirement: spawnRate Parameter
MUST register `spawnRate` (INT, 1–20, default 6). Controls max particles spawned per frame per interaction point.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Default | Scene starts | Registration | spawnRate=6 (matches hardcoded) |
| Reduce spawn | spawnRate=6 | Set to 3 | ~50% fewer particles per interaction point |

### Requirement: trailEnabled Parameter
MUST register `trailEnabled` (BOOL, default true). When disabled, trail particles SHALL stop spawning; existing trails decay naturally.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Disable trails | trailEnabled=true | Set to false | No new trail particles; existing trails fade/die |
| Re-enable | trailEnabled=false | Set to true | Trail particles resume spawning |

### Requirement: Backward Compatibility
Default values MUST produce behavior identical to current hardcoded constants.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| No params changed | maxParticles=5000, spawnRate=6, trailEnabled=true | Scene runs normally | Visual output matches pre-change code exactly |
