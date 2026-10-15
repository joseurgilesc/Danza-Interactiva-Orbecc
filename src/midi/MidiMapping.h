#pragma once

#include <string>
#include "ofJson.h"

namespace modular {

// ============================================================================
// ControlMode — how a MIDI mapping dispatches parameter values
// ============================================================================

enum class ControlMode : uint8_t {
    CONTINUOUS,  // Every CC value change dispatches scaled output
    TOGGLE,      // NoteOn flips param between minOutput and maxOutput
    TRIGGER,     // NoteOn → maxOutput, NoteOff → minOutput
    MOMENTARY,   // Same behaviour as TRIGGER for notes
    ADSR         // NoteOn → envelope (A→D→S), NoteOff → R
};

// ============================================================================
// MidiMessageType — discriminates CC vs Note messages
// ============================================================================

enum class MidiMessageType : uint8_t {
    NOTE,
    CC
};

// ============================================================================
// MidiMapping — a single MIDI-to-scene-parameter mapping entry
// ============================================================================

struct MidiMapping {
    // Core identity (persisted)
    std::string mappingId;            // Auto-generated: "midi-N"
    MidiMessageType type = MidiMessageType::CC;
    int channel = 2;                  // Must match a paramChannel (1–16)
    int number = 0;                   // 0–127: CC number or Note number (legacy single)
    std::vector<int> numbers;         // Multi-note array (e.g. [60,62,64]); overrides number when non-empty
    std::string targetParameterId;    // Format: "sceneId.localParamId"

    // Input range (persisted)
    float minInput = 0.0f;
    float maxInput = 127.0f;

    // Output range (persisted) — overrides target param's range
    float minOutput = 0.0f;
    float maxOutput = 1.0f;

    // ADSR envelope (persisted, used when controlMode == ADSR)
    float attackMs  = 50.0f;    // Attack time in milliseconds
    float decayMs   = 100.0f;   // Decay time in milliseconds
    float sustainLevel = 0.8f;  // Sustain level (0.0–1.0)
    float releaseMs = 200.0f;   // Release time in milliseconds
    bool noteToValue = false;   // Scale ADSR peak by note number (60→low, 84→high)

    // Behaviour flags (persisted)
    ControlMode controlMode = ControlMode::CONTINUOUS;
    bool enabled = true;

    // Runtime state (NOT persisted)
    float lastValue = 0.0f;
    bool noteHeld = false;            // Tracks note-on state for TOGGLE/TRIGGER/MOMENTARY
    float lastActiveTime = 0.0f;      // ofGetElapsedTimef() when last MIDI message matched
    float lastReceivedValue = 0.0f;   // Raw MIDI value last received (0–127)
};

// ============================================================================
// JSON Serialization Helpers
//
// These free functions convert MidiMapping to/from ofJson for persistence
// inside PresetDef. They are NOT member functions — serialization lives in
// PresetManager (Phase 2), but the helpers are defined here alongside the
// struct so compile units that need toJSON/midiMappingFromJson can include
// this single header.
// ============================================================================

ofJson toJson(const MidiMapping& mapping);
MidiMapping midiMappingFromJson(const ofJson& json);

} // namespace modular
