#include "presets/PresetManager.h"
#include "engine/SceneEngine.h"
#include "mapping/KinectMappingEngine.h"
#include "midi/MidiMappingEngine.h"
#include "ofJson.h"
#include "ofFileUtils.h"
#include <cmath>
#include <algorithm>
#include <set>

namespace modular {

// ============================================================================
// applyEasing — easing curve math
// ============================================================================

float applyEasing(float t, MorphEasing easing) {
    switch (easing) {
        case MorphEasing::LINEAR:      return t;
        case MorphEasing::EASE_IN:     return t * t;
        case MorphEasing::EASE_OUT:    return 1.0f - (1.0f - t) * (1.0f - t);
        case MorphEasing::EASE_IN_OUT: return t < 0.5f ? 2.0f * t * t
                                                        : 1.0f - std::pow(-2.0f * t + 2.0f, 2) / 2.0f;
    }
    return t;
}

// ============================================================================
// Constructor
// ============================================================================

PresetManager::PresetManager(SceneEngine* sceneEngine, KinectMappingEngine* mappingEngine,
                             MidiMappingEngine* midiMappingEngine)
    : sceneEngine_(sceneEngine), mappingEngine_(mappingEngine)
    , midiMappingEngine_(midiMappingEngine) {
}

// ============================================================================
// loadPresets — load JSON preset file for a scene
// ============================================================================

void PresetManager::loadPresets(const std::string& sceneId, const std::string& path) {
    // Clear any previously loaded presets for this scene
    presets_[sceneId].clear();

    // Record the path BEFORE the existence check so savePresets() works
    // even when the file doesn't exist yet (will be created on first save).
    presetPaths_[sceneId] = path;

    if (!ofFile::doesFileExist(path)) {
        ofLogWarning("PresetManager") << "Preset file not found: " << path
                                      << " — no presets for scene '" << sceneId << "'";
        return;
    }

    ofJson json = ofLoadJson(path);

    if (json.is_null()) {
        ofLogError("PresetManager") << "Malformed or empty JSON in: " << path;
        return;
    }

    // Support two formats:
    //   1. Object with "presets" array:  { "sceneId": "...", "presets": [...] }
    //   2. Direct array:                 [ { "name": "...", "params": {...} }, ... ]
    ofJson presetArray;
    if (json.is_object() && json.contains("presets") && json["presets"].is_array()) {
        presetArray = json["presets"];
    } else if (json.is_array()) {
        presetArray = json;
    } else {
        ofLogError("PresetManager") << "Unexpected JSON structure in: " << path
                                    << " — expected object with 'presets' array or a root array";
        return;
    }

    if (presetArray.empty()) {
        ofLogNotice("PresetManager") << "Empty preset array in: " << path
                                     << " — no presets for scene '" << sceneId << "'";
        return;
    }

    std::vector<PresetDef> loaded;
    for (const auto& entry : presetArray) {
        if (!entry.is_object() || !entry.contains("name") || !entry["name"].is_string()) {
            ofLogWarning("PresetManager") << "Skipping invalid preset entry in: " << path
                                          << " — missing or invalid 'name'";
            continue;
        }

        PresetDef def;
        def.name = entry["name"].get<std::string>();

        if (entry.contains("params") && entry["params"].is_object()) {
            for (auto it = entry["params"].begin(); it != entry["params"].end(); ++it) {
                if (it.value().is_number()) {
                    def.params[it.key()] = it.value().get<float>();
                } else if (it.value().is_boolean()) {
                    def.params[it.key()] = it.value().get<bool>() ? 1.0f : 0.0f;
                }
            }
        }

        // Parse optional kinectMappings array (backward-compatible: defaults to empty)
        if (entry.contains("kinectMappings") && entry["kinectMappings"].is_array()) {
            for (const auto& kmJson : entry["kinectMappings"]) {
                def.kinectMappings.push_back(kinectMappingFromJson(kmJson));
            }
        }

        // Parse optional midiMappings array (backward-compatible: defaults to empty)
        if (entry.contains("midiMappings") && entry["midiMappings"].is_array()) {
            for (const auto& mmJson : entry["midiMappings"]) {
                def.midiMappings.push_back(midiMappingFromJson(mmJson));
            }
        }

        // Parse optional morphDuration (backward-compatible: defaults to 0.0f)
        if (entry.contains("morphDuration") && entry["morphDuration"].is_number()) {
            def.morphDuration = entry["morphDuration"].get<float>();
        }

        // Parse optional morphEasing (backward-compatible: defaults to LINEAR)
        if (entry.contains("morphEasing") && entry["morphEasing"].is_string()) {
            std::string easingStr = entry["morphEasing"].get<std::string>();
            if (easingStr == "ease_in")      def.morphEasing = MorphEasing::EASE_IN;
            else if (easingStr == "ease_out")     def.morphEasing = MorphEasing::EASE_OUT;
            else if (easingStr == "ease_in_out")  def.morphEasing = MorphEasing::EASE_IN_OUT;
            // "linear" or unknown → stays LINEAR (default)
        }

        loaded.push_back(def);
    }

    presets_[sceneId] = loaded;
    ofLogNotice("PresetManager") << "Loaded " << loaded.size() << " presets for scene '"
                                 << sceneId << "' from: " << path;
}

// ============================================================================
// applyPreset — apply a named preset (instant or morphing)
// ============================================================================

void PresetManager::applyPreset(ScenePtr scene, const std::string& presetName, float morphDuration) {
    if (!scene) {
        ofLogWarning("PresetManager") << "applyPreset called with null scene";
        return;
    }

    std::string sceneId = scene->getId();
    auto it = presets_.find(sceneId);
    if (it == presets_.end() || it->second.empty()) {
        ofLogWarning("PresetManager") << "No presets loaded for scene '" << sceneId
                                      << "' — cannot apply preset '" << presetName << "'";
        return;
    }

    // Find the named preset
    const PresetDef* preset = nullptr;
    for (const auto& p : it->second) {
        if (p.name == presetName) {
            preset = &p;
            break;
        }
    }

    if (!preset) {
        ofLogError("PresetManager") << "Unknown preset '" << presetName
                                    << "' for scene '" << sceneId << "'";
        return;
    }

    // 1. Kinect mapping lifecycle — replace scene mappings with preset's mappings
    if (mappingEngine_) {
        mappingEngine_->removeMappingsByScene(sceneId);
        mappingEngine_->addMappings(preset->kinectMappings);
    }
    
    // 1a. MIDI mapping lifecycle — sync old preset's mappings, then activate new ones
    if (midiMappingEngine_) {
        // Save current active mappings back to the outgoing preset before switching
        syncMidiMappingsToCurrentPreset();
        midiMappingEngine_->setActiveMappings(preset->midiMappings);
    }

    // Track current preset for save operations
    currentPresetSceneId_ = sceneId;
    currentPresetName_ = presetName;
    
    // 1b. Clear old interaction points and particle pools immediately
    scene->clearInteractionPoints();

    // 2. Duration fallback: if caller passes 0 (or negative), use preset's own morphDuration
    float effectiveDuration = morphDuration;
    if (effectiveDuration <= 0.0f) {
        effectiveDuration = preset->morphDuration;
    }

    if (effectiveDuration > 0.0f) {
        // Most-recent-wins: cancel previous morphs for overlapping params in this scene
        for (const auto& paramEntry : preset->params) {
            // Remove existing morphs targeting the same param in this scene
            morphs_.erase(
                std::remove_if(morphs_.begin(), morphs_.end(),
                    [&](const MorphState& m) {
                        return m.sceneId == sceneId && m.paramId == paramEntry.first;
                    }),
                morphs_.end());

            // Capture current value as start (or 0 if param not found)
            float currentValue = 0.0f;
            auto paramPtr = scene->getParameter(paramEntry.first);
            if (paramPtr) {
                currentValue = paramPtr->currentValue;
            }

            // Create morph state with preset's easing captured at creation time
            MorphState morph;
            morph.sceneId = sceneId;
            morph.presetName = presetName;
            morph.paramId = paramEntry.first;
            morph.start = currentValue;
            morph.target = paramEntry.second;
            morph.elapsed = 0.0f;
            morph.duration = effectiveDuration;
            morph.active = true;
            morph.easing = preset->morphEasing;
            morphs_.push_back(morph);
        }
    } else {
        // Instant apply — set all params directly
        for (const auto& paramEntry : preset->params) {
            scene->setParameter(paramEntry.first, paramEntry.second);
        }
    }
}

// ============================================================================
// update — advance all active morphs
// ============================================================================

void PresetManager::update(float deltaTime) {
    if (deltaTime <= 0.0f || morphs_.empty()) return;

    // Clamp deltaTime to avoid huge jumps
    float dt = std::min(deltaTime, 0.1f);

    for (auto& morph : morphs_) {
        if (!morph.active) continue;

        morph.elapsed += dt;

        float value;
        if (morph.elapsed >= morph.duration) {
            // Snap to target on completion
            value = morph.target;
            morph.active = false;
        } else {
            // Eased lerp — apply easing curve to normalized progress
            float t = morph.elapsed / morph.duration;
            value = morph.start + (morph.target - morph.start) * applyEasing(t, morph.easing);
        }

        // Apply to scene
        ScenePtr scene = sceneEngine_ ? sceneEngine_->getScene(morph.sceneId) : nullptr;
        if (scene) {
            scene->setParameter(morph.paramId, value);
        }
    }

    // Remove completed morphs
    morphs_.erase(
        std::remove_if(morphs_.begin(), morphs_.end(),
            [](const MorphState& m) { return !m.active; }),
        morphs_.end());
}

// ============================================================================
// getPresets — return preset list for a scene
// ============================================================================

std::vector<PresetDef> PresetManager::getPresets(const std::string& sceneId) const {
    auto it = presets_.find(sceneId);
    if (it != presets_.end()) {
        return it->second;
    }
    return {};
}

// ============================================================================
// hasActiveMorph — true if any morph is in progress
// ============================================================================

bool PresetManager::hasActiveMorph() const {
    return !morphs_.empty();
}

// ============================================================================
// getMorphProgress — average progress of matching morphs (0.0–1.0)
// ============================================================================

float PresetManager::getMorphProgress(const std::string& sceneId, const std::string& presetName) const {
    float totalProgress = 0.0f;
    int count = 0;

    for (const auto& morph : morphs_) {
        if (morph.sceneId == sceneId && morph.presetName == presetName && morph.active) {
            if (morph.duration > 0.0f) {
                totalProgress += std::min(1.0f, morph.elapsed / morph.duration);
            } else {
                totalProgress += 1.0f;
            }
            count++;
        }
    }

    return (count > 0) ? (totalProgress / static_cast<float>(count)) : 0.0f;
}

// ============================================================================
// getActiveMorphSceneIds — unique scene IDs with active morphs
// ============================================================================

std::vector<std::string> PresetManager::getActiveMorphSceneIds() const {
    std::set<std::string> unique;
    for (const auto& morph : morphs_) {
        if (morph.active) {
            unique.insert(morph.sceneId);
        }
    }
    return std::vector<std::string>(unique.begin(), unique.end());
}

// ============================================================================
// savePresets — write presets back to JSON file
// ============================================================================

bool PresetManager::savePresets(const std::string& sceneId) {
    auto pathIt = presetPaths_.find(sceneId);
    if (pathIt == presetPaths_.end()) {
        ofLogError("PresetManager") << "No path recorded for scene '" << sceneId << "'";
        return false;
    }

    auto presetsIt = presets_.find(sceneId);
    if (presetsIt == presets_.end()) {
        ofLogError("PresetManager") << "No presets loaded for scene '" << sceneId << "'";
        return false;
    }

    ofJson json;
    json["sceneId"] = sceneId;
    ofJson presetArray = ofJson::array();

    for (const auto& preset : presetsIt->second) {
        ofJson entry;
        entry["name"] = preset.name;
        ofJson params = ofJson::object();
        for (const auto& [key, val] : preset.params) {
            params[key] = val;
        }
        entry["params"] = params;

        // Serialize kinectMappings if present
        if (!preset.kinectMappings.empty()) {
            ofJson kmArray = ofJson::array();
            for (const auto& km : preset.kinectMappings) {
                kmArray.push_back(kinectMappingToJson(km));
            }
            entry["kinectMappings"] = kmArray;
        }

        // Serialize midiMappings if present
        if (!preset.midiMappings.empty()) {
            ofJson mmArray = ofJson::array();
            for (const auto& mm : preset.midiMappings) {
                mmArray.push_back(toJson(mm));
            }
            entry["midiMappings"] = mmArray;
        }

        entry["morphDuration"] = preset.morphDuration;
        switch (preset.morphEasing) {
            case MorphEasing::EASE_IN:      entry["morphEasing"] = "ease_in";      break;
            case MorphEasing::EASE_OUT:     entry["morphEasing"] = "ease_out";     break;
            case MorphEasing::EASE_IN_OUT:  entry["morphEasing"] = "ease_in_out";  break;
            default:                        entry["morphEasing"] = "linear";        break;
        }

        presetArray.push_back(entry);
    }

    json["presets"] = presetArray;
    bool ok = ofSavePrettyJson(pathIt->second, json);
    if (ok) {
        ofLogNotice("PresetManager") << "Saved " << presetsIt->second.size()
                                     << " presets for scene '" << sceneId << "' to: " << pathIt->second;
    } else {
        ofLogError("PresetManager") << "Failed to save presets for scene '" << sceneId << "'";
    }
    return ok;
}

// ============================================================================
// addOrUpdatePreset — add new or overwrite existing (in-memory only)
// ============================================================================

void PresetManager::addOrUpdatePreset(const std::string& sceneId, const PresetDef& preset) {
    auto& list = presets_[sceneId];
    for (auto& existing : list) {
        if (existing.name == preset.name) {
            existing = preset;
            return;
        }
    }
    list.push_back(preset);
}

// ============================================================================
// deletePreset — remove a preset by name (in-memory only)
// ============================================================================

bool PresetManager::deletePreset(const std::string& sceneId, const std::string& presetName) {
    auto it = presets_.find(sceneId);
    if (it == presets_.end()) return false;

    auto& list = it->second;
    auto eraseIt = std::remove_if(list.begin(), list.end(),
        [&](const PresetDef& p) { return p.name == presetName; });
    if (eraseIt == list.end()) return false;

    list.erase(eraseIt, list.end());
    return true;
}

// ============================================================================
// captureCurrentParams — snapshot current scene param values
// ============================================================================

PresetDef PresetManager::captureCurrentParams(ScenePtr scene, const std::string& presetName) {
    PresetDef def;
    def.name = presetName;

    if (!scene) return def;

    auto paramIds = scene->getParameterIds();
    for (const auto& paramId : paramIds) {
        auto param = scene->getParameter(paramId);
        if (param) {
            def.params[paramId] = param->currentValue;
        }
    }

    return def;
}

// ============================================================================
// syncMidiMappingsToCurrentPreset — push engine's active mappings to PresetDef + save
// ============================================================================

void PresetManager::syncMidiMappingsToCurrentPreset() {
    if (!midiMappingEngine_) return;
    if (currentPresetSceneId_.empty() || currentPresetName_.empty()) return;

    auto it = presets_.find(currentPresetSceneId_);
    if (it == presets_.end()) return;

    // Find the named preset in the vector
    for (auto& p : it->second) {
        if (p.name == currentPresetName_) {
            // Copy engine's active mappings to the preset
            p.midiMappings = midiMappingEngine_->getActiveMappings();
            ofLogNotice("PresetManager") << "Synced " << p.midiMappings.size()
                                         << " MIDI mappings to preset '" << currentPresetName_
                                         << "' of scene '" << currentPresetSceneId_ << "'";
            // Persist to JSON file
            savePresets(currentPresetSceneId_);
            break;
        }
    }
}

} // namespace modular
