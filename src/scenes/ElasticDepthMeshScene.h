#pragma once

#include "scenes/BaseScene.h"
#include "registry/ParameterRegistry.h"
#include "scenes/GenesisCommon.h"
#include "ofMain.h"
#include <glm/vec2.hpp>
#include <vector>
#include <string>
#include <memory>
#include <map>

namespace modular {

struct MeshNode {
    ofVec3f restPos;   // posición de reposo (X, Y fijos, Z=0)
    ofVec3f pos;       // posición actual (Z puede deformarse)
    ofVec3f vel;       // velocidad (para la física de resortes)
};

// ---------------------------------------------------------------------------
// ElasticDepthMesh — Fase II "Tensión". Membrana elástica (rejilla 2D con
// resortes) deformada en Z por la profundidad del cuerpo.
// ---------------------------------------------------------------------------
class ElasticDepthMeshScene : public IBaseScene {
public:
    ElasticDepthMeshScene();
    ~ElasticDepthMeshScene() override;

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
    int index(int c, int r) const;

    std::vector<MeshNode> nodes_;
    std::map<std::string, InteractionPoint> interactionPoints_;
    std::map<std::string, std::shared_ptr<Parameter>> params_;
    bool isActive_ = false, isPaused_ = false;
    ofCamera cam_;
    ofVboMesh mesh_;

    int cols_ = 32;
    int rows_ = 24;
    float influenceRadius_ = 180.0f;   // radio (XY) de influencia de cada joint
    float springK_ = 0.1f;             // rigidez del resorte de reposo
};

ScenePtr CreateElasticDepthMeshScene();
} // namespace modular
