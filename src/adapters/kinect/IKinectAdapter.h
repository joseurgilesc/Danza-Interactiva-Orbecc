#pragma once

#include "KinectTypes.h"

#include <functional>
#include <vector>

namespace modular {

// Discrete gestures recognized from skeleton data.
enum class GestureType {
    WAVE,
    SWIPE_LEFT,
    SWIPE_RIGHT,
    SWIPE_UP,
    SWIPE_DOWN,
    CLICK,
    GRAB,
    RELEASE
};

// Zone state emitted by the adapter's zone callback.
struct Zone {
    int col = 0;
    int row = 0;
    bool active = false;
    float intensity = 0.0f;
};

// Abstract interface for Kinect skeleton tracking.
// All concrete adapters (OpenNI, mock, future Azure Kinect) implement this contract.
// ZERO dependency on Kinect SDK headers — includes only KinectTypes.h and standard library.
class IKinectAdapter {
public:
    virtual ~IKinectAdapter() = default;

    // ============================================
    // Lifecycle
    // ============================================
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual void update() = 0;
    virtual ConnectionState getConnectionState() const = 0;

    // ============================================
    // Callback Type Aliases (one per data channel)
    // ============================================
    using JointCallback   = std::function<void(int bodyId, JointType type, const Joint& data)>;
    using BodyCallback    = std::function<void(const TrackedBody& body)>;
    using HandCallback    = std::function<void(int bodyId, HandState state)>;
    using GestureCallback = std::function<void(GestureType gesture, float confidence)>;
    using ZoneCallback    = std::function<void(const Zone& zone)>;

    // ============================================
    // Callback Registration
    // ============================================

    // Called each frame for every tracked joint (typically 30fps).
    virtual void setJointCallback(JointCallback cb) = 0;

    // Called when a body appears, disappears, or updates.
    virtual void setBodyCallback(BodyCallback cb) = 0;

    // Called when hand state changes for a tracked body.
    virtual void setHandCallback(HandCallback cb) = 0;

    // Called when a discrete gesture is recognized.
    virtual void setGestureCallback(GestureCallback cb) = 0;

    // Called when a tracked body enters or leaves a zone.
    virtual void setZoneCallback(ZoneCallback cb) = 0;

    // ============================================
    // Polling Access (for rendering / custom processing)
    // ============================================
    virtual std::vector<TrackedBody> getAllTrackedBodies() const = 0;
    virtual Vector3D getRawJointPosition(int bodyId, JointType type) const = 0;

    // ============================================
    // Configuration
    // ============================================
    virtual void setSmoothing(float factor) = 0;
    virtual void setConfidenceThreshold(float min) = 0;
    virtual void setZoneGrid(int cols, int rows) = 0;

    // ============================================
    // Tracking Quality
    // ============================================
    // Returns pooled tracking presence from the upstream sensor.
    // 1.0 = fully tracked, 0.0 = skeleton lost.
    // Used by rendering layers for smooth fade instead of binary on/off.
    virtual float getTrackingPresence() const = 0;
};

} // namespace modular
