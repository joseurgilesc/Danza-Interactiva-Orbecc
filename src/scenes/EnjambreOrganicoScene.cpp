#include "scenes/EnjambreOrganicoScene.h"
#include "registry/ParameterRegistry.h"
#include "mapping/ParameterTypes.h"
#include "ofMain.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_set>
#include <unordered_map>

namespace modular {

// ============================================================================
// Canonical joint traversal order (interaction point IDs)
// ============================================================================

// DFS skeleton traversal order — always follows bone connectivity
// head→neck, neck→arms (left then right), neck→spine_mid→spine_base→legs (left then right)
static const std::vector<std::string> CANONICAL_JOINTS = {
    "head.position",
    "neck.position",
    "shoulder_left.position",  "elbow_left.position",  "wrist_left.position",  "hand_left.position",
    "shoulder_right.position", "elbow_right.position", "wrist_right.position", "hand_right.position",
    "spine_mid.position",
    "spine_base.position",
    "hip_left.position",       "knee_left.position",   "ankle_left.position",
    "hip_right.position",      "knee_right.position",  "ankle_right.position"
};

// ============================================================================
// Skeleton bone connectivity graph (Kinect/MediaPipe topology)
// ============================================================================
// Maps each joint .position ID → IDs of directly connected joints.
// Used by findSkeletonPath() to compute bone-aware travel routes.

static const std::map<std::string, std::vector<std::string>> SKELETON_BONES = {
    {"head.position",         {"neck.position"}},
    {"neck.position",         {"head.position", "shoulder_left.position",
                               "shoulder_right.position", "spine_mid.position"}},
    {"shoulder_left.position",  {"neck.position", "elbow_left.position"}},
    {"elbow_left.position",     {"shoulder_left.position", "wrist_left.position"}},
    {"wrist_left.position",     {"elbow_left.position", "hand_left.position"}},
    {"hand_left.position",      {"wrist_left.position"}},
    {"shoulder_right.position", {"neck.position", "elbow_right.position"}},
    {"elbow_right.position",    {"shoulder_right.position", "wrist_right.position"}},
    {"wrist_right.position",    {"elbow_right.position", "hand_right.position"}},
    {"hand_right.position",     {"wrist_right.position"}},
    {"spine_mid.position",      {"neck.position", "spine_base.position"}},
    {"spine_base.position",     {"spine_mid.position", "hip_left.position", "hip_right.position"}},
    {"hip_left.position",       {"spine_base.position", "knee_left.position"}},
    {"knee_left.position",      {"hip_left.position", "ankle_left.position"}},
    {"ankle_left.position",     {"knee_left.position"}},
    {"hip_right.position",      {"spine_base.position", "knee_right.position"}},
    {"knee_right.position",     {"hip_right.position", "ankle_right.position"}},
    {"ankle_right.position",    {"knee_right.position"}},
};

// ============================================================================
// Utility: smoothstep
// ============================================================================

inline float smoothstep(float t) {
    t = glm::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// ============================================================================
// Palette System (5 fluorescent palettes, matching PinturaFluorescenteScene)
// ============================================================================

void EnjambreOrganicoScene::setupPalettes() {
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

ofFloatColor EnjambreOrganicoScene::getColorFromPalette(int paletteIndex, float variation) {
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

EnjambreOrganicoScene::EnjambreOrganicoScene() {
    initializeParameterDefinitions();
}

EnjambreOrganicoScene::~EnjambreOrganicoScene() = default;

// ============================================================================
// Joint Sequence Builder
// ============================================================================

std::vector<EnjambreOrganicoScene::JointEntry>
EnjambreOrganicoScene::buildJointSequence() {
    std::vector<JointEntry> seq;

    // Map active interaction points for quick lookup
    std::map<std::string, InteractionPoint*> activePoints;
    for (auto& entry : interactionPoints_) {
        if (entry.second.active && entry.second.confidence > 0.0f) {
            activePoints[entry.first] = &entry.second;
        }
    }

    // Include ALL canonical joints in DFS skeleton order.
    // Active joints use live position; inactive ones use last-known position
    // from interactionPoints_ or a default (screen center).
    for (const auto& canonicalId : CANONICAL_JOINTS) {
        glm::vec2 pos;
        bool isActive = false;

        auto it = interactionPoints_.find(canonicalId);
        if (it != interactionPoints_.end()) {
            pos = it->second.position;
            isActive = it->second.active && it->second.confidence > 0.0f;
        } else {
            // Never seen this joint — use screen center as fallback
            pos = glm::vec2(ofGetWidth() * 0.5f, ofGetHeight() * 0.5f);
        }

        seq.push_back({canonicalId, pos, isActive});
    }

    // Append non-canonical active points (sorted)
    std::vector<std::string> extras;
    for (const auto& entry : activePoints) {
        bool isCanonical = false;
        for (const auto& cid : CANONICAL_JOINTS) {
            if (cid == entry.first) { isCanonical = true; break; }
        }
        if (!isCanonical) extras.push_back(entry.first);
    }
    std::sort(extras.begin(), extras.end());
    for (const auto& id : extras) {
        auto it = activePoints.find(id);
        if (it != activePoints.end()) {
            seq.push_back({id, it->second->position, true});
        }
    }

    return seq;
}

// ============================================================================
// Skeleton Path Finder — BFS through bone graph
// ============================================================================

std::vector<std::string>
EnjambreOrganicoScene::findSkeletonPath(const std::string& fromId, const std::string& toId) {
    if (fromId == toId) return {toId};

    std::unordered_map<std::string, std::string> parent;
    std::queue<std::string> frontier;
    std::unordered_set<std::string> visited;

    frontier.push(fromId);
    visited.insert(fromId);

    bool found = false;
    while (!frontier.empty()) {
        std::string current = frontier.front();
        frontier.pop();

        auto boneIt = SKELETON_BONES.find(current);
        if (boneIt == SKELETON_BONES.end()) continue;

        for (const auto& neighbor : boneIt->second) {
            if (visited.count(neighbor)) continue;
            visited.insert(neighbor);
            parent[neighbor] = current;

            if (neighbor == toId) {
                found = true;
                break;
            }
            frontier.push(neighbor);
        }
        if (found) break;
    }

    // Reconstruct path (reverse from toId back to fromId)
    std::vector<std::string> path;
    if (!found) return {toId};  // fallback: direct jump

    std::string step = toId;
    while (step != fromId) {
        path.push_back(step);
        step = parent[step];
    }
    path.push_back(fromId);
    std::reverse(path.begin(), path.end());

    return path;
}

// ============================================================================
// Parameter Definitions
// ============================================================================

void EnjambreOrganicoScene::initializeParameterDefinitions() {
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

    addParam("travelDuration", "Travel Duration",
             0.5f, 10.0f, ENJAMBRE_DEFAULT_TRAVEL_DURATION, 0.1f,
             "Seconds per joint-to-joint travel segment");
    addParam("dwellDuration", "Dwell Duration",
             0.0f, 8.0f, ENJAMBRE_DEFAULT_DWELL_DURATION, 0.1f,
             "Seconds anchor rests at each joint");
    addParam("swarmRadius", "Swarm Radius",
             10.0f, 300.0f, ENJAMBRE_DEFAULT_SWARM_RADIUS, 1.0f,
             "Particle offset distribution radius (px)");
    addParam("noiseAmount", "Noise Amount",
             0.0f, 150.0f, ENJAMBRE_DEFAULT_NOISE_AMOUNT, 1.0f,
             "Orbital wobble amplitude (px)");
    addParam("noiseSpeed", "Noise Speed",
             0.01f, 2.0f, ENJAMBRE_DEFAULT_NOISE_SPEED, 0.01f,
             "Perlin noise time rate");
    addParam("springStiffness", "Spring Stiffness",
             0.5f, 20.0f, ENJAMBRE_DEFAULT_SPRING_STIFFNESS, 0.1f,
             "Spring constant (damping = 2*sqrt(k) for critical)");
    addParam("particleCount", "Particle Count",
             static_cast<float>(ENJAMBRE_MIN_PARTICLE_COUNT),
             static_cast<float>(ENJAMBRE_MAX_PARTICLE_COUNT),
             static_cast<float>(ENJAMBRE_DEFAULT_PARTICLE_COUNT),
             50.0f, "Number of swarm particles (resizes on change)",
             ParameterType::INT);
    addParam("maxAnchorSpeed", "Max Anchor Speed",
             100.0f, 3000.0f, ENJAMBRE_DEFAULT_MAX_ANCHOR_SPEED, 10.0f,
             "Teleport clamp: max px/s anchor displacement");
    addParam("particleSize", "Particle Size",
             1.0f, 12.0f, ENJAMBRE_DEFAULT_PARTICLE_SIZE, 0.5f,
              "Base circle rendering size (px)");
    // Indexed size params for _array MIDI mapping (8 groups)
    for (int gi = 0; gi < 8; gi++) {
        auto p = std::make_shared<Parameter>();
        p->id = getId() + ".particleSize_" + std::to_string(gi);
        p->label = "Size G" + std::to_string(gi);
        p->type = ParameterType::FLOAT;
        p->scope = ParameterScope::SCENE;
        p->min = 1; p->max = 12;
        p->defaultValue = particleSize_; p->currentValue = particleSize_;
        p->step = 0.5f; p->group = getId();
        p->description = "Particle size group " + std::to_string(gi);
        p->hidden = false; p->writable = true;
        parameters_["particleSize_" + std::to_string(gi)] = p;
    }
    addParam("boneTrailRatio", "Bone Trail Ratio",
             0.0f, 1.0f, ENJAMBRE_DEFAULT_BONE_TRAIL_RATIO, 0.05f,
             "Fraction of particles spread along skeleton bones (0=none, 1=all)");
    addParam("lagRatio", "Lag Ratio",
             0.0f, 0.6f, ENJAMBRE_DEFAULT_LAG_RATIO, 0.05f,
             "Fraction of particles that trail behind at previous joints (0=none)");
    addParam("mouseStrength", "Mouse Strength",
             0.0f, 2.0f, 0.5f, 0.1f,
             "Mouse attraction strength (0=off)");
}

// ============================================================================
// Parameter Registration
// ============================================================================

void EnjambreOrganicoScene::registerParameters() {
    if (parametersRegistered_) return;
    initializeParameterDefinitions();
    auto& registry = ParameterRegistry::getGlobal();
    for (const auto& entry : parameters_) {
        const auto& param = entry.second;
        if (!param) continue;
        Parameter definition = *param;
        bool registered = registry.registerParam(definition);
        if (!registered && !registry.hasParam(definition.id)) {
            ofLogWarning("EnjambreOrganicoScene")
                << "Failed to register parameter " << definition.id;
        }
    }
    parametersRegistered_ = true;
}

void EnjambreOrganicoScene::unregisterParameters() {
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

void EnjambreOrganicoScene::syncParamsFromMap() {
    auto readFloat = [&](const std::string& key, float& target) {
        auto it = parameters_.find(key);
        if (it != parameters_.end() && it->second) target = it->second->currentValue;
    };
    auto readInt = [&](const std::string& key, int& target) {
        auto it = parameters_.find(key);
        if (it != parameters_.end() && it->second) target = static_cast<int>(it->second->currentValue);
    };

    readFloat("travelDuration",   travelDuration_);
    readFloat("dwellDuration",    dwellDuration_);
    readFloat("swarmRadius",      swarmRadius_);
    readFloat("noiseAmount",      noiseAmount_);
    readFloat("noiseSpeed",       noiseSpeed_);
    readFloat("springStiffness",  springStiffness_);
    readInt  ("particleCount",    particleCount_);
    readFloat("maxAnchorSpeed",   maxAnchorSpeed_);
    readFloat("particleSize",     particleSize_);
    readFloat("boneTrailRatio",   boneTrailRatio_);
    readFloat("lagRatio",        lagRatio_);
    readFloat("mouseStrength",   mouseStrength_);
    for (int gi = 0; gi < 8; gi++) {
        std::string key = "particleSize_" + std::to_string(gi);
        auto it = parameters_.find(key);
        if (it != parameters_.end() && it->second) groupSizes_[gi] = it->second->currentValue;
    }
}

// ============================================================================
// Particle Resize
// ============================================================================

void EnjambreOrganicoScene::applyParticleResize() {
    size_t target = static_cast<size_t>(glm::clamp(particleCount_,
        ENJAMBRE_MIN_PARTICLE_COUNT, ENJAMBRE_MAX_PARTICLE_COUNT));

    if (particles_.size() == target) return;

    size_t oldSize = particles_.size();
    particles_.resize(target);

    for (size_t i = oldSize; i < target; i++) {
        auto& p = particles_[i];
        p.pos = anchor_.pos;
        p.vel = glm::vec2(0.0f);
        p.noiseSeed = ofRandom(0.0f, 1000.0f);
        p.color = getColorFromPalette(currentPalette_, 0.5f);
        p.sizeFactor = ofRandom(0.6f, 1.4f);
        p.baseSize = ofRandom(0.15f, 2.2f);
        p.lagFactor = ofRandom(0.0f, 1.0f);
        p.boneTrailT = ofRandom(0.0f, 1.0f);
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

void EnjambreOrganicoScene::setup() {
    ofEnableAlphaBlending();
    setupPalettes();
    initializeParameterDefinitions();
    ofSetBackgroundAuto(true);
    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) bg = param->currentValue;
    ofBackground(static_cast<int>(bg));
}

void EnjambreOrganicoScene::activate() {
    isActive_ = true;
    isPaused_ = false;

    // NOTE: interactionPoints_ is NOT cleared on activate.
    // Joints accumulate while the scene is loaded-but-inactive (lesson e3df953).

    ofSetWindowTitle("Enjambre Organico — Single Cohesive Swarm");
    ofSetFrameRate(60);

    float bg = 10.0f;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) bg = param->currentValue;
    ofBackground(static_cast<int>(bg));

    if (auto paletteParam = registry.getParam("global.currentPalette"))
        currentPalette_ = static_cast<int>(paletteParam->currentValue) % 5;

    sceneAlpha_ = 1.0f;
    targetSceneAlpha_ = 1.0f;

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

    // Initialize anchor at screen center
    anchor_ = SwarmAnchor{};
    anchor_.pos = glm::vec2(ofGetWidth() * 0.5f, ofGetHeight() * 0.5f);
    prevAnchorPos_ = anchor_.pos;

    // Pre-size particle pool
    particles_.resize(static_cast<size_t>(glm::clamp(particleCount_,
        ENJAMBRE_MIN_PARTICLE_COUNT, ENJAMBRE_MAX_PARTICLE_COUNT)));
    for (auto& p : particles_) {
        p.pos = anchor_.pos;
        p.vel = glm::vec2(0.0f);
        p.noiseSeed = ofRandom(0.0f, 1000.0f);
        p.color = getColorFromPalette(currentPalette_, 0.5f);
        p.sizeFactor = ofRandom(0.6f, 1.4f);
        p.baseSize = ofRandom(0.15f, 2.2f);
        p.lagFactor = ofRandom(0.0f, 1.0f);
        p.boneTrailT = ofRandom(0.0f, 1.0f);
    }
}

void EnjambreOrganicoScene::deactivate() {
    isActive_ = false;
    unregisterParameters();
    interactionPoints_.clear();
    particles_.clear();
}

void EnjambreOrganicoScene::pause() { isPaused_ = true; }
void EnjambreOrganicoScene::resume() { isPaused_ = false; }

std::string EnjambreOrganicoScene::getId() const { return "enjambreOrganico"; }
std::string EnjambreOrganicoScene::getName() const { return "Enjambre Organico — Swarm Leader-Follower"; }

// ============================================================================
// Update — Anchor FSM + Particle Physics
// ============================================================================

void EnjambreOrganicoScene::update(float deltaTime) {
    if (!isActive_ || isPaused_) return;

    float dt = glm::clamp(deltaTime, 0.0f, 0.1f);

    // 1. Update interaction points
    for (auto& entry : interactionPoints_) {
        entry.second.update(dt);
    }

    // 1b. Mouse mode — anchor follows mouse, skip skeleton tracking
    std::vector<JointEntry> seq;
    int jointCount = 0;

    if (trackMouse_ && mouseActive_) {
        glm::vec2 target = mousePos_;
        glm::vec2 delta = target - anchor_.pos;
        float dist = glm::length(delta);
        float maxStep = maxAnchorSpeed_ * dt;
        if (dist > maxStep && maxStep > 0.0f) delta = (delta / dist) * maxStep;
        prevAnchorPos_ = anchor_.pos;
        anchor_.pos += delta;
        if (dist < 3.0f) anchor_.pos = target;
        // Fall through to particle physics (skip skeleton anchor FSM)
    } else {

    // 2. Build joint sequence this frame
    seq = buildJointSequence();
    jointCount = static_cast<int>(seq.size());

    // 3. Anchor FSM — always follows skeleton DFS order
    //    Active joints: dwell before proceeding. Inactive: pass through instantly.
    glm::vec2 targetPos;

    if (jointCount == 0) {
        anchor_.state = AnchorState::Dwelling;
        anchor_.dwellTimer = 0.0f;
        anchor_.pathQueue.clear();
        targetPos = glm::vec2(ofGetWidth() * 0.5f, ofGetHeight() * 0.5f);

    } else {
        // Resolve current joint index by ID (joint may have disappeared)
        bool foundCurrent = false;
        for (int i = 0; i < jointCount; i++) {
            if (seq[i].id == anchor_.currentJointId) {
                anchor_.currentJointIdx = i;
                foundCurrent = true;
                break;
            }
        }
        if (!foundCurrent) {
            anchor_.currentJointIdx = 0;
            anchor_.currentJointId = seq[0].id;
        }

        bool isActive = false;
        if (anchor_.currentJointIdx < jointCount) {
            isActive = seq[anchor_.currentJointIdx].active;
            targetPos = seq[anchor_.currentJointIdx].position;
        } else {
            targetPos = anchor_.pos;
        }

        if (anchor_.state == AnchorState::Dwelling) {
            // Skip dwelling for inactive joints — jump straight to travel
            if (!isActive) {
                anchor_.dwellTimer = dwellDuration_ + 1.0f;  // force immediate transition
            } else {
                anchor_.dwellTimer += dt;
            }

            if (anchor_.dwellTimer >= dwellDuration_) {
                int nextIdx = (anchor_.currentJointIdx + 1) % jointCount;
                anchor_.targetJointId = seq[nextIdx].id;
                // Snap target position NOW so it doesn't drift mid-travel
                anchor_.travelTarget = seq[nextIdx].position;
                // Inactive joints: zip through at 5x speed
                anchor_.travelSpeed = seq[nextIdx].active ? 1.0f : 5.0f;
                anchor_.state = AnchorState::Traveling;
                anchor_.travelStart = anchor_.pos;
                anchor_.travelT = 0.0f;
                anchor_.dwellTimer = 0.0f;
            }
        }

        if (anchor_.state == AnchorState::Traveling) {
            // Find target joint index (may have vanished — fallback is ok, travelTarget is locked)
            int targetIdx = -1;
            for (int i = 0; i < jointCount; i++) {
                if (seq[i].id == anchor_.targetJointId) {
                    targetIdx = i;
                    break;
                }
            }

            anchor_.travelT += (dt * anchor_.travelSpeed) / glm::max(travelDuration_, 0.01f);

            // easeInOutCubic: softer acceleration/deceleration than smoothstep
            float t = glm::clamp(anchor_.travelT, 0.0f, 1.0f);
            float eased = (t < 0.5f)
                ? 4.0f * t * t * t
                : 1.0f - glm::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
            // Use SNAPPED travelTarget, not live position
            targetPos = glm::mix(anchor_.travelStart, anchor_.travelTarget, eased);

            // Perlin wander during travel for organic, non-straight paths
            {
                float time = ofGetElapsedTimef();
                float wnX = (ofNoise(time * 0.7f + 10.0f) - 0.5f) * 30.0f;
                float wnY = (ofNoise(time * 0.7f + 60.0f) - 0.5f) * 30.0f;
                targetPos += glm::vec2(wnX, wnY);
            }

            if (anchor_.travelT >= 1.0f) {
                anchor_.state = AnchorState::Dwelling;
                anchor_.currentJointId = anchor_.targetJointId;
                anchor_.currentJointIdx = (targetIdx >= 0) ? targetIdx : ((anchor_.currentJointIdx + 1) % jointCount);
                anchor_.dwellTimer = 0.0f;
            }
        }
    }

    // 4. Clamp anchor displacement
    glm::vec2 desiredDelta = targetPos - prevAnchorPos_;
    float dist = glm::length(desiredDelta);
    float maxStep = maxAnchorSpeed_ * dt;
    if (dist > maxStep && maxStep > 0.0f) {
        desiredDelta = (desiredDelta / dist) * maxStep;
    }
    anchor_.pos = prevAnchorPos_ + desiredDelta;
    prevAnchorPos_ = anchor_.pos;

    // 5. Alpha fade — 0 joints fades to ~40%, restores on joint reappear
    {
        float alphaTarget = (jointCount == 0) ? 0.4f : 1.0f;
        targetSceneAlpha_ = glm::mix(targetSceneAlpha_, alphaTarget, dt * 0.5f);
        sceneAlpha_ = glm::mix(sceneAlpha_, targetSceneAlpha_, dt * 2.0f);
    }
    } // end skeleton tracking else block

    // 6. Particle physics — critically-damped spring + 2-octave Perlin noise
    //    Lag particles: reduced stiffness, target prevAnchorPos_ so they trail behind
    float time = ofGetElapsedTimef();
    float k = springStiffness_;
    float baseDamping = 2.0f * glm::sqrt(k);

    // Bone trail setup (skeleton mode only)
    int trailCount = 0;
    bool hasBone = false;
    glm::vec2 boneStart{0.0f}, boneEnd{0.0f};
    if (!trackMouse_) {
        trailCount = static_cast<int>(particles_.size() * boneTrailRatio_);
        hasBone = (anchor_.state == AnchorState::Traveling && jointCount >= 2);
        if (hasBone) {
            auto findPos = [&](const std::string& id) -> glm::vec2 {
                for (const auto& je : seq) {
                    if (je.id == id) return je.position;
                }
                return anchor_.pos;
            };
            boneStart = findPos(anchor_.currentJointId);
            boneEnd   = findPos(anchor_.targetJointId);
        }
    }

    for (size_t i = 0; i < particles_.size(); i++) {
        auto& p = particles_[i];
        glm::vec2 target;

        bool isTrail = (static_cast<int>(i) < trailCount) && hasBone;
        if (isTrail) {
            // Split trail: first half seep forward along bone, second half oscillate
            bool isSeep = (static_cast<int>(i) < trailCount / 2);

            float slideT;
            if (isSeep) {
                // Seep: slowly crawl along the bone from start→end, wrap at end
                p.boneTrailT += dt * 0.12f;  // ~8 seconds to traverse full bone
                if (p.boneTrailT > 1.0f) p.boneTrailT -= 1.0f;
                slideT = p.boneTrailT;
            } else {
                // Oscillate: Perlin-driven back-and-forth along the bone
                slideT = ofNoise(p.noiseSeed * 0.3f + time * 0.25f);
            }

            glm::vec2 bonePos = glm::mix(boneStart, boneEnd, slideT);
            // Wider jitter for organic dispersion
            float jx = (ofNoise(p.noiseSeed + time * 0.35f) - 0.5f) * particleSize_ * 8.0f;
            float jy = (ofNoise(p.noiseSeed * 2.1f + time * 0.35f) - 0.5f) * particleSize_ * 8.0f;
            target = bonePos + glm::vec2(jx, jy);
        } else {
            // Lag check: fraction of particles target prevAnchorPos_ instead of anchor
            bool isLag = (p.lagFactor > 0.01f && (p.lagFactor < lagRatio_ || ofRandom(1.0f) < lagRatio_));
            glm::vec2 center = isLag ? prevAnchorPos_ : anchor_.pos;

            // Normal swarm: center + aggressive multi-octave Perlin noise
            float tNx = p.noiseSeed + time * noiseSpeed_;
            float tNy = p.noiseSeed * 1.7f + time * noiseSpeed_;
            // 3-octave angle for richer organic feel
            float a1 = ofNoise(tNx, tNy);
            float a2 = ofNoise(tNx * 2.3f, tNy * 2.3f);
            float a3 = ofNoise(tNx * 5.1f + 50.0f, tNy * 5.1f - 30.0f);
            float angle = (a1 * 0.5f + a2 * 0.3f + a3 * 0.2f) * glm::two_pi<float>();
            // 3-octave radius
            float r1 = ofNoise(tNx + 100.0f, tNy + 100.0f);
            float r2 = ofNoise(tNx * 2.3f + 50.0f, tNy * 2.3f + 50.0f);
            float r3 = ofNoise(tNx * 4.7f - 20.0f, tNy * 4.7f + 70.0f);
            float radius = (r1 * 0.5f + r2 * 0.3f + r3 * 0.2f) * swarmRadius_;
            glm::vec2 noiseOffset(
                glm::cos(angle) * radius * (noiseAmount_ / glm::max(swarmRadius_, 1.0f)),
                glm::sin(angle) * radius * (noiseAmount_ / glm::max(swarmRadius_, 1.0f))
            );
            target = center + noiseOffset;
        }

        glm::vec2 offset = target - p.pos;

        // Variable spring: lag particles have reduced stiffness for trailing effect
        float lagSpring = 1.0f - p.lagFactor * lagRatio_ * 0.9f;
        lagSpring = glm::clamp(lagSpring, 0.1f, 1.0f);
        float pk = k * lagSpring;
        float pd = 2.0f * glm::sqrt(pk);  // critical damping per particle

        glm::vec2 force = pk * offset - pd * p.vel;

        // Mouse attraction — strong in mouse mode, gentle otherwise
        if (mouseActive_ && mouseStrength_ > 0.001f) {
            glm::vec2 toMouse = mousePos_ - p.pos;
            float mouseDist = glm::length(toMouse) + 1.0f;
            float modeBoost = trackMouse_ ? 3.0f : 1.0f;
            float influence = mouseStrength_ * 8.0f * modeBoost / (1.0f + mouseDist * 0.008f);
            force += toMouse * influence;
        }

        p.vel += force * dt;
        p.pos += p.vel * dt;
    }

    // Update click pings
    for (auto& ping : clickPings_) ping.life -= dt;
    clickPings_.erase(std::remove_if(clickPings_.begin(), clickPings_.end(),
        [](const ClickPing& p) { return p.life <= 0.0f; }), clickPings_.end());

    applyParticleResize();
}

// ============================================================================
// Draw
// ============================================================================

void EnjambreOrganicoScene::draw() {
    ofPushStyle();

    // Default alpha blending — like PinturaFluorescente.
    // Each particle has its own alpha based on distance from anchor,
    // so overlapping creates depth without saturating to white.
    ofEnableAlphaBlending();

    float alphaMult = sceneAlpha_;
    float time = ofGetElapsedTimef();

    // Depth relation: body proximity scales particle size.
    float depthScale = 0.7f + 0.6f * averageProximity(interactionPoints_);

    ofFill();

    for (size_t pi = 0; pi < particles_.size(); pi++) {
        const auto& p = particles_[pi];
        ofFloatColor c = p.color;

        // Per-particle alpha: falls off with distance from anchor
        // (like PinturaFluorescente's life-based fade, but distance-based)
        float dist = glm::distance(p.pos, anchor_.pos);
        float distNorm = glm::clamp(dist / glm::max(swarmRadius_, 1.0f), 0.0f, 1.0f);
        float distAlpha = 1.0f - distNorm;  // 1.0 at center, 0.0 at edge

        // Blend distance alpha with a base floor so edge particles don't vanish
        float pAlpha = glm::mix(0.15f, 1.0f, distAlpha);

        // Gentle per-particle pulse for organic variety
        float pulse = 0.85f + 0.15f * ofNoise(p.noiseSeed * 0.5f + time * 0.4f);

        float sz = groupSizes_[pi % 8] * p.sizeFactor * p.baseSize * pulse * depthScale;
        int alpha = static_cast<int>(255.0f * pAlpha * alphaMult);

        ofSetColor(c.r * 255.0f, c.g * 255.0f, c.b * 255.0f, alpha);
        ofDrawCircle(p.pos.x, p.pos.y, sz);
    }

    // Draw click pings — soft expanding rings where user clicked
    for (const auto& ping : clickPings_) {
        float ratio = ping.life / ping.maxLife;
        float r = (1.0f - ratio) * 60.0f + 8.0f;  // expand from 8→68px
        float alpha = ratio * 50.0f;                 // fade out
        ofNoFill();
        ofSetColor(255, 255, 255, alpha);
        ofSetLineWidth(1.5f);
        ofDrawCircle(ping.pos.x, ping.pos.y, r);
    }
    ofFill();

    ofEnableAlphaBlending();

    if (debugMarkers_) {
        drawDebugMarkers();
    }

    ofPopStyle();
}

// ============================================================================
// Debug Markers
// ============================================================================

void EnjambreOrganicoScene::drawDebugMarkers() {
    // Active joint dots: green
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

    // Anchor cross + state label: yellow
    ofSetColor(255, 255, 0, 230);
    ofSetLineWidth(2.0f);
    float cx = anchor_.pos.x;
    float cy = anchor_.pos.y;
    ofDrawLine(cx - 12.0f, cy, cx + 12.0f, cy);
    ofDrawLine(cx, cy - 12.0f, cx, cy + 12.0f);

    std::string stateLabel = (anchor_.state == AnchorState::Dwelling) ? "DWELL" : "TRAVEL";
    ofDrawBitmapStringHighlight(stateLabel, cx + 16.0f, cy + 4.0f,
                                ofColor(0, 0, 0, 200), ofColor::yellow);

    // Target joint ring: cyan
    auto seq = buildJointSequence();
    if (!seq.empty() && anchor_.state == AnchorState::Traveling) {
        for (const auto& entry : seq) {
            if (entry.id == anchor_.targetJointId) {
                ofSetColor(0, 255, 255, 200);
                ofNoFill();
                ofSetLineWidth(2.0f);
                ofDrawCircle(entry.position.x, entry.position.y, 16.0f);
                break;
            }
        }
    }

    // Segment line: white
    if (!seq.empty() && anchor_.state == AnchorState::Traveling) {
        glm::vec2 currentPos = anchor_.pos;
        for (const auto& entry : seq) {
            if (entry.id == anchor_.targetJointId) {
                ofSetColor(255, 255, 255, 150);
                ofSetLineWidth(1.0f);
                ofDrawLine(currentPos.x, currentPos.y, entry.position.x, entry.position.y);
                break;
            }
        }
    }

    ofSetLineWidth(1.0f);
    ofFill();
}

// ============================================================================
// Parameter Access
// ============================================================================

std::vector<std::string> EnjambreOrganicoScene::getParameterIds() const {
    std::vector<std::string> ids;
    for (const auto& entry : parameters_) {
        ids.push_back(entry.first);
    }
    return ids;
}

std::vector<std::string> EnjambreOrganicoScene::getInteractionPointIds() const {
    std::vector<std::string> ids;
    for (const auto& entry : interactionPoints_) {
        ids.push_back(entry.first);
    }
    return ids;
}

void EnjambreOrganicoScene::clearInteractionPoints() {
    interactionPoints_.clear();
}

std::shared_ptr<Parameter> EnjambreOrganicoScene::getParameter(const std::string& paramId) {
    auto it = parameters_.find(paramId);
    if (it != parameters_.end()) return it->second;
    return nullptr;
}

bool EnjambreOrganicoScene::setParameter(const std::string& paramId, float value) {
    // Strip scene prefix if present (e.g., "enjambreOrganico.travelDuration" → "travelDuration")
    std::string key = paramId;
    if (key.find("enjambreOrganico.") == 0) {
        key = key.substr(std::string("enjambreOrganico.").length());
    }
    auto it = parameters_.find(key);
    if (it == parameters_.end() || !it->second) return false;
    it->second->currentValue = value;
    syncParamsFromMap();
    return true;
}

bool EnjambreOrganicoScene::setParameter(const std::string& paramId, const glm::vec2& value) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[paramId];
    pt.sourceName = paramId;
    pt.decayRate = 0.08f;
    pt.setTarget(value, 1.0f, ofGetWidth(), ofGetHeight());
    return true;
}

bool EnjambreOrganicoScene::setParameter(const std::string& paramId, const glm::vec3& value) {
    if (!isActive_) return true;
    auto& pt = interactionPoints_[paramId];
    pt.sourceName = paramId;
    pt.decayRate = 0.08f;
    pt.setTarget(glm::vec2(value.x, value.y), 1.0f, ofGetWidth(), ofGetHeight(), value.z);
    return true;
}

// ============================================================================
// Preset / Metadata
// ============================================================================

std::vector<SuggestedMapping> EnjambreOrganicoScene::getSuggestedMappings() const { return {}; }

Preset EnjambreOrganicoScene::getDefaultPreset() const {
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

SceneMetadata EnjambreOrganicoScene::getSceneMetadata() const {
    SceneMetadata metadata;
    metadata.id = getId();
    metadata.name = getName();
    metadata.description = "Enjambre organico con un ancla lider que viaja lentamente entre joints activos.";
    metadata.author = "Proyecto Danza";
    metadata.version = "1.0.0";
    metadata.tags = {"enjambre", "spring", "perlin", "ldr-follower", "organico"};
    metadata.category = "visual";
    metadata.hasInternalState = true;
    metadata.isTimeBased = false;
    metadata.estimatedDuration = 0.0f;
    return metadata;
}

// ============================================================================
// Beat / Bar / Validation / Resources
// ============================================================================

void EnjambreOrganicoScene::onBeat(int) {}
void EnjambreOrganicoScene::onBar(int) {}

bool EnjambreOrganicoScene::isValid() const { return isValid_; }
std::string EnjambreOrganicoScene::getInvalidReason() const { return invalidReason_; }

void EnjambreOrganicoScene::unloadResources() {}
void EnjambreOrganicoScene::reloadResources() {}

// ============================================================================
// Input Handlers
// ============================================================================

void EnjambreOrganicoScene::keyPressed(int key) {
    if (key == 'k' || key == 'K') {
        trackMouse_ = !trackMouse_;
        if (trackMouse_) {
            mouseActive_ = true;  // force mouse mode on immediately
        }
    }
}
void EnjambreOrganicoScene::keyReleased(int) {}
void EnjambreOrganicoScene::mousePressed(int x, int y, int) {
    mousePos_ = glm::vec2(x, y);
    mouseActive_ = true;
    clickPings_.push_back({glm::vec2(x, y), 0.6f, 0.6f});
}
void EnjambreOrganicoScene::mouseReleased(int, int, int) {}
void EnjambreOrganicoScene::mouseDragged(int x, int y, int) {
    mousePos_ = glm::vec2(x, y);
    mouseActive_ = true;
}
void EnjambreOrganicoScene::mouseMoved(int x, int y) {
    mousePos_ = glm::vec2(x, y);
    mouseActive_ = true;
}
void EnjambreOrganicoScene::windowResized(int, int) {}

// ============================================================================
// Factory
// ============================================================================

ScenePtr CreateEnjambreOrganicoScene() {
    return std::make_shared<EnjambreOrganicoScene>();
}

} // namespace modular
