#pragma once

#include "IKinectAdapter.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Forward declarations
class ofxOscReceiver;

namespace modular {

class MediaPipePoseAdapter : public IKinectAdapter {
public:
    MediaPipePoseAdapter();
    ~MediaPipePoseAdapter() override;

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
    void setJointCallback(JointCallback callback) override;
    void setBodyCallback(BodyCallback callback) override;
    void setHandCallback(HandCallback callback) override;
    void setGestureCallback(GestureCallback callback) override;
    void setZoneCallback(ZoneCallback callback) override;

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
    // MediaPipe-Specific — OSC Port Config
    // ============================================
    void setListenPort(int port);
    int getListenPort() const;

private:
    void processOscMessages();
    JointType jointNameToType(const std::string& name) const;

    // Opaque OSC receiver (PIMPL to avoid ofxOsc headers in public API)
    struct Impl;
    std::unique_ptr<Impl> impl_;

    ConnectionState connectionState_;
    int listenPort_;
    float smoothingFactor_;
    float confidenceThreshold_;
    float presence_;          // Pooled tracking presence from /mediapipe/presence OSC

    // Accumulated bodies this frame
    std::vector<TrackedBody> trackedBodies_;
    TrackedBody currentBody_;
    TrackedBody previousBody_;  // Last committed body — used for ofLerp interpolation
    bool frameReady_;
    int frameCount_;

    // Callbacks
    JointCallback   jointCallback_;
    BodyCallback    bodyCallback_;
    HandCallback    handCallback_;
    GestureCallback gestureCallback_;
    ZoneCallback    zoneCallback_;

    mutable std::mutex mutex_;
};

} // namespace modular
