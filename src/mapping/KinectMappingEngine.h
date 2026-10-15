#pragma once

#include "KinectMappingTypes.h"
#include "../adapters/kinect/KinectTypes.h"
#include <string>
#include <vector>
#include <cstdint>

namespace modular {

// Forward declarations
class SceneEngine;

// ============================================================================
// KinectMappingEngine — manages Kinect-joint-to-scene-parameter mappings
// ============================================================================
// Structural mirror of OscMappingEngine. Key difference: processes joint data
// (bodyId, JointType, Joint) with condition-based dispatch instead of
// unconditional OSC forwarding.

class KinectMappingEngine {
public:
    explicit KinectMappingEngine(SceneEngine* sceneEngine);

    // ---- CRUD ----
    std::string addMapping(const KinectMapping& mapping);
    bool removeMapping(const std::string& mappingId);
    std::vector<KinectMapping>& getMappings();
    const std::vector<KinectMapping>& getMappings() const;
    KinectMapping* findMapping(const std::string& mappingId);

    // ---- Batch operations ----
    void removeMappingsByScene(const std::string& sceneId);
    void addMappings(const std::vector<KinectMapping>& mappings);

    // ---- Dispatch ----
    // Called by KinectAdapter joint callback each frame for every tracked joint
    void processJointData(int bodyId, JointType jointType, const Joint& joint);

    // ---- Persistence ----
    bool saveToJson(const std::string& filepath) const;
    bool loadFromJson(const std::string& filepath);

    // ---- Per-frame update ----
    void update(float deltaTime);

private:
    KinectMapping* findDuplicate(const std::string& jointSource, const std::string& targetParam) const;
    bool evaluateCondition(const KinectMapping& mapping, float value) const;
    float scaleValue(float input, const KinectMapping& mapping) const;
    std::string generateId();

    SceneEngine* sceneEngine_;
    std::vector<KinectMapping> mappings_;
    int idCounter_ = 0;
    
    // Cached body center for zoom scaling (NECK + SPINE_MID average)
    float neckX_ = 0.5f, neckY_ = 0.22f;
    float spineMidX_ = 0.5f, spineMidY_ = 0.35f;
    bool hasCenter_ = false;  // true after first NECK+SPINE_MID received

    static constexpr float kOnChangeEpsilon = 0.001f;
};

} // namespace modular
