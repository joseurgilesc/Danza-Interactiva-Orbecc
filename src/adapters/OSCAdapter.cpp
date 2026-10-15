#include "adapters/OSCAdapter.h"

// ofxOsc includes - use path relative to addons
#include "../../../addons/ofxOsc/src/ofxOscReceiver.h"
#include "../../../addons/ofxOsc/src/ofxOscMessage.h"

// Engine includes
#include "engine/Transport.h"

#include <ofLog.h>
#include <ofUtils.h>

#include <algorithm>
#include <regex>

namespace modular {

namespace {
// Extract numeric part from address like "/fader/3" -> 3
int extractChannelFromAddress(const std::string& addr, const std::string& pattern) {
    std::regex re(pattern + "/(\\d+)");
    std::smatch match;
    if (std::regex_search(addr, match, re)) {
        try {
            return std::stoi(match[1].str()) - 1; // 0-based
        } catch (...) {
            return -1;
        }
    }
    return -1;
}
} // anonymous namespace

OSCAdapter::OSCAdapter(OSCSettingsManager& settingsManager,
                       TransportController& transport)
    : settingsManager_(settingsManager)
    , transport_(transport)
    , receiver_(nullptr)
    , enabled_(true)
    , listening_(false)
    , currentPort_(9000)
    , lastSuccessfulPort_(9000)
    , prefix_("rig")
    , maxMessagesPerFrame_(64)
    , logLevel_("info")
    , backlogCount_(0)
    , messagesProcessed_(0)
    , totalMessagesReceived_(0)
    , lastBpm_(0.0f)
    , lastPlayState_(false)
    , lastStopState_(false)
{
    const auto& s = settingsManager_.current();
    enabled_ = s.enabled;
    currentPort_ = s.port;
    prefix_ = s.prefix;
    maxMessagesPerFrame_ = s.maxMessagesPerFrame;
    logLevel_ = s.logLevel;
}

OSCAdapter::~OSCAdapter() {
    stopListening();
}

OSCAdapterStatus OSCAdapter::startListening() {
    if (!enabled_) {
        ofLogNotice("OSCAdapter") << "OSC disabled in settings, not starting";
        return OSCAdapterStatus::OK;
    }
    
    if (listening_) {
        ofLogNotice("OSCAdapter") << "Already listening on port " << currentPort_;
        return OSCAdapterStatus::OK;
    }
    
    try {
        auto* receiver = new ofxOscReceiver();
        receiver->setup(currentPort_);
        receiver_ = receiver;
        
        listening_ = true;
        lastSuccessfulPort_ = currentPort_;
        ofLogNotice("OSCAdapter") << "Listening on port " << currentPort_;
        return OSCAdapterStatus::OK;
        
    } catch (const std::exception& e) {
        ofLogError("OSCAdapter") << "Failed to start OSC receiver: " << e.what();
        return OSCAdapterStatus::ERROR_BIND_FAILED;
    } catch (...) {
        ofLogError("OSCAdapter") << "Failed to start OSC receiver (unknown error)";
        return OSCAdapterStatus::ERROR_BIND_FAILED;
    }
}

void OSCAdapter::stopListening() {
    if (receiver_) {
        auto* receiver = static_cast<ofxOscReceiver*>(receiver_);
        delete receiver;
        receiver_ = nullptr;
    }
    listening_ = false;
    ofLogNotice("OSCAdapter") << "Stopped listening";
}

bool OSCAdapter::restartReceiver() {
    stopListening();
    
    const auto& s = settingsManager_.current();
    if (!s.enabled) {
        enabled_ = false;
        return true;
    }
    
    enabled_ = true;
    currentPort_ = s.port;
    prefix_ = s.prefix;
    maxMessagesPerFrame_ = s.maxMessagesPerFrame;
    logLevel_ = s.logLevel;
    
    auto status = startListening();
    return status == OSCAdapterStatus::OK;
}

void OSCAdapter::update() {
    if (!enabled_ || !listening_ || !receiver_) {
        return;
    }
    
    // Check for settings changes (hot reload)
    if (settingsManager_.refreshIfChanged()) {
        const auto& newSettings = settingsManager_.current();
        if (newSettings.port != currentPort_ || newSettings.enabled != enabled_) {
            if (!restartReceiver()) {
                ofLogWarning("OSCAdapter") << "Failed to restart after settings change";
            }
        }
        // Update runtime settings without restart
        prefix_ = newSettings.prefix;
        maxMessagesPerFrame_ = newSettings.maxMessagesPerFrame;
        logLevel_ = newSettings.logLevel;
        enabled_ = newSettings.enabled;
    }
    
    // Process messages with frame budget
    int processed = 0;
    backlogCount_ = 0;
    
    auto* receiver = static_cast<ofxOscReceiver*>(receiver_);
    while (receiver->hasWaitingMessages() && processed < maxMessagesPerFrame_) {
        ofxOscMessage msg;
        receiver->getNextMessage(msg);
        
        if (shouldLog()) {
            ofLogNotice("OSCAdapter") << "Received: " << msg.getAddress();
        }
        
        std::string addr = stripPrefix(msg.getAddress());
        
        // Route based on address pattern
        if (addr.find("/fader/") == 0) {
            int channel = extractChannelFromAddress(addr, "/fader");
            if (channel >= 0 && msg.getNumArgs() > 0) {
                handleFaderMessage(addr, msg.getArgAsFloat(0));
            }
        } else if (addr.find("/toggle/") == 0) {
            int channel = extractChannelFromAddress(addr, "/toggle");
            if (channel >= 0 && msg.getNumArgs() > 0) {
                handleToggleMessage(addr, msg.getArgAsFloat(0));
            }
        } else if (addr.find("/push/") == 0) {
            int channel = extractChannelFromAddress(addr, "/push");
            if (channel >= 0 && msg.getNumArgs() > 0) {
                handlePushMessage(addr, msg.getArgAsFloat(0));
            }
        } else if (addr == "/bpm") {
            if (msg.getNumArgs() > 0) {
                handleTransportMessage(addr, "bpm", msg.getArgAsFloat(0));
            }
        } else if (addr == "/play") {
            handleTransportMessage(addr, "play", 1.0f);
        } else if (addr == "/stop") {
            handleTransportMessage(addr, "stop", 1.0f);
        } else {
            if (shouldLog()) {
                ofLogWarning("OSCAdapter") << "Unhandled OSC address: " << addr;
            }
        }
        
        // Forward to OSC mapping callback for generic parameter binding
        if (oscMessageCallback_) {
            oscMessageCallback_(addr, msg.getNumArgs() > 0 ? msg.getArgAsFloat(0) : 0.0f);
        }
        
        processed++;
        totalMessagesReceived_++;
    }
    
    messagesProcessed_ = processed;
    
    // Log backlog warning if we hit the limit
    // Note: ofxOsc doesn't provide getWaitingMessages(), so we just detect presence
    if (receiver->hasWaitingMessages() && processed >= maxMessagesPerFrame_) {
        backlogCount_ = 1; // Indicates backlog exists (exact count unavailable)
        ofLogWarning("OSCAdapter") << "OSC backlog detected (frame budget: " 
                                   << maxMessagesPerFrame_ << ")";
    }
}

void OSCAdapter::applySettings(const OSCSettings& settings) {
    enabled_ = settings.enabled;
    prefix_ = settings.prefix;
    maxMessagesPerFrame_ = settings.maxMessagesPerFrame;
    logLevel_ = settings.logLevel;
    
    if (settings.port != currentPort_) {
        setPort(settings.port);
    }
}

void OSCAdapter::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled && listening_) {
        stopListening();
    } else if (enabled && !listening_) {
        startListening();
    }
}

bool OSCAdapter::setPort(int port) {
    if (port < 1024 || port > 65535) {
        ofLogError("OSCAdapter") << "Invalid port: " << port << " (must be 1024-65535)";
        return false;
    }
    
    if (port == currentPort_ && listening_) {
        return true;
    }
    
    currentPort_ = port;
    return restartReceiver();
}

void OSCAdapter::setLogLevel(const std::string& level) {
    logLevel_ = level;
}

void OSCAdapter::setOscMessageCallback(OscMessageCallback callback) {
    oscMessageCallback_ = std::move(callback);
}

std::string OSCAdapter::stripPrefix(const std::string& address) const {
    if (prefix_.empty()) {
        return address;
    }
    
    // Handle addresses like "/rig/fader/1" -> "/fader/1"
    std::string prefixWithSlash = "/" + prefix_;
    if (address.find(prefixWithSlash) == 0) {
        return address.substr(prefixWithSlash.length());
    }
    
    return address;
}

int OSCAdapter::mapAddressToChannel(const std::string& address) const {
    // Extract channel number from addresses like "/fader/3", "/toggle/5", "/push/1"
    if (address.find("/fader/") == 0) {
        return extractChannelFromAddress(address, "/fader");
    } else if (address.find("/toggle/") == 0) {
        return extractChannelFromAddress(address, "/toggle");
    } else if (address.find("/push/") == 0) {
        return extractChannelFromAddress(address, "/push");
    }
    return -1;
}

void OSCAdapter::handleFaderMessage(const std::string& address, float value) {
    int channel = extractChannelFromAddress(address, "/fader");
    if (channel < 0) return;
    
    // Clamp to 0-1
    float normalized = std::max(0.0f, std::min(1.0f, value));
    
    if (shouldLog()) {
        ofLogNotice("OSCAdapter") << "Fader " << (channel + 1) << " = " << normalized;
    }
    
    // TODO: Call mappingLayer_ API to set channel value
    // mappingLayer_.setChannelValue(channel, normalized);
}

void OSCAdapter::handleToggleMessage(const std::string& address, float value) {
    int channel = extractChannelFromAddress(address, "/toggle");
    if (channel < 0) return;
    
    // Normalize to 0 or 1
    float normalized = (value > 0.5f) ? 1.0f : 0.0f;
    
    if (shouldLog()) {
        ofLogNotice("OSCAdapter") << "Toggle " << (channel + 1) << " = " << (int)normalized;
    }
    
    // TODO: Call mappingLayer_ API
}

void OSCAdapter::handlePushMessage(const std::string& address, float value) {
    int channel = extractChannelFromAddress(address, "/push");
    if (channel < 0) return;
    
    // Normalize to 0 or 1
    float normalized = (value > 0.5f) ? 1.0f : 0.0f;
    
    if (shouldLog()) {
        ofLogNotice("OSCAdapter") << "Push " << (channel + 1) << " = " << (int)normalized;
    }
    
    // TODO: Call mappingLayer_ API
}

void OSCAdapter::handleTransportMessage(const std::string& address, 
                                        const std::string& type, 
                                        float value) {
    if (type == "bpm") {
        // Clamp BPM to valid range (40-240)
        float clampedBpm = std::max(40.0f, std::min(240.0f, value));
        
        // Only update if significantly different (debounce)
        if (std::abs(clampedBpm - lastBpm_) > 0.001f) {
            transport_.setBpm(clampedBpm);
            lastBpm_ = clampedBpm;
            if (shouldLog()) {
                ofLogNotice("OSCAdapter") << "BPM set to " << clampedBpm;
            }
        }
    } else if (type == "play") {
        if (!lastPlayState_) {
            transport_.play();
            lastPlayState_ = true;
            if (shouldLog()) {
                ofLogNotice("OSCAdapter") << "Transport play";
            }
        }
        lastPlayState_ = (value > 0.5f);
    } else if (type == "stop") {
        if (!lastStopState_) {
            transport_.stop();
            lastStopState_ = true;
            if (shouldLog()) {
                ofLogNotice("OSCAdapter") << "Transport stop";
            }
        }
        lastStopState_ = (value > 0.5f);
    }
}

void OSCAdapter::logMessage(const std::string& address, float value, const std::string& type) {
    if (shouldLog()) {
        ofLogNotice("OSCAdapter") << type << " " << address << " = " << value;
    }
}

bool OSCAdapter::shouldLog() const {
    return logLevel_ == "debug" || logLevel_ == "notice";
}

} // namespace modular