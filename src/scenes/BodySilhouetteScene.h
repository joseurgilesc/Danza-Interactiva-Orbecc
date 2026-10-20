#pragma once

#include "scenes/BaseScene.h"
#include "registry/ParameterRegistry.h"
#include "input/sensors/DepthStore.h"
#include "ofMain.h"
#include <glm/vec2.hpp>
#include <vector>
#include <string>
#include <memory>
#include <map>

namespace modular {

struct SilParticle {
    glm::vec2 pos;
    float size = 6.0f;
    float alpha = 1.0f;
    float life = 0.0f;
    float maxLife = 2.0f;
    ofFloatColor color;
};

// ---------------------------------------------------------------------------
// BodySilhouetteScene — rellena la silueta REAL del cuerpo (desde el depth)
// con partículas luminosas. Es el contorno verdadero píxel a píxel.
// ---------------------------------------------------------------------------
class BodySilhouetteScene : public IBaseScene {
public:
    BodySilhouetteScene();
    ~BodySilhouetteScene() override;

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
    std::vector<SilParticle> particles_;
    std::map<std::string, InteractionPoint> interactionPoints_;
    std::map<std::string, std::shared_ptr<Parameter>> params_;
    bool isActive_ = false, isPaused_ = false;

    size_t maxParticles_ = 1200;
    int spawnAttempts_ = 24;   // intentos de spawn por frame (rejection sampling)
    uint16_t thresholdMM_ = 2500;
};

ScenePtr CreateBodySilhouetteScene();
} // namespace modular
