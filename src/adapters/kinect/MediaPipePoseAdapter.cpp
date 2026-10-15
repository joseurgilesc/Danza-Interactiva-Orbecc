#include "MediaPipePoseAdapter.h"

#include "ofxOscReceiver.h"
#include "ofxOscMessage.h"
#include <ofLog.h>
#include <algorithm>
#include <cstring>

namespace modular {

// PIMPL to hide ofxOscReceiver from header
struct MediaPipePoseAdapter::Impl {
    std::unique_ptr<ofxOscReceiver> receiver;
    bool receiverSetup = false;
};

// ============================================
// Constructor / Destructor
// ============================================
MediaPipePoseAdapter::MediaPipePoseAdapter()
    : impl_(std::make_unique<Impl>())
    , connectionState_(ConnectionState::DISCONNECTED)
    , listenPort_(12345)
    , smoothingFactor_(0.3f)
    , confidenceThreshold_(0.3f)
    , presence_(1.0f)
    , frameReady_(false)
    , frameCount_(0)
{
    // Initialize body with 18 joints at default positions
    currentBody_.bodyId = 1;
    currentBody_.isTracked = false;
    currentBody_.confidence = 0.0f;
    for (int i = 0; i < static_cast<int>(JointType::COUNT); ++i) {
        currentBody_.joints[i].type       = static_cast<JointType>(i);
        currentBody_.joints[i].position   = {0.5f, 0.5f, 0.5f};
        currentBody_.joints[i].velocity   = {0.0f, 0.0f, 0.0f};
        currentBody_.joints[i].confidence = 0.0f;
    }

    // Seed previous body with same defaults so first-frame lerp is stable
    previousBody_ = currentBody_;
}

MediaPipePoseAdapter::~MediaPipePoseAdapter() {
    shutdown();
}

// ============================================
// Lifecycle
// ============================================
bool MediaPipePoseAdapter::initialize() {
    try {
        impl_->receiver = std::make_unique<ofxOscReceiver>();
        if (impl_->receiver->setup(listenPort_)) {
            impl_->receiverSetup = true;
            connectionState_ = ConnectionState::CONNECTED;
            ofLogNotice("MediaPipePoseAdapter") << "Listening on OSC port " << listenPort_;
            return true;
        } else {
            connectionState_ = ConnectionState::ERROR;
            ofLogError("MediaPipePoseAdapter") << "Failed to setup OSC receiver on port " << listenPort_;
            return false;
        }
    } catch (...) {
        connectionState_ = ConnectionState::ERROR;
        return false;
    }
}

void MediaPipePoseAdapter::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    impl_->receiver.reset();
    impl_->receiverSetup = false;
    connectionState_ = ConnectionState::DISCONNECTED;

    // Preserve callbacks across shutdown/initialize cycles.
    // Callbacks are owned by ofApp (KinectMappingEngine, SceneEngine) and
    // outlive the adapter. Clearing them here breaks Reset Tracking because
    // initialize() does not re-register them, so skeleton data arrives
    // but processJointData is never invoked → no Interactive Points.
}

void MediaPipePoseAdapter::update() {
    processOscMessages();
}

ConnectionState MediaPipePoseAdapter::getConnectionState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connectionState_;
}

// ============================================
// Callback Registration
// ============================================
void MediaPipePoseAdapter::setBodyCallback(BodyCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    bodyCallback_ = std::move(cb);
}

void MediaPipePoseAdapter::setJointCallback(JointCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    jointCallback_ = std::move(cb);
}

void MediaPipePoseAdapter::setHandCallback(HandCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    handCallback_ = std::move(cb);
}

void MediaPipePoseAdapter::setGestureCallback(GestureCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    gestureCallback_ = std::move(cb);
}

void MediaPipePoseAdapter::setZoneCallback(ZoneCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    zoneCallback_ = std::move(cb);
}

// ============================================
// Polling Access
// ============================================
std::vector<TrackedBody> MediaPipePoseAdapter::getAllTrackedBodies() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return trackedBodies_;
}

Vector3D MediaPipePoseAdapter::getRawJointPosition(int bodyId, JointType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& body : trackedBodies_) {
        if (body.bodyId == bodyId) {
            auto idx = static_cast<int>(type);
            if (idx >= 0 && idx < static_cast<int>(JointType::COUNT)) {
                return body.joints[idx].position;
            }
        }
    }
    return {0.5f, 0.5f, 0.5f};
}

float MediaPipePoseAdapter::getTrackingPresence() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return presence_;
}

// ============================================
// Configuration
// ============================================
void MediaPipePoseAdapter::setSmoothing(float factor) {
    std::lock_guard<std::mutex> lock(mutex_);
    smoothingFactor_ = factor;
}

void MediaPipePoseAdapter::setConfidenceThreshold(float threshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    confidenceThreshold_ = threshold;
}

void MediaPipePoseAdapter::setZoneGrid(int /*cols*/, int /*rows*/) {
    // Zone grid not applicable for MediaPipe — no-op
}

// ============================================
// MediaPipe-Specific — OSC Port Config
// ============================================
void MediaPipePoseAdapter::setListenPort(int port) {
    std::lock_guard<std::mutex> lock(mutex_);
    listenPort_ = port;
}

int MediaPipePoseAdapter::getListenPort() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return listenPort_;
}

// ============================================
// OSC Message Processing
// ============================================
void MediaPipePoseAdapter::processOscMessages() {
    if (!impl_->receiver || !impl_->receiverSetup) return;

    while (impl_->receiver->hasWaitingMessages()) {
        ofxOscMessage msg;
        impl_->receiver->getNextMessage(msg);

        std::string addr = msg.getAddress();

        if (addr == "/pose/begin") {
            // Start new frame: bodyId, numJoints
            // Seed positions from previous frame so ofLerp transitions smoothly
            currentBody_.bodyId = msg.getArgAsInt32(0);
            currentBody_.isTracked = true;
            currentBody_.confidence = 0.0f;
            for (int i = 0; i < static_cast<int>(JointType::COUNT); ++i) {
                currentBody_.joints[i].type       = static_cast<JointType>(i);
                currentBody_.joints[i].position   = previousBody_.joints[i].position;
                currentBody_.joints[i].velocity   = {0.0f, 0.0f, 0.0f};
                currentBody_.joints[i].confidence = 0.0f;
            }
            frameReady_ = false;
        }
        else if (addr == "/pose/joint") {
            // Joint data: jointName, x, y, z, confidence
            std::string jointName = msg.getArgAsString(0);
            float x = msg.getArgAsFloat(1);
            float y = msg.getArgAsFloat(2);
            float z = msg.getArgAsFloat(3);
            float conf = msg.getArgAsFloat(4);

            JointType type = jointNameToType(jointName);
            int idx = static_cast<int>(type);
            if (idx >= 0 && idx < static_cast<int>(JointType::COUNT) && conf >= confidenceThreshold_) {
                // MediaPipe x is 0-1 left to right. We want 0=left, 1=right.
                // Mirror horizontally: x' = 1.0 - x
                float newX = 1.0f - x;
                float newY = y;
                float newZ = z;

                currentBody_.joints[idx].type = type;
                currentBody_.joints[idx].confidence = conf;

                // Temporal interpolation from previous committed position
                // This eliminates jitter and provides smooth transitions
                const auto& prev = previousBody_.joints[idx].position;
                float s = smoothingFactor_;
                currentBody_.joints[idx].position.x = ofLerp(prev.x, newX, s);
                currentBody_.joints[idx].position.y = ofLerp(prev.y, newY, s);
                currentBody_.joints[idx].position.z = ofLerp(prev.z, newZ, s);
            }
        }
        else if (addr == "/mediapipe/presence") {
            // Global tracking presence: 1.0 = fully tracked, 0.0 = lost
            std::lock_guard<std::mutex> lock(mutex_);
            presence_ = msg.getArgAsFloat(0);
        }
        else if (addr == "/pose/end") {
            // Frame complete: commit to tracked bodies, invoke callbacks
            {
                std::lock_guard<std::mutex> lock(mutex_);
                trackedBodies_.clear();
                trackedBodies_.push_back(currentBody_);
            }

            // Invoke body callback
            if (bodyCallback_) {
                bodyCallback_(currentBody_);
            }

            // Invoke joint callback — once per joint above confidence threshold
            if (jointCallback_) {
                for (int i = 0; i < static_cast<int>(JointType::COUNT); ++i) {
                    const auto& joint = currentBody_.joints[i];
                    if (joint.confidence >= confidenceThreshold_) {
                        jointCallback_(currentBody_.bodyId, joint.type, joint);
                    }
                }
            }

            // Save as previous frame for next lerp pass
            previousBody_ = currentBody_;

            frameCount_++;
            frameReady_ = true;
        }
    }
}

// ============================================
// Joint Name Mapping
// ============================================
JointType MediaPipePoseAdapter::jointNameToType(const std::string& name) const {
    if (name == "head")           return JointType::HEAD;
    if (name == "neck")           return JointType::NECK;
    if (name == "shoulder_left")  return JointType::SHOULDER_LEFT;
    if (name == "shoulder_right") return JointType::SHOULDER_RIGHT;
    if (name == "elbow_left")     return JointType::ELBOW_LEFT;
    if (name == "elbow_right")    return JointType::ELBOW_RIGHT;
    if (name == "wrist_left")     return JointType::WRIST_LEFT;
    if (name == "wrist_right")    return JointType::WRIST_RIGHT;
    if (name == "hand_left")      return JointType::HAND_LEFT;
    if (name == "hand_right")     return JointType::HAND_RIGHT;
    if (name == "hip_left")       return JointType::HIP_LEFT;
    if (name == "hip_right")      return JointType::HIP_RIGHT;
    if (name == "knee_left")      return JointType::KNEE_LEFT;
    if (name == "knee_right")     return JointType::KNEE_RIGHT;
    if (name == "ankle_left")     return JointType::ANKLE_LEFT;
    if (name == "ankle_right")    return JointType::ANKLE_RIGHT;
    if (name == "spine_base")     return JointType::SPINE_BASE;
    if (name == "spine_mid")      return JointType::SPINE_MID;
    return JointType::HEAD;
}

} // namespace modular
