# Design: FluorescentNetScene

## Technical Approach

New self-contained `FluorescentNetScene` following the established `IBaseScene` pattern. Single particle pool with dual-physics evaluation per frame. Rendering combines fluorescent dot pass (FREE) + organic line pass (ANCHORED edges) + node dot pass (ANCHORED nodes).

## Data Model

### NetParticle struct
```cpp
enum class ParticlePhase { FREE, BLENDING, ANCHORED };

struct NetParticle {
    glm::vec2 pos, vel;
    ofFloatColor color;
    ParticlePhase phase = ParticlePhase::FREE;
    
    // FREE state
    std::string jointId;       // assigned interaction point
    float phaseOffset;         // wave phase offset per particle
    float life;                // seconds alive
    
    // ANCHORED state  
    glm::vec2 originalPos;     // anchor position for return force
    float noiseOffset;         // Perlin seed per node
    
    // Transition
    float transitionTimer;     // countdown to anchor
    float blendAlpha;          // 0→1 during blend period
};
```

### NetEdge struct
```cpp
struct NetEdge {
    int a, b;                  // particle indices
    float restLength;
    bool active;
};
```

### Pool
- `std::vector<NetParticle> particles_` — pre-sized, active/inactive via phase
- `std::vector<NetEdge> edges_` — dynamically managed
- Max capacity: `maxParticles` (default 800, range 100–3000)

## Physics Pipeline (per frame)

### FREE particles
1. For each FREE particle: find its assigned `InteractionPoint`
2. If point is active: compute orbital position using:
   - `target = point.position + orbitalOffset(phaseOffset, time)`
   - `orbitalOffset`: `A * sin(w*t - q*dist + phase)` for radial wave, `S * cos(ws*t - qs*dist + phase)` for swirl
   - Center attraction: `force = k * (target - pos)`
   - Damping: `vel *= beta`
3. If point is inactive: drift toward screen center
4. Clamp velocity to `maxSpeed`

### ANCHORED particles
1. Gravity: `vel.y += gravity * dt`
2. Wind: `vel.x += windStrength * sin(time * windFreq + windBias) * dt`
3. Perlin drift: `vel += ofSignedNoise(pos * noiseScale + time) * noiseStrength * dt`
4. Damping: `vel *= damping`
5. Return force: `vel += (originalPos - pos) * returnForce * dt`
6. Position integrate: `pos += vel * dt`
7. Spring relaxation (iterative, `springIterations` passes):
   - For each edge, compute `delta = posB - posA`, `diff = |delta| - restLength`
   - Correction: `posA += delta * diff * springStiffness * 0.5`, `posB -= delta * diff * springStiffness * 0.5`

### Transition FREE→ANCHORED
1. `transitionTimer += dt * transitionRate`
2. When `transitionTimer >= 1.0`: enter BLENDING phase
3. During BLENDING (0.5s): blend between orbital and net physics (lerp velocities), alpha transitions
4. After BLENDING: phase = ANCHORED, `originalPos = pos`, add to spatial index, scan neighbors for edges

## Edge Management

- After each anchor event: find neighbors within `connectionRadius` via spatial hash or brute force
- Create edges between new node and up to `maxConnectionsPerNode` (6) nearest ANCHORED neighbors
- Per frame: check all edges — if distance > `connectionRadius * 1.5`, remove edge
- Edge `restLength` = initial distance between the two nodes when the edge was created

## Rendering

### Pass 1: FREE particles
- `ofEnableAlphaBlending()`, `ofFill()`
- For each FREE/BLENDING particle: draw filled circle with palette color
- Size: `particleSize * (1.0 - transitionTimer * 0.5)` (shrink as they approach anchor)
- Alpha: `255 * blendAlpha` for FREE, `255 * (1.0 - blendAlpha) * 0.5` for BLENDING

### Pass 2: ANCHORED edges
- `ofNoFill()`, `ofSetLineWidth(netLineWidth)`
- For each active edge: draw line from `particles_[a].pos` to `particles_[b].pos`
- Alpha: fades with distance deviation from rest length
- Color: palette color with lower saturation

### Pass 3: ANCHORED nodes
- `ofFill()`
- For each ANCHORED particle: draw small dot at position
- Size: `nodeRadius` (2-4px)
- Alpha: `nodeAlpha` (60-150)

### Pass 4: Joint markers (debug)
- Green dots at active interaction points

## Parameters (28 total)

| Group | Parameter | Range | Default | Description |
|-------|-----------|-------|---------|-------------|
| **orbit** | `k` | 0.01–0.5 | 0.1 | Center attraction |
| | `A` | 0–50 | 15 | Wave amplitude |
| | `w` | 0.01–2 | 0.5 | Wave frequency |
| | `q` | 0–0.01 | 0.003 | Wave spatial freq |
| | `beta` | 0.8–0.999 | 0.97 | Velocity damping |
| | `S` | 0–30 | 8 | Swirl amplitude |
| | `ws` | 0.01–2 | 0.3 | Swirl frequency |
| | `qs` | 0–0.01 | 0.002 | Swirl spatial freq |
| **net** | `gravity` | 0–0.2 | 0.03 | Downward force |
| | `damping` | 0.8–0.999 | 0.96 | Net velocity damping |
| | `springStiffness` | 0.01–0.5 | 0.12 | Edge constraint stiffness |
| | `springIterations` | 1–8 | 3 | Relaxation passes |
| | `returnForce` | 0–0.05 | 0.008 | Pull to originalPos |
| **noise** | `noiseStrength` | 0–3 | 0.8 | Perlin drift intensity |
| | `noiseScale` | 0.001–0.05 | 0.015 | Perlin spatial scale |
| **wind** | `windStrength` | 0–2 | 0.4 | Wind intensity |
| | `windFrequency` | 0.05–1 | 0.3 | Wind oscillation freq |
| | `windBias` | -1–1 | 0 | Wind direction bias |
| **transition** | `transitionRate` | 0–2 | 0.3 | Speed of FREE→ANCHORED |
| | `blendPeriod` | 0.1–2 | 0.5 | Transition blend duration |
| **topology** | `connectionRadius` | 20–300 | 120 | Max distance for edge creation |
| | `maxConnectionsPerNode` | 1–12 | 6 | Max edges per node |
| **visual** | `particleSize` | 1–10 | 3 | FREE particle radius |
| | `nodeRadius` | 1–8 | 3 | ANCHORED node dot radius |
| | `netLineWidth` | 0.5–5 | 1.5 | Edge line width |
| | `netLineAlpha` | 10–255 | 120 | Edge line alpha |
| | `nodeAlpha` | 10–255 | 100 | ANCHORED node alpha |
| **pool** | `maxParticles` | 100–3000 | 800 | Pool capacity |

## ofApp Integration

```cpp
#include "scenes/FluorescentNetScene.h"

// In setup():
auto fnScene = modular::CreateFluorescentNetScene();
sceneEngine.loadScene(fnScene->getId(), fnScene);
presetManager_->loadPresets("fluorescentNet", "config/presets/fluorescentNet.json");
```

## Lessons from EnjambreOrganico

1. Use `travelTarget` snap pattern to lock target positions at state transition
2. Inactive joints: skip dwelling, pass through fast
3. All parameters registered from day one with scene-scoped IDs
4. `setParameter` strips scene prefix before map lookup
5. Conventional commits, no AI attribution
