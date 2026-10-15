#include "input/sensors/SkeletonView.h"
#include "registry/ParameterRegistry.h"

#include <cmath>
#include <string>

namespace modular {

// ============================================
// Bone Connection Map (17 connections)
// Follows human anatomy: spine chain, left/right arms, left/right legs
// ============================================
const std::vector<std::pair<JointType, JointType>>& SkeletonView::getBoneConnections() {
    static const std::vector<std::pair<JointType, JointType>> connections = {
        // Head and spine
        {JointType::HEAD, JointType::NECK},
        {JointType::NECK, JointType::SPINE_MID},
        {JointType::SPINE_MID, JointType::SPINE_BASE},
        // Left arm
        {JointType::NECK, JointType::SHOULDER_LEFT},
        {JointType::SHOULDER_LEFT, JointType::ELBOW_LEFT},
        {JointType::ELBOW_LEFT, JointType::WRIST_LEFT},
        {JointType::WRIST_LEFT, JointType::HAND_LEFT},
        // Right arm
        {JointType::NECK, JointType::SHOULDER_RIGHT},
        {JointType::SHOULDER_RIGHT, JointType::ELBOW_RIGHT},
        {JointType::ELBOW_RIGHT, JointType::WRIST_RIGHT},
        {JointType::WRIST_RIGHT, JointType::HAND_RIGHT},
        // Left leg
        {JointType::SPINE_BASE, JointType::HIP_LEFT},
        {JointType::HIP_LEFT, JointType::KNEE_LEFT},
        {JointType::KNEE_LEFT, JointType::ANKLE_LEFT},
        // Right leg
        {JointType::SPINE_BASE, JointType::HIP_RIGHT},
        {JointType::HIP_RIGHT, JointType::KNEE_RIGHT},
        {JointType::KNEE_RIGHT, JointType::ANKLE_RIGHT}
    };
    return connections;
}

// ============================================
// Constructor
// ============================================
SkeletonView::SkeletonView()
    : visible_(true)
    , skeletonAlpha_(1.0f)
    , jointRadius_(12.0f)
    , boneThickness_(2.0f) {
}

// ============================================
// Visibility Toggle
// ============================================
void SkeletonView::setVisible(bool visible) {
    visible_ = visible;
}

bool SkeletonView::isVisible() const {
    return visible_;
}

void SkeletonView::setAlpha(float alpha) {
    skeletonAlpha_ = ofClamp(alpha, 0.0f, 1.0f);
}

float SkeletonView::getAlpha() const {
    return skeletonAlpha_;
}

// ============================================
// Color Configuration
// ============================================
void SkeletonView::setJointRadius(float radius) {
    jointRadius_ = radius;
}

void SkeletonView::setBoneThickness(float thickness) {
    boneThickness_ = thickness;
}

// ============================================
// Confidence Color Coding
//
// Green  (0, 255, 0)   — confidence >= 0.7
// Yellow (255, 255, 0) — confidence >= 0.3
// Red    (255, 0, 0)   — confidence < 0.3
//
// Alpha fades from 80 (0.0) to 255 (0.7+), clamping at 0.7
// ============================================
ofColor SkeletonView::getConfidenceColor(float confidence) {
    int r, g, b;
    
    if (confidence >= 0.7f) {
        r = 0;   g = 255; b = 0;    // Green
    } else if (confidence >= 0.3f) {
        r = 255; g = 255; b = 0;    // Yellow
    } else {
        r = 255; g = 0;   b = 0;    // Red
    }
    
    int alpha = static_cast<int>(ofMap(confidence, 0.0f, 0.7f, 80.0f, 255.0f, true));
    return ofColor(r, g, b, alpha);
}

// ============================================
// drawJoint — renders a single joint as a circle
//
// Coordinate system: normalized (0.0-1.0) mapped to screen pixels.
// Mirroring is handled upstream by the adapter — coordinates already in person-space.
// Radius scales with confidence: 3px at 0.0 → jointRadius_ at 1.0
// ============================================
void SkeletonView::drawJoint(const Joint& joint, float screenW, float screenH) {
    // Positional zoom: scale distance from skeleton center
    float zoomX = centerX_ + (joint.position.x - centerX_) * skeletonScale_;
    float zoomY = centerY_ + (joint.position.y - centerY_) * skeletonScale_;
    float screenX = zoomX * screenW;
    float screenY = zoomY * screenH;
    
    // Damped radius: sqrt keeps points reasonable at high zoom
    float radiusScale = std::sqrt(skeletonScale_);
    float radius = ofMap(joint.confidence, 0.0f, 1.0f, 3.0f, jointRadius_, true) * radiusScale;
    
    ofColor color = getConfidenceColor(joint.confidence);
    color.a = static_cast<int>(color.a * skeletonAlpha_);
    ofSetColor(color);
    ofDrawCircle(screenX, screenY, radius);
}

// ============================================
// drawBone — renders a bone connection line between two joints
//
// Both joints must have confidence above kMinConfidenceForBone.
// Line color is based on average confidence of the two joints.
// ============================================
void SkeletonView::drawBone(const Joint& from, const Joint& to,
                            float screenW, float screenH, float avgConfidence) {
    if (from.confidence < kMinConfidenceForBone || to.confidence < kMinConfidenceForBone) {
        return;
    }
    
    // Positional zoom from skeleton center
    float fromX = (centerX_ + (from.position.x - centerX_) * skeletonScale_) * screenW;
    float fromY = (centerY_ + (from.position.y - centerY_) * skeletonScale_) * screenH;
    float toX   = (centerX_ + (to.position.x   - centerX_) * skeletonScale_) * screenW;
    float toY   = (centerY_ + (to.position.y   - centerY_) * skeletonScale_) * screenH;
    
    ofColor color = getConfidenceColor(avgConfidence);
    color.a = static_cast<int>(color.a * skeletonAlpha_);
    ofSetColor(color);
    ofSetLineWidth(boneThickness_ * skeletonScale_);
    ofDrawLine(fromX, fromY, toX, toY);
}

// ============================================
// drawTrackingIndicator — status overlay at given position
// ============================================
void SkeletonView::drawTrackingIndicator(float x, float y, bool tracking, float confidence) {
    std::string statusText;
    ofColor textColor;
    
    if (tracking) {
        statusText = "Tracking: ON";
        textColor = ofColor(0, 255, 0, 230);
    } else {
        statusText = "Tracking: LOST";
        textColor = ofColor(255, 0, 0, 230);
    }
    
    // Background highlight for readability
    ofSetColor(0, 0, 0, static_cast<int>(180 * skeletonAlpha_));
    ofDrawRectangle(x, y, 180, 20);
    
    textColor.a = static_cast<int>(textColor.a * skeletonAlpha_);
    ofSetColor(textColor);
    ofDrawBitmapString(statusText, x + 4, y + 14);
    
    // Show body confidence as percentage
    if (tracking) {
        std::string confText = ofToString(static_cast<int>(confidence * 100.0f)) + "%";
        ofSetColor(255, 255, 255, static_cast<int>(200 * skeletonAlpha_));
        ofDrawBitmapString(confText, x + 130, y + 14);
    }
}

// ============================================
// drawStatus — public method for standalone status overlay
// ============================================
void SkeletonView::drawStatus(bool isTracking, float confidence) {
    drawTrackingIndicator(10.0f, 20.0f, isTracking, confidence);
}

// ============================================
// draw — main render method for a single tracked body
//
// Flow:
//   1. If not visible, return immediately
//   2. If body is not tracked, draw "No body tracked" overlay and return
//   3. Draw tracking status indicator at top-left
//   4. Draw all 17 bone connections (lines behind joints)
//   5. Draw all 18 joints on top (circles on top of lines)
//
// Mirroring is done upstream by the adapter (MediaPipePoseAdapter flips X).
// Coordinates arrive in person-space (0=left, 1=right) — no second flip needed.
// ============================================
void SkeletonView::draw(const TrackedBody& body, float screenWidth, float screenHeight) {
    if (!visible_) {
        return;
    }
    
    // Untracked body — show status only
    if (!body.isTracked) {
        drawTrackingIndicator(10.0f, 20.0f, false, body.confidence);
        return;
    }
    
    // 1. Draw tracking status at top-left
    drawTrackingIndicator(10.0f, 20.0f, true, body.confidence);
    
    // Read skeleton scale from global registry each frame
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("skeleton.scale")) {
        skeletonScale_ = param->currentValue;
    }
    
    // Compute skeleton center for zoom anchor (midpoint of NECK and SPINE_MID)
    int neckIdx     = static_cast<int>(JointType::NECK);
    int spineMidIdx = static_cast<int>(JointType::SPINE_MID);
    centerX_ = (body.joints[neckIdx].position.x + body.joints[spineMidIdx].position.x) * 0.5f;
    centerY_ = (body.joints[neckIdx].position.y + body.joints[spineMidIdx].position.y) * 0.5f;
    
    // 2. Draw bones (lines, rendered behind joints)
    const auto& connections = getBoneConnections();
    int jointCount = static_cast<int>(JointType::COUNT);
    
    for (const auto& [fromType, toType] : connections) {
        int fromIdx = static_cast<int>(fromType);
        int toIdx   = static_cast<int>(toType);
        
        if (fromIdx < jointCount && toIdx < jointCount) {
            const Joint& from = body.joints[fromIdx];
            const Joint& to   = body.joints[toIdx];
            float avgConf = (from.confidence + to.confidence) * 0.5f;
            drawBone(from, to, screenWidth, screenHeight, avgConf);
        }
    }
    
    // 3. Draw joints (circles, rendered on top of bones)
    for (int i = 0; i < jointCount; ++i) {
        drawJoint(body.joints[i], screenWidth, screenHeight);
    }
}

} // namespace modular
