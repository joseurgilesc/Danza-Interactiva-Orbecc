#pragma once

#include "ofMain.h"
#include "adapters/kinect/KinectTypes.h"
#include <vector>
#include <utility>

namespace modular {

class SkeletonView {
public:
    SkeletonView();
    
    // Main draw method — call from ofApp::draw()
    // Draws all joints and bones for the given body
    void draw(const TrackedBody& body, float screenWidth, float screenHeight);
    
    // Draws just the tracking status overlay text
    void drawStatus(bool isTracking, float confidence);
    
    // Visibility toggle
    void setVisible(bool visible);
    bool isVisible() const;

    // Alpha for smooth fade (driven by tracking presence)
    void setAlpha(float alpha);
    float getAlpha() const;
    
    // Color configuration
    void setJointRadius(float radius);
    void setBoneThickness(float thickness);
    
    // Color for confidence levels (0.0-1.0)
    static ofColor getConfidenceColor(float confidence);

    // Scale factor (read from ParameterRegistry each frame)
    void setScale(float scale) { skeletonScale_ = scale; }
    float getScale() const { return skeletonScale_; }

private:
    void drawJoint(const Joint& joint, float screenW, float screenH);
    void drawBone(const Joint& from, const Joint& to, float screenW, float screenH, float avgConfidence);
    void drawTrackingIndicator(float x, float y, bool tracking, float confidence);
    
    // Bone connection map: pairs of JointType that form skeleton bones
    static const std::vector<std::pair<JointType, JointType>>& getBoneConnections();
    
    bool visible_;
    float skeletonAlpha_;   // 0.0 = fully transparent, 1.0 = fully opaque
    float jointRadius_;
    float boneThickness_;
    float skeletonScale_ = 1.0f;  // multiplier from ParameterRegistry "skeleton.scale"
    float centerX_ = 0.5f;       // skeleton center for zoom anchor (norm coords)
    float centerY_ = 0.5f;       // skeleton center for zoom anchor (norm coords)
    
    static constexpr float kMinConfidenceForBone = 0.1f;
};

} // namespace modular
