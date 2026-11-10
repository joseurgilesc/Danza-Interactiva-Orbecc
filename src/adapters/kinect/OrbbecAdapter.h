#pragma once

#include "IKinectAdapter.h"

#include "ofTexture.h"
#include "ofPixels.h"

#include <libobsensor/ObSensor.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace modular {

// ---------------------------------------------------------------------------
// OrbbecAdapter — native Orbbec Gemini 2 adapter.
//
// Reads color + depth (D2C-aligned) directly from the OrbbecSDK C++ library,
// runs MediaPipe Pose over a local TCP detector process (pose_detector.py), and
// produces the 18 OpenNI-style joints with a REAL depth: the joint's z is
// sampled from the depth map at the joint's pixel, normalized to 0..1
// (0 = near, 1 = far). This replaces the 2D OSC bridge where z was 0.
// ---------------------------------------------------------------------------
class OrbbecAdapter : public IKinectAdapter {
public:
    OrbbecAdapter();
    ~OrbbecAdapter() override;

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
    // Orbbec-specific configuration
    // ============================================
    void setDetectorPort(int port);
    void setDepthRange(float nearMM, float farMM);

    // Raw aligned depth (mm), same resolution as color after D2C. Exposed so
    // scenes can build a body silhouette/contour from real depth (T3).
    const std::vector<uint16_t>& getDepthData() const;
    int getDepthWidth() const;
    int getDepthHeight() const;

    // Textura de la cámara RGB (para vista de cámara con atajo).
    const ofTexture& getColorTexture() const { return colorTex_; }
    bool hasColorTexture() const { return colorTex_.isAllocated(); }

private:
    bool setupCamera();
    void detectorLoop();
    void grabLatestFrame();
    void pushFrameToDetector(const std::vector<unsigned char>& rgb, int w, int h);
    void applyPose(const std::vector<std::array<float, 3>>& pts,
                   const std::vector<float>& vis, int bodyId);
    JointType landmarkIndexToJointType(int index) const;
    float sampleDepthNormalized(float nx, float ny) const;
    void fireFrameCallbacks(const TrackedBody& body);

    // --- Camera ---
    std::shared_ptr<ob::Pipeline> pipe_;
    std::shared_ptr<ob::FrameSet> latestFrames_;
    std::mutex frameMutex_;
    bool cameraReady_ = false;

    std::vector<uint16_t> depthData_;   // aligned depth (mm), color resolution
    int depthW_ = 0, depthH_ = 0;
    int colorW_ = 0, colorH_ = 0;

    ofTexture colorTex_;     // textura de la cámara RGB (para vista de cámara)
    ofPixels colorPixels_;   // buffer de píxeles del color

    // --- Detector (TCP) ---
    std::thread detectorThread_;
    std::atomic<bool> detectorRunning_{false};
    std::atomic<bool> hasPending_{false};
    std::mutex pendingMutex_;
    std::vector<unsigned char> pendingFrame_;
    int pendingW_ = 0, pendingH_ = 0;
    int detectorPort_ = 5005;

    // Latest pose results from the detector thread (written by detector thread,
    // read by update() on the main thread). Multi-person: lista de poses.
    std::mutex poseMutex_;
    std::vector<std::vector<std::array<float, 3>>> latestPoses_;
    std::vector<std::vector<float>> latestVisList_;

    // --- Body state ---
    std::vector<TrackedBody> trackedBodies_;
    TrackedBody currentBody_;
    TrackedBody previousBody_;
    float smoothingFactor_ = 0.3f;
    float confidenceThreshold_ = 0.3f;
    float presence_ = 0.0f;
    float depthNearMM_ = 400.0f;
    float depthFarMM_ = 4000.0f;

    ConnectionState connectionState_ = ConnectionState::DISCONNECTED;

    // --- Callbacks ---
    JointCallback   jointCallback_;
    BodyCallback    bodyCallback_;
    HandCallback    handCallback_;
    GestureCallback gestureCallback_;
    ZoneCallback    zoneCallback_;

    mutable std::mutex mutex_;
};

} // namespace modular
