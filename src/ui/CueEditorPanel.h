#pragma once

#include "sequencer/SceneSequencer.h"
#include "engine/SceneEngine.h"
#include "presets/PresetManager.h"
#include "ui/IUIModule.h"
#include "ui/PanelIds.h"
#include "ofxMidi.h"
#include <functional>
#include <string>
#include <vector>

namespace modular {

class CueEditorPanel : public IUIModule {
public:
    /// @param seq         SceneSequencer for cue data + mutation
    /// @param engine      SceneEngine for loaded scene list
    /// @param presets     PresetManager for preset lookups
    /// @param midiIn      ofxMidiIn for port availability check
    /// @param registerLearn  Called with a callback to register MIDI Learn in ofApp
    /// @param unregisterLearn  Called with a callback to unregister MIDI Learn in ofApp
    CueEditorPanel(SceneSequencer* seq, SceneEngine* engine,
                   PresetManager* presets, ofxMidiIn* midiIn,
                   std::function<void(std::function<bool(ofxMidiMessage&)>)> registerLearn,
                   std::function<void(std::function<bool(ofxMidiMessage&)>)> unregisterLearn);

    void setup() override {}
    void update(float) override {}
    void draw() override;
    void setVisible(bool visible) override;
    bool isVisible() const override;
    std::string getPanelId() const override { return panel_ids::kCueEditor; }

    /// Handle key events. Returns true if the key was consumed (e.g., Escape
    /// cancels MIDI Learn) so the caller can stop further routing.
    bool handleKeyPressed(int key);

private:
    // Validates duplicate cue numbers across all cues. Returns a sorted vector
    // of cue numbers that appear more than once (for inline error rendering).
    std::vector<int> findDuplicateCueNumbers() const;

    // Check whether a preset reference exists for a given scene+preset pair.
    // Returns true if preset is "(none)" or empty, or if found via PresetManager.
    bool isPresetValid(const std::string& sceneId, const std::string& presetName) const;

    // Internal: build the list of preset names for a scene combo, with "(none)" first.
    std::vector<std::string> buildPresetNames(const std::string& sceneId) const;

    SceneSequencer* seq_;
    SceneEngine* engine_;
    PresetManager* presets_;
    ofxMidiIn* midiIn_;
    std::function<void(std::function<bool(ofxMidiMessage&)>)> registerLearn_;
    std::function<void(std::function<bool(ofxMidiMessage&)>)> unregisterLearn_;
    std::function<bool(ofxMidiMessage&)> learnCallback_;  // stored for unregister on deactivation

    bool visible_ = true;
    int learningRow_ = -1;   // cue number of the row currently in Learn mode, or -1
    bool saveError_ = false;
    std::string saveErrorMessage_;

    // MIDI trigger edit popup state
    int pendingEditCue_ = -1;        // cue number being edited in the popup
    int editMidiTypeIdx_ = 0;        // 0=None, 1=PC, 2=Note, 3=CC
    int editMidiValue_ = 0;          // value 0-127
    int editMidiChannel_ = -1;       // channel 1-16, -1 = unknown
    bool pendingOpenMidiPopup_ = false;  // request OpenPopup from correct ID stack level

    // Delete confirmation popup state (persists across frames)
    int cueToDelete_ = -1;           // cue number pending deletion, -1 = none
};

} // namespace modular
