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
// Constants — scene configuration
// ============================================================================

constexpr size_t RIBBON_MAX_PARTICLES = 800;
constexpr size_t DEFAULT_RIBBON_CAPACITY = 200;
constexpr float VELOCITY_EMA_FACTOR = 0.96f;

// ============================================================================
// TrailPoint — punto de historia en una cinta
// ============================================================================

struct TrailPoint {
    glm::vec2 position = glm::vec2(0.0f);
    glm::vec2 velocity = glm::vec2(0.0f);
    float age = 0.0f;       // seconds since added
    float width = 0.0f;
    float alpha = 1.0f;
    float intensity = 1.0f; // velocity contribution at creation time
};

// ============================================================================
// RibbonParticle — partícula decorativa emitida desde las cintas
// ============================================================================

struct RibbonParticle {
    glm::vec2 position = glm::vec2(0.0f);
    glm::vec2 velocity = glm::vec2(0.0f);
    float life = 0.0f;
    float maxLife = 1.0f;
    float size = 2.0f;
    float alpha = 1.0f;
    ofFloatColor color;
    bool active = false;  // false = slot libre en el pool

    void setup(const glm::vec2& pos, const glm::vec2& vel,
               ofFloatColor col, float sz, float maxL);
    void update(float dt, float gravity, float damping);
    void draw();
    bool isDead() const;
};

// ============================================================================
// Forward for RibbonParams (needed by RibbonTrail)
// ============================================================================

struct RibbonParams;

// ============================================================================
// RibbonTrail — cinta fluorescente con buffer circular + ofMesh
// ============================================================================

struct RibbonTrail {
    std::string pointId;
    std::vector<TrailPoint> points;   // ring buffer
    size_t head = 0;                  // write index
    size_t count = 0;                 // valid entries

    std::vector<ofFloatColor> paletteColors;  // full palette copy for Fibonacci flow
    size_t paletteColorCount = 0;             // cached size of paletteColors
    float smoothedVelocity = 0.0f;    // EMA-smoothed speed magnitude
    glm::vec2 smoothedPosition{0.0f}; // EMA-smoothed position for organic delay
    float lastAddTime = 0.0f;        // last time addPoint() was called (ofGetElapsedTimef)
    std::vector<glm::vec2> secondaryAttractors;  // orbital attractors from non-emitter IPs
    float alpha = 1.0f;              // overall ribbon opacity
    float life = 0.0f;              // time since creation
    bool active = false;             // receiving data from interaction point
    bool fading = false;             // source point deactivated

    ofMesh mesh;                     // rebuilt each frame

    void setup(size_t maxPoints);
    void addPoint(const glm::vec2& pos, const glm::vec2& vel, const RibbonParams& params);
    void update(float dt, const RibbonParams& params);
    void buildMesh(const RibbonParams& params, float widthScale = 1.0f);
    void draw();
    void fadeOut();
    bool isDead() const;
    void emitParticles(std::vector<RibbonParticle>& pool,
                       const InteractionPoint& ip,
                       const RibbonParams& params);
};

// ============================================================================
// RibbonParams — todos los parámetros configurables de la escena
// ============================================================================

struct RibbonParams {
    // Trail geometry
    int maxRibbonPoints = 120;
    float ribbonMaxWidth = 30.0f;
    float ribbonMinWidth = 4.0f;

    // Physics
    float maxJointVelocity = 1500.0f;
    float ribbonGravity = 0.04f;
    float ribbonDamping = 0.98f;
    float ribbonFadeSpeed = 0.004f;
    float ribbonLife = 8.0f;

    // Color cycling
    int ribbonColorCycles = 3;           // palette repeats along ribbon length (Fibonacci cycles)

    // Velocity influence
    float ribbonVelocityInfluence = 1.0f;
    float velocitySmoothing = 0.96f;

    // Wave/undulation
    float ribbonWaveAmount = 10.0f;
    float ribbonWaveSpeed = 1.0f;

    // Jump protection
    float ribbonMaxDistanceJump = 400.0f;  // updated based on screen size

    // Ribbon management
    int maxActiveRibbons = 6;             // max simultaneous ribbons (non-fading)

    // Orbital attraction (secondary interaction points)
    float orbitRadius = 300.0f;           // max distance for orbital influence (px)
    float orbitStrength = 0.03f;          // how strong the orbital pull is

    // Particles
    int particleMaxCount = 800;
    float particleEmissionThreshold = 20.0f;
    int particlesPerBurst = 3;
    float particleVelocityInfluence = 0.5f;
    float particleGravity = 15.0f;
    float particleDamping = 0.97f;
    float particleLifeMin = 0.6f;
    float particleLifeMax = 2.0f;
    float particleSizeMin = 1.5f;
    float particleSizeMax = 5.0f;
};

// ============================================================================
// RibbonDanceScene — escena de cintas fluorescentes
// ============================================================================

/**
 * @brief Escena que genera cintas fluorescentes (ribbon trails) por cada
 *        InteractionPoint activo, con velocidad modulada.
 *
 * Implementa IBaseScene siguiendo el patrón de PinturaFluorescenteScene.
 * PR 1: esqueleto que compila pero no dibuja cintas todavía.
 */
class RibbonDanceScene : public IBaseScene {
public:
    RibbonDanceScene();
    ~RibbonDanceScene() override;

    // ============================================
    // IBaseScene Implementation
    // ============================================

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

    // ============================================
    // Input Handlers — IBaseScene override
    // ============================================

    void keyPressed(int key) override;
    void keyReleased(int key) override;
    void mousePressed(int x, int y, int button) override;
    void mouseReleased(int x, int y, int button) override;
    void mouseDragged(int x, int y, int button) override;
    void mouseMoved(int x, int y) override;
    void windowResized(int w, int h) override;

    // ============================================
    // Debug
    // ============================================

    void setDebugMarkers(bool visible) { debugMarkers_ = visible; }
    bool isDebugMarkersVisible() const { return debugMarkers_; }

private:
    ofFloatColor getColorFromPalette(int paletteIndex, float variation);
    void setupPalettes();
    void initializeParameterDefinitions();
    void registerParameters();
    void unregisterParameters();
    void syncParamsFromMap();

    // Palette system (same 5-palette structure as PinturaFluorescenteScene)
    std::vector<std::vector<ofFloatColor>> palettes_;
    int currentPalette_;

    // Multi-point interaction map (shared InteractionPoint system)
    std::map<std::string, InteractionPoint> interactionPoints_;

    // Ribbon and particle storage
    std::map<std::string, RibbonTrail> ribbons_;
    std::vector<RibbonParticle> particles_;

    // Runtime parameters (synced from ParameterRegistry)
    RibbonParams ribbonParams_;

    // State flags
    bool isActive_;
    bool isPaused_;
    bool isValid_;
    std::string invalidReason_;

    // Parameter storage (registered in ParameterRegistry)
    std::map<std::string, std::shared_ptr<Parameter>> parameters_;
    bool parametersRegistered_;

    // (nextColorIdx_ removed — ribbons now store full palette for Fibonacci flow)

    // Debug visualization
    bool debugMarkers_;
};

// ============================================================================
// Factory function
// ============================================================================

ScenePtr CreateRibbonDanceScene();

} // namespace modular
