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

constexpr int NET_MAX_ROWS = 24;
constexpr int NET_MAX_COLS = 36;
constexpr int NET_MAX_NODES = NET_MAX_ROWS * NET_MAX_COLS;

// ============================================================================
// NetNode — nodo individual de la red
// ============================================================================

struct NetNode {
    glm::vec2 position{0.0f};
    glm::vec2 originalPos{0.0f};     // posición de reposo (retícula)
    glm::vec2 velocity{0.0f};
    float noiseOffset = 0.0f;         // fase única de noise por nodo
};

// ============================================================================
// NetEdge — conexión entre dos nodos
// ============================================================================

struct NetEdge {
    int a = -1;                       // índice del primer nodo
    int b = -1;                       // índice del segundo nodo
    float restLength = 50.0f;         // distancia de reposo de esta conexión
};

// ============================================================================
// NetParams
// ============================================================================

struct NetParams {
    // Grid dimensions
    int rows = 12;
    int cols = 18;
    float spacing = 45.0f;           // px entre nodos

    // Grid offset
    float randomOffsetX = 15.0f;     // desplazamiento aleatorio max en X
    float randomOffsetY = 15.0f;     // desplazamiento aleatorio max en Y
    float omitChance = 0.12f;        // probabilidad de omitir una conexión

    // Physics
    float gravity = 0.025f;          // gravedad vertical suave
    float damping = 0.96f;           // fricción general
    float springStiffness = 0.15f;   // rigidez de constraints
    int springIterations = 3;        // iteraciones de relajación por frame

    // Organic drift
    float noiseStrength = 0.8f;      // intensidad del movimiento orgánico
    float noiseScale = 0.015f;       // escala espacial del noise

    // Wind
    float windStrength = 0.4f;       // intensidad del viento
    float windFrequency = 0.3f;      // frecuencia de oscilación
    float windBias = 0.0f;           // dirección base del viento

    // Joint lift
    float liftRadius = 120.0f;       // radio de influencia del lift (px)
    float liftStrength = 1.5f;       // fuerza de elevación

    // Return to origin
    float returnForce = 0.008f;      // fuerza de retorno suave a posición original

    // Render
    int lineSegments = 10;           // segmentos por línea orgánica
    float lineNoiseScale = 0.02f;    // escala espacial del noise en líneas
    float lineNoiseAmount = 10.0f;   // amplitud del noise en líneas (px)
    float lineAlpha = 180.0f;        // alpha de las líneas (0-255)
    float lineWidth = 1.2f;          // grosor base de las líneas
    float nodeRadius = 2.5f;         // radio de los nodos (px)
    float nodeAlpha = 100.0f;        // alpha de los nodos (0-255)
};

// ============================================================================
// OrganicNetScene
// ============================================================================

class OrganicNetScene : public IBaseScene {
public:
    OrganicNetScene();
    ~OrganicNetScene() override;

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
    void applyConstraints();
    void drawOrganicLine(const glm::vec2& a, const glm::vec2& b, float noiseBase);

    std::vector<std::vector<ofFloatColor>> palettes_;
    int currentPalette_;

    std::map<std::string, InteractionPoint> interactionPoints_;

    // Net data
    std::vector<NetNode> nodes_;
    std::vector<NetEdge> edges_;
    int nodeRows_ = 0;
    int nodeCols_ = 0;

    NetParams netParams_;

    bool isActive_;
    bool isPaused_;
    bool isValid_;
    std::string invalidReason_;

    std::map<std::string, std::shared_ptr<Parameter>> parameters_;
    bool parametersRegistered_;

    bool debugMarkers_;
};

ScenePtr CreateOrganicNetScene();

} // namespace modular
