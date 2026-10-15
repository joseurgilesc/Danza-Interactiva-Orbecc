#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace modular {

struct OSCSettings {
    bool enabled = true;
    int port = 9000;
    std::string prefix;
    std::string logLevel = "info";
    int maxMessagesPerFrame = 64;

    bool operator==(const OSCSettings& other) const;
    bool operator!=(const OSCSettings& other) const { return !(*this == other); }
};

class OSCSettingsManager {
public:
    using ChangeListener = std::function<void(const OSCSettings&)>;
    using ListenerId = std::string;

    explicit OSCSettingsManager(const std::string& relativePath = "config/osc.json");

    bool load();
    bool load(const std::string& relativePath);
    bool save();
    bool save(const OSCSettings& settings);
    bool refreshIfChanged();
    void applyOverrides(const OSCSettings& overrides, bool persist = false);

    const OSCSettings& current() const;
    const std::string& relativePath() const { return relativePath_; }
    const std::string& absolutePath() const { return absolutePath_; }

    void addListener(const ListenerId& id, ChangeListener listener);
    void removeListener(const ListenerId& id);
    void clearListeners();

private:
    bool ensureDirectory(const std::string& absolutePath) const;
    bool readFile(OSCSettings& outSettings) const;
    bool writeFile(const OSCSettings& settings) const;
    OSCSettings sanitize(const OSCSettings& raw) const;
    static std::string sanitizePrefix(const std::string& value);
    void setSettingsInternal(const OSCSettings& settings, bool notify);
    void notifyListeners() const;
    std::uint64_t queryModificationTime() const;

    std::string relativePath_;
    std::string absolutePath_;
    OSCSettings settings_;
    std::uint64_t lastWriteTime_;
    bool hasLoaded_;
    std::unordered_map<ListenerId, ChangeListener> listeners_;
};

} // namespace modular
