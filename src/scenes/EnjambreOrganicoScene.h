#pragma once

#include "scenes/BaseScene.h"
#include "registry/ParameterRegistry.h"
#include "ofMain.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <deque>

namespace modular {

// ============================================================================
// Constants
// ============================================================================

constexpr int ENJAMBRE_DEFAULT_PARTICLE_COUNT = 1500;
constexpr int ENJAMBRE_MIN_PARTICLE_COUNT = 100;
constexpr int ENJAMBRE_MAX_PARTICLE_COUNT = 3000;

constexpr float ENJAMBRE_DEFAULT_TRAVEL_DURATION = 3.0f;
constexpr float ENJAMBRE_DEFAULT_DWELL_DURATION = 1.5f;
constexpr float ENJAMBRE_DEFAULT_SWARM_RADIUS = 80.0f;
constexpr float ENJAMBRE_DEFAULT_NOISE_AMOUNT = 50.0f;
constexpr float ENJAMBRE_DEFAULT_NOISE_SPEED = 0.3f;
constexpr float ENJAMBRE_DEFAULT_SPRING_STIFFNESS = 4.0f;
constexpr float ENJAMBRE_DEFAULT_MAX_ANCHOR_SPEED = 600.0f;
constexpr float ENJAMBRE_DEFAULT_PARTICLE_SIZE = 4.0f;
constexpr float ENJAMBRE_DEFAULT_BONE_TRAIL_RATIO = 0.3f;
constexpr float ENJAMBRE_DEFAULT_LAG_RATIO = 0.15f;

// ============================================================================
// AnchorState / SwarmAnchor
// ============================================================================

enum class AnchorState { Dwelling, Traveling };

struct SwarmAnchor {
    AnchorState state = AnchorState::Dwelling;
    glm::vec2 pos = glm::vec2(0.0f);
    glm::vec2 travelStart = glm::vec2(0.0f);
    glm::vec2 travelTarget = glm::vec2(0.0f);  // snapped at travel start, no mid-travel drift
    int currentJointIdx = 0;
    int targetJointIdx = 0;
    std::string currentJointId;
    std::string targetJointId;
    float travelT = 0.0f;
    float travelSpeed = 1.0f;                // 1.0=normal, 5.0=fast through inactive joints
    float dwellTimer = 0.0f;
    std::deque<std::string> pathQueue;       // skeleton-path waypoints to visit
};

// ============================================================================
// EnjambreParticle — plain struct, pre-sized pool
// ============================================================================

struct EnjambreParticle {
    glm::vec2 pos = glm::vec2(0.0f);
    glm::vec2 vel = glm::vec2(0.0f);
    float noiseSeed = 0.0f;
    ofFloatColor color;
    float sizeFactor = 1.0f;
    float baseSize = 1.0f;
    float lagFactor = 0.0f;
    float boneTrailT = 0.0f;
};

struct ClickPing {
    glm::vec2 pos{0.0f};
    float life = 0.0f;
    float maxLife = 0.6f;
};

// ============================================================================
// EnjambreOrganicoScene
// ============================================================================

class EnjambreOrganicoScene : public IBaseScene {
public:
    EnjambreOrganicoScene();
    ~EnjambreOrganicoScene() override;

    void setup() override;
    void update(float deltaTime) override;
    void draw() override;

    void activate() override;
    void deactivate() override;
    void pause() override;
    void resume() override;

    std::string getId() const override;
    std::string getName() const override;

    std::vector<std::string> getParameterIds() const override;
    std::vector<std::string> getInteractionPointIds() const override;
    void clearInteractionPoints() override;
    std::shared_ptr<Parameter> getParameter(const std::string& paramId) override;
    bool setParameter(const std::string& paramId, float value) override;
    bool setParameter(const std::string& paramId, const glm::vec2& value) override;
    bool setParameter(const std::string& paramId, const glm::vec3& value) override;

    std::vector<SuggestedMapping> getSuggestedMappings() const override;
    Preset getDefaultPreset() const override;
    SceneMetadata getSceneMetadata() const override;

    void onBeat(int beatNumber) override;
    void onBar(int barNumber) override;

    bool isValid() const override;
    std::string getInvalidReason() const override;

    void unloadResources() override;
    void reloadResources() override;

    void keyPressed(int key) override;
    void keyReleased(int key) override;
    void mousePressed(int x, int y, int button) override;
    void mouseReleased(int x, int y, int button) override;
    void mouseDragged(int x, int y, int button) override;
    void mouseMoved(int x, int y) override;
    void windowResized(int w, int h) override;

    void setDebugMarkers(bool visible) { debugMarkers_ = visible; }
    bool isDebugMarkersVisible() const { return debugMarkers_; }

private:
    void initializeParameterDefinitions();
    void registerParameters();
    void unregisterParameters();
    void syncParamsFromMap();
    void applyParticleResize();
    void drawDebugMarkers();

    void setupPalettes();
    ofFloatColor getColorFromPalette(int paletteIndex, float variation);

    struct JointEntry {
        std::string id;
        glm::vec2 position;
        bool active = false;
    };
    std::vector<JointEntry> buildJointSequence();
    std::vector<std::string> findSkeletonPath(const std::string& fromId, const std::string& toId);

    // FSM
    SwarmAnchor anchor_;
    glm::vec2 prevAnchorPos_ = glm::vec2(0.0f);

    // Particles
    std::vector<EnjambreParticle> particles_;

    // State
    std::map<std::string, InteractionPoint> interactionPoints_;

    // Runtime parameters
    float travelDuration_    = ENJAMBRE_DEFAULT_TRAVEL_DURATION;
    float dwellDuration_     = ENJAMBRE_DEFAULT_DWELL_DURATION;
    float swarmRadius_       = ENJAMBRE_DEFAULT_SWARM_RADIUS;
    float noiseAmount_       = ENJAMBRE_DEFAULT_NOISE_AMOUNT;
    float noiseSpeed_        = ENJAMBRE_DEFAULT_NOISE_SPEED;
    float springStiffness_   = ENJAMBRE_DEFAULT_SPRING_STIFFNESS;
    int   particleCount_     = ENJAMBRE_DEFAULT_PARTICLE_COUNT;
    float maxAnchorSpeed_    = ENJAMBRE_DEFAULT_MAX_ANCHOR_SPEED;
    float particleSize_      = ENJAMBRE_DEFAULT_PARTICLE_SIZE;
    float boneTrailRatio_    = ENJAMBRE_DEFAULT_BONE_TRAIL_RATIO;
    float lagRatio_          = ENJAMBRE_DEFAULT_LAG_RATIO;
    std::vector<float> groupSizes_{3,3,3,3,3,3,3,3};  // per-group for _array MIDI
    float mouseStrength_     = 0.5f;

    // Mouse interaction
    glm::vec2 mousePos_{0.0f};
    bool mouseActive_ = false;
    bool trackMouse_ = false;
    std::vector<ClickPing> clickPings_;   // M toggles: mouse mode vs skeleton mode

    // Lifecycle
    bool isActive_ = false;
    bool isPaused_ = false;
    bool isValid_  = true;
    std::string invalidReason_;

    // Parameter storage
    std::map<std::string, std::shared_ptr<Parameter>> parameters_;
    bool parametersRegistered_ = false;

    // Debug
    bool debugMarkers_ = false;

    // Visual
    std::vector<std::vector<ofFloatColor>> palettes_;
    int currentPalette_ = 0;
    float sceneAlpha_ = 1.0f;
    float targetSceneAlpha_ = 1.0f;
};

ScenePtr CreateEnjambreOrganicoScene();

} // namespace modular
