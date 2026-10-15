#include "scenes/PinturaFluorescenteScene.h"
#include "registry/ParameterRegistry.h"
#include <cmath>
#include <algorithm>

namespace modular {

// ============================================================================
// Particle Implementation
// ============================================================================

void Particle::update(float k, float A, float w, float q, float beta, float S, 
                       float ws, float qs, glm::vec2 center, float dt, 
                       bool attractorActive, glm::vec2 attractorPoint,
                       bool flockMode, float flockTransition, float flockTransitionMax,
                       float flockSpeed, float flockWaveX, float flockWaveY,
                       float flockCohesion, float flockLife,
                       Particle* allParticles, size_t particleCount) {
    glm::vec2 r = pos - center;
    float d = glm::length(r) + 0.0001f;
    glm::vec2 n = r / d;
    glm::vec2 tangent = glm::vec2(-n.y, n.x);
    
    float t = ofGetElapsedTimef();
    
    // Normal physics forces (disabled in flock mode)
    glm::vec2 F_center = flockMode ? glm::vec2(0) : -k * r;
    
    float waveArg = w * t - q * d + phase;
    glm::vec2 F_wave = flockMode ? glm::vec2(0) : A * std::sin(waveArg) * n;
    
    float swirlArg = ws * t - qs * d + phase;
    
    glm::vec2 flockForce(0, 0);
    glm::vec2 targetVel(0, 0);
    
    if (flockMode) {
        float baseSpeed = flockSpeed;
        float speedVar = size;
        
        float waveX = std::sin(t * 0.3f + phase) * flockWaveX;
        float waveY = std::cos(t * 0.2f + phase * 0.5f) * flockWaveY * 0.5f;
        
        targetVel = glm::vec2(waveX, waveY) * speedVar * 0.08f;
        targetVel.x += baseSpeed * speedVar * 0.4f;
        targetVel.y += 0.1f;
        
        float margin = 60.0f;
        float bForce = 0.03f;
        if (pos.x < margin) targetVel.x += (margin - pos.x) * bForce;
        if (pos.x > ofGetWidth() - margin) targetVel.x -= (pos.x - (ofGetWidth() - margin)) * bForce;
        if (pos.y < margin) targetVel.y += (margin - pos.y) * bForce;
        if (pos.y > ofGetHeight() - margin) targetVel.y -= (pos.y - (ofGetHeight() - margin)) * bForce;
        
        glm::vec2 screenCenter(ofGetWidth()/2, ofGetHeight()/2);
        glm::vec2 toCenter = screenCenter - pos;
        targetVel += toCenter * flockCohesion;
        
        float transitionProgress = std::min(1.0f, flockTransition / flockTransitionMax);
        float logT = std::log10(1.0f + 9.0f * transitionProgress);
        
        float blendOriginal = 1.0f - logT;
        float blendFlock = logT;
        
        glm::vec2 F_original = -k * r + 
                              A * std::sin(waveArg) * n + 
                              S * std::cos(swirlArg) * tangent;
        
        flockForce = targetVel * blendFlock + F_original * blendOriginal;
    }
    
    float dampingFactor = flockMode ? 0.01f : beta;
    glm::vec2 F_damping = -dampingFactor * vel;
    
    glm::vec2 F_swirl = flockMode ? glm::vec2(0) : S * std::cos(swirlArg) * tangent;
    
    glm::vec2 F_attractor(0, 0);
    if (attractorActive && !flockMode) {
        glm::vec2 r_attract = attractorPoint - pos;
        float d_attract = glm::length(r_attract) + 0.0001f;
        F_attractor = -ATTRACTOR_STRENGTH * r_attract;
    }
    
    glm::vec2 F_flock(0, 0);
    if (flockMode && allParticles && particleCount > 0) {
        glm::vec2 sep(0, 0), align(0, 0), coh(0, 0);
        int sepCount = 0, alignCount = 0, cohCount = 0;
        
        for (size_t i = 0; i < particleCount; i++) {
            if (&allParticles[i] == this || !allParticles[i].active) continue;
            
            glm::vec2 diff = pos - allParticles[i].pos;
            float dist = glm::length(diff);
            
            if (dist < FLOCK_RADIUS && dist > 0) {
                sep += glm::normalize(diff) / dist;
                sepCount++;
                
                align += allParticles[i].vel;
                alignCount++;
                
                coh += allParticles[i].pos;
                cohCount++;
            }
        }
        
        if (sepCount > 0) {
            sep /= sepCount;
            F_flock += FLOCK_SEPARATION * sep;
        }
        if (alignCount > 0) {
            align /= alignCount;
            F_flock += FLOCK_ALIGNMENT * (align - vel);
        }
        if (cohCount > 0) {
            coh /= cohCount;
            F_flock += FLOCK_COHESION * (coh - pos);
        }
        
        F_flock += glm::vec2(ofRandom(-0.1f, 0.1f), ofRandom(-0.1f, 0.1f));
        
        float margin = 100.0f;
        float boundaryForce = 0.03f;
        if (pos.x < margin) F_flock.x += (margin - pos.x) * boundaryForce;
        if (pos.x > ofGetWidth() - margin) F_flock.x -= (pos.x - (ofGetWidth() - margin)) * boundaryForce;
        if (pos.y < margin) F_flock.y += (margin - pos.y) * boundaryForce;
        if (pos.y > ofGetHeight() - margin) F_flock.y -= (pos.y - (ofGetHeight() - margin)) * boundaryForce;
    }
    
    glm::vec2 F = F_center + F_wave + F_damping + F_swirl + F_attractor + flockForce;
    
    glm::vec2 acc = F;
    vel += acc * dt;
    pos += vel * dt;
    
    float decay = flockMode ? flockLife : 1.5f;
    life -= decay;
    alpha = ofMap(life, 0, maxLife, 0, 255);
}

void Particle::draw(float scale) {
    ofSetColor(color.r * 255, color.g * 255, color.b * 255, alpha);
    ofDrawCircle(pos.x, pos.y, size * scale * (life / maxLife));
}

// ============================================================================
// ParticlePool Implementation
// ============================================================================

ParticlePool::ParticlePool(size_t cap)
    : particles(nullptr), capacity_(cap), activeCount(0), saturationWarningIssued_(false) {
    if (capacity_ == 0) capacity_ = 1;
    particles = new Particle[capacity_];
    for (size_t i = 0; i < capacity_; i++) {
        particles[i].active = false;
    }
}

ParticlePool::~ParticlePool() {
    delete[] particles;
    particles = nullptr;
}

Particle* ParticlePool::acquire(glm::vec2 pos, glm::vec2 vel, ofFloatColor col, float size) {
    if (activeCount >= capacity_) {
        if (!saturationWarningIssued_) {
            ofLogWarning("ParticlePool") << "Pool saturated at " << capacity_
                                         << " active particles. Spawns dropped until released.";
            saturationWarningIssued_ = true;
        }
        return nullptr;
    }

    for (size_t i = 0; i < capacity_; i++) {
        if (!particles[i].active) {
            particles[i].init(pos, vel, col, size);
            activeCount++;
            return &particles[i];
        }
    }
    return nullptr;
}

void ParticlePool::release(Particle* p) {
    if (p && p->active) {
        p->active = false;
        activeCount--;
        if (saturationWarningIssued_ && activeCount < capacity_) {
            saturationWarningIssued_ = false;
        }
    }
}

size_t ParticlePool::getActiveCount() const {
    return activeCount;
}

void ParticlePool::clear() {
    for (size_t i = 0; i < capacity_; i++) {
        particles[i].active = false;
    }
    activeCount = 0;
    saturationWarningIssued_ = false;
}

void ParticlePool::resize(size_t newCapacity) {
    if (newCapacity == capacity_) return;
    if (newCapacity == 0) newCapacity = 1;

    Particle* newParticles = new Particle[newCapacity];
    for (size_t i = 0; i < newCapacity; i++) {
        newParticles[i].active = false;
    }

    // Copy active particles from old array (up to new capacity)
    size_t copyCount = 0;
    for (size_t i = 0; i < capacity_ && copyCount < newCapacity; i++) {
        if (particles[i].active) {
            newParticles[copyCount] = particles[i];
            copyCount++;
        }
    }

    delete[] particles;
    particles = newParticles;
    capacity_ = newCapacity;
    activeCount = copyCount;
    saturationWarningIssued_ = false;

    ofLogNotice("ParticlePool") << "Resized pool to " << newCapacity
                                << " (kept " << copyCount << " active particles)";
}

Particle& ParticlePool::getParticleRef(size_t index) {
    return particles[index];
}

Particle* ParticlePool::getParticleAt(size_t index) {
    if (index < capacity_ && particles[index].active) {
        return &particles[index];
    }
    return nullptr;
}

Particle* ParticlePool::getActiveParticle(size_t idx) const {
    size_t count = 0;
    for (size_t i = 0; i < capacity_; i++) {
        if (particles[i].active) {
            if (count == idx) {
                return const_cast<Particle*>(&particles[i]);
            }
            count++;
        }
    }
    return nullptr;
}

Particle* ParticlePool::getParticlesArray() {
    return particles;
}

size_t ParticlePool::getCapacity() const {
    return capacity_;
}

// ============================================================================
// PinturaFluorescenteScene Implementation
// ============================================================================

PinturaFluorescenteScene::PinturaFluorescenteScene()
    : currentPalette_(0)
    , clearEachFrame_(true)
    , prevMouse_(0, 0)
    , mousePressedFlag_(false)
    , showParams_(false)
    , selectedParam_(0)
    , attractorPoint_(0, 0)
    , attractorActive_(false)
    , flockMode_(false)
    , flockTransitionTime_(0.0f)
    , lastSpawnedMain_(0)
    , lastSpawnedTrail_(0)
    , isActive_(false)
    , isPaused_(false)
    , isValid_(true)
    , invalidReason_("")
    , maxParticles_(5000)
    , spawnRate_(DEFAULT_SPAWN_RATE)
    , trailEnabled_(true)
    , parametersRegistered_(false) {
}

PinturaFluorescenteScene::~PinturaFluorescenteScene() {
    unregisterParameters();
}

ofFloatColor PinturaFluorescenteScene::getColorFromPalette(int paletteIndex, float variation) {
    std::vector<ofFloatColor> palette = palettes_[paletteIndex % palettes_.size()];
    int baseIndex = (int)ofRandom(palette.size());
    ofFloatColor col = palette[baseIndex];
    col += ofFloatColor(variation * ofRandom(-0.15f, 0.15f), 
                        variation * ofRandom(-0.15f, 0.15f), 
                        variation * ofRandom(-0.15f, 0.15f));
    return col;
}

void PinturaFluorescenteScene::setupPalettes() {
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

void PinturaFluorescenteScene::initializeParameterDefinitions() {
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

    addParam("k", "atraccion", 0.0f, 2.5f, params_.k, 0.05f,
             "Fuerza que jala las particulas al centro");
    addParam("A", "amplitud", 0.0f, 100.0f, params_.A, 2.0f,
             "Que tan grande es la onda");
    addParam("w", "frecuencia", 0.1f, 5.0f, params_.w, 0.05f,
             "Que tan rapido late la onda");
    addParam("q", "densidad", 0.0f, 0.5f, params_.q, 0.005f,
             "Que tan apretada esta la onda");
    addParam("beta", "friccion", 0.9f, 1.0f, params_.beta, 0.002f,
             "Que tanto frenan las particulas (mas = mas lento)");
    addParam("S", "remolino", 0.0f, 50.0f, params_.S, 2.0f,
             "Fuerza del giro en espiral");
    addParam("ws", "velRemolino", 0.0f, 5.0f, params_.ws, 0.05f,
             "Que tan rapido gira el remolino");
    addParam("qs", "tamRemolino", 0.0f, 0.5f, params_.qs, 0.005f,
             "Que tan grande es el remolino");
    addParam("flockSpeed", "velocidad", 0.1f, 3.0f, params_.flockSpeed, 0.1f,
             "Velocidad base de los pajaros");
    addParam("flockWaveX", "ondaX", 1.0f, 30.0f, params_.flockWaveX, 1.0f,
             "Amplitud de onda horizontal");
    addParam("flockWaveY", "ondaY", 1.0f, 30.0f, params_.flockWaveY, 1.0f,
             "Amplitud de onda vertical");
    addParam("flockCohesion", "cohesion", 0.0f, 0.01f, params_.flockCohesion, 0.0005f,
             "Fuerza de cohesion al centro");
    addParam("flockLife", "duracion", 0.01f, 0.5f, params_.flockLife, 0.01f,
             "Duracion de los pajaros (mas bajo = mas tiempo)");
    addParam("flockTransition", "transicion", 0.5f, 3.0f, params_.flockTransition, 0.1f,
             "Tiempo de transicion suave");

    // Registered particle pool params (replace constexpr limits)
    addParam("maxParticles", "Max Particulas", 500.0f, 5000.0f, 5000.0f, 100.0f,
             "Capacidad maxima de particulas (requiere resize del pool)",
             ParameterType::INT);
    addParam("spawnRate", "Tasa Spawn", 1.0f, 20.0f, 6.0f, 1.0f,
             "Max particulas por punto de interaccion por frame",
             ParameterType::INT);
    addParam("trailEnabled", "Estelas", 0.0f, 1.0f, 1.0f, 1.0f,
             "Activar/desactivar estelas de particulas",
             ParameterType::BOOL);
}

void PinturaFluorescenteScene::registerParameters() {
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
            ofLogWarning("PinturaFluorescenteScene")
                << "Failed to register parameter " << definition.id;
        }
    }

    parametersRegistered_ = true;
}

void PinturaFluorescenteScene::unregisterParameters() {
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

void PinturaFluorescenteScene::setup() {
    ofEnableAlphaBlending();
    
    setupPalettes();
    initializeParameterDefinitions();
    registerParameters();  // Register now so load can find them
    
    clearEachFrame_ = true;
    ofSetBackgroundAuto(clearEachFrame_);
    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) {
        bg = param->currentValue;
    }
    ofBackground(static_cast<int>(bg));
    
    prevMouse_ = glm::vec2(0, 0);
    mousePressedFlag_ = false;
    
    attractorPoint_ = glm::vec2(0, 0);
    attractorActive_ = false;
    
    flockMode_ = false;
    flockTransitionTime_ = 0.0f;
    
    showParams_ = false;
    selectedParam_ = 0;

    lastSpawnedMain_ = 0;
    lastSpawnedTrail_ = 0;
    
    // Create particle pools
    particlePool_ = std::make_unique<ParticlePool>();
    trailPool_ = std::make_unique<ParticlePool>();
    
    // Prepare parameter definitions (registration happens on activate)
    initializeParameterDefinitions();
}

void PinturaFluorescenteScene::update(float deltaTime) {
    if (!isActive_ || isPaused_) {
        return;
    }

    lastSpawnedMain_ = 0;
    lastSpawnedTrail_ = 0;

    // Check if max particles changed — resize pools
    if (maxParticles_ != static_cast<int>(particlePool_->getCapacity())) {
        applyPoolResize();
    }

    ofSetBackgroundAuto(clearEachFrame_);
    
    // Update interaction points: apply decay for points no longer receiving data
    for (auto& entry : interactionPoints_) {
        entry.second.update(deltaTime);
    }
    
    glm::vec2 screenCenter(ofGetWidth() / 2.0f, ofGetHeight() / 2.0f);
    
    // Compute physics center as average of all active interaction points
    glm::vec2 center(0.0f);
    int activeCount = 0;
    glm::vec2 spawnPos = screenCenter;  // fallback spawn position
    for (auto& entry : interactionPoints_) {
        if (entry.second.active) {
            center += entry.second.position;
            if (activeCount == 0) {
                spawnPos = entry.second.position;  // first active point for spawn
            }
            activeCount++;
        }
    }
    if (activeCount > 0) {
        center /= static_cast<float>(activeCount);
    } else {
        center = screenCenter;  // fallback: center of screen
    }
    
    float dt = ofGetLastFrameTime() * TIME_SCALE;
    
    // Spawn new particles using pool — one batch per active interaction point
    bool shouldSpawn = (activeCount > 0) && !flockMode_;
    if (shouldSpawn && particlePool_->getActiveCount() < particlePool_->getCapacity()) {
        int perPoint = std::max(1, spawnRate_ / std::max(1, activeCount));
        
        for (auto& entry : interactionPoints_) {
            if (!entry.second.active) continue;
            if (particlePool_->getActiveCount() >= particlePool_->getCapacity()) break;
            
            const std::string& pid = entry.first;
            glm::vec2 pointPos = entry.second.position;
            glm::vec2 pointVel = entry.second.velocity * 0.05f;
            
            // Spawn batch for this specific interaction point
            for (int i = 0; i < perPoint; i++) {
                if (particlePool_->getActiveCount() >= particlePool_->getCapacity()) break;
                
                ofFloatColor col = getColorFromPalette(currentPalette_, 1.0f);
                glm::vec2 offset(ofRandom(-SPAWN_OFFSET_RANGE, SPAWN_OFFSET_RANGE), 
                                ofRandom(-SPAWN_OFFSET_RANGE, SPAWN_OFFSET_RANGE));
                glm::vec2 vel(pointVel.x + ofRandom(-0.5f, 0.5f), 
                             pointVel.y + ofRandom(-0.5f, 0.5f));
                float size = ofRandom(SPAWN_SIZE_MIN, SPAWN_SIZE_MAX);
                
                Particle* spawned = particlePool_->acquire(pointPos + offset, vel, col, size);
                if (!spawned) break;
                spawned->pointId = pid;
                lastSpawnedMain_++;
                
                // Trail particles (inherit same pointId, only when enabled)
                if (trailEnabled_) {
                for (int j = 0; j < 2; j++) {
                    if (trailPool_->getActiveCount() >= trailPool_->getCapacity()) break;
                    
                    ofFloatColor trailCol = col;
                    trailCol *= TRAIL_ALPHA_FACTOR;
                    glm::vec2 trailPos = pointPos + offset + 
                        glm::vec2(ofRandom(-TRAIL_OFFSET_RANGE, TRAIL_OFFSET_RANGE), 
                                  ofRandom(-TRAIL_OFFSET_RANGE, TRAIL_OFFSET_RANGE));
                    glm::vec2 trailVel(ofRandom(-0.3f, 0.3f), ofRandom(-0.3f, 0.3f));
                    Particle* trailSpawn = trailPool_->acquire(trailPos, trailVel, trailCol, size * TRAIL_SIZE_FACTOR);
                    if (trailSpawn) {
                        trailSpawn->pointId = pid;
                        lastSpawnedTrail_++;
                    } else {
                        break;
                    }
                }
                }  // if (trailEnabled_)
            }
        }
    }
    
    // Resolve per-particle center: each particle orbits its assigned interaction point.
    // Falls back to global average center if pointId is empty or point is inactive.
    auto resolveCenter = [&](const Particle& p) -> glm::vec2 {
        if (!p.pointId.empty()) {
            auto it = interactionPoints_.find(p.pointId);
            if (it != interactionPoints_.end() && it->second.active) {
                return it->second.position;
            }
        }
        return center;
    };

    // Update all active trail particles
    for (size_t i = 0; i < trailPool_->getCapacity(); i++) {
        Particle& p = trailPool_->getParticleRef(i);
        if (p.active) {
            glm::vec2 particleCenter = resolveCenter(p);
            p.update(params_.k, params_.A, params_.w, params_.q, params_.beta, params_.S,
                     params_.ws, params_.qs, particleCenter, dt, attractorActive_, attractorPoint_,
                     flockMode_, flockTransitionTime_, params_.flockTransition,
                     params_.flockSpeed, params_.flockWaveX, params_.flockWaveY,
                     params_.flockCohesion, params_.flockLife,
                     particlePool_->getParticlesArray(), particlePool_->getCapacity());
            if (p.isDead()) {
                trailPool_->release(&p);
            }
        }
    }
    
    // Update all active main particles
    for (size_t i = 0; i < particlePool_->getCapacity(); i++) {
        Particle& p = particlePool_->getParticleRef(i);
        if (p.active) {
            glm::vec2 particleCenter = resolveCenter(p);
            p.update(params_.k, params_.A, params_.w, params_.q, params_.beta, params_.S,
                     params_.ws, params_.qs, particleCenter, dt, attractorActive_, attractorPoint_,
                     flockMode_, flockTransitionTime_, params_.flockTransition,
                     params_.flockSpeed, params_.flockWaveX, params_.flockWaveY,
                     params_.flockCohesion, params_.flockLife,
                     particlePool_->getParticlesArray(), particlePool_->getCapacity());
            if (p.isDead()) {
                particlePool_->release(&p);
            }
        }
    }
    
    // Update flock transition time
    if (flockMode_ && flockTransitionTime_ < params_.flockTransition) {
        flockTransitionTime_ += ofGetLastFrameTime();
    }
}

void PinturaFluorescenteScene::applyPoolResize() {
    size_t newSize = static_cast<size_t>(maxParticles_);
    if (newSize < 1) newSize = 1;

    if (particlePool_) {
        particlePool_->resize(newSize);
    }
    if (trailPool_) {
        trailPool_->resize(newSize);
    }
}

void PinturaFluorescenteScene::draw() {
    if (!isActive_) {
        return;
    }
    
    ofFill();

    // Depth relation: body proximity scales particle size.
    float depthScale = 0.7f + 0.6f * averageProximity(interactionPoints_);
    
    // Draw trail particles (only when enabled)
    if (trailEnabled_) {
        for (size_t i = 0; i < trailPool_->getCapacity(); i++) {
            Particle& p = trailPool_->getParticleRef(i);
            if (p.active) {
                p.draw(depthScale);
            }
        }
    }
    
    // Draw main particles
    for (size_t i = 0; i < particlePool_->getCapacity(); i++) {
        Particle& p = particlePool_->getParticleRef(i);
        if (p.active) {
            p.draw();
        }
    }
    
    // Draw debug markers for active interaction points
    if (debugMarkers_) {
        // *** ALWAYS-DRAWN indicator to confirm debug mode is active ***
        ofSetColor(255, 0, 0, 255);
        ofFill();
        ofDrawCircle(40, 40, 15);
        ofSetColor(0, 0, 0, 230);
        ofDrawBitmapString("DEBUG ON (J toggles)", 65, 35);
        ofSetColor(255, 255, 255, 240);
        ofDrawBitmapString("DEBUG ON (J toggles)", 64, 34);
        
        int activeCount = 0;
        int totalCount = 0;
        float now = ofGetElapsedTimef();
        
        for (const auto& entry : interactionPoints_) {
            const auto& pt = entry.second;
            totalCount++;
            
            if (!pt.active) {
                // Inactive point — draw a dim red X so we know it exists
                ofSetColor(255, 60, 60, 160);
                ofNoFill();
                ofSetLineWidth(2.5f);
                ofDrawLine(pt.position.x - 12, pt.position.y - 12, pt.position.x + 12, pt.position.y + 12);
                ofDrawLine(pt.position.x + 12, pt.position.y - 12, pt.position.x - 12, pt.position.y + 12);
                ofFill();
                continue;
            }
            activeCount++;
            
            float confPct = pt.confidence * 100.0f;
            float age = now - pt.lastUpdateTime;
            
            // === NEON RED circle — impossible to miss ===
            float radius = 35.0f;
            
            // Glow (outer soft circle)
            ofSetColor(255, 30, 30, 60);
            ofFill();
            ofDrawCircle(pt.position.x, pt.position.y, radius + 10);
            
            // Main circle
            ofSetColor(255, 30, 30, 220);
            ofFill();
            ofDrawCircle(pt.position.x, pt.position.y, radius);
            
            // White border
            ofSetColor(255, 255, 255, 230);
            ofNoFill();
            ofSetLineWidth(4.0f);
            ofDrawCircle(pt.position.x, pt.position.y, radius);
            
            // Center dot
            ofSetColor(255, 255, 255, 255);
            ofFill();
            ofDrawCircle(pt.position.x, pt.position.y, 7);
            
            // === Info panel with dark background ===
            std::string info = pt.sourceName 
                + " | px(" + ofToString(pt.position.x, 0) + "," + ofToString(pt.position.y, 0) + ")"
                + " | c" + ofToString(confPct, 0) + "%"
                + " | " + ofToString(age, 2) + "s";
            
            float textW = info.length() * 8.0f + 12;  // rough width estimate
            ofSetColor(0, 0, 0, 210);
            ofFill();
            ofDrawRectangle(pt.position.x + 38, pt.position.y - 24, textW, 26);
            
            ofSetColor(255, 255, 255, 240);
            ofDrawBitmapString(info, pt.position.x + 44, pt.position.y - 6);
        }
        ofFill();
        ofSetLineWidth(1.0f);
        
        // === Status bar at bottom ===
        std::string status = "IPs: " + std::to_string(activeCount) + "/" + std::to_string(totalCount) + " active";
        ofSetColor(0, 0, 0, 210);
        ofDrawRectangle(5, ofGetHeight() - 28, 300, 24);
        ofSetColor(255, 100, 100, 250);
        ofDrawBitmapString(status, 10, ofGetHeight() - 9);
    }
    
    if (showParams_) {
        std::string modeNames[5] = {"Rosa", "Cian", "Verde", "Naranja", "Amarillo"};
        
        int numParams = parameters_.size();
        int neededHeight = 150 + numParams * 28;
        neededHeight = std::max(neededHeight, 400);
        
        ofSetColor(0, 0, 0, 230);
        ofDrawRectangle(10, 10, 450, neededHeight);
        
        ofSetColor(255);
        ofDrawBitmapString("PARAMETROS (tecla 1 para cerrar)", 20, 30);
        ofDrawBitmapString("=============================================", 20, 45);
        
        std::string modoStr = clearEachFrame_ ? "LIMPIO (borra cada frame)" : "TRAZO (acumula dibujo)";
        ofDrawBitmapString("Modo [M]: " + modoStr, 20, 65);
        ofDrawBitmapString("Paleta [P]: " + ofToString(currentPalette_ + 1) + "/5 - " + modeNames[currentPalette_], 20, 80);
        
        if (selectedParam_ >= 0 && selectedParam_ < numParams) {
            auto it = parameters_.begin();
            std::advance(it, selectedParam_);
            if (it != parameters_.end()) {
                ofSetColor(100, 200, 255);
                ofDrawBitmapString(it->second->description, 20, 100);
                ofSetColor(180);
                ofDrawBitmapString("[AR/AB] seleccionar  [IZQ/DER] cambiar  [B] togglear bandada", 20, 118);
            }
        }
        
        int startY = 140;
        int idx = 0;
        for (auto& pair : parameters_) {
            float y = startY + idx * 28;
            
            if (idx == selectedParam_) {
                ofSetColor(255, 255, 0);
                ofDrawRectangle(15, y - 11, 420, 24);
            }
            
            ofSetColor(idx == selectedParam_ ? 0 : 200, idx == selectedParam_ ? 0 : 200, idx == selectedParam_ ? 0 : 200);
            ofDrawBitmapString(pair.second->label + " (" + pair.first + ")", 20, y);
            ofDrawBitmapString(": " + ofToString(pair.second->currentValue, 3), 230, y);
            
            ofSetColor(120);
            ofDrawBitmapString("(min:" + ofToString(pair.second->min, 1) + " max:" + ofToString(pair.second->max, 1) + ")", 320, y);
            
            idx++;
        }
        
        ofSetColor(150);
        ofDrawBitmapString("[M] modo  [P] paleta  [C] limpiar  [S] guardar  [B] bandada", 20, neededHeight - 15);
    }
}

void PinturaFluorescenteScene::activate() {
    isActive_ = true;
    isPaused_ = false;
    
    // Reset runtime state so scene starts fresh
    particlePool_->clear();
    trailPool_->clear();
    interactionPoints_.clear();
    attractorActive_ = false;
    attractorPoint_ = glm::vec2(0, 0);
    flockMode_ = false;
    flockTransitionTime_ = 0.0f;
    lastSpawnedMain_ = 0;
    lastSpawnedTrail_ = 0;
    
    ofSetWindowTitle("Pintura con Particulas Fluorescentes");
    ofSetFrameRate(60);
    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) {
        bg = param->currentValue;
    }
    ofBackground(static_cast<int>(bg));
    
    // Leer paleta global
    if (auto paletteParam = registry.getParam("global.currentPalette")) {
        currentPalette_ = static_cast<int>(paletteParam->currentValue);
    }
    
    // Registrar parámetros de la escena en el registry
    registerParameters();
    
    // Leer valores guardados del registry
    for (const auto& entry : parameters_) {
        const std::string& key = entry.first;
        if (auto param = registry.getParam(key)) {
            setParameter(key, param->currentValue);
        }
    }
}

void PinturaFluorescenteScene::deactivate() {
    isActive_ = false;
    
    // Desregistrar parámetros de la escena del registry
    unregisterParameters();
    
    if (particlePool_) particlePool_->clear();
    if (trailPool_) trailPool_->clear();
}

void PinturaFluorescenteScene::pause() {
    isPaused_ = true;
}

void PinturaFluorescenteScene::resume() {
    isPaused_ = false;
}

std::string PinturaFluorescenteScene::getId() const {
    return "pinturaFluorescente";
}

std::string PinturaFluorescenteScene::getName() const {
    return "Pintura con Particulas Fluorescentes";
}

std::vector<std::string> PinturaFluorescenteScene::getParameterIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : parameters_) {
        ids.push_back(pair.first);
    }
    return ids;
}

std::vector<std::string> PinturaFluorescenteScene::getInteractionPointIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : interactionPoints_) {
        ids.push_back(pair.first);
    }
    return ids;
}

void PinturaFluorescenteScene::clearInteractionPoints() {
    interactionPoints_.clear();
    particlePool_->clear();
    trailPool_->clear();
}

std::shared_ptr<Parameter> PinturaFluorescenteScene::getParameter(const std::string& paramId) {
    auto it = parameters_.find(paramId);
    if (it != parameters_.end()) {
        return it->second;
    }
    return nullptr;
}

bool PinturaFluorescenteScene::setParameter(const std::string& paramId, float value) {
    // Handle both "A" and "pinturaFluorescente.A" formats
    std::string key = paramId;
    if (key.find("pinturaFluorescente.") == 0) {
        key = key.substr(std::string("pinturaFluorescente.").length());
    }
    
    auto it = parameters_.find(key);
    if (it != parameters_.end()) {
        // Update local param - use key (which is the clean ID without prefix)
        if (key == "k") params_.k = value;
        else if (key == "A") params_.A = value;
        else if (key == "w") params_.w = value;
        else if (key == "q") params_.q = value;
        else if (key == "beta") params_.beta = value;
        else if (key == "S") params_.S = value;
        else if (key == "ws") params_.ws = value;
        else if (key == "qs") params_.qs = value;
        else if (key == "flockSpeed") params_.flockSpeed = value;
        else if (key == "flockWaveX") params_.flockWaveX = value;
        else if (key == "flockWaveY") params_.flockWaveY = value;
        else if (key == "flockCohesion") params_.flockCohesion = value;
        else if (key == "flockLife") params_.flockLife = value;
        else if (key == "flockTransition") params_.flockTransition = value;
        else if (key == "maxParticles") maxParticles_ = static_cast<int>(value);
        else if (key == "spawnRate") spawnRate_ = static_cast<int>(value);
        else if (key == "trailEnabled") trailEnabled_ = (value > 0.5f);
        
        // Update stored parameter value
        it->second->currentValue = value;
        return true;
    }
    return false;
}

bool PinturaFluorescenteScene::setParameter(const std::string& paramId, const glm::vec2& value) {
    // Populate interaction point map — normalized coords converted by setTarget
    auto& pt = interactionPoints_[paramId];
    bool wasActive = pt.active;
    pt.sourceName = paramId;
    pt.decayRate = 0.08f;  // ~12.5s decay — tolerates temporary tracking dropouts
    pt.setTarget(value, 1.0f, ofGetWidth(), ofGetHeight());
    return true;
}

bool PinturaFluorescenteScene::setParameter(const std::string& paramId, const glm::vec3& value) {
    auto& pt = interactionPoints_[paramId];
    pt.sourceName = paramId;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(value.x, value.y), 1.0f, ofGetWidth(), ofGetHeight(), value.z);
    return true;
}

std::vector<SuggestedMapping> PinturaFluorescenteScene::getSuggestedMappings() const {
    std::vector<SuggestedMapping> mappings;
    
    SuggestedMapping m;
    
    m = {};
    m.parameterId = "k";
    m.sourceType = "osc";
    m.channel = "/k";
    m.control = "float";
    m.minInput = 0.0f;
    m.maxInput = 1.0f;
    m.description = "Fuerza de atraccion al centro";
    mappings.push_back(m);
    
    m = {};
    m.parameterId = "A";
    m.sourceType = "osc";
    m.channel = "/amplitud";
    m.control = "float";
    m.minInput = 0.0f;
    m.maxInput = 1.0f;
    m.description = "Amplitud de onda";
    mappings.push_back(m);
    
    m = {};
    m.parameterId = "w";
    m.sourceType = "midi";
    m.channel = "ch1";
    m.control = "cc1";
    m.minInput = 0.0f;
    m.maxInput = 1.0f;
    m.description = "Frecuencia de onda";
    mappings.push_back(m);
    
    m = {};
    m.parameterId = "flockSpeed";
    m.sourceType = "midi";
    m.channel = "ch1";
    m.control = "cc2";
    m.minInput = 0.0f;
    m.maxInput = 1.0f;
    m.description = "Velocidad de bandada";
    mappings.push_back(m);
    
    return mappings;
}

Preset PinturaFluorescenteScene::getDefaultPreset() const {
    Preset preset;
    preset.id = "default";
    preset.name = "Default";
    preset.description = "Valores por defecto de la escena";
    preset.sceneId = getId();
    preset.isDefault = true;
    
    for (const auto& pair : parameters_) {
        Parameter p = *pair.second;
        p.currentValue = p.defaultValue;
        preset.parameters.push_back(p);
    }
    
    return preset;
}

SceneMetadata PinturaFluorescenteScene::getSceneMetadata() const {
    SceneMetadata metadata;
    metadata.id = getId();
    metadata.name = getName();
    metadata.description = "Sistema de particulas fluorescentes con fisica de ondas y comportamiento de bandada";
    metadata.author = "Proyecto Danza";
    metadata.version = "1.0.0";
    metadata.tags = {"particulas", "fluorescente", "fisica", "bandada"};
    metadata.category = "visual";
    metadata.hasInternalState = true;
    metadata.isTimeBased = false;
    metadata.estimatedDuration = 0.0f;
    metadata.presetPath = "";
    return metadata;
}

void PinturaFluorescenteScene::onBeat(int beatNumber) {
    (void)beatNumber;
    // Optional: sincronizacion con beat
}

void PinturaFluorescenteScene::onBar(int barNumber) {
    (void)barNumber;
    // Optional: sincronizacion con bar
}

bool PinturaFluorescenteScene::isValid() const {
    return isValid_;
}

std::string PinturaFluorescenteScene::getInvalidReason() const {
    return invalidReason_;
}

void PinturaFluorescenteScene::unloadResources() {
    if (particlePool_) particlePool_->clear();
    if (trailPool_) trailPool_->clear();
}

void PinturaFluorescenteScene::reloadResources() {
    setup();
}

SceneDebugStats PinturaFluorescenteScene::getDebugStats() const {
    SceneDebugStats stats;
    stats.activeParticles = particlePool_ ? particlePool_->getActiveCount() : 0;
    stats.trailParticles = trailPool_ ? trailPool_->getActiveCount() : 0;
    stats.spawnedMainLastFrame = lastSpawnedMain_;
    stats.spawnedTrailLastFrame = lastSpawnedTrail_;
    stats.attractorActive = attractorActive_;
    stats.attractorPoint = attractorPoint_;
    stats.flockMode = flockMode_;
    stats.clearEachFrame = clearEachFrame_;
    return stats;
}

// ============================================================================
// Input Handlers - DISABLED - All keyboard handled by ofApp globally
// ============================================================================

void PinturaFluorescenteScene::handleKeyPressed(int key) {
    // All keyboard input handled by ofApp - scene receives no input
    ofLogNotice("PinturaFluorescenteScene") << "keyReceived but ignored (all input via ofApp): " << key;
}

// ============================================================================
// Mouse Handlers (keep for scene interaction if needed)
// ============================================================================

void PinturaFluorescenteScene::handleMousePressed(int x, int y, int button) {
    mousePressedFlag_ = true;
    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) {
        bg = param->currentValue;
    }
    ofBackground(static_cast<int>(bg));
    
    if (!mouseAttractorEnabled_) return;
    
    glm::vec2 previousPoint = attractorPoint_;
    attractorActive_ = !attractorActive_;
    if (attractorActive_) {
        attractorPoint_ = glm::vec2(x, y);
        ofLogNotice("Attractor") << "Scene attractor activated at (" << x << ", " << y << ")";
    } else {
        ofLogNotice("Attractor") << "Scene attractor deactivated (last point "
                                  << static_cast<int>(previousPoint.x) << ", "
                                  << static_cast<int>(previousPoint.y) << ")";
    }
}

void PinturaFluorescenteScene::handleMouseReleased(int x, int y, int button) {
    mousePressedFlag_ = false;
}

void PinturaFluorescenteScene::handleWindowResized(int w, int h) {
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
// Input Wrappers - IBaseScene implementation
// ============================================================================

void PinturaFluorescenteScene::keyPressed(int key) {
    handleKeyPressed(key);
}

void PinturaFluorescenteScene::keyReleased(int key) {
    (void)key;
}

void PinturaFluorescenteScene::mousePressed(int x, int y, int button) {
    handleMousePressed(x, y, button);
}

void PinturaFluorescenteScene::mouseReleased(int x, int y, int button) {
    handleMouseReleased(x, y, button);
}

void PinturaFluorescenteScene::mouseDragged(int x, int y, int button) {
    (void)x;
    (void)y;
    (void)button;
}

void PinturaFluorescenteScene::mouseMoved(int x, int y) {
    (void)x;
    (void)y;
}

void PinturaFluorescenteScene::windowResized(int w, int h) {
    handleWindowResized(w, h);
}

// ============================================================================
// Factory Function
// ============================================================================

ScenePtr CreatePinturaFluorescenteScene() {
    return std::make_shared<PinturaFluorescenteScene>();
}

} // namespace modular
