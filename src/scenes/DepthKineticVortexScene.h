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

struct KinParticle {
    ofVec3f pos;
    ofVec3f vel;
    float life = 0.0f;
    float maxLife = 1.0f;
};

// ---------------------------------------------------------------------------
// DepthKineticVortex — Fase II "Tensión". Partículas 3D que estallan cuando
// una articulación se mueve brusco en Z (aceleración de profundidad).
// ---------------------------------------------------------------------------
class DepthKineticVortexScene : public IBaseScene {
public:
    DepthKineticVortexScene();
    ~DepthKineticVortexScene() override;

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
    void emitBurst(const ofVec3f& pos, float strength);

    std::vector<KinParticle> particles_;
    std::map<std::string, InteractionPoint> interactionPoints_;
    std::map<std::string, ofVec3f> lastPos_;   // última posición 3D por joint
    std::map<std::string, std::shared_ptr<Parameter>> params_;
    bool isActive_ = false, isPaused_ = false;
    ofCamera cam_;

    float zThreshold_ = 3.0f;     // velocidad Z mínima para emitir (unidades/frame)
    size_t maxParticles_ = 1500;
};

ScenePtr CreateDepthKineticVortexScene();
} // namespace modular
