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
#include <cstddef>

namespace modular {

// ============================================================================
// Constants
// ============================================================================

constexpr size_t SWARM_MAX_PARTICLES = 600;

// ============================================================================
// SwarmParticle — partícula del enjambre
// ============================================================================

struct SwarmParticle {
    glm::vec2 position = glm::vec2(0.0f);
    glm::vec2 velocity = glm::vec2(0.0f);
    float mass = 0.01f;          // 0.003–0.03, afecta aceleración y radio
    float age = 0.0f;            // segundos desde creación
    float maxLife = 8.0f;        // vida total
    float alpha = 1.0f;
    ofFloatColor color;
    bool active = false;
};

// ============================================================================
// Forward
// ============================================================================

struct SwarmParams;

// ============================================================================
// SwarmParams
// ============================================================================

struct SwarmParams {
    // Mutual spring physics
    float restDistance = 320.0f;     // equilibrio atracción/repulsión (px)
    float damping = 0.99f;           // fricción de velocidad
    float gravity = 0.0f;            // gravedad vertical (0 = espacio libre)
    float globalForceScale = 0.8f;   // multiplicador global de fuerza (1=original)

    // Particle spawning
    int maxParticles = 400;
    int spawnRate = 2;               // partículas por frame por joint
    float massMin = 0.003f;
    float massMax = 0.03f;

    // Lifecycle
    float particleLifeMin = 4.0f;
    float particleLifeMax = 10.0f;

    // Joint push (repels particles from joints to prevent crowding)
    float jointPushRadius = 200.0f;
    float jointPushStrength = 0.06f;

    // Color
    int colorCycles = 2;             // Fibonacci palette repeats over particle lifetime

    // Render
    float radiusScale = 800.0f;      // mass * radiusScale = display radius (px)
    float radiusMin = 2.0f;          // minimum display radius
    float radiusMax = 35.0f;         // maximum display radius
};

// ============================================================================
// ParticleSwarmScene
// ============================================================================

class ParticleSwarmScene : public IBaseScene {
public:
    ParticleSwarmScene();
    ~ParticleSwarmScene() override;

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
    ofFloatColor getColorFromPalette(int paletteIndex, float variation);
    void setupPalettes();
    void initializeParameterDefinitions();
    void registerParameters();
    void unregisterParameters();
    void syncParamsFromMap();
    void spawnParticle(const glm::vec2& pos, float depth = 0.5f);
    size_t findFreeSlot();

    std::vector<std::vector<ofFloatColor>> palettes_;
    int currentPalette_;

    std::map<std::string, InteractionPoint> interactionPoints_;

    std::vector<SwarmParticle> particles_;
    size_t activeCount_ = 0;

    SwarmParams swarmParams_;

    bool isActive_;
    bool isPaused_;
    bool isValid_;
    std::string invalidReason_;

    std::map<std::string, std::shared_ptr<Parameter>> parameters_;
    bool parametersRegistered_;

    bool debugMarkers_;
};

ScenePtr CreateParticleSwarmScene();

} // namespace modular
