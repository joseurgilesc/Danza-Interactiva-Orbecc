#pragma once

#include "presets/PresetManager.h"
#include "engine/SceneEngine.h"
#include "ui/IUIModule.h"
#include <string>
#include <unordered_map>

namespace modular {

class PresetPanel : public IUIModule {
public:
    PresetPanel(PresetManager* presetManager, SceneEngine* sceneEngine);
    void setup() override {}
    void update(float) override {}
    void draw() override;
    void setVisible(bool visible) override;
    bool isVisible() const override;
    std::string getPanelId() const override { return "PresetPanel"; }
private:
    PresetManager* presetManager_;
    SceneEngine* sceneEngine_;
    bool visible_ = true;
    int selectedSceneIndex_ = 0;
    float morphDuration_ = 2.0f;
    char newPresetName_[64] = "";
    std::unordered_map<std::string, std::string> activePresetPerScene_;  // sceneId → presetName

    // Popup state (persists across frames so modals stay open)
    int deletePendingIdx_ = -1;   // index of preset pending deletion, -1 = none
    int renamePendingIdx_ = -1;   // index of preset pending rename, -1 = none
};

} // namespace modular
