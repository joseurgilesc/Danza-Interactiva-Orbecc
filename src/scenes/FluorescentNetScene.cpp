#include "scenes/FluorescentNetScene.h"
#include "registry/ParameterRegistry.h"
#include "mapping/ParameterTypes.h"
#include "ofMain.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

namespace modular {

// ============================================================================
// Palette System (5 palettes, matching PinturaFluorescenteScene)
// ============================================================================

void FluorescentNetScene::setupPalettes() {
    if (!palettes_.empty()) return;
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

ofFloatColor FluorescentNetScene::getColorFromPalette(int paletteIndex, float variation) {
    auto& palette = palettes_[paletteIndex % palettes_.size()];
    int baseIndex = static_cast<int>(ofRandom(palette.size()));
    ofFloatColor col = palette[baseIndex];
    col += ofFloatColor(variation * ofRandom(-0.15f, 0.15f),
                        variation * ofRandom(-0.15f, 0.15f),
                        variation * ofRandom(-0.15f, 0.15f));
    return col;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

FluorescentNetScene::FluorescentNetScene() {
    initializeParameterDefinitions();
}

FluorescentNetScene::~FluorescentNetScene() {
    unregisterParameters();
}

// ============================================================================
// Parameter Definitions (28 total, fluorescentNet.* prefix)
// ============================================================================

void FluorescentNetScene::initializeParameterDefinitions() {
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

    // orbit (8)
    addParam("k",  "k (attraction)",  0.01f, 0.5f,   k_,   0.01f, "Center attraction strength");
    addParam("A",  "A (amplitude)",   0.0f,  50.0f,  A_,   1.0f,  "Wave amplitude (px)");
    addParam("w",  "w (wave freq)",   0.01f, 2.0f,   w_,   0.01f, "Wave oscillation frequency");
    addParam("q",  "q (spatial)",     0.0f,  0.01f,  q_,   0.0005f, "Wave spatial frequency");
    addParam("beta", "beta (damping)", 0.8f, 0.999f, beta_, 0.005f, "Velocity damping factor");
    addParam("S",  "S (swirl)",       0.0f,  30.0f,  S_,   1.0f,  "Swirl amplitude (px)");
    addParam("ws", "ws (swirl freq)", 0.01f, 2.0f,   ws_,  0.01f, "Swirl oscillation frequency");
    addParam("qs", "qs (swirl sp.)",  0.0f,  0.01f,  qs_,  0.0005f, "Swirl spatial frequency");

    // net (5)
    addParam("gravity",          "Gravity",          0.0f,  0.2f,   gravity_,          0.005f, "Downward force");
    addParam("damping",          "Net Damping",      0.8f,  0.999f, damping_,          0.005f, "Net velocity damping");
    addParam("springStiffness",  "Spring Stiffness", 0.01f, 0.5f,   springStiffness_,  0.01f,  "Edge constraint stiffness");
    addParam("springIterations", "Spring Iterations", 1.0f,  8.0f,  static_cast<float>(springIterations_), 1.0f, "Relaxation passes", ParameterType::INT);
    addParam("returnForce",      "Return Force",     0.0f,  0.05f,  returnForce_,      0.002f, "Pull to originalPos");

    // noise (2)
    addParam("noiseStrength", "Noise Strength",  0.0f,   3.0f,   noiseStrength_, 0.1f,   "Perlin drift intensity");
    addParam("noiseScale",    "Noise Scale",     0.001f, 0.05f,  noiseScale_,    0.001f, "Perlin spatial scale");

    // wind (3)
    addParam("windStrength",  "Wind Strength",   0.0f, 2.0f,  windStrength_,  0.1f,  "Wind intensity");
    addParam("windFrequency", "Wind Frequency",  0.05f, 1.0f, windFrequency_, 0.05f, "Wind oscillation freq");
    addParam("windBias",      "Wind Bias",      -1.0f, 1.0f,  windBias_,      0.1f,  "Wind direction bias");

    // transition (2)
    addParam("transitionRate", "Transition Rate", 0.0f, 2.0f,  transitionRate_, 0.05f, "FREE→ANCHORED speed");
    addParam("blendPeriod",    "Blend Period",    0.1f, 2.0f,  blendPeriod_,    0.1f,  "Transition blend duration (s)");

    // topology (2)
    addParam("connectionRadius",     "Connection Radius",   20.0f, 300.0f, connectionRadius_,     1.0f, "Max distance for edge creation");
    addParam("maxConnectionsPerNode", "Max Connections",    1.0f,  12.0f,  static_cast<float>(maxConnectionsPerNode_), 1.0f, "Max edges per node", ParameterType::INT);

    // joint reaction (2)
    addParam("jointLiftRadius",   "Joint Lift Radius",   20.0f, 400.0f, jointLiftRadius_,   5.0f, "Joint attraction radius for anchored net");
    addParam("jointLiftStrength", "Joint Lift Strength", 0.0f,  5.0f,   jointLiftStrength_, 0.1f, "Joint attraction force for anchored net");

    // initial net (2)
    addParam("initialNodes",  "Initial Nodes",  10.0f, 300.0f, static_cast<float>(initialNodes_), 10.0f, "Nodes spawned at net creation", ParameterType::INT);
    addParam("spreadRadius",  "Spread Radius",  50.0f, 600.0f, spreadRadius_, 10.0f, "Initial scatter radius around seed joint");

    // visual (5)
    addParam("particleSize",  "Particle Size",  1.0f,  10.0f, particleSize_,  0.5f,  "FREE particle radius (px)");
    // Indexed size params for _array MIDI mapping (8 groups)
    for (int gi = 0; gi < 8; gi++) {
        auto p = std::make_shared<Parameter>();
        p->id = getId() + ".particleSize_" + std::to_string(gi);
        p->label = "Size G" + std::to_string(gi);
        p->type = ParameterType::FLOAT;
        p->scope = ParameterScope::SCENE;
        p->min = 1; p->max = 10;
        p->defaultValue = particleSize_; p->currentValue = particleSize_;
        p->step = 0.5f; p->group = getId();
        p->description = "Particle size group " + std::to_string(gi);
        p->hidden = false; p->writable = true;
        parameters_["particleSize_" + std::to_string(gi)] = p;
    }
    addParam("nodeRadius",    "Node Radius",    1.0f,  8.0f,  nodeRadius_,    0.5f,  "ANCHORED node dot radius (px)");
    addParam("netLineWidth",  "Line Width",     0.5f,  5.0f,  netLineWidth_,  0.5f,  "Edge line width (px)");
    addParam("netLineAlpha",  "Line Alpha",     10.0f, 255.0f, netLineAlpha_, 5.0f,  "Edge line alpha");
    addParam("nodeAlpha",     "Node Alpha",     10.0f, 255.0f, nodeAlpha_,    5.0f,  "ANCHORED node alpha");

    // pool (1)
    addParam("maxParticles", "Max Particles",
             static_cast<float>(FN_MIN_MAX_PARTICLES),
             static_cast<float>(FN_MAX_MAX_PARTICLES),
             static_cast<float>(maxParticles_),
             50.0f, "Pool capacity (resizes on change)",
             ParameterType::INT);
}

// ============================================================================
// Parameter Registration
// ============================================================================

void FluorescentNetScene::registerParameters() {
    if (parametersRegistered_) return;
    initializeParameterDefinitions();
    auto& registry = ParameterRegistry::getGlobal();
    for (const auto& entry : parameters_) {
        const auto& param = entry.second;
        if (!param) continue;
        Parameter definition = *param;
        bool registered = registry.registerParam(definition);
        if (!registered && !registry.hasParam(definition.id)) {
            ofLogWarning("FluorescentNetScene")
                << "Failed to register parameter " << definition.id;
        }
    }
    parametersRegistered_ = true;
}

void FluorescentNetScene::unregisterParameters() {
    if (!parametersRegistered_) return;
    auto& registry = ParameterRegistry::getGlobal();
    for (const auto& entry : parameters_) {
        const auto& param = entry.second;
        if (param) registry.unregisterParam(param->id);
    }
    parametersRegistered_ = false;
}

// ============================================================================
// Parameter Sync — pull current values from parameters_ map into runtime members
// ============================================================================

void FluorescentNetScene::syncParamsFromMap() {
    auto readFloat = [&](const std::string& key, float& target) {
        auto it = parameters_.find(key);
        if (it != parameters_.end() && it->second) target = it->second->currentValue;
    };
    auto readInt = [&](const std::string& key, int& target) {
        auto it = parameters_.find(key);
        if (it != parameters_.end() && it->second) target = static_cast<int>(it->second->currentValue);
    };

    // orbit
    readFloat("k",    k_);
    readFloat("A",    A_);
    readFloat("w",    w_);
    readFloat("q",    q_);
    readFloat("beta", beta_);
    readFloat("S",    S_);
    readFloat("ws",   ws_);
    readFloat("qs",   qs_);

    // net
    readFloat("gravity",          gravity_);
    readFloat("damping",          damping_);
    readFloat("springStiffness",  springStiffness_);
    readInt  ("springIterations", springIterations_);
    readFloat("returnForce",      returnForce_);

    // noise/wind
    readFloat("noiseStrength", noiseStrength_);
    readFloat("noiseScale",    noiseScale_);
    readFloat("windStrength",  windStrength_);
    readFloat("windFrequency", windFrequency_);
    readFloat("windBias",      windBias_);

    // transition
    readFloat("transitionRate", transitionRate_);
    readFloat("blendPeriod",    blendPeriod_);

    // topology
    readFloat("connectionRadius",     connectionRadius_);
    readInt  ("maxConnectionsPerNode", maxConnectionsPerNode_);
    readFloat("jointLiftRadius",      jointLiftRadius_);
    readFloat("jointLiftStrength",    jointLiftStrength_);
    readInt  ("initialNodes",         initialNodes_);
    readFloat("spreadRadius",         spreadRadius_);

    // visual
    readFloat("particleSize", particleSize_);
    for (int gi = 0; gi < 8; gi++) {
        std::string key = "particleSize_" + std::to_string(gi);
        auto it = parameters_.find(key);
        if (it != parameters_.end() && it->second) groupSizes_[gi] = it->second->currentValue;
    }
    readFloat("nodeRadius",   nodeRadius_);
    readFloat("netLineWidth", netLineWidth_);
    readFloat("netLineAlpha", netLineAlpha_);
    readFloat("nodeAlpha",    nodeAlpha_);

    // pool
    readInt("maxParticles", maxParticles_);
}

// ============================================================================
// Pool Resize
// ============================================================================

void FluorescentNetScene::applyPoolResize() {
    int target = glm::clamp(maxParticles_, FN_MIN_MAX_PARTICLES, FN_MAX_MAX_PARTICLES);
    if (static_cast<int>(particles_.size()) == target) return;

    size_t oldSize = particles_.size();
    particles_.resize(static_cast<size_t>(target));

    // Count active after resize
    int count = 0;
    for (size_t i = 0; i < oldSize; i++) {
        if (particles_[i].active) count++;
    }
    activeCount_ = count;

    ofLogNotice("FluorescentNetScene") << "Pool resized to " << target
                                       << " (" << activeCount_ << " active)";
}

// ============================================================================
// Lifecycle
// ============================================================================

void FluorescentNetScene::setup() {
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

void FluorescentNetScene::activate() {
    isActive_ = true;
    isPaused_ = false;

    ofSetWindowTitle("Fluorescent Net");
    ofSetFrameRate(60);

    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) {
        bg = param->currentValue;
    }
    ofBackground(static_cast<int>(bg));

    if (auto paletteParam = registry.getParam("global.currentPalette")) {
        currentPalette_ = static_cast<int>(paletteParam->currentValue) % 5;
    }

    registerParameters();

    // Restore saved parameter values from registry
    const std::string ownPrefix = getId() + ".";
    for (const auto& entry : parameters_) {
        const std::string& key = entry.first;
        std::string fullId = ownPrefix + key;
        if (auto param = registry.getParam(fullId)) {
            setParameter(key, param->currentValue);
        }
    }
    syncParamsFromMap();

    // Pre-size particle pool
    particles_.clear();
    particles_.resize(static_cast<size_t>(glm::clamp(maxParticles_,
        FN_MIN_MAX_PARTICLES, FN_MAX_MAX_PARTICLES)));
    activeCount_ = 0;

    edges_.clear();
}

void FluorescentNetScene::deactivate() {
    isActive_ = false;
    unregisterParameters();
    interactionPoints_.clear();
    particles_.clear();
    edges_.clear();
    activeCount_ = 0;
    seedJointId_.clear();
    netGenerated_ = false;
}

void FluorescentNetScene::pause()  { isPaused_ = true; }
void FluorescentNetScene::resume() { isPaused_ = false; }

// ============================================================================
// Spawn — one batch per active interaction point per frame
// ============================================================================

void FluorescentNetScene::spawnParticles() {
    int capacity = static_cast<int>(particles_.size());
    if (activeCount_ >= capacity) return;

    // Determine seed joint: first active hand, or first active joint
    if (seedJointId_.empty() || interactionPoints_.find(seedJointId_) == interactionPoints_.end()
        || !interactionPoints_[seedJointId_].active) {
        seedJointId_.clear();
        for (const auto& candidate : {"hand_left.position", "hand_right.position"}) {
            auto it = interactionPoints_.find(candidate);
            if (it != interactionPoints_.end() && it->second.active) {
                seedJointId_ = candidate;
                break;
            }
        }
        if (seedJointId_.empty()) {
            for (const auto& entry : interactionPoints_) {
                if (entry.second.active) {
                    seedJointId_ = entry.first;
                    break;
                }
            }
        }
        // First time we found a seed — generate the initial net structure
        if (!seedJointId_.empty() && activeCount_ == 0) {
            generateInitialNet();
        }
    }

    if (seedJointId_.empty()) return;

    auto seedIt = interactionPoints_.find(seedJointId_);
    if (seedIt == interactionPoints_.end() || !seedIt->second.active) return;

    // Ongoing: spawn a few FREE particles near the frontier to slowly expand the net
    glm::vec2 pointPos = seedIt->second.position;
    int spawns = std::min(FN_SPAWN_RATE, capacity - activeCount_);

    for (int i = 0; i < spawns; i++) {
        int slot = -1;
        for (size_t s = 0; s < particles_.size(); s++) {
            if (!particles_[s].active) { slot = static_cast<int>(s); break; }
        }
        if (slot < 0) break;

        auto& p = particles_[slot];
        p.active = true;
        p.phase = ParticlePhase::FREE;
        p.jointId = seedJointId_;
        float angle = ofRandom(0.0f, glm::two_pi<float>());
        float dist = ofRandom(spreadRadius_ * 0.5f, spreadRadius_);
        p.pos = pointPos + glm::vec2(std::cos(angle), std::sin(angle)) * dist;
        p.vel = glm::vec2(0.0f);
        p.phaseOffset = ofRandom(0.0f, glm::two_pi<float>());
        p.life = FN_STALE_GRACE;
        p.transitionTimer = 0.0f;
        p.blendAlpha = 1.0f;
        p.color = getColorFromPalette(currentPalette_, 1.0f);
        p.originalPos = p.pos;
        p.noiseOffset = ofRandom(0.0f, 1000.0f);

        activeCount_++;
    }
}

// ============================================================================
// Generate Initial Net — scatter ANCHORED nodes around the seed joint
// ============================================================================

void FluorescentNetScene::generateInitialNet() {
    auto seedIt = interactionPoints_.find(seedJointId_);
    if (seedIt == interactionPoints_.end()) return;
    glm::vec2 seedPos = seedIt->second.position;

    // Spawn net above screen so it falls into view
    glm::vec2 spawnCenter(seedPos.x, -spreadRadius_);

    int initialCount = std::min(initialNodes_, static_cast<int>(particles_.size()));
    for (int i = 0; i < initialCount; i++) {
        auto& p = particles_[i];
        p.active = true;
        p.phase = ParticlePhase::ANCHORED;  // start as part of the net immediately
        p.jointId = seedJointId_;
        // Scatter randomly around spawn center within spreadRadius
        float angle = ofRandom(0.0f, glm::two_pi<float>());
        float dist = ofRandom(10.0f, spreadRadius_);
        p.pos = spawnCenter + glm::vec2(std::cos(angle), std::sin(angle)) * dist;
        p.vel = glm::vec2(0.0f);
        p.phaseOffset = ofRandom(0.0f, glm::two_pi<float>());
        p.life = 999.0f;
        p.transitionTimer = 1.0f;
        p.blendAlpha = 1.0f;
        p.color = getColorFromPalette(currentPalette_, 1.0f);
        p.originalPos = p.pos;
        p.noiseOffset = ofRandom(0.0f, 1000.0f);
        activeCount_++;
    }
    netGenerated_ = true;

    // Immediately create edges between nearby nodes
    for (int i = 0; i < initialCount; i++) {
        for (int j = i + 1; j < initialCount; j++) {
            if (!particles_[i].active || !particles_[j].active) continue;
            float dist = glm::distance(particles_[i].pos, particles_[j].pos);
            if (dist < connectionRadius_) {
                FluorescentEdge edge;
                edge.a = i;
                edge.b = j;
                edge.restLength = dist;
                edge.active = true;
                edges_.push_back(edge);
            }
        }
    }
}

// ============================================================================
// Deactivate stale FREE particles (joint inactive > FN_STALE_GRACE seconds)
// ============================================================================

void FluorescentNetScene::deactivateStaleParticles(float dt) {
    for (auto& p : particles_) {
        if (!p.active || p.phase != ParticlePhase::FREE) continue;

        auto it = interactionPoints_.find(p.jointId);
        bool jointActive = (it != interactionPoints_.end() && it->second.active);

        if (jointActive) {
            p.life = FN_STALE_GRACE;  // reset grace timer
        } else {
            p.life -= dt;
            if (p.life <= 0.0f) {
                p.active = false;
                activeCount_--;
            }
        }
    }
}

// ============================================================================
// FREE Physics Update — orbital motion around assigned interaction points
// ============================================================================

void FluorescentNetScene::updateFREEParticles(float dt) {
    glm::vec2 screenCenter(ofGetWidth() / 2.0f, ofGetHeight() / 2.0f);
    float t = ofGetElapsedTimef();

    for (auto& p : particles_) {
        if (!p.active || p.phase != ParticlePhase::FREE) continue;

        // Resolve center: use assigned interaction point if active, else fallback
        glm::vec2 center = screenCenter;
        auto it = interactionPoints_.find(p.jointId);
        if (it != interactionPoints_.end() && it->second.active) {
            center = it->second.position;
        }

        glm::vec2 r = p.pos - center;
        float d = glm::length(r) + 0.0001f;
        glm::vec2 n = r / d;
        glm::vec2 tangent(-n.y, n.x);

        // Center attraction (gentle — particles should spread first, orbit later)
        glm::vec2 F_center = -k_ * 0.3f * r;

        // Wave oscillation (radial)
        float waveArg = w_ * t - q_ * d + p.phaseOffset;
        glm::vec2 F_wave = A_ * std::sin(waveArg) * n;

        // Swirl (tangential)
        float swirlArg = ws_ * t - qs_ * d + p.phaseOffset;
        glm::vec2 F_swirl = S_ * std::cos(swirlArg) * tangent;

        // Damping (additive, from PinturaFluorescente)
        glm::vec2 F_damping = -beta_ * p.vel;

        glm::vec2 F = F_center + F_wave + F_swirl + F_damping;

        // Integrate
        p.vel += F * dt;
        p.pos += p.vel * dt;

        // Clamp speed
        float speed = glm::length(p.vel);
        if (speed > FN_MAX_SPEED) {
            p.vel = (p.vel / speed) * FN_MAX_SPEED;
        }
    }
}

// ============================================================================
// Transition FSM — FREE → BLENDING → ANCHORED
// ============================================================================

void FluorescentNetScene::updateTransitions(float dt) {
    // Count current ANCHORED nodes for cap enforcement
    int anchoredCount = 0;
    for (const auto& p : particles_) {
        if (p.active && p.phase == ParticlePhase::ANCHORED) anchoredCount++;
    }

    newlyAnchoredIndices_.clear();

    for (size_t i = 0; i < particles_.size(); i++) {
        auto& p = particles_[i];
        if (!p.active) continue;

        if (p.phase == ParticlePhase::FREE) {
            p.transitionTimer += dt * transitionRate_;
            if (p.transitionTimer >= 1.0f) {
                // Enter BLENDING phase
                p.phase = ParticlePhase::BLENDING;
                p.transitionTimer = 0.0f; // repurpose as blend progress tracker
                p.blendAlpha = 1.0f;
            }
        } else if (p.phase == ParticlePhase::BLENDING) {
            p.transitionTimer += dt;
            p.blendAlpha = 1.0f - (p.transitionTimer / glm::max(blendPeriod_, 0.001f));

            if (p.transitionTimer >= blendPeriod_) {
                // Resolve to ANCHORED (respect hard cap)
                if (anchoredCount >= FN_MAX_ANCHORED_NODES) {
                    p.active = false;
                    activeCount_--;
                    continue;
                }

                p.phase = ParticlePhase::ANCHORED;
                p.originalPos = p.pos;
                p.blendAlpha = 0.0f;

                // Cap velocity on anchor event (prevents impulse spikes)
                float speed = glm::length(p.vel);
                if (speed > FN_MAX_ANCHOR_SPEED) {
                    p.vel = (p.vel / speed) * FN_MAX_ANCHOR_SPEED;
                }

                newlyAnchoredIndices_.push_back(static_cast<int>(i));
                anchoredCount++;
            }
        }
    }
}

// ============================================================================
// ANCHORED Physics — gravity, wind, Perlin drift, damping, return force
// ============================================================================

void FluorescentNetScene::updateANCHOREDParticles(float dt) {
    float t = ofGetElapsedTimef();

    for (auto& p : particles_) {
        if (!p.active || p.phase != ParticlePhase::ANCHORED) continue;

        // 1. Gravity
        p.vel.y += gravity_ * dt;

        // 2. Wind (horizontal oscillation)
        float windForce = windStrength_ * std::sin(t * windFrequency_ + windBias_) * dt;
        p.vel.x += windForce;

        // 3. Perlin drift
        float nx = ofSignedNoise(p.pos.x * noiseScale_ + t + p.noiseOffset);
        float ny = ofSignedNoise(p.pos.y * noiseScale_ + t + p.noiseOffset + 500.0f);
        p.vel += glm::vec2(nx, ny) * noiseStrength_ * dt;

        // 4. Velocity damping
        p.vel *= damping_;

        // 5. Return force (elastic pull to originalPos)
        p.vel += (p.originalPos - p.pos) * returnForce_ * dt;

        // 6. Joint reaction: centroid + individual joints
        //    Centroid keeps net following the body; individual joints lift nearby nodes
        if (jointLiftStrength_ > 0.001f) {
            glm::vec2 jointCentroid(0.0f);
            int activeJoints = 0;
            for (const auto& entry : interactionPoints_) {
                if (entry.second.active) {
                    jointCentroid += entry.second.position;
                    activeJoints++;
                }
            }
            // 6a. Centroid pull — gentle, keeps net following body as a whole
            if (activeJoints > 0) {
                jointCentroid /= static_cast<float>(activeJoints);
                float dist = glm::distance(p.pos, jointCentroid);
                if (dist < jointLiftRadius_) {
                    float influence = 1.0f - (dist / jointLiftRadius_);
                    influence *= influence;
                    glm::vec2 toward = jointCentroid - p.pos;
                    p.vel += toward * influence * jointLiftStrength_ * 0.5f * dt;
                }
            }
            // 6b. Individual joint lift — each nearby joint pulls nodes, velocity adds momentum
            for (const auto& entry : interactionPoints_) {
                if (!entry.second.active) continue;
                float dist = glm::distance(p.pos, entry.second.position);
                if (dist < jointLiftRadius_ * 0.6f) {
                    float influence = 1.0f - (dist / (jointLiftRadius_ * 0.6f));
                    influence *= influence;
                    glm::vec2 toward = entry.second.position - p.pos;
                    // Bias upward: counter gravity by favoring negative Y
                    toward.y -= 0.3f;
                    p.vel += toward * influence * jointLiftStrength_ * 1.2f * dt;
                    // Joint velocity imparts momentum — fast joints fling the net
                    p.vel += entry.second.velocity * influence * jointLiftStrength_ * 0.25f * dt;
                }
            }
        }

        // 7. Integrate position
        p.pos += p.vel * dt;

        // Clamp speed
        float speed = glm::length(p.vel);
        if (speed > FN_MAX_SPEED) {
            p.vel = (p.vel / speed) * FN_MAX_SPEED;
        }
    }
}

// ============================================================================
// BLENDING Physics — compute both FREE and ANCHORED velocities, lerp by blendAlpha
// ============================================================================

void FluorescentNetScene::handleBlendingParticles(float dt) {
    glm::vec2 screenCenter(ofGetWidth() / 2.0f, ofGetHeight() / 2.0f);
    float t = ofGetElapsedTimef();

    for (auto& p : particles_) {
        if (!p.active || p.phase != ParticlePhase::BLENDING) continue;

        // ── Compute FREE-physics velocity contribution ──
        glm::vec2 freeVel = p.vel;

        glm::vec2 center = screenCenter;
        auto it = interactionPoints_.find(p.jointId);
        if (it != interactionPoints_.end() && it->second.active) {
            center = it->second.position;
        }

        glm::vec2 r = p.pos - center;
        float d = glm::length(r) + 0.0001f;
        glm::vec2 n = r / d;
        glm::vec2 tangent(-n.y, n.x);

        glm::vec2 F_center = -k_ * r;
        float waveArg = w_ * t - q_ * d + p.phaseOffset;
        glm::vec2 F_wave = A_ * std::sin(waveArg) * n;
        float swirlArg = ws_ * t - qs_ * d + p.phaseOffset;
        glm::vec2 F_swirl = S_ * std::cos(swirlArg) * tangent;
        glm::vec2 F_damping = -beta_ * p.vel;

        freeVel += (F_center + F_wave + F_swirl + F_damping) * dt;

        // ── Compute ANCHORED-physics velocity contribution ──
        glm::vec2 anchorVel = p.vel;

        anchorVel.y += gravity_ * dt;
        float windForce = windStrength_ * std::sin(t * windFrequency_ + windBias_) * dt;
        anchorVel.x += windForce;
        float nx = ofSignedNoise(p.pos.x * noiseScale_ + t + p.noiseOffset);
        float ny = ofSignedNoise(p.pos.y * noiseScale_ + t + p.noiseOffset + 500.0f);
        anchorVel += glm::vec2(nx, ny) * noiseStrength_ * dt;
        anchorVel *= damping_;

        // ── Blend velocities ──
        p.vel = freeVel * p.blendAlpha + anchorVel * (1.0f - p.blendAlpha);

        // Integrate
        p.pos += p.vel * dt;

        // Clamp speed
        float speed = glm::length(p.vel);
        if (speed > FN_MAX_SPEED) {
            p.vel = (p.vel / speed) * FN_MAX_SPEED;
        }
    }
}

// ============================================================================
// Spring Relaxation — iterative edge constraint solving
// ============================================================================

void FluorescentNetScene::relaxSprings() {
    for (int iter = 0; iter < springIterations_; iter++) {
        for (auto& edge : edges_) {
            if (!edge.active) continue;
            if (edge.a < 0 || edge.b < 0) continue;
            if (static_cast<size_t>(edge.a) >= particles_.size() ||
                static_cast<size_t>(edge.b) >= particles_.size()) continue;

            auto& pA = particles_[edge.a];
            auto& pB = particles_[edge.b];

            if (!pA.active || !pB.active) continue;

            glm::vec2 delta = pB.pos - pA.pos;
            float dist = glm::length(delta);
            if (dist < 0.0001f) continue;

            float diff = dist - edge.restLength;
            glm::vec2 dir = delta / dist; // normalized
            glm::vec2 correction = dir * diff * springStiffness_ * 0.5f;

            pA.pos += correction;
            pB.pos -= correction;
        }
    }
}

// ============================================================================
// Edge Management — create edges for newly ANCHORED nodes, dissolve distant edges
// ============================================================================

void FluorescentNetScene::manageEdges() {
    // ── 1. Create edges for newly anchored nodes ──
    for (int idx : newlyAnchoredIndices_) {
        if (idx < 0 || static_cast<size_t>(idx) >= particles_.size()) continue;

        auto& pNew = particles_[idx];
        if (!pNew.active || pNew.phase != ParticlePhase::ANCHORED) continue;

        // Count existing connections for this node
        int existingConnections = 0;
        for (const auto& e : edges_) {
            if (!e.active) continue;
            if (e.a == idx || e.b == idx) existingConnections++;
        }

        int remaining = maxConnectionsPerNode_ - existingConnections;
        if (remaining <= 0) continue;

        // Find candidate ANCHORED neighbors within connectionRadius
        // Brute-force — acceptable under ~500 anchored nodes (per design.md)
        struct Candidate { int index; float distSq; };
        std::vector<Candidate> candidates;

        float radiusSq = connectionRadius_ * connectionRadius_;

        for (size_t j = 0; j < particles_.size(); j++) {
            if (static_cast<int>(j) == idx) continue;
            auto& pOther = particles_[j];
            if (!pOther.active || pOther.phase != ParticlePhase::ANCHORED) continue;

            float distSq = glm::distance2(pNew.pos, pOther.pos);
            if (distSq <= radiusSq) {
                // Check not already connected
                bool alreadyConnected = false;
                for (const auto& e : edges_) {
                    if (!e.active) continue;
                    if ((e.a == idx && e.b == static_cast<int>(j)) ||
                        (e.b == idx && e.a == static_cast<int>(j))) {
                        alreadyConnected = true;
                        break;
                    }
                }
                if (!alreadyConnected) {
                    candidates.push_back({static_cast<int>(j), distSq});
                }
            }
        }

        // Sort by distance, take nearest up to 'remaining'
        std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) { return a.distSq < b.distSq; });

        int toCreate = std::min(remaining, static_cast<int>(candidates.size()));
        for (int c = 0; c < toCreate; c++) {
            FluorescentEdge edge;
            edge.a = idx;
            edge.b = candidates[c].index;
            edge.restLength = std::sqrt(candidates[c].distSq);
            edge.active = true;
            edges_.push_back(edge);
        }
    }

    // ── 2. Dissolve edges where distance > connectionRadius * 1.5 ──
    float dissolveRadiusSq = (connectionRadius_ * 1.5f) * (connectionRadius_ * 1.5f);
    for (auto& edge : edges_) {
        if (!edge.active) continue;
        if (edge.a < 0 || edge.b < 0) {
            edge.active = false;
            continue;
        }
        if (static_cast<size_t>(edge.a) >= particles_.size() ||
            static_cast<size_t>(edge.b) >= particles_.size()) {
            edge.active = false;
            continue;
        }

        auto& pA = particles_[edge.a];
        auto& pB = particles_[edge.b];

        if (!pA.active || !pB.active) {
            edge.active = false;
            continue;
        }

        float distSq = glm::distance2(pA.pos, pB.pos);
        if (distSq > dissolveRadiusSq) {
            edge.active = false;
        }
    }

    // ── 3. Compact: remove inactive edges ──
    edges_.erase(
        std::remove_if(edges_.begin(), edges_.end(),
            [](const FluorescentEdge& e) { return !e.active; }),
        edges_.end()
    );
}

// ============================================================================
// Main Update
// ============================================================================

void FluorescentNetScene::update(float deltaTime) {
    if (!isActive_ || isPaused_) return;

    float dt = glm::clamp(deltaTime, 0.0f, 0.1f);
    float physicsDt = dt * FN_TIME_SCALE;

    // Update interaction points (apply decay)
    for (auto& entry : interactionPoints_) {
        entry.second.update(dt);
    }

    // Resize pool if maxParticles changed
    int targetCap = glm::clamp(maxParticles_, FN_MIN_MAX_PARTICLES, FN_MAX_MAX_PARTICLES);
    if (static_cast<int>(particles_.size()) != targetCap) {
        applyPoolResize();
    }

    // Spawn new FREE particles
    spawnParticles();

    // Handle transitions (FREE → BLENDING → ANCHORED)
    updateTransitions(physicsDt);

    // FREE physics (orbital)
    updateFREEParticles(physicsDt);

    // BLENDING physics (lerp FREE/ANCHORED velocities)
    handleBlendingParticles(physicsDt);

    // ANCHORED physics (gravity, wind, noise, damping, return force)
    updateANCHOREDParticles(physicsDt);

    // Spring relaxation over edge mesh
    relaxSprings();

    // Edge creation for newly ANCHORED nodes + dissolution of distant edges
    manageEdges();

    // Remove stale particles
    deactivateStaleParticles(dt);

    // Zero-joint graceful fade: if no active joints, drift FREE particles to center
    bool hasActiveJoint = false;
    for (const auto& entry : interactionPoints_) {
        if (entry.second.active) { hasActiveJoint = true; break; }
    }
    if (!hasActiveJoint) {
        glm::vec2 center(ofGetWidth() / 2.0f, ofGetHeight() / 2.0f);
        for (auto& p : particles_) {
            if (!p.active || p.phase != ParticlePhase::FREE) continue;
            glm::vec2 toCenter = center - p.pos;
            p.vel += toCenter * 0.5f * physicsDt;
        }
    }
}

// ============================================================================
// Rendering
// ============================================================================

void FluorescentNetScene::drawFREEParticles() {
    ofEnableAlphaBlending();
    ofFill();

    // Depth relation: body proximity scales free-particle size.
    float depthScale = 0.7f + 0.6f * averageProximity(interactionPoints_);

    for (size_t pi = 0; pi < particles_.size(); pi++) {
        const auto& p = particles_[pi];
        if (!p.active) continue;
        if (p.phase != ParticlePhase::FREE && p.phase != ParticlePhase::BLENDING) continue;

        float alpha;
        if (p.phase == ParticlePhase::FREE) {
            alpha = 255.0f * p.blendAlpha;
        } else {
            alpha = 255.0f * (1.0f - p.blendAlpha) * 0.5f;
        }

        float size = groupSizes_[pi % 8] * (1.0f - p.transitionTimer * 0.5f) * depthScale;

        ofSetColor(p.color.r * 255.0f, p.color.g * 255.0f, p.color.b * 255.0f, alpha);
        ofDrawCircle(p.pos.x, p.pos.y, size);
    }
}

// ============================================================================
// Rendering Pass 2: ANCHORED edges — alpha-faded organic lines
// ============================================================================

void FluorescentNetScene::drawANCHOREDEdges() {
    if (edges_.empty()) return;

    ofEnableAlphaBlending();
    ofNoFill();
    ofSetLineWidth(netLineWidth_ * (0.7f + 0.6f * averageProximity(interactionPoints_)));

    // Use first palette color as base, lower saturation
    int paletteIndex = currentPalette_ % palettes_.size();
    if (palettes_.empty()) return;
    auto& palette = palettes_[paletteIndex];
    ofFloatColor baseColor = palette[0];

    for (const auto& edge : edges_) {
        if (!edge.active) continue;
        if (edge.a < 0 || edge.b < 0) continue;
        if (static_cast<size_t>(edge.a) >= particles_.size() ||
            static_cast<size_t>(edge.b) >= particles_.size()) continue;

        auto& pA = particles_[edge.a];
        auto& pB = particles_[edge.b];

        if (!pA.active || !pB.active) continue;

        float dist = glm::distance(pA.pos, pB.pos);
        float deviation = std::abs(dist - edge.restLength) / (edge.restLength + 1.0f);
        float alpha = netLineAlpha_ * (1.0f - glm::clamp(deviation, 0.0f, 1.0f) * 0.7f);

        ofFloatColor edgeColor = baseColor;
        edgeColor.setSaturation(baseColor.getSaturation() * 0.7f);

        ofSetColor(edgeColor.r * 255.0f, edgeColor.g * 255.0f, edgeColor.b * 255.0f, alpha);
        ofDrawLine(pA.pos.x, pA.pos.y, pB.pos.x, pB.pos.y);
    }
}

// ============================================================================
// Rendering Pass 3: ANCHORED nodes — small dots at node positions
// ============================================================================

void FluorescentNetScene::drawANCHOREDNodes() {
    ofEnableAlphaBlending();
    ofFill();

    // Depth relation: body proximity scales anchored node size.
    float depthScale = 0.7f + 0.6f * averageProximity(interactionPoints_);

    for (const auto& p : particles_) {
        if (!p.active || p.phase != ParticlePhase::ANCHORED) continue;

        ofSetColor(p.color.r * 255.0f, p.color.g * 255.0f, p.color.b * 255.0f, nodeAlpha_);
        ofDrawCircle(p.pos.x, p.pos.y, nodeRadius_ * depthScale);
    }
}

void FluorescentNetScene::drawDebugMarkers() {
    // Active interaction point dots: green
    for (const auto& entry : interactionPoints_) {
        if (entry.second.active && entry.second.confidence > 0.0f) {
            ofSetColor(0, 255, 0, 200);
            ofFill();
            ofDrawCircle(entry.second.position.x, entry.second.position.y, 6.0f);
            ofSetColor(0, 200, 0, 120);
            ofNoFill();
            ofDrawCircle(entry.second.position.x, entry.second.position.y, 10.0f);
        }
    }

    // Status bar
    int freeCount = 0, blendCount = 0, anchorCount = 0;
    for (const auto& p : particles_) {
        if (!p.active) continue;
        if (p.phase == ParticlePhase::FREE) freeCount++;
        else if (p.phase == ParticlePhase::BLENDING) blendCount++;
        else anchorCount++;
    }

    std::string status = "FluorescentNet | F:" + std::to_string(freeCount)
                       + " B:" + std::to_string(blendCount)
                       + " A:" + std::to_string(anchorCount)
                       + " / " + std::to_string(particles_.size())
                       + " | J:" + std::to_string(interactionPoints_.size())
                       + " | E:" + std::to_string(edges_.size());
    ofSetColor(0, 0, 0, 210);
    ofFill();
    ofDrawRectangle(5.0f, ofGetHeight() - 28.0f, 400.0f, 24.0f);
    ofSetColor(0, 255, 0, 250);
    ofDrawBitmapString(status, 10.0f, ofGetHeight() - 9.0f);
}

void FluorescentNetScene::draw() {
    if (!isActive_) return;

    ofPushStyle();

    // Pass 1: FREE and BLENDING particles (fluorescent dots)
    drawFREEParticles();

    // Pass 2: ANCHORED edges (alpha-faded organic lines)
    drawANCHOREDEdges();

    // Pass 3: ANCHORED nodes (small dots)
    drawANCHOREDNodes();

    // Pass 4: Debug markers (green joint dots + status bar)
    if (debugMarkers_) {
        drawDebugMarkers();
    }

    ofPopStyle();
}

// ============================================================================
// IBaseScene — Identification
// ============================================================================

std::string FluorescentNetScene::getId() const   { return "fluorescentNet"; }
std::string FluorescentNetScene::getName() const { return "Fluorescent Net"; }

// ============================================================================
// IBaseScene — Parameter Access
// ============================================================================

std::vector<std::string> FluorescentNetScene::getParameterIds() const {
    std::vector<std::string> ids;
    for (const auto& entry : parameters_) {
        ids.push_back(entry.first);
    }
    return ids;
}

std::vector<std::string> FluorescentNetScene::getInteractionPointIds() const {
    std::vector<std::string> ids;
    for (const auto& entry : interactionPoints_) {
        ids.push_back(entry.first);
    }
    return ids;
}

void FluorescentNetScene::clearInteractionPoints() {
    interactionPoints_.clear();
}

std::shared_ptr<Parameter> FluorescentNetScene::getParameter(const std::string& paramId) {
    auto it = parameters_.find(paramId);
    if (it != parameters_.end()) return it->second;
    return nullptr;
}

bool FluorescentNetScene::setParameter(const std::string& paramId, float value) {
    // Strip scene prefix if present (e.g., "fluorescentNet.k" → "k")
    std::string key = paramId;
    std::string prefix = getId() + ".";
    if (key.find(prefix) == 0) {
        key = key.substr(prefix.length());
    }

    auto it = parameters_.find(key);
    if (it == parameters_.end() || !it->second) return false;
    it->second->currentValue = value;
    syncParamsFromMap();
    return true;
}

bool FluorescentNetScene::setParameter(const std::string& paramId, const glm::vec2& value) {
    if (!isActive_) return true;  // accept silently, apply on next activate

    auto& pt = interactionPoints_[paramId];
    pt.sourceName = paramId;
    pt.decayRate = 0.08f;  // ~12.5s decay — tolerates tracking dropouts
    pt.setTarget(value, 1.0f, ofGetWidth(), ofGetHeight());
    return true;
}

bool FluorescentNetScene::setParameter(const std::string& paramId, const glm::vec3& value) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[paramId];
    pt.sourceName = paramId;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(value.x, value.y), 1.0f, ofGetWidth(), ofGetHeight(), value.z);
    return true;
}

// ============================================================================
// IBaseScene — Preset / Metadata
// ============================================================================

std::vector<SuggestedMapping> FluorescentNetScene::getSuggestedMappings() const {
    return {};  // no suggested mappings for now
}

Preset FluorescentNetScene::getDefaultPreset() const {
    Preset preset;
    preset.id = "default";
    preset.name = "Default";
    preset.sceneId = getId();
    preset.isDefault = true;
    for (const auto& pair : parameters_) {
        Parameter p = *pair.second;
        p.currentValue = p.defaultValue;
        preset.parameters.push_back(p);
    }
    return preset;
}

SceneMetadata FluorescentNetScene::getSceneMetadata() const {
    SceneMetadata metadata;
    metadata.id = getId();
    metadata.name = getName();
    metadata.description = "Dual-phase particle scene: FREE particles orbit Kinect joints "
                           "with fluorescent wave physics, then anchor into a spring-constrained "
                           "organic net.";
    metadata.author = "Proyecto Danza";
    metadata.version = "1.0.0";
    metadata.tags = {"fluorescent", "net", "kinect", "dual-phase", "particles", "spring"};
    metadata.category = "visual";
    metadata.hasInternalState = true;
    metadata.isTimeBased = false;
    metadata.estimatedDuration = 0.0f;
    return metadata;
}

// ============================================================================
// IBaseScene — Beat / Bar / Validation / Resources
// ============================================================================

void FluorescentNetScene::onBeat(int) {}
void FluorescentNetScene::onBar(int) {}

bool FluorescentNetScene::isValid() const { return isValid_; }
std::string FluorescentNetScene::getInvalidReason() const { return invalidReason_; }

void FluorescentNetScene::unloadResources() {
    particles_.clear();
    edges_.clear();
    activeCount_ = 0;
}

void FluorescentNetScene::reloadResources() {
    setup();
}

// ============================================================================
// IBaseScene — Input Handlers (no-op stubs)
// ============================================================================

void FluorescentNetScene::keyPressed(int) {}
void FluorescentNetScene::keyReleased(int) {}
void FluorescentNetScene::mousePressed(int, int, int) {}
void FluorescentNetScene::mouseReleased(int, int, int) {}
void FluorescentNetScene::mouseDragged(int, int, int) {}
void FluorescentNetScene::mouseMoved(int, int) {}
void FluorescentNetScene::windowResized(int, int) {}

// ============================================================================
// Factory
// ============================================================================

ScenePtr CreateFluorescentNetScene() {
    return std::make_shared<FluorescentNetScene>();
}

} // namespace modular
