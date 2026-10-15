#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace modular {

// Forward declarations
class PresetManager;
class SceneEngine;
class SceneSequencer;

// ============================================================================
// OscFrequencyMode — how often a mapping dispatches values
// ============================================================================

enum class OscFrequencyMode : uint8_t {
    EVERY_FRAME,  // Dispatch on every received OSC message
    ON_CHANGE,    // Dispatch only when value changes beyond epsilon
    THROTTLED     // Dispatch every N frames
};

// ============================================================================
// OscTransformType — how input maps to output
// ============================================================================

enum class OscTransformType : uint8_t {
    LINEAR,       // Default: ofMap(input, minIn, maxIn, minOut, maxOut)
    EXPONENTIAL,  // y = minOut + (maxOut-minOut) * (input-minIn)/(maxIn-minIn) ^ 2
    LOGARITHMIC,  // y = minOut + (maxOut-minOut) * log2(1 + 9*(input-minIn)/(maxIn-minIn))
    INVERT        // y = maxOut - (input-minIn)/(maxIn-minIn) * (maxOut-minOut)
};

// ============================================================================
// LearnState — single-slot learning FSM states
// ============================================================================

enum class LearnState : uint8_t {
    IDLE,      // Not learning, ready to start
    LEARNING,  // Waiting for next OSC message to auto-bind
    TIMEOUT    // Learning window expired, will reset to IDLE next frame
};

// ============================================================================
// OscMapping — a single OSC-to-scene-parameter mapping entry
// ============================================================================

struct OscMapping {
    // Core identity (persisted)
    std::string mappingId;
    std::string oscAddress;
    std::string targetParameterId;  // Format: "sceneId.localParamId"

    // Input range (persisted)
    float minInput = 0.0f;
    float maxInput = 1.0f;

    // Output range (persisted) — overrides target param's range
    float minOutput = 0.0f;
    float maxOutput = 1.0f;

    // Transform type (persisted)
    OscTransformType transformType = OscTransformType::LINEAR;

    // Frequency control (persisted)
    OscFrequencyMode frequencyMode = OscFrequencyMode::EVERY_FRAME;
    int throttleFrames = 5;
    bool enabled = true;

    // Runtime state (NOT persisted)
    float lastValue = 0.0f;
    int frameCounter = 0;
};

// ============================================================================
// OscMappingEngine — manages OSC-to-scene-parameter mappings
// ============================================================================

class OscMappingEngine {
public:
    explicit OscMappingEngine(SceneEngine* sceneEngine);

    // ---- CRUD ----
    std::string addMapping(const OscMapping& mapping);
    bool removeMapping(const std::string& mappingId);
    std::vector<OscMapping>& getMappings();
    const std::vector<OscMapping>& getMappings() const;
    OscMapping* findMapping(const std::string& mappingId);

    // ---- Dispatch ---
    void processOscMessage(const std::string& oscAddress, float value);

    // ---- Learning Mode ----
    bool startLearning(const std::string& targetParameterId);
    void cancelLearning();
    LearnState getLearnState() const;
    std::string getLearningTarget() const;
    float getLearnTimeRemaining() const;

    // ---- Persistence ----
    bool saveToJson(const std::string& filepath) const;
    bool loadFromJson(const std::string& filepath);

    // ---- Per-frame update ----
    void update(float deltaTime);

    // ---- Preset / Sequencer Routing ----
    void setPresetManager(PresetManager* pm);
    void setSceneSequencer(SceneSequencer* seq);

private:
    OscMapping* findDuplicate(const std::string& oscAddr, const std::string& targetParam) const;
    float scaleValue(float input, const OscMapping& mapping) const;
    bool shouldDispatch(const OscMapping& mapping, float value);
    std::string generateId();

    SceneEngine* sceneEngine_;
    PresetManager* presetManager_ = nullptr;
    SceneSequencer* sceneSequencer_ = nullptr;
    std::vector<OscMapping> mappings_;
    int idCounter_;

    // Learning state
    LearnState learnState_ = LearnState::IDLE;
    std::string learnTargetParamId_;
    float learnTimer_ = 0.0f;

    static constexpr float kLearnTimeout = 10.0f;
    static constexpr float kOnChangeEpsilon = 0.001f;
};

} // namespace modular
