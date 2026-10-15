#pragma once

#include "input/MidiPortsStore.h"
#include "ui/IUIModule.h"

#include "imgui.h"

namespace modular {

class InputPortsPanel : public IUIModule {
public:
    explicit InputPortsPanel(MidiPortsStore* store);

    void setup() override {}
    void update(float /*deltaTime*/) override {}
    void draw() override;

    void setVisible(bool visible) override;
    bool isVisible() const override;

    std::string getPanelId() const override { return "InputPortsPanel"; }

private:
    void drawPortsTable();

    MidiPortsStore* store_;
    bool visible_;
};

} // namespace modular
