#pragma once

#include <cstdint>

namespace modular {

// 18 human body joints (matching OpenNI skeleton)
// COUNT returns 18 — no joints beyond this set.
enum class JointType : uint8_t {
    HEAD,
    NECK,
    SHOULDER_LEFT,
    SHOULDER_RIGHT,
    ELBOW_LEFT,
    ELBOW_RIGHT,
    WRIST_LEFT,
    WRIST_RIGHT,
    HAND_LEFT,
    HAND_RIGHT,
    HIP_LEFT,
    HIP_RIGHT,
    KNEE_LEFT,
    KNEE_RIGHT,
    ANKLE_LEFT,
    ANKLE_RIGHT,
    SPINE_BASE,
    SPINE_MID,
    COUNT  // = 18
};

// Normalized 0.0–1.0 screen-proportional coordinates.
// x: 0.0 = left, 1.0 = right
// y: 0.0 = top,  1.0 = bottom
// z: 0.0 = near, 1.0 = far
struct Vector3D {
    float x = 0.5f;
    float y = 0.5f;
    float z = 0.5f;
};

struct Joint {
    JointType type;
    Vector3D position;    // Normalized 0.0–1.0 screen space
    Vector3D velocity;    // Normalized delta per frame
    float confidence = 0.0f;  // 0.0–1.0 tracking quality
};

struct TrackedBody {
    int bodyId = 0;
    bool isTracked = false;
    float confidence = 0.0f;                  // Overall body tracking confidence
    Joint joints[static_cast<int>(JointType::COUNT)]; // Fixed-size, indexed by JointType
};

enum class HandState {
    OPEN,
    CLOSED,
    POINTING,
    UNKNOWN
};

enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
};

} // namespace modular
