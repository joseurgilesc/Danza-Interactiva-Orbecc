#include "mapping/OscMappingEngine.h"
#include "engine/SceneEngine.h"
#include "presets/PresetManager.h"
#include "sequencer/SceneSequencer.h"
#include "scenes/BaseScene.h"
#include <ofLog.h>
#include <ofMath.h>
#include <cmath>
#include "ofJson.h"
#include "ofFileUtils.h"
#include <sstream>
#include <vector>

namespace modular {

// ============================================================================
// Constructor
// ============================================================================

OscMappingEngine::OscMappingEngine(SceneEngine* sceneEngine)
    : sceneEngine_(sceneEngine)
    , idCounter_(0)
    , learnState_(LearnState::IDLE) {}

// ============================================================================
// generateId — produces unique mapping identifiers
// ============================================================================

std::string OscMappingEngine::generateId() {
    return "osc-map-" + std::to_string(++idCounter_);
}

// ============================================================================
// findDuplicate — checks for existing (oscAddress, targetParameterId) pair
// ============================================================================

OscMapping* OscMappingEngine::findDuplicate(const std::string& oscAddr,
                                              const std::string& targetParam) const {
    for (auto& mapping : mappings_) {
        if (mapping.oscAddress == oscAddr && mapping.targetParameterId == targetParam) {
            // Const cast is safe here — we return non-owning pointer for read/modify
            return const_cast<OscMapping*>(&mapping);
        }
    }
    return nullptr;
}

// ============================================================================
// addMapping — registers a new mapping (rejects duplicates)
// ============================================================================

std::string OscMappingEngine::addMapping(const OscMapping& mapping) {
    if (findDuplicate(mapping.oscAddress, mapping.targetParameterId) != nullptr) {
        ofLogWarning("OscMappingEngine")
            << "Duplicate mapping rejected: " << mapping.oscAddress
            << " -> " << mapping.targetParameterId;
        return "";
    }

    OscMapping copy = mapping;
    copy.mappingId = generateId();
    mappings_.push_back(std::move(copy));
    return mappings_.back().mappingId;
}

// ============================================================================
// removeMapping — removes a mapping by its id
// ============================================================================

bool OscMappingEngine::removeMapping(const std::string& mappingId) {
    for (auto it = mappings_.begin(); it != mappings_.end(); ++it) {
        if (it->mappingId == mappingId) {
            mappings_.erase(it);
            return true;
        }
    }
    return false;
}

// ============================================================================
// getMappings — returns const reference to all mappings
// ============================================================================

std::vector<OscMapping>& OscMappingEngine::getMappings() {
    return mappings_;
}

const std::vector<OscMapping>& OscMappingEngine::getMappings() const {
    return mappings_;
}

// ============================================================================
// findMapping — linear search by mappingId
// ============================================================================

OscMapping* OscMappingEngine::findMapping(const std::string& mappingId) {
    for (auto& mapping : mappings_) {
        if (mapping.mappingId == mappingId) {
            return &mapping;
        }
    }
    return nullptr;
}

// ============================================================================
// processOscMessage — core dispatch: match, scale, apply to scene parameter
// ============================================================================

void OscMappingEngine::processOscMessage(const std::string& oscAddress, float value) {
    // ── PRESET / CUE ROUTING (checked BEFORE mapping loop) ──────────────────

    // 1. /cue <N> — trigger sequencer cue
    if (oscAddress == "/cue") {
        if (sceneSequencer_) {
            int cueNum = static_cast<int>(std::round(value));
            if (cueNum > 0) {
                sceneSequencer_->triggerCue(cueNum);
            }
        }
        return;  // Matched — skip mapping dispatch
    }

    // 2. /preset/<sceneId>/<presetName>[/<duration>]
    if (oscAddress.find("/preset/") == 0 && oscAddress.size() > 8) {
        if (presetManager_) {
            // Split address on '/'
            std::vector<std::string> parts;
            std::string addr = oscAddress;
            // Remove leading '/'
            if (!addr.empty() && addr[0] == '/') addr = addr.substr(1);
            std::stringstream ss(addr);
            std::string token;
            while (std::getline(ss, token, '/')) {
                if (!token.empty()) parts.push_back(token);
            }

            // Expected: parts[0]="preset", parts[1]=sceneId, parts[2]=presetName, parts[3]=duration (optional)
            if (parts.size() >= 3) {
                std::string sceneId = parts[1];
                std::string presetName = parts[2];
                float duration = (parts.size() >= 4) ? std::stof(parts[3]) : 0.0f;

                auto scene = sceneEngine_->getScene(sceneId);
                if (scene) {
                    presetManager_->applyPreset(scene, presetName, duration);
                    ofLogNotice("OscMappingEngine")
                        << "OSC /preset → scene=" << sceneId
                        << " preset=" << presetName
                        << " duration=" << duration;
                } else {
                    ofLogWarning("OscMappingEngine")
                        << "OSC /preset: scene '" << sceneId << "' not found";
                }
            } else if (parts.size() == 2) {
                // /preset/<sceneId> without presetName — log warning
                ofLogWarning("OscMappingEngine")
                    << "OSC /preset: incomplete address, expected /preset/<sceneId>/<presetName>";
            }
        }
        return;  // Matched — skip mapping dispatch
    }

    // ── LEARNING MODE ───────────────────────────────────────────────────────
    // 1. If learnState_ == LEARNING, auto-create mapping from this message
    if (learnState_ == LearnState::LEARNING) {
        OscMapping newMapping;
        newMapping.oscAddress = oscAddress;
        newMapping.targetParameterId = learnTargetParamId_;
        addMapping(newMapping);
        ofLogNotice("OscMappingEngine")
            << "Learning: assigned " << oscAddress << " -> " << learnTargetParamId_;
        learnState_ = LearnState::IDLE;
        learnTargetParamId_.clear();
        learnTimer_ = 0.0f;
        return;
    }

    // 2. Find matching mappings
    for (auto& mapping : mappings_) {
        if (mapping.oscAddress != oscAddress || !mapping.enabled) continue;

        // 3. Check frequency mode gating
        if (!shouldDispatch(mapping, value)) {
            mapping.frameCounter++;
            continue;
        }

        // 4. Resolve targetParameterId -> sceneId.localParamId
        size_t dotPos = mapping.targetParameterId.find('.');
        if (dotPos == std::string::npos) {
            ofLogWarning("OscMappingEngine")
                << "Invalid targetParameterId (no dot): " << mapping.targetParameterId;
            continue;
        }
        std::string sceneId = mapping.targetParameterId.substr(0, dotPos);
        std::string localParamId = mapping.targetParameterId.substr(dotPos + 1);

        // 5. Get scene
        auto scene = sceneEngine_->getScene(sceneId);
        if (!scene) {
            // Skip silently for inactive/unloaded scenes
            continue;
        }

        // 6. Get parameter to know its range for scaling
        auto param = scene->getParameter(localParamId);
        float paramMin = 0.0f, paramMax = 1.0f;
        if (param) {
            paramMin = param->min;
            paramMax = param->max;
        } else {
            ofLogWarning("OscMappingEngine")
                << "Parameter not found: " << mapping.targetParameterId;
        }

        // 7. Scale using transform type + output range
        float scaledValue = scaleValue(value, mapping);

        // 7. Apply to scene
        scene->setParameter(localParamId, scaledValue);

        // 8. Update runtime state
        mapping.lastValue = value;
        mapping.frameCounter = 1;  // Set to 1 so next frame 1%N != 0 (THROTTLED fix)
    }
}

// ============================================================================
// scaleValue — applies transform and output range mapping
// ============================================================================

float OscMappingEngine::scaleValue(float input, const OscMapping& mapping) const {
    // 1. Clamp input to [minInput, maxInput] (handle inversion)
    float t;
    if (mapping.maxInput != mapping.minInput) {
        t = (input - mapping.minInput) / (mapping.maxInput - mapping.minInput);
    } else {
        t = 0.0f;
    }
    t = std::max(0.0f, std::min(1.0f, t)); // Clamp to 0-1

    // 2. Apply transform
    switch (mapping.transformType) {
        case OscTransformType::EXPONENTIAL:
            t = t * t; // Quadratic easing
            break;
        case OscTransformType::LOGARITHMIC:
            // log2(1 + 9*t) / log2(10) — maps 0→0, 1→1 with log curve
            if (t > 0.0f) {
                t = std::log2(1.0f + 9.0f * t) / std::log2(10.0f);
            }
            break;
        case OscTransformType::INVERT:
            t = 1.0f - t;
            break;
        case OscTransformType::LINEAR:
        default:
            // t is already linear
            break;
    }

    // 3. Map to output range (always use mapping's output range)
    return ofMap(t, 0.0f, 1.0f, mapping.minOutput, mapping.maxOutput);
}

// ============================================================================
// shouldDispatch — frequency-mode gating
// ============================================================================

bool OscMappingEngine::shouldDispatch(const OscMapping& mapping, float value) {
    switch (mapping.frequencyMode) {
        case OscFrequencyMode::EVERY_FRAME:
            return true;

        case OscFrequencyMode::ON_CHANGE:
            return std::abs(value - mapping.lastValue) > kOnChangeEpsilon;

        case OscFrequencyMode::THROTTLED:
            return (mapping.frameCounter % mapping.throttleFrames) == 0;

        default:
            return true;
    }
}

// ============================================================================
// Preset / Sequencer Routing setters
// ============================================================================

void OscMappingEngine::setPresetManager(PresetManager* pm) {
    presetManager_ = pm;
}

void OscMappingEngine::setSceneSequencer(SceneSequencer* seq) {
    sceneSequencer_ = seq;
}

// ============================================================================
// update — per-frame housekeeping: learning timeout, frame counters
// ============================================================================

void OscMappingEngine::update(float deltaTime) {
    if (learnState_ == LearnState::LEARNING) {
        learnTimer_ += deltaTime;
        if (learnTimer_ >= kLearnTimeout) {
            learnState_ = LearnState::TIMEOUT;
            ofLogWarning("OscMappingEngine")
                << "Learning timeout for: " << learnTargetParamId_;
            learnTargetParamId_.clear();
            learnTimer_ = 0.0f;
        }
    }

    // TIMEOUT persists for one full frame so UI can display it
    else if (learnState_ == LearnState::TIMEOUT) {
        learnState_ = LearnState::IDLE;
    }

    // Frame counters for THROTTLED mode are incremented in shouldDispatch
    // and reset to 0 in processOscMessage upon dispatch.
}

// ============================================================================
// Learning FSM
// ============================================================================

bool OscMappingEngine::startLearning(const std::string& targetParameterId) {
    if (learnState_ == LearnState::LEARNING) {
        ofLogWarning("OscMappingEngine") << "Learning already active for: " << learnTargetParamId_ 
                                         << ", rejecting: " << targetParameterId;
        return false;
    }
    learnState_ = LearnState::LEARNING;
    learnTargetParamId_ = targetParameterId;
    learnTimer_ = 0.0f;
    ofLogNotice("OscMappingEngine") << "Learning started for: " << targetParameterId;
    return true;
}

void OscMappingEngine::cancelLearning() {
    if (learnState_ == LearnState::LEARNING) {
        ofLogNotice("OscMappingEngine") << "Learning cancelled for: " << learnTargetParamId_;
    }
    learnState_ = LearnState::IDLE;
    learnTargetParamId_.clear();
    learnTimer_ = 0.0f;
}

LearnState OscMappingEngine::getLearnState() const {
    return learnState_;
}

std::string OscMappingEngine::getLearningTarget() const {
    return learnTargetParamId_;
}

float OscMappingEngine::getLearnTimeRemaining() const {
    if (learnState_ != LearnState::LEARNING) return 0.0f;
    return std::max(0.0f, kLearnTimeout - learnTimer_);
}

// ============================================================================
// Persistence
// ============================================================================

bool OscMappingEngine::saveToJson(const std::string& filepath) const {
    ofJson jsonArray = ofJson::array();
    
    for (const auto& mapping : mappings_) {
        ofJson entry;
        entry["mappingId"] = mapping.mappingId;
        entry["oscAddress"] = mapping.oscAddress;
        entry["targetParameterId"] = mapping.targetParameterId;
        entry["minInput"] = mapping.minInput;
        entry["maxInput"] = mapping.maxInput;
        entry["minOutput"] = mapping.minOutput;
        entry["maxOutput"] = mapping.maxOutput;
        entry["transformType"] = static_cast<int>(mapping.transformType);
        entry["frequencyMode"] = static_cast<int>(mapping.frequencyMode);
        entry["throttleFrames"] = mapping.throttleFrames;
        entry["enabled"] = mapping.enabled;
        // NOTE: lastValue and frameCounter are runtime-only, NOT persisted
        
        jsonArray.push_back(entry);
    }
    
    std::string dir = ofFilePath::getEnclosingDirectory(filepath);
    if (!dir.empty()) {
        ofDirectory::createDirectory(dir, true, true);
    }
    
    return ofSaveJson(filepath, jsonArray);
}

bool OscMappingEngine::loadFromJson(const std::string& filepath) {
    ofFile file(filepath);
    if (!file.exists()) {
        ofLogNotice("OscMappingEngine") << "No mappings file found: " << filepath << " — starting with empty set";
        return true; // Not an error — first run
    }
    
    ofJson jsonArray = ofLoadJson(filepath);
    if (jsonArray.is_null() || !jsonArray.is_array()) {
        ofLogError("OscMappingEngine") << "Invalid or corrupt JSON in: " << filepath;
        return false;
    }
    
    mappings_.clear();
    idCounter_ = 0;
    
    for (const auto& entry : jsonArray) {
        OscMapping mapping;
        
        // mappingId
        if (entry.contains("mappingId") && entry["mappingId"].is_string()) {
            mapping.mappingId = entry["mappingId"].get<std::string>();
        } else {
            continue; // Skip invalid entries
        }
        
        // Required fields with defaults
        mapping.oscAddress = entry.value("oscAddress", "");
        mapping.targetParameterId = entry.value("targetParameterId", "");
        mapping.minInput = entry.value("minInput", 0.0f);
        mapping.maxInput = entry.value("maxInput", 1.0f);
        mapping.minOutput = entry.value("minOutput", 0.0f);
        mapping.maxOutput = entry.value("maxOutput", 1.0f);
        mapping.transformType = static_cast<OscTransformType>(entry.value("transformType", 0));
        mapping.frequencyMode = static_cast<OscFrequencyMode>(entry.value("frequencyMode", 0));
        mapping.throttleFrames = entry.value("throttleFrames", 5);
        mapping.enabled = entry.value("enabled", true);
        
        // Runtime fields reset to defaults
        mapping.lastValue = 0.0f;
        mapping.frameCounter = 0;
        
        // Update idCounter_ to avoid collisions
        std::string idNum = mapping.mappingId;
        // Extract number from "osc-map-N"
        size_t pos = idNum.rfind('-');
        if (pos != std::string::npos) {
            try {
                int num = std::stoi(idNum.substr(pos + 1));
                if (num > idCounter_) idCounter_ = num;
            } catch (...) {}
        }
        
        mappings_.push_back(mapping);
    }
    
    ofLogNotice("OscMappingEngine") << "Loaded " << mappings_.size() << " mappings from: " << filepath;
    return true;
}

} // namespace modular
