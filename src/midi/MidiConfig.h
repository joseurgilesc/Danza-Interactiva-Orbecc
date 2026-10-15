#pragma once

#include <string>
#include <vector>

namespace modular {

// ============================================================================
// MidiConfig — MIDI channel routing and port configuration
//
// Loaded from config/midi.json. Validates channel ranges (1–16) and
// enforces cueChannel not in paramChannels. Degrades to defaults on any error.
// ============================================================================

struct MidiConfig {
    std::string portName = "IAC Driver Bus 1";
    int cueChannel = 1;
    std::vector<int> paramChannels = {2};

    // ---- Helpers ----

    /// Returns true if `ch` is one of the configured param channels.
    bool isParamChannel(int ch) const;

    // ---- Persistence ----

    /// Reads config/midi.json. Returns false (with defaults still valid) if
    /// the file is missing, malformed, or contains invalid values.
    bool loadFromJson(const std::string& path);

    /// Writes current config to path. Auto-creates enclosing directory.
    bool saveToJson(const std::string& path) const;

    // ---- Validation ----

    /// Returns true when cueChannel is in [1,16], all paramChannels are in
    /// [1,16], and cueChannel is not in paramChannels.
    bool isValid() const;
};

} // namespace modular
