#include "midi/MidiMapping.h"

namespace modular {

// ============================================================================
// toJson — serialize MidiMapping to ofJson (runtime fields excluded)
// ============================================================================

ofJson toJson(const MidiMapping& mapping) {
    ofJson json;
    json["mappingId"]         = mapping.mappingId;
    json["type"]              = (mapping.type == MidiMessageType::CC) ? "cc" : "note";
    json["channel"]           = mapping.channel;
    json["number"]            = mapping.number;
    if (!mapping.numbers.empty()) json["numbers"] = mapping.numbers;
    json["targetParameterId"] = mapping.targetParameterId;
    json["minInput"]          = mapping.minInput;
    json["maxInput"]          = mapping.maxInput;
    json["minOutput"]         = mapping.minOutput;
    json["maxOutput"]         = mapping.maxOutput;
    json["controlMode"]       = static_cast<int>(mapping.controlMode);
    json["enabled"]           = mapping.enabled;
    // ADSR envelope fields
    json["attackMs"]          = mapping.attackMs;
    json["decayMs"]           = mapping.decayMs;
    json["sustainLevel"]      = mapping.sustainLevel;
    json["releaseMs"]         = mapping.releaseMs;
    json["noteToValue"]       = mapping.noteToValue;
    // NOTE: lastValue and noteHeld are runtime-only, NOT persisted
    return json;
}

// ============================================================================
// midiMappingFromJson — deserialize ofJson to MidiMapping with defaults
// ============================================================================

MidiMapping midiMappingFromJson(const ofJson& json) {
    MidiMapping mapping;

    mapping.mappingId = json.value("mappingId", "");
    mapping.channel   = json.value("channel", 2);
    mapping.number    = json.value("number", 0);
    // Multi-note array (backward-compat: missing → empty)
    if (json.contains("numbers") && json["numbers"].is_array()) {
        for (const auto& n : json["numbers"]) mapping.numbers.push_back(n.get<int>());
    }
    mapping.targetParameterId = json.value("targetParameterId", "");
    mapping.minInput  = json.value("minInput", 0.0f);
    mapping.maxInput  = json.value("maxInput", 127.0f);
    mapping.minOutput = json.value("minOutput", 0.0f);
    mapping.maxOutput = json.value("maxOutput", 1.0f);
    mapping.enabled   = json.value("enabled", true);

    // ADSR fields (backward-compat: missing → defaults)
    mapping.attackMs     = json.value("attackMs", 50.0f);
    mapping.decayMs      = json.value("decayMs", 100.0f);
    mapping.sustainLevel = json.value("sustainLevel", 0.8f);
    mapping.releaseMs    = json.value("releaseMs", 200.0f);
    mapping.noteToValue  = json.value("noteToValue", false);

    // Parse type: "cc" or "note"
    if (json.contains("type") && json["type"].is_string()) {
        std::string typeStr = json["type"].get<std::string>();
        mapping.type = (typeStr == "note") ? MidiMessageType::NOTE : MidiMessageType::CC;
    } else {
        mapping.type = MidiMessageType::CC;
    }

    // Parse controlMode (int in JSON → enum in struct)
    mapping.controlMode = static_cast<ControlMode>(json.value("controlMode", 0));

    // Runtime fields always reset
    mapping.lastValue = 0.0f;
    mapping.noteHeld  = false;

    return mapping;
}

} // namespace modular
