#pragma once

#include "ui/IUIModule.h"
#include "imgui.h"

#include <string>

namespace modular {

// Forward declarations
class OscMappingEngine;
class SceneEngine;

class OscMappingPanel : public IUIModule {
public:
    OscMappingPanel(OscMappingEngine* engine, SceneEngine* sceneEngine);

    void setup() override {}
    void update(float deltaTime) override;
    void draw() override;

    void setVisible(bool visible) override;
    bool isVisible() const override;

    std::string getPanelId() const override { return "OscMappingPanel"; }

private:
    void drawToolbar();
    void drawMappingList();
    void drawMappingEditor();
    void drawLearningIndicator();

    OscMappingEngine* engine_;
    SceneEngine* sceneEngine_;
    bool visible_;
    ImGuiTextFilter filter_;
    
    // Inline editing buffers
    char newOscAddrBuf_[128];
    char newTargetParamBuf_[128];
    
    // Selection state
    int selectedRow_;
};

} // namespace modular
