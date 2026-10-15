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

struct DissolveShard {
    std::vector<glm::vec2> vertices;  // polygon shape (local coords)
    glm::vec2 offset{0.0f};           // world position after movement
    float alpha = 1.0f;
    float angle = 0.0f;
    float angVel = 0.0f;
};

struct DissolveCell {
    ofRectangle rect;
    ofFloatColor color;
    bool alive = true;
    float dissolveTime = 0.0f;
    std::vector<DissolveShard> shards;  // irregular polygon fragments
};

constexpr int GD_DEFAULT_COLS = 20;
constexpr int GD_DEFAULT_ROWS = 15;
constexpr float GD_DISSOLVE_DURATION = 0.8f;

class GridDissolveScene : public IBaseScene {
public:
    GridDissolveScene();
    ~GridDissolveScene() override;

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
    void setupPalettes();
    ofFloatColor randomColor();

    void initParams();
    void registerParams();
    void unregisterParams();
    void syncParams();

    std::vector<DissolveCell> cells_;
    std::map<std::string, InteractionPoint> interactionPoints_;
    std::vector<std::vector<ofFloatColor>> palettes_;

    bool isActive_ = false, isPaused_ = false, paramsReg_ = false;
    std::map<std::string, std::shared_ptr<Parameter>> params_;

    int cols_ = GD_DEFAULT_COLS, rows_ = GD_DEFAULT_ROWS;
    float dissolveDuration_ = GD_DISSOLVE_DURATION;
    float touchRadius_ = 10.0f;
    int currentPalette_ = 0;
};

ScenePtr CreateGridDissolveScene();
} // namespace modular
