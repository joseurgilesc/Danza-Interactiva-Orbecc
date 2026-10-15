# Design: RibbonDanceScene

## Technical Approach

New self-contained `RibbonDanceScene` following `IBaseScene` pattern from PinturaFluorescenteScene. Each active `InteractionPoint` drives a `RibbonTrail` rendered via `ofMesh` + `OF_PRIMITIVE_TRIANGLE_STRIP` — first use in codebase. `RibbonParticle` pool emits dust/spark bursts. Velocity smoothed via EMA modulates width, undulation, alpha, and emission rate. Gravity pulls aged trail points downward like lightweight fabric.

```
InteractionPoint ──→ setParameter(id, vec2) → interactionPoints_
                              │
  update(): for each IP:
    if active → find/create RibbonTrail by ID → addPoint(pos, smoothedVel)
    else      → mark ribbon as fading
  for each ribbon:
    update(dt) — gravity on points, age fade, ring-buffer shift
    emitParticles() if velocity > threshold
    if isDead() → remove from map
  draw():
    for each ribbon → buildMesh() → mesh.draw()
    for each particle → p.draw() (glow circle)
    debugMarkers_ → IP indicators + velocity lines
```

## Architecture Decisions

| Decision | Options considered | Choice | Rationale |
|----------|--------------------|--------|-----------|
| Triangle strip generation | Rebuild mesh every frame vs persistent VBO update | Rebuild each frame | 200 pts × 2 verts × N ribbons = small; simplicity beats caching |
| RibbonTrail storage | `std::deque` vs ring buffer offset array | Ring buffer (`std::vector` + head index) | Fixed allocation, no realloc, fast wrap-around insert |
| Particle pool eviction | Oldest by age vs lowest remaining life | Oldest by index (age) | Simpler; linear scan of active particles found first |
| Velocity smoothing window | Simple EMA vs rolling average over N frames | EMA: `smoothed = 0.96*v + 0.04*raw` | Matches spec; single state variable per ribbon |
| Jump protection | Soft-cut (new segment) vs clamp position | Soft-cut: mark fading, spawn new ribbon | No teleport artifacts; old trail fades naturally |
| Palette colors | Per-frame sampling vs ribbon-assigned | Assigned at creation, stable per ribbon | Visual identity per joint; jitter applied per-point |

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/scenes/RibbonDanceScene.h` | Create | RibbonTrail, RibbonParticle structs, RibbonDanceScene class (~180 lines) |
| `src/scenes/RibbonDanceScene.cpp` | Create | Full implementation (~900 lines) |
| `src/ofApp.cpp` | Modify | `#include` + factory registration + `loadScene()` (~5 lines) |

## Data Structures

### RibbonTrail (embedded struct in header)

```cpp
struct TrailPoint {
    glm::vec2 position;
    glm::vec2 velocity;   // raw frame velocity (px/s)
    float age = 0.0f;     // seconds since added
    float width;          // computed per-frame
    float alpha;          // 0-1, fades toward tail
    float intensity;      // velocity contribution multiplier
};

struct RibbonTrail {
    std::string pointId;
    ofFloatColor color;           // assigned from palette at creation
    float smoothedVelocity = 0.0f; // EMA magnitude
    float timeSinceCreation = 0.0f;
    float timeSinceLastUpdate = 0.0f;
    float fadeAlpha = 1.0f;       // 1→0 during fade-out
    bool active = true;           // receiving data
    bool fading = false;          // source point deactivated/skeleton lost

    // Ring buffer — maxPoints from parameter
    std::vector<TrailPoint> buffer;
    size_t head = 0;              // write index
    size_t count = 0;             // valid entries
    size_t maxPoints = 200;

    ofMesh mesh;                  // rebuilt each frame in draw()

    void setup(const std::string& id, ofFloatColor col, size_t capacity);
    void addPoint(const glm::vec2& pos, const glm::vec2& rawVel, float emaFactor);
    void update(float dt, float gravity, float damping);
    void buildMesh(float maxW, float minW, float waveAmt, float waveSpd, float time);
    void draw();
    void fadeOut(float speed);
    void emitParticles(std::vector<RibbonParticle>& pool, size_t maxCount, int perBurst);
    bool isDead() const;          // fadeAlpha <= 0.01 && count == 0
};
```

### RibbonParticle (embedded struct in header)

```cpp
struct RibbonParticle {
    glm::vec2 position;
    glm::vec2 velocity;
    ofFloatColor color;
    float life = 0.0f;      // remaining seconds
    float maxLife = 1.0f;
    float size = 2.0f;
    float alpha = 1.0f;
    bool active = false;

    void setup(const glm::vec2& pos, const glm::vec2& vel,
               ofFloatColor col, float sz, float maxL);
    void update(float dt, float gravity, float damping);
    void draw();             // ofDrawCircle with glow
    bool isDead() const;     // life <= 0
};
```

### RibbonDanceScene (class)

```cpp
class RibbonDanceScene : public IBaseScene {
    // -- Ribbon/Particle storage
    std::map<std::string, RibbonTrail> ribbons_;
    std::vector<RibbonParticle> particles_;  // pre-allocated to MAX_PARTICLES
    size_t particleCount_ = 0;

    // -- Palette (same 5-palette system as PinturaFluorescente)
    std::vector<std::vector<ofFloatColor>> palettes_;
    int currentPalette_ = 0;

    // -- Interaction points
    std::map<std::string, InteractionPoint> interactionPoints_;

    // -- Parameters (registered via ParameterRegistry, scene-scoped)
    std::map<std::string, std::shared_ptr<Parameter>> parameters_;
    bool parametersRegistered_ = false;

    // -- Lifecycle
    bool isActive_ = false, isPaused_ = false;
    bool debugMarkers_ = false;
};
```

### Scene Constants (constexpr in header)

```cpp
constexpr size_t MAX_PARTICLES = 800;
constexpr size_t DEFAULT_RIBBON_CAPACITY = 200;
constexpr float VELOCITY_EMA_FACTOR = 0.96f;
constexpr float JUMP_THRESHOLD_SCREEN_SCALE = 0.15f;  // 15% of min(w,h)
```

## Mesh Generation Algorithm (Triangle Strip)

```
for i in [head, head+count-1] (wrapping):
    current = buffer[i]
    next = buffer[i+1]     // wrap around tail, skip if i==last
    dir = normalize(next.pos - current.pos)
    perp = vec2(-dir.y, dir.x)
    progress = i / count   // 0=tail, 1=head

    width = lerp(minWidth, maxWidth, progress)
    alpha = progress * fadeAlpha
    waveOffset = sin(time * waveSpeed + current.age * 3.0 + progress * 6.28)
               * waveAmount * current.intensity

    left = current.pos + perp * (halfWidth + waveOffset)
    right = current.pos - perp * (halfWidth + waveOffset)

    mesh.addVertex(vec3(left, 0))
    mesh.addVertex(vec3(right, 0))
    mesh.addColor(ofFloatColor(color, alpha))
    mesh.addColor(ofFloatColor(color, alpha))

// indices auto-generated for TRIANGLE_STRIP
mesh.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);
```

## Critical Method Pseudocode

### RibbonTrail::addPoint(pos, rawVel, emaFactor)

```cpp
float rawSpeed = glm::length(rawVel);
smoothedVelocity = emaFactor * smoothedVelocity + (1.0f - emaFactor) * rawSpeed;

if (count > 0) {
    glm::vec2& last = buffer[prevIndex()].position; // ring-buffer previous
    float dist = glm::distance(pos, last);
    if (dist > maxJumpThreshold) {
        fading = true; active = false; return;  // jump — start fresh
    }
}

size_t idx = (head + count) % maxPoints;
if (count == maxPoints) head = (head + 1) % maxPoints; else count++;

TrailPoint& pt = buffer[idx];
pt.position = pos; pt.velocity = rawVel; pt.age = 0.0f;
pt.width = 0.0f; pt.alpha = 1.0f; pt.intensity = 0.0f;
timeSinceLastUpdate = 0.0f;
```

### RibbonTrail::update(dt, gravity, damping)

```cpp
timeSinceCreation += dt;
timeSinceLastUpdate += dt;

if (fading) {
    fadeAlpha -= fadeSpeed * dt;
    fadeAlpha = max(0.0f, fadeAlpha);
}

for size_t i = 0; i < count; i++:
    TrailPoint& pt = buffer[(head + i) % maxPoints];
    pt.age += dt;
    // Gravity: stronger on older points
    float ageFactor = min(1.0f, pt.age / 3.0f);
    pt.velocity.y += gravity * ageFactor * dt;
    // Damping
    pt.velocity *= damping;
    pt.position += pt.velocity * dt;
    // Intensity: proportional to smoothed velocity, decays with age
    pt.intensity = (smoothedVelocity / 200.0f) * (1.0f - ageFactor * 0.5f);
```

### RibbonDanceScene::update(dt)

```cpp
if (!isActive_ || isPaused_) return;

// 1. Update interaction points (decay)
for (auto& [id, ip] : interactionPoints_) ip.update(dt);

// 2. Manage ribbons per interaction point
for (auto& [id, ip] : interactionPoints_) {
    if (ip.active) {
        auto it = ribbons_.find(id);
        if (it == ribbons_.end()) {
            RibbonTrail rt;
            ofFloatColor col = getColorFromPalette(currentPalette_, 1.0f);
            rt.setup(id, col, DEFAULT_RIBBON_CAPACITY);
            ribbons_[id] = rt;
            it = ribbons_.find(id);
        }
        it->second.addPoint(ip.position, ip.velocity, VELOCITY_EMA_FACTOR);
    } else {
        auto it = ribbons_.find(id);
        if (it != ribbons_.end() && !it->second.fading)
            it->second.fading = true;
    }
}

// 3. Update ribbons + emit particles
for (auto& [id, rb] : ribbons_) {
    rb.update(dt, ribbonGravity_, ribbonDamping_);
    if (rb.active && rb.smoothedVelocity > emissionThreshold_)
        rb.emitParticles(particles_, MAX_PARTICLES, particlesPerBurst_);
}

// 4. Update particles
for (auto& p : particles_) {
    if (!p.active) continue;
    p.update(dt, particleGravity_, particleDamping_);
}

// 5. Cleanup dead ribbons
for (auto it = ribbons_.begin(); it != ribbons_.end(); ) {
    if (it->second.isDead()) it = ribbons_.erase(it);
    else ++it;
}
```

### RibbonDanceScene::draw()

```cpp
ofEnableAlphaBlending(); ofFill();

// 1. Ribbon meshes (back to front for blend order)
for (auto& [id, rb] : ribbons_) {
    rb.buildMesh(ribbonMaxWidth_, ribbonMinWidth_,
                 waveAmount_, waveSpeed_, ofGetElapsedTimef());
    rb.draw();
}

// 2. Particles
for (auto& p : particles_) { if (p.active) p.draw(); }

// 3. Debug markers
if (debugMarkers_) {
    for (auto& [id, ip] : interactionPoints_) {
        // red circle + ID label at ip.position
        // velocity line: ip.position → ip.position + ip.velocity*0.3
    }
}
```

## Parameter Table (21 params, "ribbonDance." prefix)

Maintains `RibbonParams` struct (like PinturaFluorescente's `SceneParams`), synchronized bidirectionally with `ParameterRegistry` on `setParameter()`.

| Key | Min | Max | Default | Description |
|-----|-----|-----|---------|-------------|
| ribbon.maxPoints | 50 | 500 | 200 | Ring buffer capacity per trail |
| ribbon.maxWidth | 5 | 80 | 40 | Head width (px) |
| ribbon.minWidth | 1 | 20 | 2 | Tail width (px) |
| ribbon.gravity | 0 | 0.5 | 0.15 | Downward force/frame on aged pts |
| ribbon.damping | 0.9 | 0.999 | 0.96 | Velocity multiplier/frame |
| ribbon.fadeSpeed | 0.005 | 0.1 | 0.02 | Alpha loss/frame on fade |
| ribbon.life | 0.5 | 8 | 3 | Seconds to full fade |
| ribbon.velocityInfluence | 0 | 1 | 0.5 | Velocity→width/undulation factor |
| ribbon.waveAmount | 0 | 30 | 8 | Max wave displacement (px) |
| ribbon.waveSpeed | 0.1 | 6 | 2 | Wave oscillation frequency |
| ribbon.maxDistanceJump | 30 | 500 | 150 | Jump threshold (px) |
| ribbon.emaFactor | 0.8 | 0.995 | 0.96 | Velocity smoothing window |
| particle.maxCount | 100 | 2000 | 800 | Hard cap |
| particle.emissionThreshold | 20 | 300 | 80 | Velocity to trigger burst (px/s) |
| particle.perBurst | 1 | 20 | 5 | Particles per trigger |
| particle.velocityInfluence | 0 | 1 | 0.3 | Ribbon vel→particle vel factor |
| particle.gravity | 0 | 0.3 | 0.05 | Downward force on particles |
| particle.damping | 0.9 | 0.999 | 0.98 | Particle velocity decay |
| particle.lifeMin | 0.1 | 3 | 0.5 | Min particle life (seconds) |
| particle.lifeMax | 0.5 | 6 | 2 | Max particle life (seconds) |
| particle.sizeMin | 0.5 | 6 | 1 | Min radius (px) |
| particle.sizeMax | 1 | 12 | 4 | Max radius (px) |

## Testing Strategy

| Layer | What | How |
|-------|------|-----|
| Build | Compiles with no new warnings | `make` in project root |
| Visual | Multi-ribbon draw, width taper, alpha fade, wave undulation, particle bursts | Manual with MockKinectAdapter |
| Edge | 0 interaction points (all fade), 8 simultaneous ribbons (max IPs), jump > threshold | MockKinectAdapter testing |
| Lifecycle | activate/deactivate clears ribbons + particles, no leaks | Repeated scene switching (keys 1-2-3) |

## Migration / Rollout

No migration. New files only (`src/scenes/RibbonDanceScene.{h,cpp}`). Rollback: delete both files, remove 5 lines from `src/ofApp.cpp`.

## Open Questions

None. All decisions resolved from spec constraints, existing PinturaFluorescenteScene patterns, and IBaseScene contract.
