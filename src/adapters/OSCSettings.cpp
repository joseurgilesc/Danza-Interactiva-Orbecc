#include "OSCSettings.h"

#include "ofMain.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

namespace modular {

namespace {
constexpr const char* kLogTag = "OSCSettingsManager";
constexpr int kMinPort = 1;
constexpr int kMaxPort = 65535;
constexpr int kMinMessagesPerFrame = 1;
}

bool OSCSettings::operator==(const OSCSettings& other) const {
    return enabled == other.enabled
        && port == other.port
        && prefix == other.prefix
        && logLevel == other.logLevel
        && maxMessagesPerFrame == other.maxMessagesPerFrame;
}

OSCSettingsManager::OSCSettingsManager(const std::string& relativePath)
    : relativePath_(relativePath.empty() ? "config/osc.json" : relativePath)
    , absolutePath_(ofToDataPath(relativePath_.empty() ? "config/osc.json" : relativePath_, true))
    , lastWriteTime_(0)
    , hasLoaded_(false) {
    settings_ = sanitize(OSCSettings{});
}

bool OSCSettingsManager::load() {
    if (relativePath_.empty()) {
        relativePath_ = "config/osc.json";
    }
    absolutePath_ = ofToDataPath(relativePath_, true);

    OSCSettings loaded;
    if (!readFile(loaded)) {
        ofLogNotice(kLogTag) << "Creating default OSC config at " << absolutePath_;
        if (!writeFile(settings_)) {
            ofLogError(kLogTag) << "Failed to write default OSC config to " << absolutePath_;
            return false;
        }
        lastWriteTime_ = queryModificationTime();
        hasLoaded_ = true;
        return true;
    }

    setSettingsInternal(loaded, true);
    hasLoaded_ = true;
    lastWriteTime_ = queryModificationTime();
    return true;
}

bool OSCSettingsManager::load(const std::string& relativePath) {
    relativePath_ = relativePath.empty() ? "config/osc.json" : relativePath;
    return load();
}

bool OSCSettingsManager::save() {
    if (absolutePath_.empty()) {
        absolutePath_ = ofToDataPath(relativePath_.empty() ? "config/osc.json" : relativePath_, true);
    }

    if (!writeFile(settings_)) {
        return false;
    }

    lastWriteTime_ = queryModificationTime();
    return true;
}

bool OSCSettingsManager::save(const OSCSettings& settings) {
    setSettingsInternal(settings, true);
    return save();
}

bool OSCSettingsManager::refreshIfChanged() {
    if (absolutePath_.empty()) {
        absolutePath_ = ofToDataPath(relativePath_.empty() ? "config/osc.json" : relativePath_, true);
    }

    if (!hasLoaded_) {
        return load();
    }

    const std::uint64_t currentTime = queryModificationTime();
    if (currentTime == 0) {
        ofLogWarning(kLogTag) << "OSC config missing at " << absolutePath_
                              << ", recreating defaults.";
        if (writeFile(settings_)) {
            lastWriteTime_ = queryModificationTime();
        }
        return false;
    }
    if (currentTime == lastWriteTime_) {
        return false;
    }

    OSCSettings loaded;
    if (!readFile(loaded)) {
        ofLogWarning(kLogTag) << "Failed to refresh OSC settings from " << absolutePath_;
        return false;
    }

    lastWriteTime_ = currentTime;
    const bool previousDiffers = loaded != settings_;
    setSettingsInternal(loaded, true);
    return previousDiffers;
}

void OSCSettingsManager::applyOverrides(const OSCSettings& overrides, bool persist) {
    setSettingsInternal(overrides, true);
    if (persist) {
        save();
    }
}

const OSCSettings& OSCSettingsManager::current() const {
    return settings_;
}

void OSCSettingsManager::addListener(const ListenerId& id, ChangeListener listener) {
    if (id.empty() || !listener) {
        return;
    }
    listeners_[id] = std::move(listener);
}

void OSCSettingsManager::removeListener(const ListenerId& id) {
    if (id.empty()) {
        return;
    }
    listeners_.erase(id);
}

void OSCSettingsManager::clearListeners() {
    listeners_.clear();
}

bool OSCSettingsManager::ensureDirectory(const std::string& absolutePath) const {
    std::error_code ec;
    const std::filesystem::path target(absolutePath);
    auto directory = target.parent_path();
    if (directory.empty()) {
        return true;
    }
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        ofLogError(kLogTag) << "Failed to create directory " << directory << ": " << ec.message();
        return false;
    }
    return true;
}

bool OSCSettingsManager::readFile(OSCSettings& outSettings) const {
    std::ifstream stream(absolutePath_);
    if (!stream.good()) {
        return false;
    }

    ofJson data;
    try {
        stream >> data;
    } catch (const std::exception& ex) {
        ofLogError(kLogTag) << "Invalid OSC config JSON at " << absolutePath_ << ": " << ex.what();
        return false;
    }

    OSCSettings parsed;
    parsed.enabled = data.value("enabled", parsed.enabled);
    parsed.port = data.value("port", parsed.port);
    parsed.prefix = data.value("prefix", parsed.prefix);
    parsed.logLevel = data.value("logLevel", parsed.logLevel);
    parsed.maxMessagesPerFrame = data.value("maxMessagesPerFrame", parsed.maxMessagesPerFrame);

    outSettings = parsed;
    return true;
}

bool OSCSettingsManager::writeFile(const OSCSettings& settings) const {
    if (!ensureDirectory(absolutePath_)) {
        return false;
    }

    const OSCSettings sanitized = sanitize(settings);
    ofJson json;
    json["_notes"] = ofJson::array({
        "enabled: set false to pause OSC without losing settings",
        "port: UDP port for ofxOscReceiver (1-65535)",
        "prefix: optional namespace without leading slash",
        "logLevel: info/debug/warning/error logging granularity",
        "maxMessagesPerFrame: limit of packets drained each frame"
    });
    json["enabled"] = sanitized.enabled;
    json["port"] = sanitized.port;
    json["prefix"] = sanitized.prefix;
    json["logLevel"] = sanitized.logLevel;
    json["maxMessagesPerFrame"] = sanitized.maxMessagesPerFrame;

    try {
        std::ofstream stream(absolutePath_, std::ios::trunc);
        stream << json.dump(2);
        return true;
    } catch (const std::exception& ex) {
        ofLogError(kLogTag) << "Failed to write OSC config to " << absolutePath_ << ": " << ex.what();
        return false;
    }
}

OSCSettings OSCSettingsManager::sanitize(const OSCSettings& raw) const {
    OSCSettings sanitized = raw;
    sanitized.port = std::clamp(raw.port, kMinPort, kMaxPort);
    sanitized.prefix = sanitizePrefix(raw.prefix);
    sanitized.logLevel = raw.logLevel.empty() ? std::string("info") : raw.logLevel;
    sanitized.maxMessagesPerFrame = std::max(kMinMessagesPerFrame, raw.maxMessagesPerFrame);
    return sanitized;
}

std::string OSCSettingsManager::sanitizePrefix(const std::string& value) {
    std::string trimmed = ofTrim(value);
    // Remove leading slash characters
    while (!trimmed.empty() && trimmed.front() == '/') {
        trimmed.erase(trimmed.begin());
    }
    // Remove trailing slash characters
    while (!trimmed.empty() && trimmed.back() == '/') {
        trimmed.pop_back();
    }
    return trimmed;
}

void OSCSettingsManager::setSettingsInternal(const OSCSettings& settings, bool notify) {
    OSCSettings sanitized = sanitize(settings);
    if (sanitized == settings_) {
        return;
    }
    settings_ = sanitized;
    if (notify) {
        notifyListeners();
    }
}

void OSCSettingsManager::notifyListeners() const {
    for (const auto& entry : listeners_) {
        if (entry.second) {
            entry.second(settings_);
        }
    }
}

std::uint64_t OSCSettingsManager::queryModificationTime() const {
    if (absolutePath_.empty()) {
        return 0;
    }
    std::error_code ec;
    const std::filesystem::path path(absolutePath_);
    if (!std::filesystem::exists(path, ec) || ec) {
        return 0;
    }
    auto timePoint = std::filesystem::last_write_time(path, ec);
    if (ec) {
        return 0;
    }
    const auto duration = timePoint.time_since_epoch();
    const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
    return static_cast<std::uint64_t>(nanos.count());
}

} // namespace modular
