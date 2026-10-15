#pragma once

#include <string>
#include <cstdint>

// Include KinectTypes for JointType, Vector3D — needed by inline helpers
// Must be OUTSIDE the namespace block (KinectTypes.h defines its own namespace)
#include "../adapters/kinect/KinectTypes.h"
#include "ofJson.h"

namespace modular {

// ============================================================================
// KinectCondition — how a joint value triggers dispatch
// ============================================================================

enum class KinectCondition : uint8_t {
    ABOVE_THRESHOLD,   // value > threshold → dispatch maxOutput, else minOutput
    BELOW_THRESHOLD,   // value < threshold → dispatch maxOutput, else minOutput
    IN_RANGE,          // minRange <= value <= maxRange → dispatch maxOutput, else minOutput
    GESTURE,           // Match gesture name in gesture callback (not joint data)
    CONTINUOUS         // Always dispatch, ofMap(value, minInput, maxInput, minOutput, maxOutput)
};

// ============================================================================
// KinectTransformType — maps to OscTransformType pattern exactly
// ============================================================================

enum class KinectTransformType : uint8_t {
    LINEAR,
    EXPONENTIAL,
    LOGARITHMIC,
    INVERT,
    VEC2_XY,      // Raw (X,Y) dispatch with pixel conversion — no scaling/smoothing
    VEC3_XYZ      // Raw (X,Y,Z) dispatch — includes real depth (z) for body relation
};

// ============================================================================
// KinectMapping — a single joint/gesture-to-scene-parameter mapping entry
// ============================================================================

struct KinectMapping {
    // Core identity (persisted)
    std::string mappingId;           // Unique ID: "kinect-map-N"
    std::string jointSource;         // e.g. "hand_right.position.y" or "head.position.x"
    std::string targetParameterId;   // Format: "sceneId.localParamId"

    // Condition (persisted)
    KinectCondition condition = KinectCondition::CONTINUOUS;
    float threshold = 0.5f;          // For ABOVE_THRESHOLD / BELOW_THRESHOLD
    float minRange = 0.0f;           // For IN_RANGE
    float maxRange = 1.0f;           // For IN_RANGE
    std::string gestureType;         // For GESTURE: "wave", "swipe_left", etc.

    // Input range (for CONTINUOUS mode) (persisted)
    float minInput = 0.0f;
    float maxInput = 1.0f;

    // Output range (persisted)
    float minOutput = 0.0f;
    float maxOutput = 1.0f;

    // Transform (persisted)
    KinectTransformType transformType = KinectTransformType::LINEAR;

    // State (persisted)
    bool enabled = true;
    float smoothing = 0.0f;

    // Runtime state (NOT persisted)
    float lastValue = 0.0f;

    KinectMapping() = default;
};

// ============================================================================
// Joint source string helpers (inline — need full Vector3D definition above)
// ============================================================================

// Extract axis character from jointSource string
// "hand_right.position.y" → 'y', "head.position.x" → 'x'
inline char parseJointAxis(const std::string& jointSource) {
    auto pos = jointSource.rfind('.');
    if (pos != std::string::npos && pos + 1 < jointSource.size()) {
        return jointSource[pos + 1];
    }
    return 'y';
}

// Extract joint type name from jointSource
// "hand_right.position.y" → "hand_right"
inline std::string parseJointName(const std::string& jointSource) {
    auto pos = jointSource.find('.');
    if (pos != std::string::npos) {
        return jointSource.substr(0, pos);
    }
    return jointSource;
}

// Get the axis value from a joint position Vector3D
inline float getJointAxisValue(const Vector3D& pos, char axis) {
    switch (axis) {
        case 'x': return pos.x;
        case 'y': return pos.y;
        case 'z': return pos.z;
        default:  return pos.y;
    }
}

// Convert joint name string ↔ JointType enum (implemented in .cpp)
JointType jointNameToType(const std::string& name);
std::string jointTypeToName(JointType type);

// ============================================================================
// Serialization free functions — single-mapping JSON round-trip
// ============================================================================
// Declared here so PresetManager (and other consumers) can use them
// without depending on KinectMappingEngine. Implemented in KinectMappingEngine.cpp.

ofJson kinectMappingToJson(const KinectMapping& m);
KinectMapping kinectMappingFromJson(const ofJson& j);

} // namespace modular
