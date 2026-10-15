#pragma once

#include "adapters/OSCSettings.h"

#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

// Forward declare types (included in .cpp)
// ofxOsc uses global namespace, not 'ofx' namespace
class ofxOscReceiver;
class ofxOscMessage;

namespace modular {

// Forward declarations
class TransportController;

enum class OSCAdapterStatus {
    OK,
    ERROR_PORT_IN_USE,
    ERROR_BIND_FAILED,
    NOT_LISTENING
};

// Callback for OSC-to-parameter mapping
using OscMessageCallback = std::function<void(const std::string& address, float value)>;

class OSCAdapter {
public:
    OSCAdapter(OSCSettingsManager& settingsManager,
               TransportController& transport);
    
    ~OSCAdapter();
    
    // Lifecycle
    OSCAdapterStatus startListening();
    void stopListening();
    bool restartReceiver();
    
    // Called every frame
    void update();
    
    // Configuration
    void applySettings(const OSCSettings& settings);
    void setEnabled(bool enabled);
    bool setPort(int port);
    
    // Stats
    int getBacklogCount() const { return backlogCount_; }
    bool isListening() const { return listening_; }
    int getMessagesProcessed() const { return messagesProcessed_; }
    
    // Debug
    void setLogLevel(const std::string& level);
    
    // Callback for OSC-to-parameter mapping engine
    void setOscMessageCallback(OscMessageCallback callback);
    
private:
    // Internal helpers
    int mapAddressToChannel(const std::string& address) const;
    void handleFaderMessage(const std::string& address, float value);
    void handleToggleMessage(const std::string& address, float value);
    void handlePushMessage(const std::string& address, float value);
    void handleTransportMessage(const std::string& address, const std::string& type, float value);
    std::string stripPrefix(const std::string& address) const;
    void logMessage(const std::string& address, float value, const std::string& type);
    bool shouldLog() const;
    
    // Settings reference
    OSCSettingsManager& settingsManager_;
    
    // Transport controller reference
    TransportController& transport_;
    
    // OSC receiver (opaque handle)
    void* receiver_;
    
    // State
    bool enabled_;
    bool listening_;
    int currentPort_;
    int lastSuccessfulPort_;
    std::string prefix_;
    int maxMessagesPerFrame_;
    std::string logLevel_;
    
    // OSC message callback (for OscMappingEngine)
    OscMessageCallback oscMessageCallback_;
    
    // Stats
    int backlogCount_;
    int messagesProcessed_;
    int totalMessagesReceived_;
    
    // Debounce for transport
    float lastBpm_;
    bool lastPlayState_;
    bool lastStopState_;
};

} // namespace modular