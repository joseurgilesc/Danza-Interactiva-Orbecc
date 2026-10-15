#pragma once

#include "mapping/MappingLayer.h"
#include "ui/IUIModule.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include <string>

namespace modular {

class MappingPanel : public IUIModule {
public:
    explicit MappingPanel(MappingLayer* mappingLayer);

    void setup() override {}
    void update(float /*deltaTime*/) override {}
    void draw() override;

    void setVisible(bool visible) override;
    bool isVisible() const override;

    std::string getPanelId() const override { return "MappingPanel"; }

private:
    void drawMappingTable();

    MappingLayer* mappingLayer_;
    bool visible_;
    std::string selectedId_;
    ImGuiTextFilter filter_;
};

} // namespace modular
