#pragma once

#include "engine/Transport.h"
#include "ui/IUIModule.h"

namespace modular {

class TransportPanel : public IUIModule {
public:
    explicit TransportPanel(TransportController* transport);

    void setup() override {}
    void update(float /*deltaTime*/) override {}
    void draw() override;

    void setVisible(bool visible) override;
    bool isVisible() const override;

    std::string getPanelId() const override { return "TransportPanel"; }

private:
    void drawPlaybackButtons();
    void drawTimeline();

    TransportController* transport_;
    bool visible_;
};

} // namespace modular
