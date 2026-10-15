# Apply Progress — refactor-particle-system

## 2026-04-07
- Added a toggleable (`D`) debug overlay that surfaces main/trail pool counts vs `MAX_PARTICLES`, frame FPS, per-frame spawn counts, and attractor/fleet states. Overlay automatically pulls stats from the active scene engine when present.
- Instrumented `ParticlePool` (both legacy and scene versions) to log a single `ofLogWarning` when saturation occurs and to reset the latch when capacity frees up.
- Added attractor activation/deactivation logs (with coordinates) in both legacy `ofApp` and `PinturaFluorescenteScene` handlers.
- Documented Phase 6 testers instructions in `tasks.md` so QA knows how to toggle/interpret the overlay.

## Testing
- `make`
- `OF_LOG_TO_STDOUT=1 ./tests/pool_logging_demo` *(temporary harness to trigger saturation warning twice; logs captured in apply report)*
