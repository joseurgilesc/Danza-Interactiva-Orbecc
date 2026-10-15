#include "scenes/RibbonDanceScene.h"
#include "registry/ParameterRegistry.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace modular {

// ============================================================================
// RibbonDanceScene Implementation
// ============================================================================

RibbonDanceScene::RibbonDanceScene()
    : currentPalette_(0)
    , isActive_(false)
    , isPaused_(false)
    , isValid_(true)
    , invalidReason_("")
    , parametersRegistered_(false)
    , debugMarkers_(false) {
}

RibbonDanceScene::~RibbonDanceScene() {
    unregisterParameters();
}

// ============================================================================
// Palette System (identical to PinturaFluorescenteScene)
// ============================================================================

ofFloatColor RibbonDanceScene::getColorFromPalette(int paletteIndex, float variation) {
    std::vector<ofFloatColor> palette = palettes_[paletteIndex % palettes_.size()];
    int baseIndex = (int)ofRandom(palette.size());
    ofFloatColor col = palette[baseIndex];
    col += ofFloatColor(variation * ofRandom(-0.15f, 0.15f),
                        variation * ofRandom(-0.15f, 0.15f),
                        variation * ofRandom(-0.15f, 0.15f));
    return col;
}

void RibbonDanceScene::setupPalettes() {
    palettes_.push_back({
        ofFloatColor::fromHex(0xFF00FF),
        ofFloatColor::fromHex(0xFF1493),
        ofFloatColor::fromHex(0xFF0080),
        ofFloatColor::fromHex(0xFF69B4),
        ofFloatColor::fromHex(0xFF00CC)
    });
    palettes_.push_back({
        ofFloatColor::fromHex(0x00FFFF),
        ofFloatColor::fromHex(0x00CED1),
        ofFloatColor::fromHex(0x40E0D0),
        ofFloatColor::fromHex(0x00BFFF),
        ofFloatColor::fromHex(0x04D9FF)
    });
    palettes_.push_back({
        ofFloatColor::fromHex(0x00FF00),
        ofFloatColor::fromHex(0x39FF14),
        ofFloatColor::fromHex(0x32CD32),
        ofFloatColor::fromHex(0x00FA9A),
        ofFloatColor::fromHex(0x7CFC00)
    });
    palettes_.push_back({
        ofFloatColor::fromHex(0xFF6B00),
        ofFloatColor::fromHex(0xFF4500),
        ofFloatColor::fromHex(0xFF8C00),
        ofFloatColor::fromHex(0xFF7F50),
        ofFloatColor::fromHex(0xFF6347)
    });
    palettes_.push_back({
        ofFloatColor::fromHex(0xFFFF00),
        ofFloatColor::fromHex(0xFFD700),
        ofFloatColor::fromHex(0xCCFF00),
        ofFloatColor::fromHex(0xFFFF33),
        ofFloatColor::fromHex(0xE5FF00)
    });
}

// ============================================================================
// RibbonParticle Implementation
// ============================================================================

void RibbonParticle::setup(const glm::vec2& pos, const glm::vec2& vel,
                           ofFloatColor col, float sz, float maxL) {
    position = pos;
    velocity = vel;
    color = col;
    size = sz;
    maxLife = maxL;
    life = maxL;
    alpha = 1.0f;
    active = true;
}

void RibbonParticle::update(float dt, float gravity, float damping) {
    if (!active) return;

    // Apply gravity
    velocity.y += gravity * dt;

    // Apply damping
    velocity *= damping;

    // Integrate position
    position += velocity * dt;

    // Life decay
    life -= dt;
    if (life <= 0.0f) {
        life = 0.0f;
        active = false;
    }

    // Alpha fades with remaining life
    alpha = (maxLife > 0.0f) ? (life / maxLife) : 0.0f;
}

void RibbonParticle::draw() {
    if (!active) return;

    float a = glm::clamp(alpha, 0.0f, 1.0f);
    ofSetColor(color.r * 255.0f, color.g * 255.0f, color.b * 255.0f,
               static_cast<int>(a * 255.0f));
    ofFill();
    ofDrawCircle(position.x, position.y, size);
}

bool RibbonParticle::isDead() const {
    return life <= 0.0f || !active;
}

// ============================================================================
// RibbonTrail Implementation
// ============================================================================

void RibbonTrail::setup(size_t maxPoints) {
    points.resize(maxPoints);
    head = 0;
    count = 0;
    alpha = 1.0f;
    life = 0.0f;
    smoothedVelocity = 0.0f;
    smoothedPosition = glm::vec2(0.0f);
    active = false;
    fading = false;
    lastAddTime = 0.0f;
    mesh.clear();
}

void RibbonTrail::addPoint(const glm::vec2& pos, const glm::vec2& vel,
                           const RibbonParams& params) {
    float rawSpeed = glm::length(vel);
    float factor = params.velocitySmoothing;
    smoothedVelocity = factor * smoothedVelocity + (1.0f - factor) * rawSpeed;

    // Issue 2a: Velocity clamping — prevent extreme joint velocities
    glm::vec2 clampedVel = vel;
    if (rawSpeed > params.maxJointVelocity) {
        clampedVel = vel * (params.maxJointVelocity / rawSpeed);
    }

    // Issue 2b: Position smoothing (EMA) for organic delay
    if (count == 0) {
        smoothedPosition = pos;
    } else {
        smoothedPosition = smoothedPosition * 0.75f + pos * 0.25f;
    }

    // Jump protection: if distance to last point exceeds threshold, start fresh
    if (count > 0) {
        size_t prevIdx = (head + points.size() - 1) % points.size();
        float dist = glm::distance(smoothedPosition, points[prevIdx].position);
        if (dist > params.ribbonMaxDistanceJump) {
            // Soft-cut: clear trail and start fresh at this position
            count = 0;
            head = 0;
        }
    }

    // Issue 2c: Launch velocity amplification for overshoot/drag
    float boost = 1.0f + params.ribbonVelocityInfluence * 0.8f;
    glm::vec2 launchVel = clampedVel * boost;

    // STORE the point in the ring buffer
    TrailPoint& pt = points[head];
    pt.position = smoothedPosition;
    pt.velocity = launchVel;
    pt.age = 0.0f;
    pt.width = 0.0f;
    pt.alpha = 1.0f;
    pt.intensity = glm::clamp(smoothedVelocity / 200.0f, 0.0f, 1.0f);

    // Advance head
    head = (head + 1) % points.size();
    if (count < points.size()) {
        count++;
    }

    active = true;
    fading = false;
    lastAddTime = ofGetElapsedTimef();
    alpha = 1.0f;
}

void RibbonTrail::update(float dt, const RibbonParams& params) {
    if (count == 0) return;

    life += dt;

    // Fade-out alpha decrement
    if (fading) {
        alpha -= params.ribbonFadeSpeed * dt * 60.0f;
        alpha = std::max(0.0f, alpha);
    }

    // Iterate all points (oldest first)
    size_t startIdx = (head + points.size() - count) % points.size();
    for (size_t i = 0; i < count; i++) {
        size_t idx = (startIdx + i) % points.size();
        TrailPoint& pt = points[idx];

        pt.age += dt;

        // Issue 2d: Speed-dependent gravity — ribbon droops when joint stops,
        // resists gravity when joint moves fast. Extra gravity during fade for graceful falling.
        float speedFactor = glm::clamp(smoothedVelocity / 100.0f, 0.0f, 1.0f);
        float gravityMultiplier = 1.0f + (1.0f - speedFactor) * 2.0f;  // range 1.0 to 3.0
        if (fading) gravityMultiplier *= 1.8f;  // extra fall when dying — graceful descent
        float ageFactor = glm::clamp(pt.age / params.ribbonLife, 0.0f, 1.0f);
        pt.velocity.y += params.ribbonGravity * ageFactor * gravityMultiplier * dt * 60.0f;

        // Damping
        pt.velocity *= params.ribbonDamping;

        // Position integration
        pt.position += pt.velocity * dt;

        // Intensity decays with age
        float baseIntensity = glm::clamp(smoothedVelocity / 200.0f, 0.0f, 1.0f);
        pt.intensity = baseIntensity * (1.0f - ageFactor * 0.5f);

        // Alpha: fades toward tail
        pt.alpha = 1.0f - ageFactor;
    }

    // Secondary orbital attractors — gentle pull toward non-emitter interaction points
    for (const auto& attractor : secondaryAttractors) {
        size_t startIdx2 = (head + points.size() - count) % points.size();
        for (size_t i = 0; i < count; i++) {
            size_t idx = (startIdx2 + i) % points.size();
            TrailPoint& pt = points[idx];
            glm::vec2 toAttractor = attractor - pt.position;
            float dist = glm::length(toAttractor);
            if (dist > 0.001f && dist < params.orbitRadius) {
                float strength = params.orbitStrength * (1.0f - dist / params.orbitRadius);
                pt.velocity += (toAttractor / dist) * strength * dt * 60.0f;
            }
        }
    }

    // Remove points older than ribbonLife seconds from the tail
    while (count > 0) {
        size_t oldestIdx = (head + points.size() - count) % points.size();
        TrailPoint& oldest = points[oldestIdx];
        if (oldest.age > params.ribbonLife || (fading && oldest.alpha <= 0.01f)) {
            // Remove by advancing logical start (decrement count)
            count--;
        } else {
            break;
        }
    }

    // Allow alpha to continue fading even after all points are gone (graceful dissolve)
}

void RibbonTrail::buildMesh(const RibbonParams& params, float widthScale) {
    mesh.clear();
    if (count < 2) return;

    mesh.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);

    float time = ofGetElapsedTimef();
    size_t startIdx = (head + points.size() - count) % points.size();

    // Build vertices: iterate all points oldest→newest
    // Each point produces left+right vertices offset perpendicular to the trail direction
    glm::vec2 lastDir(0.0f);

    for (size_t i = 0; i < count; i++) {
        size_t idx = (startIdx + i) % points.size();
        const TrailPoint& pt = points[idx];

        // Direction: toward next point (or reuse last direction for final point)
        glm::vec2 dir;
        if (i + 1 < count) {
            size_t nextIdx = (startIdx + i + 1) % points.size();
            dir = points[nextIdx].position - pt.position;
        }
        float dirLen = glm::length(dir);
        if (dirLen > 0.0001f) {
            dir /= dirLen;
            lastDir = dir;
        } else {
            dir = lastDir;
        }

        // Perpendicular: rotate 90° CCW
        glm::vec2 perp(-dir.y, dir.x);

        // normalizedAge: 0 = newest (head), 1 = oldest (tail) — uses actual point age
        float normalizedAge = (params.ribbonLife > 0.0f)
            ? glm::clamp(pt.age / params.ribbonLife, 0.0f, 1.0f)
            : 0.0f;

        // Width: max at head (age=0), min at tail (age=ribbonLife)
        float width = glm::mix(params.ribbonMaxWidth, params.ribbonMinWidth, normalizedAge) * widthScale;
        width *= pt.intensity;

        // Alpha: full at head, transparent at tail
        float pointAlpha = 1.0f - normalizedAge;  // 1 at head (newest), 0 at tail (oldest)
        pointAlpha *= alpha;  // overall ribbon alpha

        // Wave undulation — multi-frequency for organic, fabric-like motion
        float wave1 = std::sin(time * params.ribbonWaveSpeed * 1.0f
                               + normalizedAge * params.ribbonWaveAmount * 1.3f
                               + pt.intensity * 3.0f)
                      * (params.ribbonWaveAmount * 0.45f);

        float wave2 = std::sin(time * params.ribbonWaveSpeed * 1.7f
                               + normalizedAge * params.ribbonWaveAmount * 0.7f
                               + pt.intensity * 7.0f)
                      * (params.ribbonWaveAmount * 0.25f);

        float wave3 = std::sin(time * params.ribbonWaveSpeed * 0.4f
                               + normalizedAge * params.ribbonWaveAmount * 2.1f)
                      * (params.ribbonWaveAmount * 0.15f);

        float wave = wave1 + wave2 + wave3;

        // Along-direction wiggle for organic "S" curves between points
        float alongWiggle = std::sin(time * params.ribbonWaveSpeed * 1.2f
                                     + normalizedAge * params.ribbonWaveAmount * 0.8f
                                     + pt.intensity * 4.0f)
                            * (params.ribbonWaveAmount * 0.25f);

        float halfWidth = width * 0.5f;
        float edgeOffset = halfWidth + wave;

        // Displace point position along the direction for wiggle, and perpendicular for wave
        glm::vec2 displacedPos = pt.position + dir * alongWiggle;

        glm::vec2 left = displacedPos + perp * edgeOffset;
        glm::vec2 right = displacedPos - perp * edgeOffset;

        // Fibonacci multi-color blending across ribbon length
        // Precompute Fibonacci cumulative positions for palette segment boundaries
        float fibPositions[16];
        {
            int N = static_cast<int>(paletteColorCount);
            if (N < 1) N = 1;
            if (N > 16) N = 16;
            int fib[16] = {1, 1};  // first two Fibonacci numbers
            for (int f = 2; f < N; f++) fib[f] = fib[f-1] + fib[f-2];
            float cumSum = 0;
            for (int f = 0; f < N; f++) cumSum += static_cast<float>(fib[f]);
            float running = 0;
            for (int f = 0; f < N; f++) {
                running += static_cast<float>(fib[f]);
                fibPositions[f] = running / cumSum;  // e.g. 1/12, 2/12, 4/12, 7/12, 12/12
            }
        }

        // Find which Fibonacci segment this point's normalizedAge falls in
        // Wrap normalizedAge into repeating cycles for shorter color segments
        float rawT = normalizedAge * static_cast<float>(params.ribbonColorCycles);
        float cycleT = std::fmod(rawT, 1.0f);
        if (cycleT < 0.0f) cycleT += 1.0f;  // ensure positive
        int N = static_cast<int>(paletteColorCount);
        int seg = 0;
        for (int s = 0; s < N; s++) {
            if (cycleT <= fibPositions[s]) { seg = s; break; }
        }

        ofFloatColor ptColor(0, 0, 0);
        if (N <= 0) {
            // Fallback: white
            ptColor = ofFloatColor(1, 1, 1);
        } else if (seg == 0) {
            // Pure first palette color (crisp head section)
            ptColor = paletteColors[0];
        } else {
            // Interpolate between paletteColors[seg-1] and paletteColors[seg]
            float segStart = (seg > 0) ? fibPositions[seg - 1] : 0.0f;
            float segEnd = fibPositions[seg];
            float localT = (segEnd - segStart > 0.0001f)
                ? (cycleT - segStart) / (segEnd - segStart)
                : 0.0f;
            localT = glm::clamp(localT, 0.0f, 1.0f);
            ptColor.r = glm::mix(paletteColors[seg - 1].r, paletteColors[seg].r, localT);
            ptColor.g = glm::mix(paletteColors[seg - 1].g, paletteColors[seg].g, localT);
            ptColor.b = glm::mix(paletteColors[seg - 1].b, paletteColors[seg].b, localT);
        }

        mesh.addVertex(glm::vec3(left.x, left.y, 0.0f));
        mesh.addVertex(glm::vec3(right.x, right.y, 0.0f));

        ofFloatColor col(ptColor.r, ptColor.g, ptColor.b, pointAlpha);
        mesh.addColor(col);
        mesh.addColor(col);
    }
}

void RibbonTrail::draw() {
    if (count < 2 || alpha <= 0.01f) return;
    mesh.draw();
}

void RibbonTrail::fadeOut() {
    fading = true;
    active = false;
}

bool RibbonTrail::isDead() const {
    return alpha <= 0.005f;  // only truly dead when fully dissolved
}

void RibbonTrail::emitParticles(std::vector<RibbonParticle>& pool,
                                 const InteractionPoint& ip,
                                 const RibbonParams& params) {
    if (smoothedVelocity < params.particleEmissionThreshold) return;
    if (!active) return;

    int burstCount = params.particlesPerBurst;

    // Find free slots in the pool
    for (int b = 0; b < burstCount; b++) {
        // First try to find an inactive slot
        size_t freeSlot = pool.size();
        for (size_t i = 0; i < pool.size(); i++) {
            if (!pool[i].active) {
                freeSlot = i;
                break;
            }
        }

        // If pool is full, evict oldest (find first active with lowest life)
        if (freeSlot == pool.size()) {
            size_t oldestIdx = 0;
            float lowestLife = pool[0].life;
            for (size_t i = 1; i < pool.size(); i++) {
                if (pool[i].active && pool[i].life < lowestLife) {
                    lowestLife = pool[i].life;
                    oldestIdx = i;
                }
            }
            freeSlot = oldestIdx;
        }

        // Position near the interaction point with random offset
        float offsetRange = params.ribbonMaxWidth * 0.3f;
        glm::vec2 pos = ip.position + glm::vec2(ofRandom(-offsetRange, offsetRange),
                                                  ofRandom(-offsetRange, offsetRange));

        // Velocity influenced by ribbon velocity
        float rawSpeed = smoothedVelocity;
        glm::vec2 dir = (glm::length(ip.velocity) > 0.01f)
                            ? glm::normalize(ip.velocity)
                            : glm::vec2(ofRandom(-1.0f, 1.0f), ofRandom(-1.0f, 1.0f));
        float speed = rawSpeed * params.particleVelocityInfluence * 0.01f;
        glm::vec2 vel = dir * speed + glm::vec2(ofRandom(-0.5f, 0.5f), ofRandom(-0.5f, 0.5f));

        // Random size and life within configured ranges
        float sz = ofRandom(params.particleSizeMin, params.particleSizeMax);
        float maxL = ofRandom(params.particleLifeMin, params.particleLifeMax);

        // Color: use ribbon's head palette color with slight variation
        ofFloatColor col = (paletteColorCount > 0) ? paletteColors[0] : ofFloatColor(1, 1, 1);
        col.r = glm::clamp(col.r + ofRandom(-0.1f, 0.1f), 0.0f, 1.0f);
        col.g = glm::clamp(col.g + ofRandom(-0.1f, 0.1f), 0.0f, 1.0f);
        col.b = glm::clamp(col.b + ofRandom(-0.1f, 0.1f), 0.0f, 1.0f);

        if (freeSlot < pool.size()) {
            pool[freeSlot].setup(pos, vel, col, sz, maxL);
        }
    }
}

// ============================================================================
// Parameter Definitions (5 essential params — expands to 21 in PR 2)
// ============================================================================

void RibbonDanceScene::initializeParameterDefinitions() {
    if (!parameters_.empty()) {
        return;
    }

    const std::string sceneId = getId();
    auto addParam = [&](const std::string& key,
                        const std::string& label,
                        float minVal,
                        float maxVal,
                        float defaultVal,
                        float stepVal,
                        const std::string& description,
                        ParameterType type = ParameterType::FLOAT) {
        auto param = std::make_shared<Parameter>();
        param->id = sceneId + "." + key;
        param->label = label;
        param->type = type;
        param->scope = ParameterScope::SCENE;
        param->min = minVal;
        param->max = maxVal;
        param->defaultValue = defaultVal;
        param->currentValue = defaultVal;
        param->step = stepVal;
        param->group = sceneId;
        param->description = description;
        param->hidden = false;
        param->writable = true;
        parameters_[key] = param;
    };

    // Ribbon trail parameters (12)
    addParam("ribbon.maxPoints", "Puntos Max",
              50.0f, 500.0f, 120.0f, 10.0f,
              "Cantidad maxima de puntos por cinta (ring buffer)",
              ParameterType::INT);

    addParam("ribbon.maxWidth", "Ancho Max",
             5.0f, 80.0f, 30.0f, 1.0f,
             "Ancho de la cinta en la cabeza (px)");

    addParam("ribbon.minWidth", "Ancho Min",
             1.0f, 20.0f, 4.0f, 0.5f,
             "Ancho de la cinta en la cola (px)");

    addParam("ribbon.gravity", "Gravedad",
             0.0f, 0.5f, 0.04f, 0.01f,
             "Fuerza gravitacional sobre puntos viejos de la cinta");

    addParam("ribbon.damping", "Amortiguacion",
             0.9f, 0.999f, 0.98f, 0.001f,
             "Factor de amortiguacion de velocidad por frame");

    addParam("ribbon.fadeSpeed", "Velocidad Fade",
              0.001f, 0.1f, 0.004f, 0.001f,
              "Velocidad de desvanecimiento al perder la fuente");

    addParam("ribbon.life", "Vida Cinta",
              0.5f, 15.0f, 8.0f, 0.1f,
              "Segundos hasta desvanecimiento total");

    addParam("ribbon.velocityInfluence", "Influencia Velocidad",
             0.0f, 1.0f, 1.0f, 0.05f,
             "Cuanto afecta la velocidad al ancho y ondulacion");

    addParam("ribbon.emaFactor", "Suavizado Velocidad",
             0.8f, 0.995f, 0.96f, 0.005f,
             "Factor de suavizado EMA de la velocidad");

    addParam("ribbon.waveAmount", "Ondulacion",
              0.0f, 30.0f, 10.0f, 0.5f,
              "Amplitud maxima de la ondulacion (px)");

    addParam("ribbon.waveSpeed", "Velocidad Onda",
             0.1f, 6.0f, 1.0f, 0.1f,
             "Frecuencia de oscilacion de la ondulacion");

    addParam("ribbon.maxDistanceJump", "Salto Maximo",
             30.0f, 500.0f, 400.0f, 10.0f,
             "Distancia maxima entre puntos consecutivos antes de corte");

    addParam("ribbon.maxActiveRibbons", "Cintas Max Activas",
              1.0f, 12.0f, 6.0f, 1.0f,
              "Cantidad maxima de cintas activas simultaneas",
              ParameterType::INT);

    addParam("ribbon.maxJointVelocity", "Velocidad Max Articulacion",
              100.0f, 5000.0f, 1500.0f, 50.0f,
              "Velocidad maxima de entrada del joint (clamping, px/s)");

    addParam("ribbon.colorCycles", "Ciclos de Color",
              1.0f, 8.0f, 3.0f, 1.0f,
              "Cuantas veces se repite la paleta Fibonacci a lo largo de la cinta",
              ParameterType::INT);

    addParam("ribbon.orbitRadius", "Radio Orbital",
              50.0f, 1000.0f, 300.0f, 10.0f,
              "Radio de influencia orbital (px)");

    addParam("ribbon.orbitStrength", "Fuerza Orbital",
              0.001f, 0.2f, 0.03f, 0.001f,
              "Fuerza de atraccion orbital");

    // Particle parameters (10)
    addParam("particle.maxCount", "Particulas Max",
              100.0f, 2000.0f, 800.0f, 50.0f,
               "Limite duro de particulas activas",
               ParameterType::INT);

    addParam("particle.emissionThreshold", "Umbral Emision",
             20.0f, 300.0f, 20.0f, 5.0f,
             "Velocidad minima (px/s) para emitir particulas");

    addParam("particle.perBurst", "Particulas x Rafaga",
              1.0f, 20.0f, 3.0f, 1.0f,
              "Particulas emitidas por rafaga",
              ParameterType::INT);

    addParam("particle.velocityInfluence", "Influencia Vel Particula",
             0.0f, 1.0f, 0.5f, 0.05f,
             "Factor de velocidad de la cinta hacia las particulas");

    addParam("particle.gravity", "Gravedad Particula",
             0.0f, 30.0f, 15.0f, 0.5f,
             "Fuerza gravitacional sobre particulas");

    addParam("particle.damping", "Amortiguacion Particula",
             0.9f, 0.999f, 0.97f, 0.001f,
             "Factor de amortiguacion de velocidad de particulas");

    addParam("particle.lifeMin", "Vida Min Particula",
             0.1f, 3.0f, 0.6f, 0.1f,
             "Vida minima de particula (segundos)");

    addParam("particle.lifeMax", "Vida Max Particula",
             0.5f, 6.0f, 2.0f, 0.1f,
             "Vida maxima de particula (segundos)");

    addParam("particle.sizeMin", "Tamano Min Particula",
             0.5f, 6.0f, 1.5f, 0.5f,
             "Radio minimo de particula (px)");

    addParam("particle.sizeMax", "Tamano Max Particula",
             1.0f, 12.0f, 5.0f, 0.5f,
             "Radio maximo de particula (px)");
}

// ============================================================================
// Parameter Registration (same pattern as PinturaFluorescenteScene)
// ============================================================================

void RibbonDanceScene::registerParameters() {
    if (parametersRegistered_) {
        return;
    }

    initializeParameterDefinitions();
    auto& registry = ParameterRegistry::getGlobal();

    for (const auto& entry : parameters_) {
        const auto& param = entry.second;
        if (!param) continue;

        Parameter definition = *param;
        bool registered = registry.registerParam(definition);
        if (!registered && !registry.hasParam(definition.id)) {
            ofLogWarning("RibbonDanceScene")
                << "Failed to register parameter " << definition.id;
        }
    }

    parametersRegistered_ = true;
}

void RibbonDanceScene::unregisterParameters() {
    if (!parametersRegistered_) {
        return;
    }

    auto& registry = ParameterRegistry::getGlobal();
    for (const auto& entry : parameters_) {
        const auto& param = entry.second;
        if (param) {
            registry.unregisterParam(param->id);
        }
    }

    parametersRegistered_ = false;
}

// ============================================================================
// Parameter Sync — reads parameters_ map into ribbonParams_ struct
// ============================================================================

void RibbonDanceScene::syncParamsFromMap() {
    // Read each parameter value from the parameters_ map into ribbonParams_
    // This mirrors the pattern from PinturaFluorescenteScene::setParameter()

    auto readFloat = [&](const std::string& key, float& target) {
        auto it = parameters_.find(key);
        if (it != parameters_.end() && it->second) {
            target = it->second->currentValue;
        }
    };

    auto readInt = [&](const std::string& key, int& target) {
        auto it = parameters_.find(key);
        if (it != parameters_.end() && it->second) {
            target = static_cast<int>(it->second->currentValue);
        }
    };

    // Ribbon trail params
    readInt("ribbon.maxPoints", ribbonParams_.maxRibbonPoints);
    readFloat("ribbon.maxWidth", ribbonParams_.ribbonMaxWidth);
    readFloat("ribbon.minWidth", ribbonParams_.ribbonMinWidth);
    readFloat("ribbon.gravity", ribbonParams_.ribbonGravity);
    readFloat("ribbon.damping", ribbonParams_.ribbonDamping);
    readFloat("ribbon.fadeSpeed", ribbonParams_.ribbonFadeSpeed);
    readFloat("ribbon.life", ribbonParams_.ribbonLife);
    readFloat("ribbon.velocityInfluence", ribbonParams_.ribbonVelocityInfluence);
    readFloat("ribbon.emaFactor", ribbonParams_.velocitySmoothing);
    readFloat("ribbon.waveAmount", ribbonParams_.ribbonWaveAmount);
    readFloat("ribbon.waveSpeed", ribbonParams_.ribbonWaveSpeed);
    readFloat("ribbon.maxDistanceJump", ribbonParams_.ribbonMaxDistanceJump);
    readInt("ribbon.maxActiveRibbons", ribbonParams_.maxActiveRibbons);
    readFloat("ribbon.maxJointVelocity", ribbonParams_.maxJointVelocity);
    readInt("ribbon.colorCycles", ribbonParams_.ribbonColorCycles);
    readFloat("ribbon.orbitRadius", ribbonParams_.orbitRadius);
    readFloat("ribbon.orbitStrength", ribbonParams_.orbitStrength);

    // Particle params
    readInt("particle.maxCount", ribbonParams_.particleMaxCount);
    readFloat("particle.emissionThreshold", ribbonParams_.particleEmissionThreshold);
    readInt("particle.perBurst", ribbonParams_.particlesPerBurst);
    readFloat("particle.velocityInfluence", ribbonParams_.particleVelocityInfluence);
    readFloat("particle.gravity", ribbonParams_.particleGravity);
    readFloat("particle.damping", ribbonParams_.particleDamping);
    readFloat("particle.lifeMin", ribbonParams_.particleLifeMin);
    readFloat("particle.lifeMax", ribbonParams_.particleLifeMax);
    readFloat("particle.sizeMin", ribbonParams_.particleSizeMin);
    readFloat("particle.sizeMax", ribbonParams_.particleSizeMax);
}

// ============================================================================
// Lifecycle Methods
// ============================================================================

void RibbonDanceScene::setup() {
    ofEnableAlphaBlending();

    setupPalettes();
    initializeParameterDefinitions();

    ofSetBackgroundAuto(true);
    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) {
        bg = param->currentValue;
    }
    ofBackground(static_cast<int>(bg));
}

void RibbonDanceScene::activate() {
    ofLogNotice("RibbonDanceScene") << "activate() START";
    isActive_ = true;
    isPaused_ = false;

    // Reset runtime state so scene starts fresh
    interactionPoints_.clear();
    ribbons_.clear();
    particles_.clear();

    // Pre-allocate particle pool to configured max
    particles_.resize(ribbonParams_.particleMaxCount);

    ofSetWindowTitle("Ribbon Dance — Cintas Fluorescentes");
    ofSetFrameRate(60);

    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) {
        bg = param->currentValue;
    }
    ofBackground(static_cast<int>(bg));

    // Read global palette from registry
    if (auto paletteParam = registry.getParam("global.currentPalette")) {
        currentPalette_ = static_cast<int>(paletteParam->currentValue);
    }

    // Register scene parameters in the registry
    registerParameters();

    // Read saved values from registry
    for (const auto& entry : parameters_) {
        const std::string& key = entry.first;
        if (auto param = registry.getParam(key)) {
            setParameter(key, param->currentValue);
        }
    }

    // Sync parameters to runtime struct
    syncParamsFromMap();

    // Update jump threshold based on screen size
    float screenDiag = glm::length(glm::vec2(ofGetWidth(), ofGetHeight()));
    ribbonParams_.ribbonMaxDistanceJump = screenDiag * 0.5f;

    // Re-allocate particle pool with configured max count
    particles_.resize(ribbonParams_.particleMaxCount);
    ofLogNotice("RibbonDanceScene") << "activate() END — palette=" << currentPalette_ << " maxParticles=" << ribbonParams_.particleMaxCount;
}

void RibbonDanceScene::deactivate() {
    isActive_ = false;

    // Unregister scene parameters from registry
    unregisterParameters();

    interactionPoints_.clear();
    ribbons_.clear();
    particles_.clear();
}

void RibbonDanceScene::pause() {
    isPaused_ = true;
}

void RibbonDanceScene::resume() {
    isPaused_ = false;
}

// ============================================================================
// Identification
// ============================================================================

std::string RibbonDanceScene::getId() const {
    return "ribbonDance";
}

std::string RibbonDanceScene::getName() const {
    return "Ribbon Dance — Cintas Fluorescentes";
}

// ============================================================================
// Update Loop
// ============================================================================

void RibbonDanceScene::update(float deltaTime) {
    if (!isActive_ || isPaused_) return;
    
    // 1. Update interaction points
    for (auto& entry : interactionPoints_) {
        entry.second.update(deltaTime);
    }
    
    // 2. Each active IP gets its own ribbon. Multiple simultaneous ribbons.
    for (auto& entry : interactionPoints_) {
        const std::string& pid = entry.first;
        InteractionPoint& ip = entry.second;
        
        if (ip.active) {
            auto it = ribbons_.find(pid);
            if (it == ribbons_.end()) {
                // New ribbon for this IP
                RibbonTrail ribbon;
                ribbon.pointId = pid;
                size_t pIdx = currentPalette_ % palettes_.size();
                ribbon.paletteColors = palettes_[pIdx];
                ribbon.paletteColorCount = ribbon.paletteColors.size();
                ribbon.setup(ribbonParams_.maxRibbonPoints);
                ribbon.active = true;
                ribbons_[pid] = ribbon;
                it = ribbons_.find(pid);
            }
            it->second.addPoint(ip.position, ip.velocity, ribbonParams_);
        } else {
            auto it = ribbons_.find(pid);
            if (it != ribbons_.end() && it->second.active && !it->second.fading) {
                it->second.fadeOut();
            }
        }
    }
    
    // 3. Update ribbon physics
    for (auto& entry : ribbons_) {
        entry.second.update(deltaTime, ribbonParams_);
    }
    
    // 4. Remove dead ribbons
    for (auto it = ribbons_.begin(); it != ribbons_.end(); ) {
        if (it->second.isDead()) it = ribbons_.erase(it);
        else ++it;
    }
}

// ============================================================================
// Draw Loop
// ============================================================================
// Draw Loop — stub: debug markers only, no ribbons yet
// ============================================================================

void RibbonDanceScene::draw() {
    if (!isActive_) return;
    
    ofEnableAlphaBlending();
    ofFill();

    // Depth relation: body proximity scales ribbon width.
    float depthScale = 0.7f + 0.6f * averageProximity(interactionPoints_);
    
    // Draw ribbon meshes
    for (auto& entry : ribbons_) {
        entry.second.buildMesh(ribbonParams_, depthScale);
        entry.second.draw();
    }
    
    // Interaction point markers (debug)
    for (const auto& entry : interactionPoints_) {
        const auto& ip = entry.second;
        if (!ip.active) continue;
        ofSetColor(255, 0, 0, 120);
        ofDrawCircle(ip.position.x, ip.position.y, 6);
    }
}

// ============================================================================
// Parameter Access
// ============================================================================

std::vector<std::string> RibbonDanceScene::getParameterIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : parameters_) {
        ids.push_back(pair.first);
    }
    return ids;
}

std::vector<std::string> RibbonDanceScene::getInteractionPointIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : interactionPoints_) {
        ids.push_back(pair.first);
    }
    return ids;
}

std::shared_ptr<Parameter> RibbonDanceScene::getParameter(const std::string& paramId) {
    auto it = parameters_.find(paramId);
    if (it != parameters_.end()) {
        return it->second;
    }
    return nullptr;
}

bool RibbonDanceScene::setParameter(const std::string& paramId, float value) {
    // Handle both "ribbon.maxPoints" and "ribbonDance.ribbon.maxPoints" formats
    std::string key = paramId;
    if (key.find("ribbonDance.") == 0) {
        key = key.substr(std::string("ribbonDance.").length());
    }

    auto it = parameters_.find(key);
    if (it != parameters_.end()) {
        // Update stored parameter value
        it->second->currentValue = value;

        // Direct sync to RibbonParams struct (bidirectional, like PinturaFluorescenteScene)
        if (key == "ribbon.maxPoints") ribbonParams_.maxRibbonPoints = static_cast<int>(value);
        else if (key == "ribbon.maxWidth") ribbonParams_.ribbonMaxWidth = value;
        else if (key == "ribbon.minWidth") ribbonParams_.ribbonMinWidth = value;
        else if (key == "ribbon.gravity") ribbonParams_.ribbonGravity = value;
        else if (key == "ribbon.damping") ribbonParams_.ribbonDamping = value;
        else if (key == "ribbon.fadeSpeed") ribbonParams_.ribbonFadeSpeed = value;
        else if (key == "ribbon.life") ribbonParams_.ribbonLife = value;
        else if (key == "ribbon.velocityInfluence") ribbonParams_.ribbonVelocityInfluence = value;
        else if (key == "ribbon.emaFactor") ribbonParams_.velocitySmoothing = value;
        else if (key == "ribbon.waveAmount") ribbonParams_.ribbonWaveAmount = value;
        else if (key == "ribbon.waveSpeed") ribbonParams_.ribbonWaveSpeed = value;
        else if (key == "ribbon.maxDistanceJump") ribbonParams_.ribbonMaxDistanceJump = value;
        else if (key == "ribbon.maxActiveRibbons") ribbonParams_.maxActiveRibbons = static_cast<int>(value);
        else if (key == "ribbon.maxJointVelocity") ribbonParams_.maxJointVelocity = value;
        else if (key == "ribbon.colorCycles") ribbonParams_.ribbonColorCycles = static_cast<int>(value);
        else if (key == "ribbon.orbitRadius") ribbonParams_.orbitRadius = value;
        else if (key == "ribbon.orbitStrength") ribbonParams_.orbitStrength = value;
        else if (key == "particle.maxCount") ribbonParams_.particleMaxCount = static_cast<int>(value);
        else if (key == "particle.emissionThreshold") ribbonParams_.particleEmissionThreshold = value;
        else if (key == "particle.perBurst") ribbonParams_.particlesPerBurst = static_cast<int>(value);
        else if (key == "particle.velocityInfluence") ribbonParams_.particleVelocityInfluence = value;
        else if (key == "particle.gravity") ribbonParams_.particleGravity = value;
        else if (key == "particle.damping") ribbonParams_.particleDamping = value;
        else if (key == "particle.lifeMin") ribbonParams_.particleLifeMin = value;
        else if (key == "particle.lifeMax") ribbonParams_.particleLifeMax = value;
        else if (key == "particle.sizeMin") ribbonParams_.particleSizeMin = value;
        else if (key == "particle.sizeMax") ribbonParams_.particleSizeMax = value;

        return true;
    }
    return false;
}

bool RibbonDanceScene::setParameter(const std::string& paramId, const glm::vec2& value) {
    // Ignore interaction points when scene is not active — prevents crashes
    // from KinectMappingEngine dispatching to an inactive scene
    if (!isActive_) return true;
    
    // Populate interaction point map — normalized coords converted by setTarget
    auto& pt = interactionPoints_[paramId];
    pt.sourceName = paramId;
    pt.decayRate = 0.08f;  // ~12.5s decay — tolerates temporary tracking dropouts
    pt.setTarget(value, 1.0f, ofGetWidth(), ofGetHeight());
    return true;
}

bool RibbonDanceScene::setParameter(const std::string& paramId, const glm::vec3& value) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[paramId];
    pt.sourceName = paramId;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(value.x, value.y), 1.0f, ofGetWidth(), ofGetHeight(), value.z);
    return true;
}

// ============================================================================
// Suggested Mappings — stub (empty for now)
// ============================================================================

std::vector<SuggestedMapping> RibbonDanceScene::getSuggestedMappings() const {
    return {};
}

// ============================================================================
// Preset
// ============================================================================

Preset RibbonDanceScene::getDefaultPreset() const {
    Preset preset;
    preset.id = "default";
    preset.name = "Default";
    preset.description = "Valores por defecto de la escena Ribbon Dance";
    preset.sceneId = getId();
    preset.isDefault = true;

    for (const auto& pair : parameters_) {
        Parameter p = *pair.second;
        p.currentValue = p.defaultValue;
        preset.parameters.push_back(p);
    }

    return preset;
}

// ============================================================================
// Scene Metadata
// ============================================================================

SceneMetadata RibbonDanceScene::getSceneMetadata() const {
    SceneMetadata metadata;
    metadata.id = getId();
    metadata.name = getName();
    metadata.description = "Cintas fluorescentes que responden a puntos de interaccion con velocidad modulada";
    metadata.author = "Proyecto Danza";
    metadata.version = "1.0.0";
    metadata.tags = {"cintas", "fluorescente", "danza", "ribbon"};
    metadata.category = "visual";
    metadata.hasInternalState = true;
    metadata.isTimeBased = false;
    metadata.estimatedDuration = 0.0f;
    metadata.presetPath = "";
    return metadata;
}

// ============================================================================
// Timing & Synchronization — stubs
// ============================================================================

void RibbonDanceScene::onBeat(int beatNumber) {
    (void)beatNumber;
}

void RibbonDanceScene::onBar(int barNumber) {
    (void)barNumber;
}

// ============================================================================
// Validation
// ============================================================================

bool RibbonDanceScene::isValid() const {
    return isValid_;
}

std::string RibbonDanceScene::getInvalidReason() const {
    return invalidReason_;
}

// ============================================================================
// Resource Management — stubs
// ============================================================================

void RibbonDanceScene::unloadResources() {
    interactionPoints_.clear();
}

void RibbonDanceScene::reloadResources() {
    setup();
}

// ============================================================================
// Input Handlers — stubs (all keyboard handled by ofApp globally)
// ============================================================================

void RibbonDanceScene::keyPressed(int key) {
    (void)key;
}

void RibbonDanceScene::keyReleased(int key) {
    (void)key;
}

void RibbonDanceScene::mousePressed(int x, int y, int button) {
    (void)x;
    (void)y;
    (void)button;
}

void RibbonDanceScene::mouseReleased(int x, int y, int button) {
    (void)x;
    (void)y;
    (void)button;
}

void RibbonDanceScene::mouseDragged(int x, int y, int button) {
    (void)x;
    (void)y;
    (void)button;
}

void RibbonDanceScene::mouseMoved(int x, int y) {
    (void)x;
    (void)y;
}

void RibbonDanceScene::windowResized(int w, int h) {
    (void)w;
    (void)h;
    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) {
        bg = param->currentValue;
    }
    ofBackground(static_cast<int>(bg));
}

// ============================================================================
// Factory Function
// ============================================================================

ScenePtr CreateRibbonDanceScene() {
    return std::make_shared<RibbonDanceScene>();
}

} // namespace modular
