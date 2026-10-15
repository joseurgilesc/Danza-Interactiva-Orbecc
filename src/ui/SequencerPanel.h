#pragma once

#include "sequencer/SceneSequencer.h"
#include "ui/IUIModule.h"
#include <string>

namespace modular {

class SequencerPanel : public IUIModule {
public:
    explicit SequencerPanel(SceneSequencer* sequencer);
    void setup() override {}
    void update(float) override {}
    void draw() override;
    void setVisible(bool visible) override;
    bool isVisible() const override;
    std::string getPanelId() const override { return "SequencerPanel"; }
private:
    SceneSequencer* sequencer_;
    bool visible_ = true;
};

} // namespace modular
