#include "mapping/KinectMappingEngine.h"
#include "mapping/KinectMappingTypes.h"
#include "registry/ParameterRegistry.h"
#include "engine/SceneEngine.h"
#include "scenes/BaseScene.h"
#include "mapping/ParameterTypes.h"
#include <ofLog.h>
#include <ofMath.h>
#include <cmath>
#include <algorithm>
#include "ofJson.h"
#include "ofFileUtils.h"

namespace modular {

// ============================================================================
// jointTypeToName — JointType enum → lowercase underscore string
// ============================================================================

std::string jointTypeToName(JointType type) {
    switch (type) {
        case JointType::HEAD:           return "head";
        case JointType::NECK:           return "neck";
        case JointType::SHOULDER_LEFT:  return "shoulder_left";
        case JointType::SHOULDER_RIGHT: return "shoulder_right";
        case JointType::ELBOW_LEFT:     return "elbow_left";
        case JointType::ELBOW_RIGHT:    return "elbow_right";
        case JointType::WRIST_LEFT:     return "wrist_left";
        case JointType::WRIST_RIGHT:    return "wrist_right";
        case JointType::HAND_LEFT:      return "hand_left";
        case JointType::HAND_RIGHT:     return "hand_right";
        case JointType::HIP_LEFT:       return "hip_left";
        case JointType::HIP_RIGHT:      return "hip_right";
        case JointType::KNEE_LEFT:      return "knee_left";
        case JointType::KNEE_RIGHT:     return "knee_right";
        case JointType::ANKLE_LEFT:     return "ankle_left";
        case JointType::ANKLE_RIGHT:    return "ankle_right";
        case JointType::SPINE_BASE:     return "spine_base";
        case JointType::SPINE_MID:      return "spine_mid";
        default:                        return "hand_right";
    }
}

// ============================================================================
// jointNameToType — lowercase underscore string → JointType enum
// ============================================================================

JointType jointNameToType(const std::string& name) {
    if (name == "head")            return JointType::HEAD;
    if (name == "neck")            return JointType::NECK;
    if (name == "shoulder_left")   return JointType::SHOULDER_LEFT;
    if (name == "shoulder_right")  return JointType::SHOULDER_RIGHT;
    if (name == "elbow_left")      return JointType::ELBOW_LEFT;
    if (name == "elbow_right")     return JointType::ELBOW_RIGHT;
    if (name == "wrist_left")      return JointType::WRIST_LEFT;
    if (name == "wrist_right")     return JointType::WRIST_RIGHT;
    if (name == "hand_left")       return JointType::HAND_LEFT;
    if (name == "hand_right")      return JointType::HAND_RIGHT;
    if (name == "hip_left")        return JointType::HIP_LEFT;
    if (name == "hip_right")       return JointType::HIP_RIGHT;
    if (name == "knee_left")       return JointType::KNEE_LEFT;
    if (name == "knee_right")      return JointType::KNEE_RIGHT;
    if (name == "ankle_left")      return JointType::ANKLE_LEFT;
    if (name == "ankle_right")     return JointType::ANKLE_RIGHT;
    if (name == "spine_base")      return JointType::SPINE_BASE;
    if (name == "spine_mid")       return JointType::SPINE_MID;
    return JointType::HAND_RIGHT;
}

// ============================================================================
// kinectMappingToJson — serialize a single KinectMapping to a JSON object
// ============================================================================
// Free function (declared in KinectMappingTypes.h). Extracted from
// saveToJson so PresetManager and other consumers can serialize without
// depending on KinectMappingEngine.

ofJson kinectMappingToJson(const KinectMapping& m) {
    ofJson entry;
    entry["mappingId"]         = m.mappingId;
    entry["jointSource"]       = m.jointSource;
    entry["targetParameterId"] = m.targetParameterId;
    entry["condition"]         = static_cast<int>(m.condition);
    entry["threshold"]         = m.threshold;
    entry["minRange"]          = m.minRange;
    entry["maxRange"]          = m.maxRange;
    entry["gestureType"]       = m.gestureType;
    entry["minInput"]          = m.minInput;
    entry["maxInput"]          = m.maxInput;
    entry["minOutput"]         = m.minOutput;
    entry["maxOutput"]         = m.maxOutput;
    entry["transformType"]     = static_cast<int>(m.transformType);
    entry["enabled"]           = m.enabled;
    entry["smoothing"]         = m.smoothing;
    // NOTE: lastValue is runtime-only, NOT persisted
    return entry;
}

// ============================================================================
// kinectMappingFromJson — deserialize a single KinectMapping from a JSON object
// ============================================================================
// Free function (declared in KinectMappingTypes.h). Extracted from
// loadFromJson. Returns a mapping with empty mappingId on parse failure;
// callers skip empty-id entries. runtime fields (lastValue) are reset to 0.

KinectMapping kinectMappingFromJson(const ofJson& j) {
    KinectMapping mapping;

    // mappingId is the identity field — empty means invalid entry
    if (j.contains("mappingId") && j["mappingId"].is_string()) {
        mapping.mappingId = j["mappingId"].get<std::string>();
    }
    // If no valid mappingId, return with empty id (caller skips)

    mapping.jointSource       = j.value("jointSource", "");
    mapping.targetParameterId = j.value("targetParameterId", "");
    mapping.condition         = static_cast<KinectCondition>(j.value("condition", 4));
    mapping.threshold         = j.value("threshold", 0.5f);
    mapping.minRange          = j.value("minRange", 0.0f);
    mapping.maxRange          = j.value("maxRange", 1.0f);
    mapping.gestureType       = j.value("gestureType", "");
    mapping.minInput          = j.value("minInput", 0.0f);
    mapping.maxInput          = j.value("maxInput", 1.0f);
    mapping.minOutput         = j.value("minOutput", 0.0f);
    mapping.maxOutput         = j.value("maxOutput", 1.0f);
    // transformType may be an int (persisted format) or a string name
    // (older preset files, e.g. gridDissolve.json use "VEC2_XY").
    if (j.contains("transformType") && j["transformType"].is_string()) {
        std::string t = j["transformType"].get<std::string>();
        if      (t == "LINEAR")       mapping.transformType = KinectTransformType::LINEAR;
        else if (t == "EXPONENTIAL")  mapping.transformType = KinectTransformType::EXPONENTIAL;
        else if (t == "LOGARITHMIC")  mapping.transformType = KinectTransformType::LOGARITHMIC;
        else if (t == "INVERT")       mapping.transformType = KinectTransformType::INVERT;
        else if (t == "VEC2_XY")      mapping.transformType = KinectTransformType::VEC2_XY;
        else if (t == "VEC3_XYZ")     mapping.transformType = KinectTransformType::VEC3_XYZ;
        else                          mapping.transformType = KinectTransformType::LINEAR;
    } else {
        mapping.transformType = static_cast<KinectTransformType>(j.value("transformType", 0));
    }
    mapping.enabled           = j.value("enabled", true);
    mapping.smoothing         = j.value("smoothing", 0.0f);

    // Runtime fields reset to defaults
    mapping.lastValue = 0.0f;

    return mapping;
}

// ============================================================================
// Constructor
// ============================================================================

KinectMappingEngine::KinectMappingEngine(SceneEngine* sceneEngine)
    : sceneEngine_(sceneEngine)
    , idCounter_(0) {}

// ============================================================================
// generateId — produces unique mapping identifiers
// ============================================================================

std::string KinectMappingEngine::generateId() {
    return "kinect-map-" + std::to_string(++idCounter_);
}

// ============================================================================
// findDuplicate — checks for existing (jointSource, targetParameterId) pair
// ============================================================================

KinectMapping* KinectMappingEngine::findDuplicate(const std::string& jointSource,
                                                    const std::string& targetParam) const {
    for (auto& mapping : mappings_) {
        if (mapping.jointSource == jointSource && mapping.targetParameterId == targetParam) {
            return const_cast<KinectMapping*>(&mapping);
        }
    }
    return nullptr;
}

// ============================================================================
// addMapping — registers a new mapping (rejects duplicates)
// ============================================================================

std::string KinectMappingEngine::addMapping(const KinectMapping& mapping) {
    if (findDuplicate(mapping.jointSource, mapping.targetParameterId) != nullptr) {
        ofLogWarning("KinectMappingEngine")
            << "Duplicate mapping rejected: " << mapping.jointSource
            << " -> " << mapping.targetParameterId;
        return "";
    }

    KinectMapping copy = mapping;
    copy.mappingId = generateId();
    mappings_.push_back(std::move(copy));
    return mappings_.back().mappingId;
}

// ============================================================================
// removeMapping — removes a mapping by its id
// ============================================================================

bool KinectMappingEngine::removeMapping(const std::string& mappingId) {
    for (auto it = mappings_.begin(); it != mappings_.end(); ++it) {
        if (it->mappingId == mappingId) {
            mappings_.erase(it);
            return true;
        }
    }
    return false;
}

// ============================================================================
// getMappings — returns reference to all mappings
// ============================================================================

std::vector<KinectMapping>& KinectMappingEngine::getMappings() {
    return mappings_;
}

const std::vector<KinectMapping>& KinectMappingEngine::getMappings() const {
    return mappings_;
}

// ============================================================================
// findMapping — linear search by mappingId
// ============================================================================

KinectMapping* KinectMappingEngine::findMapping(const std::string& mappingId) {
    for (auto& mapping : mappings_) {
        if (mapping.mappingId == mappingId) {
            return &mapping;
        }
    }
    return nullptr;
}

// ============================================================================
// evaluateCondition — determines if a joint value meets the mapping's condition
// ============================================================================

bool KinectMappingEngine::evaluateCondition(const KinectMapping& mapping, float value) const {
    switch (mapping.condition) {
        case KinectCondition::ABOVE_THRESHOLD:
            return value > mapping.threshold;

        case KinectCondition::BELOW_THRESHOLD:
            return value < mapping.threshold;

        case KinectCondition::IN_RANGE:
            return value >= mapping.minRange && value <= mapping.maxRange;

        case KinectCondition::GESTURE:
            // Gestures are handled via gesture callback, not joint data
            return false;

        case KinectCondition::CONTINUOUS:
        default:
            return true;
    }
}

// ============================================================================
// scaleValue — applies transform and output range mapping
// ============================================================================
// EXACT same logic as OscMappingEngine::scaleValue()

float KinectMappingEngine::scaleValue(float input, const KinectMapping& mapping) const {
    // 1. Clamp input to [minInput, maxInput] range
    float t;
    if (mapping.maxInput != mapping.minInput) {
        t = (input - mapping.minInput) / (mapping.maxInput - mapping.minInput);
    } else {
        t = 0.0f;
    }
    t = std::max(0.0f, std::min(1.0f, t)); // Clamp to 0-1

    // 2. Apply transform
    switch (mapping.transformType) {
        case KinectTransformType::EXPONENTIAL:
            t = t * t; // Quadratic easing
            break;
        case KinectTransformType::LOGARITHMIC:
            // log2(1 + 9*t) / log2(10) — maps 0→0, 1→1 with log curve
            if (t > 0.0f) {
                t = std::log2(1.0f + 9.0f * t) / std::log2(10.0f);
            }
            break;
        case KinectTransformType::INVERT:
            t = 1.0f - t;
            break;
        case KinectTransformType::VEC2_XY:
        case KinectTransformType::VEC3_XYZ:
            // VEC2_XY / VEC3_XYZ bypass scaleValue entirely — handled in processJointData
            break;
        case KinectTransformType::LINEAR:
        default:
            // t is already linear
            break;
    }

    // 3. Map to output range
    return ofMap(t, 0.0f, 1.0f, mapping.minOutput, mapping.maxOutput);
}

// ============================================================================
// processJointData — core dispatch: match, evaluate, scale, dispatch
// ============================================================================

void KinectMappingEngine::processJointData(int bodyId, JointType jointType, const Joint& joint) {
    // 1. Build joint source name (e.g. "hand_right")
    std::string jointName = jointTypeToName(jointType);
    
    // Cache NECK and SPINE_MID positions for body-center zoom
    if (jointType == JointType::NECK) {
        neckX_ = joint.position.x;
        neckY_ = joint.position.y;
        hasCenter_ = true;
    } else if (jointType == JointType::SPINE_MID) {
        spineMidX_ = joint.position.x;
        spineMidY_ = joint.position.y;
        hasCenter_ = true;
    }

    // 2. Iterate over all mappings with matching joint name
    for (auto& mapping : mappings_) {
        if (!mapping.enabled) continue;

        // Check if this mapping targets this joint type
        std::string mappingJointName = parseJointName(mapping.jointSource);
        if (mappingJointName != jointName) continue;

        // GESTURE condition is handled by gesture callback, skip in joint dispatch
        if (mapping.condition == KinectCondition::GESTURE) continue;

        // ---- VEC2_XY / VEC3_XYZ dispatch: raw (X,Y[,Z]) to scene, no scaling/smoothing ----
        if (mapping.transformType == KinectTransformType::VEC2_XY ||
            mapping.transformType == KinectTransformType::VEC3_XYZ) {
            // Log first few matches for diagnostics
            static int vec2MatchCount = 0;
            if (vec2MatchCount < 5) {
                ofLogNotice("KinectMappingEngine")
                    << "VEC dispatch: joint=" << jointName
                    << " bodyId=" << bodyId
                    << " pos=(" << ofToString(joint.position.x, 3) << "," << ofToString(joint.position.y, 3) << ","
                    << ofToString(joint.position.z, 3) << ")"
                    << " conf=" << ofToString(joint.confidence, 2)
                    << " -> " << mapping.targetParameterId;
                vec2MatchCount++;
            }
            
            // Resolve targetParameterId → sceneId + localParamId
            size_t dotPos = mapping.targetParameterId.find('.');
            if (dotPos == std::string::npos) {
                ofLogWarning("KinectMappingEngine")
                    << "Invalid targetParameterId for VEC dispatch: " << mapping.targetParameterId;
                continue;
            }
            std::string sceneId = mapping.targetParameterId.substr(0, dotPos);
            std::string localParamId = mapping.targetParameterId.substr(dotPos + 1);

            auto scene = sceneEngine_->getScene(sceneId);
            if (scene) {
                // Read global skeleton scale
                float scale = 1.0f;
                auto scaleParam = ParameterRegistry::getGlobal().getParam("skeleton.scale");
                if (scaleParam.has_value()) {
                    scale = scaleParam->currentValue;
                }
                
                // Use real body center (NECK ↔ SPINE_MID midpoint), fallback to (0.5, 0.5)
                float centerX = hasCenter_ ? (neckX_ + spineMidX_) * 0.5f : 0.5f;
                float centerY = hasCenter_ ? (neckY_ + spineMidY_) * 0.5f : 0.5f;
                
                float scaledX = centerX + (joint.position.x - centerX) * scale;
                float scaledY = centerY + (joint.position.y - centerY) * scale;

                // Dispatch normalized coords (0-1) — conversion to pixels
                // happens in InteractionPoint::setTarget inside the scene.
                if (mapping.transformType == KinectTransformType::VEC3_XYZ) {
                    // Include real depth (z) for body relation.
                    scene->setParameter(localParamId, glm::vec3(scaledX, scaledY, joint.position.z));
                } else {
                    scene->setParameter(localParamId, glm::vec2(scaledX, scaledY));
                }
            }
            continue;  // skip SCALAR path below
        }

        // ---- SCALAR dispatch (existing path) ----
        char axis = parseJointAxis(mapping.jointSource);

        // 4. Get scalar value from joint position
        float value = getJointAxisValue(joint.position, axis);

        // 5. Evaluate condition
        bool conditionMet = evaluateCondition(mapping, value);

        // 6. Compute raw output based on condition type
        float outputValue;
        if (mapping.condition == KinectCondition::CONTINUOUS) {
            // CONTINUOUS: scale through full pipeline
            outputValue = scaleValue(value, mapping);
        } else {
            // Binary conditions (ABOVE_THRESHOLD, BELOW_THRESHOLD, IN_RANGE):
            // condition met → maxOutput, condition not met → minOutput
            outputValue = conditionMet ? mapping.maxOutput : mapping.minOutput;
        }

        // 7. Apply EMA smoothing
        float smoothed = mapping.lastValue + (outputValue - mapping.lastValue) * (1.0f - mapping.smoothing);
        mapping.lastValue = smoothed;

        // 8. Resolve targetParameterId → sceneId + localParamId
        size_t dotPos = mapping.targetParameterId.find('.');
        if (dotPos == std::string::npos) {
            ofLogWarning("KinectMappingEngine")
                << "Invalid targetParameterId (no dot): " << mapping.targetParameterId;
            continue;
        }
        std::string sceneId = mapping.targetParameterId.substr(0, dotPos);
        std::string localParamId = mapping.targetParameterId.substr(dotPos + 1);

        // 9. Get scene
        auto scene = sceneEngine_->getScene(sceneId);
        if (!scene) {
            // Skip silently for inactive/unloaded scenes
            continue;
        }

        // 10. Dispatch to scene parameter
        scene->setParameter(localParamId, smoothed);
    }
}

// ============================================================================
// update — per-frame housekeeping (no-op for now, reserved for future use)
// ============================================================================

void KinectMappingEngine::update(float deltaTime) {
    // Currently no-op. Reserved for future frame-level processing
    // (e.g. frame counters, gesture timeouts, auto-reset logic).
}

// ============================================================================
// Persistence — JSON save/load (exact same pattern as OscMappingEngine)
// ============================================================================

bool KinectMappingEngine::saveToJson(const std::string& filepath) const {
    ofJson jsonArray = ofJson::array();

    for (const auto& mapping : mappings_) {
        jsonArray.push_back(kinectMappingToJson(mapping));
    }

    std::string dir = ofFilePath::getEnclosingDirectory(filepath);
    if (!dir.empty()) {
        ofDirectory::createDirectory(dir, true, true);
    }

    return ofSaveJson(filepath, jsonArray);
}

bool KinectMappingEngine::loadFromJson(const std::string& filepath) {
    ofFile file(filepath);
    if (!file.exists()) {
        ofLogNotice("KinectMappingEngine")
            << "No mappings file found: " << filepath << " — starting with empty set";
        return true; // Not an error — first run
    }

    ofJson jsonArray = ofLoadJson(filepath);
    if (jsonArray.is_null() || !jsonArray.is_array()) {
        ofLogError("KinectMappingEngine") << "Invalid or corrupt JSON in: " << filepath;
        return false;
    }

    mappings_.clear();
    idCounter_ = 0;

    for (const auto& entry : jsonArray) {
        KinectMapping mapping = kinectMappingFromJson(entry);
        if (mapping.mappingId.empty()) {
            continue; // Skip invalid entries
        }

        // Update idCounter_ to avoid collisions
        std::string idNum = mapping.mappingId;
        size_t pos = idNum.rfind('-');
        if (pos != std::string::npos) {
            try {
                int num = std::stoi(idNum.substr(pos + 1));
                if (num > idCounter_) idCounter_ = num;
            } catch (...) {}
        }

        mappings_.push_back(std::move(mapping));
    }

    ofLogNotice("KinectMappingEngine") << "Loaded " << mappings_.size() << " mappings from: " << filepath;
    return true;
}

// ============================================================================
// removeMappingsByScene — erase all mappings for a given scene
// ============================================================================
// Uses erase-remove idiom: removes every mapping whose targetParameterId
// starts with sceneId + "." (e.g. "pinturaFluorescente.wrist_right").

void KinectMappingEngine::removeMappingsByScene(const std::string& sceneId) {
    std::string prefix = sceneId + ".";
    mappings_.erase(
        std::remove_if(mappings_.begin(), mappings_.end(),
            [&prefix](const KinectMapping& m) {
                return m.targetParameterId.compare(0, prefix.size(), prefix) == 0;
            }),
        mappings_.end()
    );
}

// ============================================================================
// addMappings — batch-add mappings with per-entry duplicate detection
// ============================================================================
// Loops and calls addMapping() which already rejects duplicates.
// Returns void — individual duplicates are logged by addMapping().

void KinectMappingEngine::addMappings(const std::vector<KinectMapping>& mappings) {
    for (const auto& mapping : mappings) {
        addMapping(mapping);
    }
}

} // namespace modular
