#pragma once

#include "midi/MidiMapping.h"
#include <string>
#include <vector>
#include <map>
#include <cstdint>

class ofxMidiMessage;

namespace modular {

// Forward declarations
class SceneEngine;

// ============================================================================
// MidiLearnState — single-slot learning FSM
// ============================================================================

enum class MidiLearnState : uint8_t {
    IDLE, LEARNING, TIMEOUT
};

// ============================================================================
// AdsrVoice — per-note envelope state for ADSR control mode
// ============================================================================

enum class AdsrPhase : uint8_t { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };

struct AdsrVoice {
    int noteNumber = 0;
    float velocity = 0.0f;       // 0.0–1.0 (normalized from MIDI 0–127)
    AdsrPhase phase = AdsrPhase::IDLE;
    float elapsed = 0.0f;        // ms elapsed in current phase
    float currentLevel = 0.0f;   // 0.0–1.0, current envelope output
    float releaseStartLevel = 0.0f;  // level at NoteOff for release ramp
};

// ============================================================================
// MidiMappingEngine — MIDI-to-scene-parameter mapping with Learn support
//
// Receives messages dispatched by MidiRouter on the param channel, matches
// against per-preset active mappings, scales values, and dispatches to the
// active scene via SceneEngine. Follows OscMappingEngine's Learn FSM pattern
// (single-slot, 10 s timeout, double-learn guard).
// ============================================================================

class MidiMappingEngine {
public:
    explicit MidiMappingEngine(SceneEngine* sceneEngine);

    // ---- Core Dispatch ----

    /// Matches an incoming MIDI message against activeMappings_ by type,
    /// channel, and number. For matched+enabled entries, scales the input
    /// value and calls scene->setParameter() with the control-mode dispatch
    /// behaviour (CONTINUOUS / TOGGLE / TRIGGER / MOMENTARY).
    /// If Learn is active, captures the message and auto-creates a mapping.
    void processMidiMessage(const ofxMidiMessage& msg);

    // ---- Active Mapping Set (per-preset lifecycle) ----

    /// Replaces the entire active mapping set. Called by PresetManager on
    /// preset apply. Old mappings are discarded.
    void setActiveMappings(const std::vector<MidiMapping>& mappings);

    /// Returns the current active mapping set for UI panel access.
    std::vector<MidiMapping>& getActiveMappings();

    /// Enable/disable verbose dispatch logging (off by default).
    void setDebugLogging(bool enabled) { debugLogging_ = enabled; }

    // ---- Learn (engine-level double-learn guard) ----

    /// Starts single-slot Learn for the given parameter.
    /// Returns false if Learn is already active (double-learn guard).
    bool startLearning(const std::string& targetParameterId);

    /// Cancels an active Learn without creating a mapping.
    void cancelLearning();

    /// Returns IDLE, LEARNING, or TIMEOUT.
    MidiLearnState getMidiLearnState() const;

    // ---- Per-frame update (Learn timeout) ----
    void update(float deltaTime);

private:
    // ---- Dispatch helpers ----

    float scaleValue(const MidiMapping& mapping, float raw) const;
    void dispatchContinuous(const MidiMapping& mapping, float raw);
    void dispatchToggle(const MidiMapping& mapping, bool noteOn);
    void dispatchTrigger(const MidiMapping& mapping, bool noteOn);
    void dispatchMomentary(const MidiMapping& mapping, bool noteOn);
    void dispatchAdsr(const MidiMapping& mapping, int note, float velocity, bool noteOn);

    // ---- ADSR envelope update (called each frame) ----
    void updateAdsrVoices(float deltaTime);

    // ---- ID generation ----
    std::string generateId();

    // ---- Resolve "sceneId.localParamId" → scene + setParameter ----
    bool applyToScene(const std::string& targetParameterId, float value);

    SceneEngine* sceneEngine_;
    std::vector<MidiMapping> activeMappings_;
    int idCounter_ = 0;
    bool debugLogging_ = false;

    // ADSR voice state — keyed by "mappingId:noteNumber" for polyphonic envelopes
    std::map<std::string, AdsrVoice> adsrVoices_;

    // Learn state
    MidiLearnState learnState_ = MidiLearnState::IDLE;
    std::string learnTargetParamId_;
    float learnTimer_ = 0.0f;

    static constexpr float kLearnTimeout = 10.0f;
};

} // namespace modular
