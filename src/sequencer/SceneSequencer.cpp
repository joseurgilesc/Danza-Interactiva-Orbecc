#include "sequencer/SceneSequencer.h"
#include "presets/PresetManager.h"
#include "engine/SceneEngine.h"
#include "scenes/BaseScene.h"
#include "ofJson.h"
#include "ofFileUtils.h"
#include <ofLog.h>

namespace modular {

// ============================================================================
// Constructor
// ============================================================================

SceneSequencer::SceneSequencer(PresetManager* presetManager, SceneEngine* sceneEngine)
    : presetManager_(presetManager)
    , sceneEngine_(sceneEngine)
{
    if (!sceneEngine_) {
        ofLogError("SceneSequencer") << "SceneEngine pointer is null — sequencer disabled";
    }
}

// ============================================================================
// loadSequence — loads cues from a JSON file
// ============================================================================

void SceneSequencer::loadSequence(const std::string& path) {
    cues_.clear();
    currentCueIndex_ = -1;

    ofFile file(path);
    if (!file.exists()) {
        ofLogWarning("SceneSequencer") << "Sequence file not found: " << path << " — no cues loaded";
        return;
    }

    ofJson json = ofLoadJson(path);
    if (json.is_null()) {
        ofLogError("SceneSequencer") << "Malformed JSON in sequence file: " << path;
        return;
    }

    // Support both {"cues": [...]} and root-array [...] formats
    ofJson cuesArray;
    if (json.is_array()) {
        cuesArray = json;
    } else if (json.contains("cues") && json["cues"].is_array()) {
        cuesArray = json["cues"];
    } else {
        ofLogError("SceneSequencer") << "Sequence JSON must contain a 'cues' array: " << path;
        return;
    }

    if (cuesArray.empty()) {
        ofLogNotice("SceneSequencer") << "Empty cue array in: " << path;
        return;
    }

    for (const auto& entry : cuesArray) {
        if (!entry.is_object()) continue;

        CueDef cue;
        cue.cue = entry.value("cue", 0);
        cue.sceneId = entry.value("sceneId", "");
        cue.presetName = entry.value("presetName", "");
        cue.morphDuration = entry.value("morphDuration", 0.0f);

        // MIDI trigger: backward-compat with "midiPC" field
        if (entry.contains("midiType")) {
            cue.midiType = entry.value("midiType", "");
            cue.midiValue = entry.value("midiValue", -1);
            cue.midiChannel = entry.value("midiChannel", -1);
        } else if (entry.contains("midiPC")) {
            cue.midiType = "pc";
            cue.midiValue = entry.value("midiPC", -1);
        }
        // else keep defaults (empty string, -1)

        if (cue.cue <= 0 || cue.sceneId.empty()) {
            ofLogWarning("SceneSequencer")
                << "Skipping invalid cue entry (missing cue number or sceneId)";
            continue;
        }

        cues_.push_back(cue);
    }

    ofLogNotice("SceneSequencer") << "Loaded " << cues_.size() << " cues from: " << path;
}

// ============================================================================
// findCueIndex — internal lookup by 1-indexed cue number
// ============================================================================

int SceneSequencer::findCueIndex(int cueNumber) const {
    for (size_t i = 0; i < cues_.size(); ++i) {
        if (cues_[i].cue == cueNumber) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// ============================================================================
// triggerCue — applies preset then transitions scene
// ============================================================================

bool SceneSequencer::triggerCue(int cueNumber) {
    if (!sceneEngine_) {
        ofLogError("SceneSequencer") << "No SceneEngine — cannot trigger cue " << cueNumber;
        return false;
    }

    int idx = findCueIndex(cueNumber);
    if (idx < 0) {
        ofLogError("SceneSequencer") << "Cue " << cueNumber << " not found in sequence";
        return false;
    }

    const CueDef& cue = cues_[idx];

    // Validate scene exists
    auto scene = sceneEngine_->getScene(cue.sceneId);
    if (!scene) {
        ofLogError("SceneSequencer")
            << "Cue " << cueNumber << ": scene '" << cue.sceneId << "' is not loaded — rejected";
        return false;
    }

    // Apply preset if PresetManager is available and presetName is non-empty
    if (presetManager_ && !cue.presetName.empty()) {
        // Validate preset exists for this scene
        auto presets = presetManager_->getPresets(cue.sceneId);
        bool presetFound = false;
        for (const auto& p : presets) {
            if (p.name == cue.presetName) {
                presetFound = true;
                break;
            }
        }

        if (presetFound) {
            presetManager_->applyPreset(scene, cue.presetName, cue.morphDuration);
        } else {
            ofLogWarning("SceneSequencer")
                << "Cue " << cueNumber << ": preset '" << cue.presetName
                << "' not found for scene '" << cue.sceneId
                << "' — transitioning without preset";
        }
    } else if (!presetManager_ && !cue.presetName.empty()) {
        ofLogWarning("SceneSequencer")
            << "Cue " << cueNumber << ": PresetManager not available — skipping preset '"
            << cue.presetName << "'";
    }

    // Transition to target scene
    sceneEngine_->transitionTo(cue.sceneId);

    currentCueIndex_ = idx;
    return true;
}

// ============================================================================
// triggerNext — advances to next cue (clamps at max)
// ============================================================================

bool SceneSequencer::triggerNext() {
    if (cues_.empty()) {
        ofLogWarning("SceneSequencer") << "No cues loaded — triggerNext() ignored";
        return false;
    }

    int nextIdx = currentCueIndex_ + 1;
    if (nextIdx >= static_cast<int>(cues_.size())) {
        ofLogNotice("SceneSequencer") << "Already at last cue — triggerNext() clamped";
        return false;
    }

    return triggerCue(cues_[nextIdx].cue);
}

// ============================================================================
// triggerPrev — retreats to previous cue (clamps at 0)
// ============================================================================

bool SceneSequencer::triggerPrev() {
    if (cues_.empty()) {
        ofLogWarning("SceneSequencer") << "No cues loaded — triggerPrev() ignored";
        return false;
    }

    int prevIdx = currentCueIndex_ - 1;
    if (prevIdx < 0) {
        ofLogNotice("SceneSequencer") << "Already at first cue — triggerPrev() clamped";
        return false;
    }

    return triggerCue(cues_[prevIdx].cue);
}

// ============================================================================
// Accessors
// ============================================================================

int SceneSequencer::getCurrentCueIndex() const {
    return currentCueIndex_;
}

int SceneSequencer::getCueCount() const {
    return static_cast<int>(cues_.size());
}

const CueDef* SceneSequencer::getCue(int index) const {
    if (index < 0 || index >= static_cast<int>(cues_.size())) {
        return nullptr;
    }
    return &cues_[index];
}

bool SceneSequencer::hasSequence() const {
    return !cues_.empty();
}

// ============================================================================
// isDuplicateCueNumber — uniqueness check, optionally excluding one index
// ============================================================================

bool SceneSequencer::isDuplicateCueNumber(int cueNumber, int excludeIdx) const {
    for (size_t i = 0; i < cues_.size(); ++i) {
        if (static_cast<int>(i) == excludeIdx) continue;
        if (cues_[i].cue == cueNumber) return true;
    }
    return false;
}

// ============================================================================
// addCue — append a new cue (rejects duplicate cue numbers)
// ============================================================================

bool SceneSequencer::addCue(const CueDef& cue) {
    if (isDuplicateCueNumber(cue.cue)) {
        ofLogError("SceneSequencer") << "addCue: cue number " << cue.cue
                                     << " already exists — rejected";
        return false;
    }
    cues_.push_back(cue);
    return true;
}

// ============================================================================
// removeCue — removes by cue number
// ============================================================================

bool SceneSequencer::removeCue(int cueNumber) {
    int idx = findCueIndex(cueNumber);
    if (idx < 0) {
        ofLogError("SceneSequencer") << "removeCue: cue " << cueNumber
                                     << " not found";
        return false;
    }

    // Adjust currentCueIndex_ if removing at or before current position
    if (idx <= currentCueIndex_) {
        currentCueIndex_ = std::max(-1, currentCueIndex_ - 1);
    }

    cues_.erase(cues_.begin() + idx);

    // If all cues gone, reset
    if (cues_.empty()) {
        currentCueIndex_ = -1;
    }

    return true;
}

// ============================================================================
// updateCue — replace a cue by its number (self-update allowed)
// ============================================================================

bool SceneSequencer::updateCue(int cueNumber, const CueDef& newDef) {
    int idx = findCueIndex(cueNumber);
    if (idx < 0) {
        ofLogError("SceneSequencer") << "updateCue: cue " << cueNumber
                                     << " not found";
        return false;
    }

    // If cue number changed, validate uniqueness (self is excluded)
    if (newDef.cue != cueNumber && isDuplicateCueNumber(newDef.cue, idx)) {
        ofLogError("SceneSequencer") << "updateCue: cue number " << newDef.cue
                                     << " already exists — rejected";
        return false;
    }

    cues_[idx] = newDef;
    return true;
}

// ============================================================================
// saveSequence — serialize to JSON matching loadSequence format
// ============================================================================

bool SceneSequencer::saveSequence(const std::string& path) const {
    ofJson json;
    json["cues"] = ofJson::array();

    for (const auto& cue : cues_) {
        ofJson entry;
        entry["cue"] = cue.cue;
        entry["sceneId"] = cue.sceneId;
        entry["presetName"] = cue.presetName;
        entry["morphDuration"] = cue.morphDuration;
        if (!cue.midiType.empty() && cue.midiValue >= 0) {
            entry["midiType"] = cue.midiType;
            entry["midiValue"] = cue.midiValue;
            if (cue.midiChannel > 0) entry["midiChannel"] = cue.midiChannel;
        }
        json["cues"].push_back(entry);
    }

    bool ok = ofSaveJson(path, json);
    if (!ok) {
        ofLogError("SceneSequencer") << "saveSequence: failed to write " << path;
    }
    return ok;
}

// ============================================================================
// reorderCues — move cue from one index to another (0-based)
// ============================================================================

bool SceneSequencer::reorderCues(int fromIndex, int toIndex) {
    int count = static_cast<int>(cues_.size());

    if (fromIndex < 0 || fromIndex >= count || toIndex < 0 || toIndex >= count) {
        ofLogError("SceneSequencer") << "reorderCues: index out of range ("
                                     << fromIndex << " → " << toIndex
                                     << ", size=" << count << ")";
        return false;
    }

    if (fromIndex == toIndex) return true;

    CueDef cue = cues_[fromIndex];
    cues_.erase(cues_.begin() + fromIndex);

    // After erase, indices shift — re-compute target if fromIndex was before toIndex
    int adjustedTo = toIndex;
    if (fromIndex < toIndex) {
        adjustedTo = toIndex - 1;
    }

    cues_.insert(cues_.begin() + adjustedTo, cue);

    // Keep currentCueIndex_ tracking the moved cue
    if (currentCueIndex_ == fromIndex) {
        currentCueIndex_ = adjustedTo;
    } else if (fromIndex < currentCueIndex_ && adjustedTo >= currentCueIndex_) {
        currentCueIndex_--;
    } else if (fromIndex > currentCueIndex_ && adjustedTo <= currentCueIndex_) {
        currentCueIndex_++;
    }

    return true;
}

// ============================================================================
// findCueByMidi — lookup by MIDI trigger type + value
// ============================================================================

int SceneSequencer::findCueByMidi(const std::string& type, int value) const {
    if (type.empty() || value < 0 || value > 127) return -1;
    for (int i = 0; i < (int)cues_.size(); ++i) {
        if (cues_[i].midiType == type && cues_[i].midiValue == value) {
            return i;  // 0-based index
        }
    }
    return -1;
}

} // namespace modular
