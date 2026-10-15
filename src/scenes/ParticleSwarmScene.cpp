#include "scenes/ParticleSwarmScene.h"
#include "registry/ParameterRegistry.h"
#include <cmath>
#include <algorithm>

namespace modular {

// ============================================================================
// Constructor / Destructor
// ============================================================================

ParticleSwarmScene::ParticleSwarmScene()
    : currentPalette_(0)
    , isActive_(false)
    , isPaused_(false)
    , isValid_(true)
    , invalidReason_("")
    , parametersRegistered_(false)
    , debugMarkers_(false) {
}

ParticleSwarmScene::~ParticleSwarmScene() {
    unregisterParameters();
}

// ============================================================================
// Palette System
// ============================================================================

ofFloatColor ParticleSwarmScene::getColorFromPalette(int paletteIndex, float variation) {
    std::vector<ofFloatColor> palette = palettes_[paletteIndex % palettes_.size()];
    int baseIndex = (int)ofRandom(palette.size());
    ofFloatColor col = palette[baseIndex];
    col += ofFloatColor(variation * ofRandom(-0.15f, 0.15f),
                        variation * ofRandom(-0.15f, 0.15f),
                        variation * ofRandom(-0.15f, 0.15f));
    return col;
}

void ParticleSwarmScene::setupPalettes() {
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
// Pool helpers
// ============================================================================

size_t ParticleSwarmScene::findFreeSlot() {
    for (size_t i = 0; i < particles_.size(); i++) {
        if (!particles_[i].active) return i;
    }
    return particles_.size();
}

void ParticleSwarmScene::spawnParticle(const glm::vec2& pos, float depth) {
    if (particles_.empty()) return;

    size_t slot = findFreeSlot();
    if (slot >= particles_.size()) {
        // Pool full — don't spawn
        return;
    }

    // Depth relation: closer body parts (z→0) spawn larger/brighter particles.
    float proximity = 1.0f - glm::clamp(depth, 0.0f, 1.0f);  // 1=close, 0=far
    float depthScale = glm::mix(0.5f, 1.6f, proximity);      // far=0.5x, close=1.6x

    SwarmParticle& p = particles_[slot];
    p.position = pos + glm::vec2(ofRandom(-10.0f, 10.0f), ofRandom(-10.0f, 10.0f));
    p.velocity = glm::vec2(ofRandom(-1.0f, 1.0f), ofRandom(-1.0f, 1.0f));
    p.mass = ofRandom(swarmParams_.massMin, swarmParams_.massMax) * depthScale;
    p.age = 0.0f;
    p.maxLife = ofRandom(swarmParams_.particleLifeMin, swarmParams_.particleLifeMax);
    p.alpha = 1.0f;
    p.active = true;

    // Color from palette with slight variation
    size_t pIdx = currentPalette_ % palettes_.size();
    const auto& palette = palettes_[pIdx];
    p.color = palette[static_cast<int>(ofRandom(palette.size()))];
    p.color.r = glm::clamp(p.color.r + ofRandom(-0.08f, 0.08f), 0.0f, 1.0f);
    p.color.g = glm::clamp(p.color.g + ofRandom(-0.08f, 0.08f), 0.0f, 1.0f);
    p.color.b = glm::clamp(p.color.b + ofRandom(-0.08f, 0.08f), 0.0f, 1.0f);

    activeCount_++;
}

// ============================================================================
// Parameter Definitions
// ============================================================================

void ParticleSwarmScene::initializeParameterDefinitions() {
    if (!parameters_.empty()) return;

    const std::string sceneId = getId();
    auto addParam = [&](const std::string& key,
                        const std::string& label,
                        float minVal, float maxVal, float defaultVal,
                        float stepVal, const std::string& description,
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

    addParam("swarm.restDistance", "Dist Equilibrio",
             50.0f, 800.0f, 320.0f, 10.0f,
             "Distancia de equilibrio atraccion/repulsion (px)");
    addParam("swarm.damping", "Amortiguacion",
             0.9f, 0.999f, 0.99f, 0.001f,
             "Friccion de velocidad por frame");
    addParam("swarm.gravity", "Gravedad",
             0.0f, 0.2f, 0.0f, 0.005f,
             "Gravedad vertical (0 = espacio libre)");
    addParam("swarm.forceScale", "Escala Fuerza",
             0.1f, 2.0f, 0.8f, 0.05f,
             "Multiplicador global de fuerzas (1 = original p5)");

    addParam("swarm.maxParticles", "Particulas Max",
             50.0f, 800.0f, 400.0f, 10.0f,
             "Limite maximo de particulas activas", ParameterType::INT);
    addParam("swarm.spawnRate", "Tasa Spawn",
             1.0f, 10.0f, 2.0f, 1.0f,
             "Particulas nuevas por frame por joint", ParameterType::INT);
    addParam("swarm.massMin", "Masa Min",
             0.001f, 0.02f, 0.003f, 0.001f,
             "Masa minima de particula");
    addParam("swarm.massMax", "Masa Max",
             0.005f, 0.06f, 0.03f, 0.001f,
             "Masa maxima de particula");

    addParam("swarm.lifeMin", "Vida Min",
             1.0f, 10.0f, 4.0f, 0.5f,
             "Vida minima de particula (s)");
    addParam("swarm.lifeMax", "Vida Max",
             3.0f, 20.0f, 10.0f, 0.5f,
             "Vida maxima de particula (s)");

    addParam("push.radius", "Radio Empuje Joint",
             30.0f, 500.0f, 200.0f, 10.0f,
             "Distancia a la que un joint empuja particulas (px)");
    addParam("push.strength", "Fuerza Empuje Joint",
             0.01f, 0.2f, 0.06f, 0.005f,
             "Intensidad del empuje de joints");

    addParam("color.cycles", "Ciclos Color",
             1.0f, 5.0f, 2.0f, 1.0f,
             "Repeticiones de paleta en la vida de la particula", ParameterType::INT);

    addParam("visual.radiusScale", "Escala Radio",
             200.0f, 2000.0f, 800.0f, 50.0f,
             "multiplicador masa->radio (px): radio = masa * escala");
    addParam("visual.radiusMin", "Radio Min",
             1.0f, 10.0f, 2.0f, 0.5f,
             "Radio minimo de display (px)");
    addParam("visual.radiusMax", "Radio Max",
             15.0f, 60.0f, 35.0f, 1.0f,
             "Radio maximo de display (px)");
}

// ============================================================================
// Parameter Registration
// ============================================================================

void ParticleSwarmScene::registerParameters() {
    if (parametersRegistered_) return;
    initializeParameterDefinitions();
    auto& registry = ParameterRegistry::getGlobal();
    for (const auto& entry : parameters_) {
        const auto& param = entry.second;
        if (!param) continue;
        Parameter definition = *param;
        bool registered = registry.registerParam(definition);
        if (!registered && !registry.hasParam(definition.id)) {
            ofLogWarning("ParticleSwarmScene")
                << "Failed to register parameter " << definition.id;
        }
    }
    parametersRegistered_ = true;
}

void ParticleSwarmScene::unregisterParameters() {
    if (!parametersRegistered_) return;
    auto& registry = ParameterRegistry::getGlobal();
    for (const auto& entry : parameters_) {
        const auto& param = entry.second;
        if (param) registry.unregisterParam(param->id);
    }
    parametersRegistered_ = false;
}

// ============================================================================
// Parameter Sync
// ============================================================================

void ParticleSwarmScene::syncParamsFromMap() {
    auto readFloat = [&](const std::string& key, float& target) {
        auto it = parameters_.find(key);
        if (it != parameters_.end() && it->second) target = it->second->currentValue;
    };
    auto readInt = [&](const std::string& key, int& target) {
        auto it = parameters_.find(key);
        if (it != parameters_.end() && it->second) target = static_cast<int>(it->second->currentValue);
    };

    readFloat("swarm.restDistance", swarmParams_.restDistance);
    readFloat("swarm.damping", swarmParams_.damping);
    readFloat("swarm.gravity", swarmParams_.gravity);
    readFloat("swarm.forceScale", swarmParams_.globalForceScale);
    readInt("swarm.maxParticles", swarmParams_.maxParticles);
    readInt("swarm.spawnRate", swarmParams_.spawnRate);
    readFloat("swarm.massMin", swarmParams_.massMin);
    readFloat("swarm.massMax", swarmParams_.massMax);
    readFloat("swarm.lifeMin", swarmParams_.particleLifeMin);
    readFloat("swarm.lifeMax", swarmParams_.particleLifeMax);
    readFloat("push.radius", swarmParams_.jointPushRadius);
    readFloat("push.strength", swarmParams_.jointPushStrength);
    readInt("color.cycles", swarmParams_.colorCycles);
    readFloat("visual.radiusScale", swarmParams_.radiusScale);
    readFloat("visual.radiusMin", swarmParams_.radiusMin);
    readFloat("visual.radiusMax", swarmParams_.radiusMax);
}

// ============================================================================
// Lifecycle
// ============================================================================

void ParticleSwarmScene::setup() {
    ofEnableAlphaBlending();
    setupPalettes();
    initializeParameterDefinitions();
    ofSetBackgroundAuto(true);
    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) bg = param->currentValue;
    ofBackground(static_cast<int>(bg));
}

void ParticleSwarmScene::activate() {
    isActive_ = true;
    isPaused_ = false;
    interactionPoints_.clear();
    particles_.clear();
    activeCount_ = 0;

    ofSetWindowTitle("Particle Swarm — Enjambre N-Body");
    ofSetFrameRate(60);

    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) bg = param->currentValue;
    ofBackground(static_cast<int>(bg));

    if (auto paletteParam = registry.getParam("global.currentPalette"))
        currentPalette_ = static_cast<int>(paletteParam->currentValue);

    registerParameters();

    const std::string ownPrefix = getId() + ".";
    for (const auto& entry : parameters_) {
        const std::string& key = entry.first;
        std::string fullId = ownPrefix + key;
        if (auto param = registry.getParam(fullId)) {
            setParameter(key, param->currentValue);
        }
    }

    syncParamsFromMap();

    particles_.resize(swarmParams_.maxParticles);
    activeCount_ = 0;
}

void ParticleSwarmScene::deactivate() {
    isActive_ = false;
    unregisterParameters();
    interactionPoints_.clear();
    particles_.clear();
    activeCount_ = 0;
}

void ParticleSwarmScene::pause() { isPaused_ = true; }
void ParticleSwarmScene::resume() { isPaused_ = false; }

std::string ParticleSwarmScene::getId() const { return "particleSwarm"; }
std::string ParticleSwarmScene::getName() const { return "Particle Swarm — Enjambre N-Body"; }

// ============================================================================
// Update — N² mutual forces
// ============================================================================

void ParticleSwarmScene::update(float deltaTime) {
    if (!isActive_ || isPaused_) return;

    // 1. Update interaction points
    for (auto& entry : interactionPoints_) {
        entry.second.update(deltaTime);
    }

    // 2. Collect active joint positions + real depth
    std::vector<glm::vec2> jointPositions;
    std::vector<float> jointDepths;
    for (const auto& entry : interactionPoints_) {
        if (entry.second.active) {
            jointPositions.push_back(entry.second.position);
            jointDepths.push_back(entry.second.z);
        }
    }

    // 3. Spawn particles at each active joint (depth → size)
    for (size_t i = 0; i < jointPositions.size(); i++) {
        for (int s = 0; s < swarmParams_.spawnRate; s++) {
            if (activeCount_ >= particles_.size()) break;
            spawnParticle(jointPositions[i], jointDepths[i]);
        }
    }

    // 4. N² mutual spring forces (p5.js algorithm adapted)
    float dt60 = deltaTime * 60.0f;  // frame-rate independent scaling
    const float restDist = swarmParams_.restDistance;
    const float damping = swarmParams_.damping;
    const float forceScale = swarmParams_.globalForceScale;

    for (size_t i = 0; i < particles_.size(); i++) {
        if (!particles_[i].active) continue;

        SwarmParticle& pi = particles_[i];
        float ax = 0.0f, ay = 0.0f;

        for (size_t j = 0; j < particles_.size(); j++) {
            if (i == j || !particles_[j].active) continue;

            const SwarmParticle& pj = particles_[j];

            float dx = pj.position.x - pi.position.x;
            float dy = pj.position.y - pi.position.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < 1.0f) dist = 1.0f;

            float force = (dist - restDist) * pj.mass / dist * forceScale;
            ax += force * dx;
            ay += force * dy;
        }

        // Apply acceleration (scaled by own mass — heavier = slower response)
        pi.velocity.x = pi.velocity.x * damping + ax * pi.mass * dt60;
        pi.velocity.y = pi.velocity.y * damping + ay * pi.mass * dt60;
    }

    // 5. Gravity + joint push + integrate position
    for (size_t i = 0; i < particles_.size(); i++) {
        if (!particles_[i].active) continue;

        SwarmParticle& p = particles_[i];

        // Gravity
        p.velocity.y += swarmParams_.gravity * dt60;

        // Joint push: joints repel nearby particles to prevent crowding
        for (const auto& jpos : jointPositions) {
            float dx = p.position.x - jpos.x;
            float dy = p.position.y - jpos.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > 0.01f && dist < swarmParams_.jointPushRadius) {
                float strength = swarmParams_.jointPushStrength
                               * (1.0f - dist / swarmParams_.jointPushRadius);
                glm::vec2 pushDir(dx / dist, dy / dist);
                p.velocity += pushDir * strength * dt60;
            }
        }

        // Integrate
        p.position += p.velocity * dt60;

        // Age
        p.age += deltaTime;

        // Alpha: smooth fade toward end of life
        float lifeRatio = (p.maxLife > 0.0f) ? (p.age / p.maxLife) : 1.0f;
        p.alpha = 1.0f - lifeRatio;
        p.alpha = glm::clamp(p.alpha, 0.0f, 1.0f);

        // Kill old particles
        if (p.age >= p.maxLife) {
            p.active = false;
            activeCount_--;
        }
    }
}

// ============================================================================
// Draw — Fibonacci-colored circles
// ============================================================================

void ParticleSwarmScene::draw() {
    if (!isActive_) return;

    ofEnableAlphaBlending();
    ofFill();

    const auto& palette = palettes_[currentPalette_ % palettes_.size()];
    int N = static_cast<int>(palette.size());
    if (N < 1) N = 1;

    // Precompute Fibonacci positions for color lookup
    float fibPositions[8];
    {
        int fib[8] = {1, 1};
        for (int f = 2; f < N && f < 8; f++) fib[f] = fib[f-1] + fib[f-2];
        float cumSum = 0;
        for (int f = 0; f < N && f < 8; f++) cumSum += static_cast<float>(fib[f]);
        float running = 0;
        for (int f = 0; f < N && f < 8; f++) {
            running += static_cast<float>(fib[f]);
            fibPositions[f] = running / cumSum;
        }
    }

    for (size_t i = 0; i < particles_.size(); i++) {
        const SwarmParticle& p = particles_[i];
        if (!p.active || p.alpha <= 0.01f) continue;

        // Fibonacci color based on age ratio
        float lifeRatio = (p.maxLife > 0.0f) ? (p.age / p.maxLife) : 0.0f;
        float rawT = lifeRatio * static_cast<float>(swarmParams_.colorCycles);
        float cycleT = std::fmod(rawT, 1.0f);
        if (cycleT < 0.0f) cycleT += 1.0f;

        int seg = 0;
        for (int s = 0; s < N; s++) {
            if (cycleT <= fibPositions[s]) { seg = s; break; }
        }

        ofFloatColor ptColor;
        if (seg == 0) {
            ptColor = palette[0];
        } else {
            float segStart = fibPositions[seg - 1];
            float segEnd = fibPositions[seg];
            float localT = (segEnd - segStart > 0.0001f)
                ? (cycleT - segStart) / (segEnd - segStart) : 0.0f;
            localT = glm::clamp(localT, 0.0f, 1.0f);
            ptColor.r = glm::mix(palette[seg-1].r, palette[seg].r, localT);
            ptColor.g = glm::mix(palette[seg-1].g, palette[seg].g, localT);
            ptColor.b = glm::mix(palette[seg-1].b, palette[seg].b, localT);
        }

        // Radius = mass * scale, clamped
        float r = glm::clamp(p.mass * swarmParams_.radiusScale,
                             swarmParams_.radiusMin,
                             swarmParams_.radiusMax);

        int a = static_cast<int>(p.alpha * 200.0f);
        ofSetColor(ptColor.r * 255.0f, ptColor.g * 255.0f, ptColor.b * 255.0f, a);
        ofDrawCircle(p.position.x, p.position.y, r);
    }

    // Joint markers (debug)
    if (debugMarkers_) {
        ofSetColor(255, 0, 0, 120);
        for (const auto& entry : interactionPoints_) {
            if (entry.second.active) {
                ofDrawCircle(entry.second.position.x, entry.second.position.y, 5);
            }
        }
    }
}

// ============================================================================
// Parameter Access
// ============================================================================

std::vector<std::string> ParticleSwarmScene::getParameterIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : parameters_) ids.push_back(pair.first);
    return ids;
}

std::vector<std::string> ParticleSwarmScene::getInteractionPointIds() const {
    std::vector<std::string> ids;
    for (const auto& pair : interactionPoints_) ids.push_back(pair.first);
    return ids;
}

std::shared_ptr<Parameter> ParticleSwarmScene::getParameter(const std::string& paramId) {
    auto it = parameters_.find(paramId);
    return (it != parameters_.end()) ? it->second : nullptr;
}

bool ParticleSwarmScene::setParameter(const std::string& paramId, float value) {
    std::string key = paramId;
    if (key.find("particleSwarm.") == 0)
        key = key.substr(std::string("particleSwarm.").length());

    auto it = parameters_.find(key);
    if (it != parameters_.end()) {
        it->second->currentValue = value;

        if (key == "swarm.restDistance") swarmParams_.restDistance = value;
        else if (key == "swarm.damping") swarmParams_.damping = value;
        else if (key == "swarm.gravity") swarmParams_.gravity = value;
        else if (key == "swarm.forceScale") swarmParams_.globalForceScale = value;
        else if (key == "swarm.maxParticles") {
            swarmParams_.maxParticles = static_cast<int>(value);
            particles_.resize(swarmParams_.maxParticles);
        }
        else if (key == "swarm.spawnRate") swarmParams_.spawnRate = static_cast<int>(value);
        else if (key == "swarm.massMin") swarmParams_.massMin = value;
        else if (key == "swarm.massMax") swarmParams_.massMax = value;
        else if (key == "swarm.lifeMin") swarmParams_.particleLifeMin = value;
        else if (key == "swarm.lifeMax") swarmParams_.particleLifeMax = value;
        else if (key == "push.radius") swarmParams_.jointPushRadius = value;
        else if (key == "push.strength") swarmParams_.jointPushStrength = value;
        else if (key == "color.cycles") swarmParams_.colorCycles = static_cast<int>(value);
        else if (key == "visual.radiusScale") swarmParams_.radiusScale = value;
        else if (key == "visual.radiusMin") swarmParams_.radiusMin = value;
        else if (key == "visual.radiusMax") swarmParams_.radiusMax = value;

        return true;
    }
    return false;
}

bool ParticleSwarmScene::setParameter(const std::string& paramId, const glm::vec2& value) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[paramId];
    pt.sourceName = paramId;
    pt.decayRate = 0.08f;
    pt.setTarget(value, 1.0f, ofGetWidth(), ofGetHeight());
    return true;
}

bool ParticleSwarmScene::setParameter(const std::string& paramId, const glm::vec3& value) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[paramId];
    pt.sourceName = paramId;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(value.x, value.y), 1.0f, ofGetWidth(), ofGetHeight(), value.z);
    return true;
}

std::vector<SuggestedMapping> ParticleSwarmScene::getSuggestedMappings() const { return {}; }

Preset ParticleSwarmScene::getDefaultPreset() const {
    Preset preset;
    preset.id = "default";
    preset.name = "Default";
    preset.description = "Valores por defecto de Particle Swarm";
    preset.sceneId = getId();
    preset.isDefault = true;
    for (const auto& pair : parameters_) {
        Parameter p = *pair.second;
        p.currentValue = p.defaultValue;
        preset.parameters.push_back(p);
    }
    return preset;
}

SceneMetadata ParticleSwarmScene::getSceneMetadata() const {
    SceneMetadata metadata;
    metadata.id = getId();
    metadata.name = getName();
    metadata.description = "Enjambre de particulas con fuerzas mutuas N-body. Cada joint suelta particulas que se atraen y repelen entre si formando estructuras organicas.";
    metadata.author = "Proyecto Danza";
    metadata.version = "1.0.0";
    metadata.tags = {"enjambre", "nbody", "resorte", "organico", "gravedad"};
    metadata.category = "visual";
    metadata.hasInternalState = true;
    metadata.isTimeBased = false;
    metadata.estimatedDuration = 0.0f;
    metadata.presetPath = "";
    return metadata;
}

void ParticleSwarmScene::onBeat(int) {}
void ParticleSwarmScene::onBar(int) {}
bool ParticleSwarmScene::isValid() const { return isValid_; }
std::string ParticleSwarmScene::getInvalidReason() const { return invalidReason_; }
void ParticleSwarmScene::unloadResources() { interactionPoints_.clear(); }
void ParticleSwarmScene::reloadResources() { setup(); }
void ParticleSwarmScene::keyPressed(int) {}
void ParticleSwarmScene::keyReleased(int) {}
void ParticleSwarmScene::mousePressed(int, int, int) {}
void ParticleSwarmScene::mouseReleased(int, int, int) {}
void ParticleSwarmScene::mouseDragged(int, int, int) {}
void ParticleSwarmScene::mouseMoved(int, int) {}

void ParticleSwarmScene::windowResized(int, int) {
    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) bg = param->currentValue;
    ofBackground(static_cast<int>(bg));
}

ScenePtr CreateParticleSwarmScene() {
    return std::make_shared<ParticleSwarmScene>();
}

} // namespace modular
