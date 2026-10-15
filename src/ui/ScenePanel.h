#pragma once

#include "engine/SceneEngine.h"
#include "ui/IUIModule.h"

#include <string>

namespace modular {

class ScenePanel : public IUIModule {
public:
    explicit ScenePanel(SceneEngine* sceneEngine);

    void setup() override {}
    void update(float /*deltaTime*/) override {}
    void draw() override;

    void setVisible(bool visible) override;
    bool isVisible() const override;

    std::string getPanelId() const override { return "ScenePanel"; }

private:
    void drawSceneTable();

    SceneEngine* sceneEngine_;
    bool visible_;
};

} // namespace modular
