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

struct StarPoint {
    ofVec3f basePos;
    ofVec3f currPos;
};

// ---------------------------------------------------------------------------
// GravitationalPointCloud — Fase III "Disolución". Nube densa de puntos
// deformada por la gravedad del cuerpo (pecho) al moverse en Z.
// ---------------------------------------------------------------------------
class GravitationalPointCloudScene : public IBaseScene {
public:
    GravitationalPointCloudScene();
    ~GravitationalPointCloudScene() override;

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
    void buildCloud();

    std::vector<StarPoint> stars_;
    std::map<std::string, InteractionPoint> interactionPoints_;
    std::map<std::string, std::shared_ptr<Parameter>> params_;
    bool isActive_ = false, isPaused_ = false;
    ofCamera cam_;
    ofVboMesh starMesh_;

    int numStars_ = 1200;
    float gravityRadius_ = 500.0f;   // radio de atracción del pecho
    float gravityStrength_ = 0.7f;
};

ScenePtr CreateGravitationalPointCloudScene();
} // namespace modular
