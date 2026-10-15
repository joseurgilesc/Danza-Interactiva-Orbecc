#include "midi/MidiConfig.h"
#include "ofJson.h"
#include "ofFileUtils.h"
#include "ofLog.h"
#include <algorithm>
#include <sstream>

namespace modular {

// ============================================================================
// isParamChannel — check if a channel is in the param list
// ============================================================================

bool MidiConfig::isParamChannel(int ch) const {
    return std::find(paramChannels.begin(), paramChannels.end(), ch) != paramChannels.end();
}

// ============================================================================
// loadFromJson — parse config/midi.json with full validation
// ============================================================================

bool MidiConfig::loadFromJson(const std::string& path) {
    ofFile file(path);
    if (!file.exists()) {
        ofLogNotice("MidiConfig")
            << "No config file found at: " << path
            << " — using defaults (cue=1, paramChannels=[2], port=IAC Driver Bus 1)";
        return false;
    }

    ofJson json = ofLoadJson(path);
    if (json.is_null()) {
        ofLogError("MidiConfig")
            << "Malformed or empty JSON in: " << path
            << " — using defaults";
        return false;
    }

    // Read fields with defaults from current instance values
    portName   = json.value("portName", portName);
    cueChannel = json.value("cueChannel", cueChannel);

    // Read paramChannels — supports both legacy int and new array format
    paramChannels.clear();
    if (json.contains("paramChannels") && json["paramChannels"].is_array()) {
        for (const auto& ch : json["paramChannels"]) {
            if (ch.is_number_integer()) {
                paramChannels.push_back(ch.get<int>());
            }
        }
    } else if (json.contains("paramChannel") && json["paramChannel"].is_number_integer()) {
        // Backward compat: single int → single-element array
        paramChannels.push_back(json["paramChannel"].get<int>());
    }
    if (paramChannels.empty()) {
        paramChannels = {2};  // fallback default
    }

    // Validate
    if (!isValid()) {
        std::ostringstream chList;
        for (size_t i = 0; i < paramChannels.size(); i++) {
            if (i > 0) chList << ",";
            chList << paramChannels[i];
        }
        ofLogError("MidiConfig")
            << "Invalid config in: " << path
            << " (cueChannel=" << cueChannel
            << ", paramChannels=[" << chList.str() << "]"
            << "). Channels must be 1–16 and cueChannel must not be in paramChannels."
            << " Falling back to defaults.";
        cueChannel    = 1;
        paramChannels = {2};
        portName      = "IAC Driver Bus 1";
        return false;
    }

    std::ostringstream chList;
    for (size_t i = 0; i < paramChannels.size(); i++) {
        if (i > 0) chList << ",";
        chList << paramChannels[i];
    }
    ofLogNotice("MidiConfig")
        << "Loaded: port=" << portName
        << " cueChannel=" << cueChannel
        << " paramChannels=[" << chList.str() << "]";
    return true;
}

// ============================================================================
// saveToJson — persist current config, auto-create directory
// ============================================================================

bool MidiConfig::saveToJson(const std::string& path) const {
    ofJson json;
    json["portName"]   = portName;
    json["cueChannel"] = cueChannel;
    json["paramChannels"] = paramChannels;

    // Ensure config/ directory exists
    std::string dir = ofFilePath::getEnclosingDirectory(path);
    if (!dir.empty()) {
        ofDirectory::createDirectory(dir, true, true);
    }

    bool ok = ofSaveJson(path, json);
    if (ok) {
        ofLogNotice("MidiConfig") << "Saved config to: " << path;
    } else {
        ofLogError("MidiConfig") << "Failed to save config to: " << path;
    }
    return ok;
}

// ============================================================================
// isValid — channel range + uniqueness validation
// ============================================================================

bool MidiConfig::isValid() const {
    if (cueChannel < 1 || cueChannel > 16) return false;
    for (int ch : paramChannels) {
        if (ch < 1 || ch > 16) return false;
        if (ch == cueChannel)  return false;
    }
    return true;
}

} // namespace modular
