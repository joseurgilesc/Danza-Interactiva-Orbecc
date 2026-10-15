#pragma once

#include "scenes/BaseScene.h"
#include "registry/ParameterRegistry.h"
#include "ofMain.h"
#include <glm/vec2.hpp>
#include <vector>
#include <string>
#include <memory>
#include <map>

namespace modular {

struct BCPParticle {
    glm::vec2 pos{0}, vel{0};
    ofFloatColor color;
    float life=0, maxLife=0, size=3, phase=0;
    bool active=false;
};

constexpr int BCP_DEFAULT_MAX=1200, BCP_MIN_MAX=100, BCP_MAX_MAX=4000;
constexpr float BCP_MAX_SPEED=600;

class BodyContourPaintScene : public IBaseScene {
public:
    BodyContourPaintScene(); ~BodyContourPaintScene() override;
    void setup()override; void update(float)override; void draw()override;
    void activate()override; void deactivate()override;
    void pause()override; void resume()override;
    std::string getId()const override; std::string getName()const override;
    std::vector<std::string> getParameterIds()const override;
    std::vector<std::string> getInteractionPointIds()const override;
    void clearInteractionPoints()override;
    std::shared_ptr<Parameter> getParameter(const std::string&)override;
    bool setParameter(const std::string&,float)override;
    bool setParameter(const std::string&,const glm::vec2&)override;
    bool setParameter(const std::string&,const glm::vec3&)override;
    std::vector<SuggestedMapping> getSuggestedMappings()const override;
    Preset getDefaultPreset()const override;
    SceneMetadata getSceneMetadata()const override;
    void onBeat(int)override; void onBar(int)override;
    bool isValid()const override; std::string getInvalidReason()const override;
    void unloadResources()override; void reloadResources()override;
    void keyPressed(int)override; void keyReleased(int)override;
    void mousePressed(int,int,int)override; void mouseReleased(int,int,int)override;
    void mouseDragged(int,int,int)override; void mouseMoved(int,int)override;
    void windowResized(int,int)override;
    void setDebugMarkers(bool v){debug_=v;}

private:
    void setupPalettes(); ofFloatColor getColor(int pi,float var);
    void initParams(); void registerParams(); void unregisterParams();
    void syncParams(); void applyResize();
    void buildCircle();
    void spawnParticles(float dt);
    void updateParticles(float dt);
    void drawRing();
    void drawParticles();

    std::vector<BCPParticle> particles_; int activeCount_=0;

    glm::vec2 circleCenter_{0};
    float circleRadius_=100, prevRadius_=100;

    std::map<std::string,InteractionPoint> interactionPoints_;
    std::vector<std::vector<ofFloatColor>> palettes_; int currentPalette_=0;

    bool isActive_=false,isPaused_=false,isValid_=true,debug_=false,paramsReg_=false;
    std::string invalidReason_;
    std::map<std::string,std::shared_ptr<Parameter>> params_;
    float spawnAccum_=0;

    // PinturaFluorescente orbit params (7)
    float k_=.08f,A_=18,w_=.5f,q_=.003f,beta_=.5f,S_=180,ws_=.3f,qs_=.002f;
    // ring + spawn (4)
    float ringPull_=1.2f,repelForce_=.3f,spreadRadius_=80,spawnRate_=40;
    // visual (3)
    float particleLife_=6,particleSize_=4,gravity_=.01f;
    // per-group sizes for _array MIDI mapping (8 groups)
    std::vector<float> groupSizes_{4,4,4,4,4,4,4,4};
    // pool
    int maxParticles_=BCP_DEFAULT_MAX;
};

ScenePtr CreateBodyContourPaintScene();
} // namespace modular
