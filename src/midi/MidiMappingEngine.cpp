#include "midi/MidiMappingEngine.h"
#include "engine/SceneEngine.h"
#include "scenes/BaseScene.h"
#include "ofxMidi.h"
#include "ofLog.h"
#include <cmath>
#include <algorithm>

namespace modular {

// ============================================================================
// Constructor
// ============================================================================

MidiMappingEngine::MidiMappingEngine(SceneEngine* sceneEngine)
    : sceneEngine_(sceneEngine)
    , idCounter_(0)
    , learnState_(MidiLearnState::IDLE)
{}

// ============================================================================
// generateId — produces unique mapping identifiers ("midi-N")
// ============================================================================

std::string MidiMappingEngine::generateId() {
    return "midi-" + std::to_string(++idCounter_);
}

// ============================================================================
// processMidiMessage — core dispatch: Learn capture or match+scale+apply
// ============================================================================

void MidiMappingEngine::processMidiMessage(const ofxMidiMessage& msg) {
    // ── LEARN MODE ────────────────────────────────────────────────────────
    if (learnState_ == MidiLearnState::LEARNING) {
        // Determine message type and number
        MidiMessageType msgType;
        int msgNumber;

        if (msg.status == MIDI_CONTROL_CHANGE) {
            msgType   = MidiMessageType::CC;
            msgNumber = msg.control;
        } else if (msg.status == MIDI_NOTE_ON && msg.velocity > 0) {
            msgType   = MidiMessageType::NOTE;
            msgNumber = msg.pitch;
        } else if (msg.status == MIDI_NOTE_OFF
                || (msg.status == MIDI_NOTE_ON && msg.velocity == 0)) {
            // NoteOff — ignore for Learn purposes
            return;
        } else {
            // PC or other non-param message — ignore for Learn
            return;
        }

        // Auto-create mapping from captured message
        MidiMapping newMapping;
        newMapping.mappingId        = generateId();
        newMapping.type             = msgType;
        newMapping.channel          = msg.channel;
        newMapping.number           = msgNumber;
        newMapping.targetParameterId = learnTargetParamId_;
        newMapping.enabled          = true;

        activeMappings_.push_back(std::move(newMapping));

        ofLogNotice("MidiMappingEngine")
            << "Learn captured: " << (msgType == MidiMessageType::CC ? "CC" : "Note")
            << " ch=" << msg.channel
            << " num=" << msgNumber
            << " → " << learnTargetParamId_;

        learnState_ = MidiLearnState::IDLE;
        learnTargetParamId_.clear();
        learnTimer_ = 0.0f;
        return;
    }

    // ── MATCHING DISPATCH ─────────────────────────────────────────────────
    // Determine message type and numeric value for matching
    MidiMessageType incomingType;
    int incomingNumber;
    float rawValue;
    bool isNoteOn  = false;
    bool isNoteOff = false;

    if (msg.status == MIDI_CONTROL_CHANGE) {
        incomingType   = MidiMessageType::CC;
        incomingNumber = msg.control;
        rawValue       = static_cast<float>(msg.value);
    } else if (msg.status == MIDI_NOTE_ON && msg.velocity > 0) {
        incomingType   = MidiMessageType::NOTE;
        incomingNumber = msg.pitch;
        rawValue       = static_cast<float>(msg.velocity);
        isNoteOn       = true;
    } else if (msg.status == MIDI_NOTE_OFF
            || (msg.status == MIDI_NOTE_ON && msg.velocity == 0)) {
        incomingType   = MidiMessageType::NOTE;
        incomingNumber = msg.pitch;
        rawValue       = 0.0f;
        isNoteOff      = true;
    } else {
        // PC or other non-param message — not handled by mapping engine
        return;
    }

    // Iterate active mappings — match by type + channel + number (or numbers[])
    for (auto& mapping : activeMappings_) {
        if (!mapping.enabled) continue;
        if (mapping.type != incomingType) continue;
        if (mapping.channel != msg.channel) continue;

        // Match number: check numbers[] first, then fall back to legacy number
        bool matched = false;
        if (!mapping.numbers.empty()) {
            for (int n : mapping.numbers) {
                if (n == incomingNumber) { matched = true; break; }
            }
        } else if (mapping.number == incomingNumber) {
            matched = true;
        }
        if (!matched) continue;

        // Matched — skip if no target parameter set yet
        if (mapping.targetParameterId.empty()) continue;

        // Matched — dispatch by control mode
        switch (mapping.controlMode) {
            case ControlMode::CONTINUOUS:
                dispatchContinuous(mapping, rawValue);
                break;

            case ControlMode::TOGGLE:
                if (isNoteOn) dispatchToggle(mapping, true);
                break;

            case ControlMode::TRIGGER:
                if (isNoteOn) dispatchTrigger(mapping, true);
                else if (isNoteOff) dispatchTrigger(mapping, false);
                break;

            case ControlMode::MOMENTARY:
                if (isNoteOn) dispatchMomentary(mapping, true);
                else if (isNoteOff) dispatchMomentary(mapping, false);
                break;

            case ControlMode::ADSR:
                if (isNoteOn) {
                    float vel = rawValue / 127.0f;
                    dispatchAdsr(mapping, incomingNumber, vel, true);
                } else if (isNoteOff) {
                    dispatchAdsr(mapping, incomingNumber, 0.0f, false);
                }
                break;
        }
    }
}

// ============================================================================
// scaleValue — linear interpolation with inversion support
// ============================================================================

float MidiMappingEngine::scaleValue(const MidiMapping& mapping, float raw) const {
    float inMin = mapping.minInput;
    float inMax = mapping.maxInput;
    float outMin = mapping.minOutput;
    float outMax = mapping.maxOutput;

    // Guard against division by zero
    if (inMax == inMin) {
        return outMin;
    }

    // Normalise input to [0, 1] — works correctly even if minInput > maxInput
    float t = (raw - inMin) / (inMax - inMin);
    t = std::max(0.0f, std::min(1.0f, t));

    // Linear interpolation to output range
    return outMin + t * (outMax - outMin);
}

// ============================================================================
// Dispatch helpers
// ============================================================================

void MidiMappingEngine::dispatchContinuous(const MidiMapping& mapping, float raw) {
    float scaled = scaleValue(mapping, raw);
    applyToScene(mapping.targetParameterId, scaled);

    auto& m = const_cast<MidiMapping&>(mapping);
    m.lastValue = raw;
    m.lastActiveTime = ofGetElapsedTimef();
    m.lastReceivedValue = raw;
}

void MidiMappingEngine::dispatchToggle(const MidiMapping& mapping, bool /*noteOn*/) {
    // Toggle only reacts to NoteOn — flip param between minOutput and maxOutput
    // by reading lastValue as the current toggle state
    float currentValue = mapping.lastValue;
    float newValue;

    // If currently at (or near) minOutput, flip to maxOutput; otherwise to minOutput
    float midpoint = (mapping.minOutput + mapping.maxOutput) * 0.5f;
    if (currentValue <= midpoint) {
        newValue = mapping.maxOutput;
    } else {
        newValue = mapping.minOutput;
    }

    applyToScene(mapping.targetParameterId, newValue);

    auto& m = const_cast<MidiMapping&>(mapping);
    m.lastValue = newValue;
    m.lastActiveTime = ofGetElapsedTimef();
    m.lastReceivedValue = (newValue == mapping.maxOutput) ? 127.0f : 0.0f;
}

void MidiMappingEngine::dispatchTrigger(const MidiMapping& mapping, bool noteOn) {
    float value = noteOn ? mapping.maxOutput : mapping.minOutput;
    applyToScene(mapping.targetParameterId, value);

    auto& m = const_cast<MidiMapping&>(mapping);
    m.lastValue = value;
    m.noteHeld  = noteOn;
    m.lastActiveTime = ofGetElapsedTimef();
    m.lastReceivedValue = noteOn ? 127.0f : 0.0f;
}

void MidiMappingEngine::dispatchMomentary(const MidiMapping& mapping, bool noteOn) {
    // Same behaviour as TRIGGER for notes
    dispatchTrigger(mapping, noteOn);
}

// ============================================================================
// dispatchAdsr — NoteOn starts envelope, NoteOff triggers release
// ============================================================================

void MidiMappingEngine::dispatchAdsr(const MidiMapping& mapping, int note, float velocity, bool noteOn) {
    std::string voiceKey = mapping.mappingId + ":" + std::to_string(note);

    if (noteOn) {
        AdsrVoice voice;
        voice.noteNumber = note;
        voice.velocity = velocity;
        voice.phase = AdsrPhase::ATTACK;
        voice.elapsed = 0.0f;
        voice.currentLevel = 0.0f;
        voice.releaseStartLevel = 0.0f;
        adsrVoices_[voiceKey] = voice;
    } else {
        // NoteOff: transition to RELEASE
        auto it = adsrVoices_.find(voiceKey);
        if (it != adsrVoices_.end()) {
            it->second.releaseStartLevel = it->second.currentLevel;
            it->second.phase = AdsrPhase::RELEASE;
            it->second.elapsed = 0.0f;
        }
    }
}

// ============================================================================
// updateAdsrVoices — per-frame envelope advancement, polyphonic combining
// ============================================================================

void MidiMappingEngine::updateAdsrVoices(float deltaTime) {
    float dtMs = deltaTime * 1000.0f;  // seconds → milliseconds

    // Clear per-mapping output accumulator
    std::map<std::string, float> mappingMax;

    std::vector<std::string> finishedKeys;

    for (auto& [key, voice] : adsrVoices_) {
        voice.elapsed += dtMs;

        // Extract mappingId from "mappingId:noteNumber"
        std::string mapId = key.substr(0, key.rfind(':'));

        // Find mapping to get ADSR params
        const MidiMapping* mapping = nullptr;
        for (auto& m : activeMappings_) {
            if (m.mappingId == mapId) { mapping = &m; break; }
        }
        if (!mapping) { finishedKeys.push_back(key); continue; }

        switch (voice.phase) {
            case AdsrPhase::ATTACK: {
                float attackMs = std::max(mapping->attackMs, 1.0f);
                float t = voice.elapsed / attackMs;
                // Peak level: velocity × note scale (if enabled)
                float peak = voice.velocity;
                if (mapping->noteToValue) {
                    peak *= static_cast<float>(voice.noteNumber) / 127.0f;
                }
                if (t >= 1.0f) {
                    voice.currentLevel = voice.velocity;
                    voice.phase = AdsrPhase::DECAY;
                    voice.elapsed = 0.0f;
                } else {
                    voice.currentLevel = voice.velocity * t;
                }
                break;
            }
            case AdsrPhase::DECAY: {
                float decayMs = std::max(mapping->decayMs, 1.0f);
                float t = voice.elapsed / decayMs;
                float sustain = mapping->sustainLevel * voice.velocity;
                if (t >= 1.0f) {
                    voice.currentLevel = sustain;
                    voice.phase = AdsrPhase::SUSTAIN;
                    voice.elapsed = 0.0f;
                } else {
                    voice.currentLevel = voice.velocity + (sustain - voice.velocity) * t;
                }
                break;
            }
            case AdsrPhase::SUSTAIN:
                voice.currentLevel = mapping->sustainLevel * voice.velocity;
                break;
            case AdsrPhase::RELEASE: {
                float releaseMs = std::max(mapping->releaseMs, 1.0f);
                float t = voice.elapsed / releaseMs;
                if (t >= 1.0f) {
                    voice.currentLevel = 0.0f;
                    finishedKeys.push_back(key);
                } else {
                    voice.currentLevel = voice.releaseStartLevel * (1.0f - t);
                }
                break;
            }
            case AdsrPhase::IDLE:
                finishedKeys.push_back(key);
                break;
        }

        // Accumulate max per mapping (polyphonic: take highest voice)
        if (voice.currentLevel > mappingMax[mapId]) {
            mappingMax[mapId] = voice.currentLevel;
        }
    }

    // Clean up finished voices
    for (auto& k : finishedKeys) adsrVoices_.erase(k);

    // Scale by output range and apply to scene
    for (auto& mapping : activeMappings_) {
        if (mapping.controlMode != ControlMode::ADSR) continue;

        // Check for _array suffix → per-note routing
        bool isArray = false;
        std::string baseParamId = mapping.targetParameterId;
        if (baseParamId.size() > 6 && baseParamId.substr(baseParamId.size() - 6) == "_array") {
            isArray = true;
            baseParamId = baseParamId.substr(0, baseParamId.size() - 6);
        }

        if (isArray && mapping.noteToValue && !mapping.numbers.empty()) {
            // Per-note routing: each note → indexed sub-parameter
            for (size_t ni = 0; ni < mapping.numbers.size(); ni++) {
                int note = mapping.numbers[ni];
                std::string indexedId = baseParamId + "_" + std::to_string(ni);
                // Get this note's voice level
                std::string vk = mapping.mappingId + ":" + std::to_string(note);
                auto vit = adsrVoices_.find(vk);
                float level = (vit != adsrVoices_.end()) ? vit->second.currentLevel : 0.0f;
                float value = mapping.minOutput + level * (mapping.maxOutput - mapping.minOutput);
                applyToScene(indexedId, value);
            }
        } else {
            // Single output: max of all active voices
            auto it = mappingMax.find(mapping.mappingId);
            float level = (it != mappingMax.end()) ? it->second : 0.0f;
            float value = mapping.minOutput + level * (mapping.maxOutput - mapping.minOutput);
            applyToScene(mapping.targetParameterId, value);
        }
    }
}

// ============================================================================
// applyToScene — resolve targetParameterId and call scene->setParameter()
// ============================================================================

bool MidiMappingEngine::applyToScene(const std::string& targetParameterId, float value) {
    size_t dotPos = targetParameterId.find('.');
    if (dotPos == std::string::npos) {
        ofLogWarning("MidiMappingEngine")
            << "Invalid targetParameterId (no dot): " << targetParameterId;
        return false;
    }

    std::string sceneId      = targetParameterId.substr(0, dotPos);
    std::string localParamId = targetParameterId.substr(dotPos + 1);

    auto scene = sceneEngine_->getScene(sceneId);
    if (!scene) {
        // Scene not loaded / inactive — silently skip
        return false;
    }

    bool ok = scene->setParameter(localParamId, value);
    if (!ok) {
        ofLogWarning("MidiMappingEngine")
            << "setParameter failed: " << targetParameterId << " = " << value;
    }
    return ok;
}

// ============================================================================
// Active mapping set management
// ============================================================================

void MidiMappingEngine::setActiveMappings(const std::vector<MidiMapping>& mappings) {
    activeMappings_ = mappings;

    // Update idCounter_ to avoid collisions with loaded mapping IDs
    for (const auto& m : activeMappings_) {
        // Extract number from "midi-N"
        const std::string& id = m.mappingId;
        size_t pos = id.rfind('-');
        if (pos != std::string::npos) {
            try {
                int num = std::stoi(id.substr(pos + 1));
                if (num > idCounter_) idCounter_ = num;
            } catch (...) {}
        }
    }

    ofLogNotice("MidiMappingEngine")
        << "Active mappings set: " << activeMappings_.size() << " entries";
}

std::vector<MidiMapping>& MidiMappingEngine::getActiveMappings() {
    return activeMappings_;
}

// ============================================================================
// Learn FSM (mirrors OscMappingEngine pattern)
// ============================================================================

bool MidiMappingEngine::startLearning(const std::string& targetParameterId) {
    if (learnState_ == MidiLearnState::LEARNING) {
        ofLogWarning("MidiMappingEngine")
            << "Learn already active for: " << learnTargetParamId_
            << ", rejecting: " << targetParameterId;
        return false;
    }

    learnState_ = MidiLearnState::LEARNING;
    learnTargetParamId_ = targetParameterId;
    learnTimer_ = 0.0f;

    ofLogNotice("MidiMappingEngine")
        << "Learn started for: " << targetParameterId;
    return true;
}

void MidiMappingEngine::cancelLearning() {
    if (learnState_ == MidiLearnState::LEARNING) {
        ofLogNotice("MidiMappingEngine")
            << "Learn cancelled for: " << learnTargetParamId_;
    }
    learnState_ = MidiLearnState::IDLE;
    learnTargetParamId_.clear();
    learnTimer_ = 0.0f;
}

MidiLearnState MidiMappingEngine::getMidiLearnState() const {
    return learnState_;
}

// ============================================================================
// update — per-frame housekeeping: Learn timeout → TIMEOUT → IDLE
// ============================================================================

void MidiMappingEngine::update(float deltaTime) {
    // Learn timeout
    if (learnState_ == MidiLearnState::LEARNING) {
        learnTimer_ += deltaTime;
        if (learnTimer_ >= kLearnTimeout) {
            learnState_ = MidiLearnState::TIMEOUT;
            ofLogWarning("MidiMappingEngine")
                << "Learn timeout for: " << learnTargetParamId_;
            learnTargetParamId_.clear();
            learnTimer_ = 0.0f;
        }
    }
    else if (learnState_ == MidiLearnState::TIMEOUT) {
        learnState_ = MidiLearnState::IDLE;
    }

    // ADSR envelope update
    updateAdsrVoices(deltaTime);
}

} // namespace modular
