#pragma once

#include "scenes/BaseScene.h"
#include "registry/ParameterRegistry.h"
#include "scenes/GenesisCommon.h"
#include "scenes/Pendulum.h"
#include "ofxOscReceiver.h"
#include "ofxOscMessage.h"
#include "ofMain.h"
#include <glm/vec2.hpp>
#include <vector>
#include <string>
#include <memory>
#include <map>

namespace modular {

// ============================================================================
// PendulumShape — péndulo anclado a un joint del cuerpo, con trazo por velocidad.
// ============================================================================
struct PendulumShape {
    std::vector<glm::vec2> shapePath;     // hilo guía (trayectoria del joint)
    std::vector<glm::vec2> pendulumPath;  // trazo de la punta del péndulo
    ofFloatColor color;
    std::shared_ptr<Pendulum> pendulum;
    glm::vec2 currentPos{0.0f};
    glm::vec2 vel{0.0f};
    int state = 0;       // 0=attached, 1=detached, 2=dead
    float lifeTime = 0.0f;
    float detachTime = 150.0f;
    bool hasPos = false;

    PendulumShape(ofFloatColor c, float linelength, int joints, float gravity, float damping);

    void updatePos(float x, float y, float gravity, float damping);
    void updateDetached(float gravity);
    void draw(float minThickness, float maxThickness, float speedSensitivity);
};

// ============================================================================
// BodyEmitter — joint del cuerpo que emite un péndulo.
// ============================================================================
struct BodyEmitter {
    std::string jointName;
    ofFloatColor color;
    std::shared_ptr<PendulumShape> active;
};

// ---------------------------------------------------------------------------
// PendulumScene — péndulos generativos anclados al cuerpo (muñecas, cabeza,
// tobillos). Trazos cuyo grosor depende de la velocidad (port del p5.js).
// ---------------------------------------------------------------------------
class PendulumScene : public IBaseScene {
public:
    PendulumScene();
    ~PendulumScene() override;

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
    void drawBones();

    std::vector<BodyEmitter> emitters_;
    std::vector<std::shared_ptr<PendulumShape>> floating_;
    std::map<std::string, InteractionPoint> interactionPoints_;
    std::map<std::string, std::shared_ptr<Parameter>> params_;
    bool isActive_ = false, isPaused_ = false;

    // Parámetros del péndulo (port del p5.js)
    float linelength_ = 28.0f;
    int joints_ = 6;
    float gravity_ = 0.094f;
    float damping_ = 0.998f;
    float minThickness_ = 1.5f;
    float maxThickness_ = 18.0f;
    float speedSensitivity_ = 18.0f;

    // Audio (Csound) → modulación visual
    ofxOscReceiver csoundReceiver_;
    bool csoundReceiverSetup_ = false;
    float audioEnergy_ = 0.5f;   // 0..1 (energía recibida de Csound)
    float audioFreq_ = 110.0f;
};

ScenePtr CreatePendulumScene();
} // namespace modular
