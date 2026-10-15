#pragma once

#include "ui/IUIModule.h"
#include "ui/PanelIds.h"
#include "midi/MidiMappingEngine.h"
#include "presets/PresetManager.h"
#include "imgui.h"
#include <functional>
#include <string>
#include <vector>

namespace modular {

class SceneEngine;

class MidiMappingPanel : public IUIModule {
public:
    /// @param engine            MidiMappingEngine for mapping data + active set access
    /// @param presetManager     PresetManager for preset context (future use)
    /// @param sceneEngine       SceneEngine for resolving target parameter dropdown options
    /// @param registerLearn     Called with a callback to register MIDI Learn in ofApp
    /// @param unregisterLearn   Called with a callback to unregister MIDI Learn in ofApp
    MidiMappingPanel(MidiMappingEngine* engine, PresetManager* presetManager,
                     SceneEngine* sceneEngine,
                     std::function<void(std::function<bool(ofxMidiMessage&)>)> registerLearn,
                     std::function<void(std::function<bool(ofxMidiMessage&)>)> unregisterLearn);

    void setup() override;
    void update(float deltaTime) override;
    void draw() override;

    void setVisible(bool visible) override;
    bool isVisible() const override;
    std::string getPanelId() const override { return panel_ids::kMidiMapping; }

private:
    void drawMappingTable();
    std::string controlModeToString(ControlMode mode) const;
    static std::string midiNoteName(int noteNumber);  // 36 → "C2", 60 → "C4"

    MidiMappingEngine* engine_;
    PresetManager* presetManager_;
    SceneEngine* sceneEngine_;
    bool visible_ = true;
    bool debugLogging_ = false;  // off by default — suppress per-message logs

    // Learn state
    bool learnActive_ = false;
    int learnRowIndex_ = -1;
    float learnTimer_ = 0.0f;
    bool timeoutMessageShown_ = false;
    static constexpr float kLearnTimeout = 10.0f;

    // Learn registration
    std::function<void(std::function<bool(ofxMidiMessage&)>)> registerLearn_;
    std::function<void(std::function<bool(ofxMidiMessage&)>)> unregisterLearn_;
    std::function<bool(ofxMidiMessage&)> learnCallback_;  // stored for unregister on deactivation
};

} // namespace modular
