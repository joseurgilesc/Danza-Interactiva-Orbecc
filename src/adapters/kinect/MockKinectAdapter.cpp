#include "MockKinectAdapter.h"

#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace modular {

// ============================================
// Default Joint Pose Table
// Anatomically reasonable resting pose in 0.0-1.0 normalized space.
// Indexed by JointType enum value (0..17).
// ============================================
namespace {
    // 18 joints indexed by JointType enum (0..17). COUNT == 18.
    // Y values: 0.0 = top of screen, 1.0 = bottom (screen-space convention)
    // Head at top (~0.15), feet at bottom (~0.92)
    const Vector3D kDefaultPose[] = {
        // HEAD               (0)
        {0.50f, 0.15f, 0.50f},
        // NECK               (1)
        {0.50f, 0.22f, 0.50f},
        // SHOULDER_LEFT      (2)
        {0.42f, 0.25f, 0.48f},
        // SHOULDER_RIGHT     (3)
        {0.58f, 0.25f, 0.52f},
        // ELBOW_LEFT         (4)
        {0.35f, 0.40f, 0.47f},
        // ELBOW_RIGHT        (5)
        {0.65f, 0.40f, 0.53f},
        // WRIST_LEFT         (6)
        {0.30f, 0.55f, 0.46f},
        // WRIST_RIGHT        (7)
        {0.70f, 0.55f, 0.54f},
        // HAND_LEFT          (8)
        {0.28f, 0.62f, 0.45f},
        // HAND_RIGHT         (9)
        {0.72f, 0.62f, 0.55f},
        // HIP_LEFT           (10)
        {0.44f, 0.52f, 0.48f},
        // HIP_RIGHT          (11)
        {0.56f, 0.52f, 0.52f},
        // KNEE_LEFT          (12)
        {0.44f, 0.72f, 0.49f},
        // KNEE_RIGHT         (13)
        {0.56f, 0.72f, 0.51f},
        // ANKLE_LEFT         (14)
        {0.44f, 0.92f, 0.50f},
        // ANKLE_RIGHT        (15)
        {0.56f, 0.92f, 0.50f},
        // SPINE_BASE         (16)
        {0.50f, 0.52f, 0.50f},
        // SPINE_MID          (17)
        {0.50f, 0.35f, 0.50f},
    };

    static_assert(sizeof(kDefaultPose) / sizeof(kDefaultPose[0]) ==
                      static_cast<int>(JointType::COUNT),
                  "kDefaultPose must match JointType::COUNT entries");
} // anonymous namespace

// ============================================
// Constructor / Destructor
// ============================================
MockKinectAdapter::MockKinectAdapter(float fps)
    : connectionState_(ConnectionState::DISCONNECTED)
    , simMode_(SimMode::IDLE)
    , frameTime_(1.0f / fps)
    , elapsedTime_(0.0f)
    , smoothingFactor_(0.0f)
    , confidenceThreshold_(0.0f)
    , zoneColumns_(0)
    , zoneRows_(0)
{
    body_.bodyId = 1;
    body_.isTracked = true;
    body_.confidence = 1.0f;

    std::memset(mockJointSet_, 0, sizeof(mockJointSet_));
    generateDefaultPose();
}

MockKinectAdapter::~MockKinectAdapter() {
    shutdown();
}

// ============================================
// Lifecycle
// ============================================
bool MockKinectAdapter::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    connectionState_ = ConnectionState::CONNECTED;
    generateDefaultPose();
    return true;
}

void MockKinectAdapter::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    connectionState_ = ConnectionState::DISCONNECTED;

    // Preserve callbacks across shutdown/initialize cycles.
    // Same rationale as MediaPipePoseAdapter — clearing callbacks here
    // breaks Reset Tracking because initialize() does not re-register them.
}

void MockKinectAdapter::update() {
    std::lock_guard<std::mutex> lock(mutex_);

    elapsedTime_ += frameTime_;

    // Generate pose for current simulation mode
    switch (simMode_) {
        case SimMode::IDLE:
            generateIdlePose(elapsedTime_);
            break;
        case SimMode::WALKING:
            generateWalkingPose(elapsedTime_);
            break;
        case SimMode::WAVING:
            generateWavingPose(elapsedTime_);
            break;
    }

    invokeCallbacks();
}

ConnectionState MockKinectAdapter::getConnectionState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connectionState_;
}

// ============================================
// Callback Registration
// ============================================
void MockKinectAdapter::setJointCallback(JointCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    jointCallback_ = std::move(cb);
}

void MockKinectAdapter::setBodyCallback(BodyCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    bodyCallback_ = std::move(cb);
}

void MockKinectAdapter::setHandCallback(HandCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    handCallback_ = std::move(cb);
}

void MockKinectAdapter::setGestureCallback(GestureCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    gestureCallback_ = std::move(cb);
}

void MockKinectAdapter::setZoneCallback(ZoneCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    zoneCallback_ = std::move(cb);
}

// ============================================
// Polling Access
// ============================================
std::vector<TrackedBody> MockKinectAdapter::getAllTrackedBodies() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return { body_ };
}

Vector3D MockKinectAdapter::getRawJointPosition(int bodyId, JointType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (bodyId != body_.bodyId) {
        return { 0.5f, 0.5f, 0.5f };
    }
    auto idx = static_cast<int>(type);
    if (idx < 0 || idx >= static_cast<int>(JointType::COUNT)) {
        return { 0.5f, 0.5f, 0.5f };
    }
    return body_.joints[idx].position;
}

// ============================================
// Configuration
// ============================================
void MockKinectAdapter::setSmoothing(float factor) {
    std::lock_guard<std::mutex> lock(mutex_);
    smoothingFactor_ = factor;
}

void MockKinectAdapter::setConfidenceThreshold(float threshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    confidenceThreshold_ = threshold;
}

void MockKinectAdapter::setZoneGrid(int columns, int rows) {
    std::lock_guard<std::mutex> lock(mutex_);
    zoneColumns_ = columns;
    zoneRows_ = rows;
}

float MockKinectAdapter::getTrackingPresence() const {
    // Mock always has perfect tracking — no loss simulation
    return 1.0f;
}

// ============================================
// Mock-Specific — Simulation Control
// ============================================
void MockKinectAdapter::setSimMode(SimMode mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    simMode_ = mode;
}

MockKinectAdapter::SimMode MockKinectAdapter::getSimMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return simMode_;
}

void MockKinectAdapter::setMockJointPosition(JointType type, const Vector3D& pos) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto idx = static_cast<int>(type);
    if (idx >= 0 && idx < static_cast<int>(JointType::COUNT)) {
        mockJointSet_[idx]   = true;
        mockJointPositions_[idx] = pos;
    }
}

// ============================================
// Pose Generation — Default / IDLE
// ============================================
void MockKinectAdapter::generateDefaultPose() {
    const auto count = static_cast<int>(JointType::COUNT);
    for (auto i = 0; i < count; ++i) {
        body_.joints[i].type       = static_cast<JointType>(i);
        body_.joints[i].position   = kDefaultPose[i];
        body_.joints[i].velocity   = { 0.0f, 0.0f, 0.0f };
        body_.joints[i].confidence = 1.0f;
    }
}

void MockKinectAdapter::generateIdlePose(float /*time*/) {
    // Restore default pose, then apply manual overrides (if any).
    const auto count = static_cast<int>(JointType::COUNT);
    for (auto i = 0; i < count; ++i) {
        body_.joints[i].position = kDefaultPose[i];
        if (mockJointSet_[i]) {
            body_.joints[i].position = mockJointPositions_[i];
        }
    }
}

// ============================================
// Pose Generation — WALKING
// ============================================
void MockKinectAdapter::generateWalkingPose(float time) {
    // All joints oscillate vertically: y = 0.5 + 0.2 * sin(t * 2.0)
    // Range: 0.3 – 0.7, matching spec §mock-adapter walking scenario.
    const float baseTime = time;
    const auto  count    = static_cast<int>(JointType::COUNT);

    for (auto i = 0; i < count; ++i) {
        body_.joints[i].position = kDefaultPose[i];
        body_.joints[i].position.y =
            kDefaultPose[i].y + 0.2f * std::sin(baseTime * 2.0f);
    }

    // Arm–leg swing: right arm + left leg forward when left arm + right leg back.
    const float swing = 0.12f * std::sin(baseTime * 2.0f);

    // Arms
    body_.joints[static_cast<int>(JointType::HAND_RIGHT)].position.x += swing;
    body_.joints[static_cast<int>(JointType::ELBOW_RIGHT)].position.x += swing * 0.6f;
    body_.joints[static_cast<int>(JointType::WRIST_RIGHT)].position.x += swing * 0.8f;

    body_.joints[static_cast<int>(JointType::HAND_LEFT)].position.x -= swing;
    body_.joints[static_cast<int>(JointType::ELBOW_LEFT)].position.x -= swing * 0.6f;
    body_.joints[static_cast<int>(JointType::WRIST_LEFT)].position.x -= swing * 0.8f;

    // Legs
    body_.joints[static_cast<int>(JointType::ANKLE_RIGHT)].position.x += swing * 0.5f;
    body_.joints[static_cast<int>(JointType::ANKLE_LEFT)].position.x -= swing * 0.5f;
}

// ============================================
// Pose Generation — WAVING
// ============================================
void MockKinectAdapter::generateWavingPose(float time) {
    const auto count = static_cast<int>(JointType::COUNT);

    // Restore default pose for all joints first (spec: "other joints remain stable")
    for (auto i = 0; i < count; ++i) {
        body_.joints[i].position = kDefaultPose[i];
    }

    // Right hand oscillates: x 0.3–0.7, y 0.4–0.8  (spec §mock-adapter waving scenario)
    const float baseTime = time;
    const float xOsc = 0.2f * std::sin(baseTime * 3.0f);
    const float yOsc = 0.2f * std::cos(baseTime * 3.0f);

    auto& handRight = body_.joints[static_cast<int>(JointType::HAND_RIGHT)];
    handRight.position.x = kDefaultPose[static_cast<int>(JointType::HAND_RIGHT)].x + xOsc;
    handRight.position.y = kDefaultPose[static_cast<int>(JointType::HAND_RIGHT)].y + yOsc;

    // Slight follow-through on wrist and elbow for visual continuity
    auto& wristRight = body_.joints[static_cast<int>(JointType::WRIST_RIGHT)];
    wristRight.position.x += xOsc * 0.5f;
    wristRight.position.y += yOsc * 0.5f;

    auto& elbowRight = body_.joints[static_cast<int>(JointType::ELBOW_RIGHT)];
    elbowRight.position.x += xOsc * 0.2f;
    elbowRight.position.y += yOsc * 0.2f;
}

// ============================================
// Callback Dispatch
// ============================================
void MockKinectAdapter::invokeCallbacks() {
    // Joint callback — once per joint (matching real adapter behavior)
    if (jointCallback_) {
        const auto count = static_cast<int>(JointType::COUNT);
        for (auto i = 0; i < count; ++i) {
            JointType type = static_cast<JointType>(i);
            jointCallback_(body_.bodyId, type, body_.joints[i]);
        }
    }

    // Body callback — once per frame with full TrackedBody
    if (bodyCallback_) {
        bodyCallback_(body_);
    }

    // Hand callback — once per frame
    if (handCallback_) {
        HandState state = HandState::UNKNOWN;
        if (simMode_ == SimMode::WAVING) {
            state = HandState::OPEN;
        }
        handCallback_(body_.bodyId, state);
    }

    // Gesture and Zone callbacks are not automatically fired by the mock.
    // Integration tests or external code can register them and poll
    // via the adapter interface if needed.
}

} // namespace modular
