#pragma once

#include "scenes/BaseScene.h"
#include "registry/ParameterRegistry.h"
#include "scenes/GenesisCommon.h"
#include "scenes/Pendulum.h"
#include "ofMain.h"
#include <glm/vec2.hpp>
#include <vector>
#include <string>
#include <memory>
#include <map>

namespace modular {

// ============================================================================
// BodyShape — péndulo INVISIBLE anclado a un joint. Su punta deforma la red.
// ============================================================================
struct BodyShape {
    std::vector<glm::vec2> pendulumPath;   // punta del péndulo (invisible)
    std::shared_ptr<Pendulum> pendulum;
    glm::vec2 currentPos{0.0f};
    glm::vec2 vel{0.0f};
    float z = 0.5f;          // profundidad (0=cerca, 1=lejos)
    float speed = 0.0f;      // velocidad suavizada (EMA) para la estaticidad
    int state = 0;           // 0=attached, 1=detached, 2=dead
    float lifeTime = 0.0f;
    float detachTime = 180.0f;
    bool hasPos = false;

    BodyShape(float linelength, int joints, float gravity, float damping);

    void updatePos(float x, float y, float z, float gravity, float damping, float jitterThreshold);
    bool isMoving(float threshold) const;   // estaticidad (velocidad suavizada)
};

// ============================================================================
// GridNode — nodo de la red elástica.
// ============================================================================
struct Tip {
    glm::vec2 pos;
    float proximity = 0.5f;   // 1 = cerca (más fuerza)
};

struct GridNode {
    glm::vec2 basePos;
    glm::vec2 pos;
    glm::vec2 vel{0.0f};
    glm::vec2 acc{0.0f};
    float maxSpeed = 20.0f;

    GridNode(float x, float y) : basePos(x, y), pos(x, y) {}

    bool repel(const std::vector<Tip>& tips, float radius, float strength);
    void returnToBase();
    void update(bool isIdle, float dt);
};

// ---------------------------------------------------------------------------
// MeshScene — "generative mesh": péndulos invisibles del cuerpo deforman una
// red elástica. La estaticidad (velocidad suavizada) decide si un joint deforma.
// ---------------------------------------------------------------------------
class MeshScene : public IBaseScene {
public:
    MeshScene();
    ~MeshScene() override;

    void setup() override;
    void update(float dt) override;
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
    std::shared_ptr<Parameter> getParameter(const std::string& id) override;
    bool setParameter(const std::string& id, float v) override;
    bool setParameter(const std::string& id, const glm::vec2& v) override;
    bool setParameter(const std::string& id, const glm::vec3& v) override;

    std::vector<SuggestedMapping> getSuggestedMappings() const override;
    Preset getDefaultPreset() const override;
    SceneMetadata getSceneMetadata() const override;
    void onBeat(int) override;
    void onBar(int) override;
    bool isValid() const override;
    std::string getInvalidReason() const override;
    void unloadResources() override;
    void reloadResources() override;
    void keyPressed(int) override;
    void keyReleased(int) override;
    void mousePressed(int,int,int) override;
    void mouseReleased(int,int,int) override;
    void mouseDragged(int,int,int) override;
    void mouseMoved(int,int) override;
    void windowResized(int,int) override;

private:
    void buildGrid();
    void drawBones();

    std::vector<GridNode> grid_;
    std::vector<std::pair<std::string, std::string>> emitters_;  // jointName → ? (no se usa color)
    std::vector<std::shared_ptr<BodyShape>> shapes_;
    std::map<std::string, InteractionPoint> interactionPoints_;
    std::map<std::string, std::shared_ptr<Parameter>> params_;
    bool isActive_ = false, isPaused_ = false;

    int cols_ = 0, rows_ = 0;
    float spacing_ = 28.0f;
    float linelength_ = 50.0f;
    int joints_ = 8;
    float gravity_ = 0.094f;
    float damping_ = 0.998f;
    float moveThreshold_ = 1.0f;      // estaticidad: velocidad mínima (px/frame) para deformar
    float jitterThreshold_ = 2.5f;    // filtro de temblor de cámara (px)
    float repulsionRadius_ = 200.0f;
    float repulsionStrength_ = 25.0f;
    float idleDelayMs_ = 800.0f;
    float lastDisturbance_ = -1e9f;
};

ScenePtr CreateMeshScene();
} // namespace modular
