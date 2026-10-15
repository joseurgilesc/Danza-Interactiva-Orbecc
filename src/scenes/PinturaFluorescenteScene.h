#pragma once

#include "scenes/BaseScene.h"
#include "registry/ParameterRegistry.h"
#include "engine/Transport.h"
#include "ofMain.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vector>
#include <string>
#include <memory>
#include <map>

namespace modular {

// ============================================================================
// Constants - configuración de partículas
// ============================================================================

constexpr size_t MAX_PARTICLES = 5000;        // default pool capacity
constexpr int DEFAULT_SPAWN_RATE = 6;          // default max particles per frame per point
constexpr float PARTICLE_MAX_LIFE = 400.0f;
constexpr float SPAWN_OFFSET_RANGE = 15.0f;
constexpr float SPAWN_SIZE_MIN = 2.0f;
constexpr float SPAWN_SIZE_MAX = 8.0f;
constexpr float TIME_SCALE = 30.0f;
constexpr float MOUSE_SPEED_THRESHOLD = 50.0f;
// MIN_PARTICLES_PER_FRAME / MAX_PARTICLES_PER_FRAME removed — use spawnRate_ param
constexpr float TRAIL_SIZE_FACTOR = 0.5f;
constexpr float TRAIL_ALPHA_FACTOR = 0.7f;
constexpr float TRAIL_OFFSET_RANGE = 8.0f;
constexpr float ATTRACTOR_STRENGTH = 0.15f;

// Boid / Flock constants
constexpr float FLOCK_SEPARATION = 0.5f;
constexpr float FLOCK_ALIGNMENT = 0.8f;
constexpr float FLOCK_COHESION = 1.0f;
constexpr float FLOCK_RADIUS = 40.0f;

// ============================================================================
// Particle - partícula individual
// ============================================================================

class Particle {
public:
    glm::vec2 pos;
    glm::vec2 vel;
    ofFloatColor color;
    float alpha;
    float size;
    float life;
    float maxLife;
    float phase;
    bool active;
    glm::vec2 originalVel;  // Original velocity for smooth transition
    std::string pointId;     // Interaction point this particle orbits (empty = global center)

    Particle() : active(false) {}

    void init(glm::vec2 _pos, glm::vec2 _vel, ofFloatColor _color, float _size,
              const std::string& _pointId = "") {
        pos = _pos;
        vel = _vel;
        originalVel = _vel;
        color = _color;
        alpha = 255;
        size = _size;
        maxLife = PARTICLE_MAX_LIFE;
        life = maxLife;
        phase = ofRandom(0, glm::two_pi<float>());
        active = true;
        pointId = _pointId;
    }

    void update(float k, float A, float w, float q, float beta, float S, 
                float ws, float qs, glm::vec2 center, float dt, 
                bool attractorActive, glm::vec2 attractorPoint,
                bool flockMode, float flockTransition, float flockTransitionMax,
                float flockSpeed, float flockWaveX, float flockWaveY,
                float flockCohesion, float flockLife,
                Particle* allParticles, size_t particleCount);

    void draw(float scale = 1.0f);

    bool isDead() {
        return life <= 0;
    }
};

// ============================================================================
// ParticlePool - pool pre-allocado de partículas
// ============================================================================

class ParticlePool {
public:
    ParticlePool(size_t cap = MAX_PARTICLES);
    ~ParticlePool();

    Particle* acquire(glm::vec2 pos, glm::vec2 vel, ofFloatColor col, float size);
    void release(Particle* p);
    size_t getActiveCount() const;
    void clear();
    void resize(size_t newCapacity);

    Particle& getParticleRef(size_t index);
    Particle* getParticleAt(size_t index);
    Particle* getActiveParticle(size_t idx) const;
    Particle* getParticlesArray();
    size_t getCapacity() const;

private:
    Particle* particles;
    size_t capacity_;
    size_t activeCount;
    bool saturationWarningIssued_;
};

struct SceneDebugStats {
    size_t activeParticles = 0;
    size_t trailParticles = 0;
    int spawnedMainLastFrame = 0;
    int spawnedTrailLastFrame = 0;
    bool attractorActive = false;
    glm::vec2 attractorPoint = glm::vec2(0, 0);
    bool flockMode = false;
    bool clearEachFrame = true;
};

// ============================================================================
// Scene Parameters - parámetros de la escena
// ============================================================================

struct SceneParams {
    // Physics params (organic defaults — gentle, fluid motion)
    float k = 0.12f;
    float A = 15.0f;
    float w = 0.15f;
    float q = 0.08f;
    float beta = 0.97f;
    float S = 6.0f;
    float ws = 0.5f;
    float qs = 0.02f;
    
    // Flock params
    float flockSpeed = 0.2f;
    float flockWaveX = 3.0f;
    float flockWaveY = 2.0f;
    float flockCohesion = 0.0005f;
    float flockLife = 0.2f;
    float flockTransition = 1.5f;
};

// ============================================================================
// PinturaFluorescenteScene - implementación de IBaseScene
// ============================================================================

/**
 * @brief Primera escena concreta - Pintura con Partículas Fluorescentes
 * 
 * Implementa IBaseScene y encapsula toda la funcionalidad de partículas
 *迁移自 ofApp.cpp original.
 */
class PinturaFluorescenteScene : public IBaseScene {
public:
    PinturaFluorescenteScene();
    ~PinturaFluorescenteScene() override;

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

    // ============================================
    // Input Handlers - IBaseScene override
    // ============================================
    
    void keyPressed(int key) override;
    void keyReleased(int key) override;
    void mousePressed(int x, int y, int button) override;
    void mouseReleased(int x, int y, int button) override;
    void mouseDragged(int x, int y, int button) override;
    void mouseMoved(int x, int y) override;
    void windowResized(int w, int h) override;

    SceneDebugStats getDebugStats() const;

    void setDebugMarkers(bool visible) { debugMarkers_ = visible; }
    bool isDebugMarkersVisible() const { return debugMarkers_; }

    void setMouseAttractorEnabled(bool enabled) { mouseAttractorEnabled_ = enabled; }
    bool isMouseAttractorEnabled() const { return mouseAttractorEnabled_; }

    // ============================================
    // Input Handlers - métodos internos
    // ============================================
    
    void handleKeyPressed(int key);
    void handleMousePressed(int x, int y, int button);
    void handleMouseReleased(int x, int y, int button);
    void handleWindowResized(int w, int h);

private:
    ofFloatColor getColorFromPalette(int paletteIndex, float variation);
    void setupPalettes();
        void initializeParameterDefinitions();
        void registerParameters();
        void unregisterParameters();

    // Particle pools
        std::unique_ptr<ParticlePool> particlePool_;
        std::unique_ptr<ParticlePool> trailPool_;

    // State
    std::vector<std::vector<ofFloatColor>> palettes_;
    int currentPalette_;
    bool clearEachFrame_;

    // Mouse tracking
    glm::vec2 prevMouse_;
    bool mousePressedFlag_;

    // Parameters
    SceneParams params_;

    // UI state
    bool showParams_;
    int selectedParam_;

    // Click attractor
    glm::vec2 attractorPoint_;
    bool attractorActive_;

    // Multi-point interaction map (replaces single mouse-based center)
    std::map<std::string, InteractionPoint> interactionPoints_;

    // Flock mode
    bool flockMode_;
    float flockTransitionTime_;

    // Instrumentation helpers
    int lastSpawnedMain_;
    int lastSpawnedTrail_;

    // State flags
    bool isActive_;
    bool isPaused_;
    bool isValid_;
    std::string invalidReason_;

    // Registered particle params (replacing old constexpr limits)
    int maxParticles_ = 1500;
    int spawnRate_ = 3;
    bool trailEnabled_ = true;

    void applyPoolResize();

    // Parameter storage (registered in registry)
        std::map<std::string, std::shared_ptr<Parameter>> parameters_;
        bool parametersRegistered_;

    // Debug
    bool debugMarkers_ = false;
    bool mouseAttractorEnabled_ = false;  // disabled by default — interferes with joint tracking
};

// ============================================================================
// Factory function
// ============================================================================

ScenePtr CreatePinturaFluorescenteScene();

} // namespace modular
