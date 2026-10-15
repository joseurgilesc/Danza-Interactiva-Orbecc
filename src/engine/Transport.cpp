#include "engine/Transport.h"

#include "ofMain.h"

#include <cmath>

namespace modular {

namespace {
constexpr float kMinBpm = 20.0f;
constexpr float kMaxBpm = 240.0f;
constexpr float kDefaultLoopLengthBeats = 64.0f;
}

TransportController::TransportController()
    : online_(true)
    , playing_(false)
    , bpm_(120.0f)
    , beatsElapsed_(0.0f)
    , loopLengthBeats_(kDefaultLoopLengthBeats) {}

void TransportController::update(float deltaTimeSeconds) {
    if (!playing_ || !online_) {
        return;
    }
    const float beatsPerSecond = bpm_ / 60.0f;
    beatsElapsed_ += beatsPerSecond * deltaTimeSeconds;
}

void TransportController::play() {
    if (!online_) {
        ofLogWarning("Transport") << "Cannot start transport while offline";
        return;
    }
    playing_ = true;
}

void TransportController::pause() {
    playing_ = false;
}

void TransportController::stop() {
    playing_ = false;
    beatsElapsed_ = 0.0f;
}

bool TransportController::isPlaying() const {
    return playing_;
}

bool TransportController::isOnline() const {
    return online_;
}

void TransportController::setOnline(bool online) {
    if (!online) {
        play();
    }
    online_ = online;
    if (!online_) {
        playing_ = false;
    }
}

void TransportController::setBpm(float bpm) {
    bpm_ = ofClamp(bpm, kMinBpm, kMaxBpm);
}

float TransportController::getBpm() const {
    return bpm_;
}

float TransportController::getBeatsElapsed() const {
    return beatsElapsed_;
}

float TransportController::getNormalizedPhase() const {
    if (loopLengthBeats_ <= 0.0f) {
        return 0.0f;
    }
    float wrapped = std::fmod(beatsElapsed_, loopLengthBeats_);
    if (wrapped < 0.0f) {
        wrapped += loopLengthBeats_;
    }
    return wrapped / loopLengthBeats_;
}

void TransportController::resetPhase() {
    beatsElapsed_ = 0.0f;
}

} // namespace modular
