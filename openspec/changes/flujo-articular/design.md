# Design: Flujo Articular Scene

## Technical Approach

New self-contained `FlujoArticularScene` following the IBaseScene pattern established by PinturaFluorescenteScene. Particles spawn at active Kinect joints and flow sequentially through a canonical anatomical chain — filtered each frame to only currently-tracked joints. Steering physics is direct seek toward the current target with smooth deceleration, deliberately simpler than orbital wave physics. Rendering reuses the same palette/drawCircle approach.

```
KinectMappingEngine ──vec2──→ setParameter("hand_left", xy) → interactionPoints_
                                      │
  update(): buildJointSequence() → filtered active joints [0..N]
    for each particle:
      target  = sequence[particle.targetJointIndex]
      approachPhase = 1 - clamp(distance / flowRadius, 0, 1)
      approachIntensity = 0.3 + 0.7 * approachPhase
      direction = normalize(target.pos - position)
      velocity += direction * flowSpeed * approachIntensity * dt * 60
      velocity *= 0.98f  // hardcoded damping
      position += velocity * dt
      if distance(position, target) < flowRadius:
        particle.targetJointIndex = (targetJointIndex + 1) % sequenceSize
      age += dt; if age > maxAge → deactivate
```

## Architecture Decisions

| Decision | Options considered | Choice | Rationale |
|----------|--------------------|--------|-----------|
| Particle struct | Extract Particle to shared header vs inline in scene | Inline FlowParticle struct | Proposal forbids PinturaFluorescenteScene dependency; extraction out of scope |
| Joint sequence ordering | Dynamic proximity sort vs canonical anatomical chain | Canonical anatomical order | Predictable organic path through body; proximity-based would produce chaotic jumps |
| Pool implementation | Reuse ParticlePool vs simplified `std::vector`-based pool | Fixed-capacity array (5000) with active flag | Simpler than PinturaFluorescente's dual-pool; no trail pool needed for flow |
| Fallback when no joints active | Particles freeze vs drift to screen center | Drift to center with weak spring (`springForce = 0.03 * flowSpeed`) | Visually graceful — particles fade out naturally instead of snapping |
| setParameter(vec2) contract | Custom joint dispatch vs same interactionPoints_ pattern | Same as PinturaFluorescente | KinectMappingEngine dispatches uniformly; consistency across scenes |

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/scenes/FlujoArticularScene.h` | Create | FlowParticle struct, FlowParticlePool class, FlujoArticularScene declaration (~150 lines) |
| `src/scenes/FlujoArticularScene.cpp` | Create | Physics, rendering, 5 parameters, 24 IBaseScene stubs (~500 lines) |
| `src/ofApp.cpp` | Modify | `#include "scenes/FlujoArticularScene.h"` + create/load scene (3 lines) |

## Interfaces / Contracts

**FlowParticle** (struct, inline in .h):
```cpp
struct FlowParticle {
    glm::vec2 position, velocity, acceleration;
    ofFloatColor color;
    float age, maxAge;
    bool alive = false;
    int targetJointIndex = 0;    // index into active joint sequence
    float approachPhase = 0.0f;  // 0-1, progress toward current target joint
    // Trail ring buffer
    glm::vec2 trailPositions[MAX_TRAIL_POINTS];
    int trailWriteIndex, trailCount;
};
```
Physics: homing acceleration `(0.3 + 0.7 * approachPhase)`, velocity damping `*= 0.98f` (hardcoded), advance to next joint when `distance < flowRadius`.

**Canonical joint order** (static `const std::vector<std::string>`):
`head → neck → shoulder_left → elbow_left → wrist_left → hand_left → shoulder_right → elbow_right → wrist_right → hand_right → spine_mid → spine_base → hip_left → knee_left → ankle_left → hip_right → knee_right → ankle_right`

Each frame, `buildJointSequence()` filters to entries present AND active in `interactionPoints_`. If empty, all particles drift toward screen center.

**Parameters** (5 floats, scene-scoped):

| Param ID | Range | Default | Description |
|----------|-------|---------|-------------|
| flowSpeed | 0.1–5.0 | 1.0 | multiplicador de velocidad de flujo |
| spawnRate | 1–20 | 5 | partículas por joint por frame |
| particleLifespan | 5–60 | 20 | vida máxima en segundos |
| trailLength | 0–30 | 10 | buffer de estela (posiciones) |
| flowRadius | 10–200 | 80 | px distancia para avanzar al siguiente joint |

**Lifecycle**: `activate()` clears pool + interactionPoints, registers parameters. `deactivate()` unregisters + clears pool. `setup()` creates pool + palettes + parameter definitions.

**setParameter("jointName", vec2)**: Same contract as PinturaFluorescente — delegates to `interactionPoints_["jointName"].setTarget(normalizedPos, 1.0f, w, h)`.

## Testing Strategy

| Layer | What | How |
|-------|------|-----|
| Build | Compiles with no new warnings | `make` |
| Visual | Particles flow through active joints, wrap around, fade on joint loss | MockKinectAdapter testing |
| Edge | Pool saturation (5000), zero active joints, single active joint | Visual inspection during development |
| Lifecycle | activate/deactivate clears state, no leaks on scene switch | Repeated 1-2-3 key switching |

## Migration / Rollout

No migration. New files only. Rollback: delete `FlujoArticularScene.{h,cpp}`, remove 3 lines from `ofApp.cpp`.

## Open Questions

None. All decisions resolved from proposal constraints, existing patterns, and the 18-joint Kinect type system.
