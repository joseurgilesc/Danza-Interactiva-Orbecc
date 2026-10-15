#pragma once

#include "engine/SceneEngine.h"
#include "registry/ParameterRegistry.h"
#include "ui/IUIModule.h"

#include "imgui.h"

#include <string>
#include <vector>

namespace modular {

class ParamPanel : public IUIModule {
public:
    ParamPanel(SceneEngine* sceneEngine, ParameterRegistry* registry);

    void setup() override {}
    void update(float /*deltaTime*/) override {}
    void draw() override;

    void setVisible(bool visible) override;
    bool isVisible() const override;

    std::string getPanelId() const override { return "ParamPanel"; }

private:
struct ParameterRow {
    Parameter parameter;  // Copy of parameter for display
    bool isGlobal;
    std::string owner;
    bool valueUpdated = false;  // Track if value was changed this frame
};

    void rebuildRows(std::vector<ParameterRow>& rows);
    void drawRow(ParameterRow& row, ScenePtr activeScene);
    void applyChange(const ParameterRow& row, float value, ScenePtr activeScene);

    SceneEngine* sceneEngine_;
    ParameterRegistry* registry_;
    bool visible_;
    bool showGlobal_;
    bool showScene_;
    ImGuiTextFilter filter_;
};

} // namespace modular
