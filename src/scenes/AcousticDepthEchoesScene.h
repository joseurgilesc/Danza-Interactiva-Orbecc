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

struct Echo {
    ofVec3f pos;
    float radius = 0.0f;
    float maxRadius = 200.0f;
    float alpha = 1.0f;
};

// ---------------------------------------------------------------------------
// AcousticDepthEchoes — Fase IV "Trascendencia". Esferas wireframe que nacen
// del frenado brusco tras un movimiento rápido en Z (resonancia final).
// ---------------------------------------------------------------------------
class AcousticDepthEchoesScene : public IBaseScene {
public:
    AcousticDepthEchoesScene();
    ~AcousticDepthEchoesScene() override;

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
    std::vector<Echo> echoes_;
    std::map<std::string, InteractionPoint> interactionPoints_;
    std::map<std::string, ofVec3f> lastPos_;
    std::map<std::string, float> lastSpeedZ_;
    std::map<std::string, std::shared_ptr<Parameter>> params_;
    bool isActive_ = false, isPaused_ = false;
    ofCamera cam_;

    float fastThreshold_ = 6.0f;    // velocidad Z "rápida"
    float slowThreshold_ = 2.0f;    // velocidad Z "frenada"
    size_t maxEchoes_ = 60;
};

ScenePtr CreateAcousticDepthEchoesScene();
} // namespace modular
