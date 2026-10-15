#pragma once

#include "KinectTypes.h"
#include "IKinectAdapter.h"

#include <functional>
#include <vector>
#include <mutex>

namespace modular {

class MockKinectAdapter : public IKinectAdapter {
public:
    enum class SimMode {
        IDLE,       // Body stands still at center — applies setMockJointPosition overrides
        WALKING,    // Body moves with walking animation (sinusoidal Y oscillation)
        WAVING      // Body at center, right hand waves — other joints stable
    };

    explicit MockKinectAdapter(float fps = 30.0f);
    ~MockKinectAdapter() override;

    // ============================================
    // IKinectAdapter — Lifecycle
    // ============================================
    bool initialize() override;
    void shutdown() override;
    void update() override;
    ConnectionState getConnectionState() const override;

    // ============================================
    // IKinectAdapter — Callback Registration
    // ============================================
    void setJointCallback(JointCallback cb) override;
    void setBodyCallback(BodyCallback cb) override;
    void setHandCallback(HandCallback cb) override;
    void setGestureCallback(GestureCallback cb) override;
    void setZoneCallback(ZoneCallback cb) override;

    // ============================================
    // IKinectAdapter — Polling Access
    // ============================================
    std::vector<TrackedBody> getAllTrackedBodies() const override;
    Vector3D getRawJointPosition(int bodyId, JointType type) const override;
    float getTrackingPresence() const override;

    // ============================================
    // IKinectAdapter — Configuration
    // ============================================
    void setSmoothing(float factor) override;
    void setConfidenceThreshold(float threshold) override;
    void setZoneGrid(int columns, int rows) override;

    // ============================================
    // Mock-Specific — Simulation Control
    // ============================================
    void setSimMode(SimMode mode);
    SimMode getSimMode() const;

    // Override a single joint position (applied in IDLE mode only).
    // Simulation modes (WALKING, WAVING) override per-joint settings.
    void setMockJointPosition(JointType type, const Vector3D& pos);

private:
    // ============================================
    // Pose Generation (one per simulation mode)
    // ============================================
    void generateDefaultPose();
    void generateIdlePose(float time);
    void generateWalkingPose(float time);
    void generateWavingPose(float time);

    // ============================================
    // Callback Dispatch
    // ============================================
    void invokeCallbacks();

    // ============================================
    // State
    // ============================================
    TrackedBody body_;
    ConnectionState connectionState_;
    SimMode simMode_;
    float frameTime_;
    float elapsedTime_;
    float smoothingFactor_;
    float confidenceThreshold_;

    // Per-joint manual overrides (effective in IDLE mode)
    bool  mockJointSet_[static_cast<int>(JointType::COUNT)];
    Vector3D mockJointPositions_[static_cast<int>(JointType::COUNT)];

    // Callbacks
    JointCallback   jointCallback_;
    BodyCallback    bodyCallback_;
    HandCallback    handCallback_;
    GestureCallback gestureCallback_;
    ZoneCallback    zoneCallback_;

    // Zone grid config
    int zoneColumns_;
    int zoneRows_;

    mutable std::mutex mutex_;
};

} // namespace modular
