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
#include <utility>

namespace modular {

// ============================================================================
// Constants
// ============================================================================

constexpr int TRAPPED_MAX_RINGS = 10;
constexpr int TRAPPED_MAX_POINTS_PER_RING = 60;
constexpr int TRAPPED_MAX_NODES = TRAPPED_MAX_RINGS * TRAPPED_MAX_POINTS_PER_RING;

// ============================================================================
// RingNode — nodo individual en un anillo de la red atrapada
// ============================================================================

struct RingNode {
    glm::vec2 position{0.0f};
    glm::vec2 velocity{0.0f};
    float angle = 0.0f;              // ángulo en el anillo (radianes)
    int ringIndex = 0;                // 0 = más interno
    float baseRadius = 40.0f;        // radio base de este nodo
    float targetRadius = 40.0f;      // radio objetivo (dinámico con openness)
    float noiseOffset = 0.0f;        // fase única de noise
};

// ============================================================================
// NetEdge — conexión entre dos nodos
// ============================================================================

struct TrappedEdge {
    int a = -1;
    int b = -1;
    float restLength = 50.0f;
};

// ============================================================================
// TrappedNetParams
// ============================================================================

struct TrappedNetParams {
    // Ring topology
    int rings = 5;
    int pointsPerRing = 22;
    float baseRadius = 35.0f;        // radio del anillo más interno
    float ringSpacing = 38.0f;       // separación base entre anillos

    // Opening mechanics
    float maxExpansion = 160.0f;     // expansión máxima en openness=1.0
    float interactionRadius = 110.0f; // radio de influencia de cada joint
    float openingStrength = 0.8f;     // fuerza radial de apertura por joint

    // Elasticity
    float elasticReturn = 0.010f;     // fuerza de retorno elástico a targetRadius
    float damping = 0.97f;           // fricción general
    float springStiffness = 0.12f;   // rigidez de constraints entre nodos
    int springIterations = 3;

    // Organic noise
    float noiseStrength = 14.0f;     // intensidad del noise orgánico
    float noiseScale = 0.018f;       // escala espacial del noise

    // Openness dynamics
    float openSpeed = 0.022f;        // velocidad de apertura con movimiento
    float closeSpeed = 0.003f;       // velocidad de cierre sin movimiento

    // Render
    int lineSegments = 8;
    float lineNoiseAmount = 6.0f;    // ondulación orgánica de líneas
    float lineAlpha = 130.0f;
    float lineWidth = 0.8f;          // grosor base de las líneas
    float nodeRadius = 1.2f;
    float tensionAlphaBoost = 0.5f;  // extra alpha when line is stretched
};

// ============================================================================
// TrappedBodyNetScene
// ============================================================================

class TrappedBodyNetScene : public IBaseScene {
public:
    TrappedBodyNetScene();
    ~TrappedBodyNetScene() override;

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
    void setupPalettes();
    void initializeParameterDefinitions();
    void registerParameters();
    void unregisterParameters();
    void syncParamsFromMap();

    void buildNet();
    void resetNet();
    void computeBodyCenter();
    void computeBodyMovement(float deltaTime);
    void updateOpenness(float movementAmount, float deltaTime);
    void applyJointOpeningForces(float dt);
    void updateNetPhysics(float dt);
    void solveConnections();
    void drawOrganicLine(const glm::vec2& a, const glm::vec2& b, float noiseBase, float tension);

    std::vector<std::vector<ofFloatColor>> palettes_;
    int currentPalette_;

    std::map<std::string, InteractionPoint> interactionPoints_;

    // Net data
    std::vector<RingNode> nodes_;
    std::vector<TrappedEdge> edges_;

    // Body state
    glm::vec2 bodyCenter_{0.0f};
    float openness_ = 0.25f;          // 0.0 (cerrada) → 1.0 (abierta)
    float prevMovement_ = 0.0f;

    TrappedNetParams netParams_;

    bool isActive_;
    bool isPaused_;
    bool isValid_;
    std::string invalidReason_;

    std::map<std::string, std::shared_ptr<Parameter>> parameters_;
    bool parametersRegistered_;

    bool debugMarkers_;
};

ScenePtr CreateTrappedBodyNetScene();

} // namespace modular
