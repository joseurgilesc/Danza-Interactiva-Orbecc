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

struct PolyEcho {
    ofVec3f pos;
    float size = 0.0f;
    float rotation = 0.0f;
    float rotSpeed = 0.0f;
    float alpha = 1.0f;
    ofColor color;
};

// ---------------------------------------------------------------------------
// VolumetricLattice — Fase I "Origen". Malla cristalina 3D + polígonos que se
// expanden desde las articulaciones del cuerpo (figuras que nacen del cuerpo).
// ---------------------------------------------------------------------------
class VolumetricLatticeScene : public IBaseScene {
public:
    VolumetricLatticeScene();
    ~VolumetricLatticeScene() override;

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
    std::map<std::string, InteractionPoint> interactionPoints_;
    std::map<std::string, std::shared_ptr<Parameter>> params_;
    bool isActive_ = false, isPaused_ = false;
    ofCamera cam_;
    ofVboMesh latticeMesh_;
    float connectionThreshold_ = 150.0f;   // distancia umbral (unidades de mundo)

    std::vector<PolyEcho> echoes_;
    size_t maxEchoes_ = 300;
    int emitEvery_ = 12;
    int frameCount_ = 0;
};

ScenePtr CreateVolumetricLatticeScene();
} // namespace modular
