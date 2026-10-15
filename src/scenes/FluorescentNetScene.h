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

namespace modular {

// ============================================================================
// ParticlePhase — dual-phase lifecycle
// ============================================================================

enum class ParticlePhase { FREE, BLENDING, ANCHORED };

// ============================================================================
// NetParticle — single particle in the dual-phase pool
// ============================================================================

struct NetParticle {
    glm::vec2 pos{0.0f};
    glm::vec2 vel{0.0f};
    ofFloatColor color;
    ParticlePhase phase = ParticlePhase::FREE;
    bool active = false;

    // FREE state
    std::string jointId;       // assigned interaction point
    float phaseOffset = 0.0f;  // wave phase offset per particle
    float life = 0.0f;         // seconds alive; used for stale-removal grace

    // ANCHORED state (reserved for PR 2)
    glm::vec2 originalPos{0.0f};
    float noiseOffset = 0.0f;

    // Transition (reserved for PR 2)
    float transitionTimer = 0.0f;
    float blendAlpha = 1.0f;
};

// ============================================================================
// FluorescentEdge — spring connection between two ANCHORED nodes
// ============================================================================

struct FluorescentEdge {
    int a = -1;
    int b = -1;
    float restLength = 0.0f;
    bool active = true;
};

// ============================================================================
// Constants
// ============================================================================

constexpr int FN_DEFAULT_MAX_PARTICLES = 800;
constexpr int FN_MIN_MAX_PARTICLES = 100;
constexpr int FN_MAX_MAX_PARTICLES = 3000;
constexpr int FN_SPAWN_RATE = 2;               // particles per active joint per frame
constexpr float FN_MAX_SPEED = 400.0f;         // px/s velocity clamp for FREE particles
constexpr float FN_TIME_SCALE = 30.0f;          // dt multiplier (matches PinturaFluorescente)
constexpr float FN_STALE_GRACE = 3.0f;          // seconds before stale FREE particles are removed
constexpr float FN_MAX_ANCHOR_SPEED = 200.0f;    // px/s velocity clamp for newly anchored particles
constexpr int FN_MAX_ANCHORED_NODES = 2000;      // hard cap for ANCHORED particles

// ============================================================================
// FluorescentNetScene — dual-phase particle scene
// ============================================================================

class FluorescentNetScene : public IBaseScene {
public:
    FluorescentNetScene();
    ~FluorescentNetScene() override;

    // IBaseScene implementation
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

    // Debug
    void setDebugMarkers(bool visible) { debugMarkers_ = visible; }
    bool isDebugMarkersVisible() const { return debugMarkers_; }

private:
    // Palettes
    void setupPalettes();
    ofFloatColor getColorFromPalette(int paletteIndex, float variation);

    // Parameters
    void initializeParameterDefinitions();
    void registerParameters();
    void unregisterParameters();
    void syncParamsFromMap();

    // FREE physics helpers
    void updateFREEParticles(float dt);
    void spawnParticles();
    void generateInitialNet();
    void deactivateStaleParticles(float dt);
    void applyPoolResize();

    // ANCHORED physics helpers
    void updateANCHOREDParticles(float dt);
    void handleBlendingParticles(float dt);
    void updateTransitions(float dt);
    void relaxSprings();
    void manageEdges();

    // Rendering
    void drawFREEParticles();
    void drawANCHOREDEdges();
    void drawANCHOREDNodes();
    void drawDebugMarkers();

    // Pool
    std::vector<NetParticle> particles_;
    int activeCount_ = 0;

    // Edge mesh (reserved for PR 2 — compiled but unused here)
    std::vector<FluorescentEdge> edges_;

    // Edge management state — indices of particles that just became ANCHORED
    std::vector<int> newlyAnchoredIndices_;

    // Interaction points
    std::map<std::string, InteractionPoint> interactionPoints_;
    std::string seedJointId_;          // primary joint for single-net spawn

    // Palettes (5 fluorescent palettes, matching PinturaFluorescenteScene)
    std::vector<std::vector<ofFloatColor>> palettes_;
    int currentPalette_ = 0;

    // Lifecycle
    bool isActive_ = false;
    bool isPaused_ = false;
    bool isValid_ = true;
    std::string invalidReason_;

    // Parameter storage
    std::map<std::string, std::shared_ptr<Parameter>> parameters_;
    bool parametersRegistered_ = false;

    // Debug
    bool debugMarkers_ = false;

    // ── 28 Runtime Parameters ────────────────────────────────────────────
    // orbit (8)
    float k_       = 0.1f;
    float A_       = 15.0f;
    float w_       = 0.5f;
    float q_       = 0.003f;
    float beta_    = 0.97f;
    float S_       = 8.0f;
    float ws_      = 0.3f;
    float qs_      = 0.002f;

    // net (5) — compiled, used in PR 2
    float gravity_          = 0.05f;
    float damping_          = 0.96f;
    float springStiffness_  = 0.12f;
    int   springIterations_ = 3;
    float returnForce_      = 0.008f;

    // noise/wind (5)
    float noiseStrength_ = 0.8f;
    float noiseScale_    = 0.015f;
    float windStrength_  = 0.4f;
    float windFrequency_ = 0.3f;
    float windBias_      = 0.0f;

    // transition (2)
    float transitionRate_ = 0.3f;
    float blendPeriod_    = 0.5f;

    // topology (2)
    float connectionRadius_     = 120.0f;
    int   maxConnectionsPerNode_ = 6;

    // joint reaction (2)
    float jointLiftRadius_   = 180.0f;
    float jointLiftStrength_ = 0.08f;

    // initial net (2)
    int   initialNodes_ = 80;
    float spreadRadius_ = 250.0f;
    bool  netGenerated_ = false;

    // visual (5)
    float particleSize_ = 3.0f;
    std::vector<float> groupSizes_{3,3,3,3,3,3,3,3};  // per-group for _array MIDI
    float nodeRadius_   = 3.0f;
    float netLineWidth_ = 1.5f;
    float netLineAlpha_ = 120.0f;
    float nodeAlpha_    = 100.0f;

    // pool (1)
    int   maxParticles_ = FN_DEFAULT_MAX_PARTICLES;
};

// ============================================================================
// Factory function
// ============================================================================

ScenePtr CreateFluorescentNetScene();

} // namespace modular
