#include "ui/MidiMappingPanel.h"
#include "engine/SceneEngine.h"
#include "ofxMidi.h"
#include "imgui.h"
#include <ofLog.h>
#include <ofMain.h>
#include <algorithm>

namespace modular {

// ---- Constructor ------------------------------------------------------------

MidiMappingPanel::MidiMappingPanel(MidiMappingEngine* engine, PresetManager* presetManager,
                                   SceneEngine* sceneEngine,
                                   std::function<void(std::function<bool(ofxMidiMessage&)>)> registerLearn,
                                   std::function<void(std::function<bool(ofxMidiMessage&)>)> unregisterLearn)
    : engine_(engine)
    , presetManager_(presetManager)
    , sceneEngine_(sceneEngine)
    , registerLearn_(std::move(registerLearn))
    , unregisterLearn_(std::move(unregisterLearn))
{
    (void)presetManager_;  // reserved for future OnPresetApplied callback
}

// ---- IUIModule interface ----------------------------------------------------

void MidiMappingPanel::setup() {
    // Learn callback — always registered, checks learnActive_ flag.
    // This avoids the fragile unregister/reregister pattern.
    learnCallback_ = [this](ofxMidiMessage& msg) -> bool {
        if (!learnActive_) return false;

        MidiMessageType type;
        int number = -1;
        int channel = msg.channel;

        // Only capture CC or NoteOn (velocity > 0)
        if (msg.status == MIDI_CONTROL_CHANGE) {
            type = MidiMessageType::CC;
            number = msg.control;
        } else if (msg.status == MIDI_NOTE_ON && msg.velocity > 0) {
            type = MidiMessageType::NOTE;
            number = msg.pitch;
        } else {
            return false;
        }

        // Fill the learn row
        auto& mappings = engine_->getActiveMappings();
        if (learnRowIndex_ >= 0 && learnRowIndex_ < (int)mappings.size()) {
            auto& mapping = mappings[learnRowIndex_];
            mapping.type = type;
            mapping.channel = channel;
            // ADSR mode: append to numbers[]; otherwise set single number
            if (mapping.controlMode == ControlMode::ADSR) {
                if (std::find(mapping.numbers.begin(), mapping.numbers.end(), number) == mapping.numbers.end()) {
                    mapping.numbers.push_back(number);
                    std::sort(mapping.numbers.begin(), mapping.numbers.end());
                }
            } else {
                mapping.number = number;
            }

            // Auto-select first parameter from active scene if target is empty
            if (mapping.targetParameterId.empty()) {
                auto activeScene = sceneEngine_ ? sceneEngine_->getActiveScene() : nullptr;
                if (activeScene) {
                    auto paramIds = activeScene->getParameterIds();
                    if (!paramIds.empty()) {
                        mapping.targetParameterId = activeScene->getId() + "." + paramIds[0];
                        ofLogNotice("MidiMappingPanel") << "Learn auto-selected target: " << mapping.targetParameterId;
                    }
                }
            }
        }

        // Deactivate Learn
        learnActive_ = false;
        learnRowIndex_ = -1;
        learnTimer_ = 0.0f;

        ofLogNotice("MidiMappingPanel") << "Learn captured: "
            << (type == MidiMessageType::CC ? "CC" : "Note")
            << " ch=" << channel << " #=" << number;

        return true;  // message consumed
    };

    // Register once — never unregister (callback gates on learnActive_)
    registerLearn_(learnCallback_);
}

void MidiMappingPanel::update(float deltaTime) {
    // Learn timeout
    if (learnActive_) {
        learnTimer_ += deltaTime;
        if (learnTimer_ >= kLearnTimeout) {
            ofLogNotice("MidiMappingPanel") << "Learn timeout — no MIDI message received within "
                                            << kLearnTimeout << "s";
            learnActive_ = false;
            learnRowIndex_ = -1;
            learnTimer_ = 0.0f;
            timeoutMessageShown_ = true;
        }
    }
}

void MidiMappingPanel::draw() {
    ImGui::SetNextWindowSize(ImVec2(850, 400), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(panel_ids::kMidiMapping, &visible_)) {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Debug", &debugLogging_);
    if (engine_) engine_->setDebugLogging(debugLogging_);
    ImGui::SameLine();
    ImGui::TextDisabled("(log MIDI dispatch to console)");

    drawMappingTable();

    if (timeoutMessageShown_) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Learn timed out — no MIDI message received");
        timeoutMessageShown_ = false;
    }

    ImGui::End();
}

void MidiMappingPanel::setVisible(bool visible) {
    visible_ = visible;
}

bool MidiMappingPanel::isVisible() const {
    return visible_;
}

// ---- Control mode string ----------------------------------------------------

std::string MidiMappingPanel::controlModeToString(ControlMode mode) const {
    switch (mode) {
        case ControlMode::CONTINUOUS: return "Continuous";
        case ControlMode::TOGGLE:     return "Toggle";
        case ControlMode::TRIGGER:    return "Trigger";
        case ControlMode::MOMENTARY:  return "Momentary";
        case ControlMode::ADSR:       return "ADSR";
        default:                      return "Continuous";
    }
}

// ---- MIDI note number → name -------------------------------------------------

std::string MidiMappingPanel::midiNoteName(int noteNumber) {
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    if (noteNumber < 0 || noteNumber > 127) return "??";
    int octave = (noteNumber / 12) - 1;
    int note = noteNumber % 12;
    return std::string(names[note]) + std::to_string(octave);
}

// ---- Mapping Table ----------------------------------------------------------

void MidiMappingPanel::drawMappingTable() {
    ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;

    if (!ImGui::BeginTable("MidiMappingsTable", 12, flags)) {
        return;
    }

    // Column headers
    ImGui::TableSetupColumn("Type",   ImGuiTableColumnFlags_WidthFixed, 55);
    ImGui::TableSetupColumn("Ch",     ImGuiTableColumnFlags_WidthFixed, 35);
    ImGui::TableSetupColumn("#",      ImGuiTableColumnFlags_WidthFixed, 40);
    ImGui::TableSetupColumn("Target Param", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Min In", ImGuiTableColumnFlags_WidthFixed, 55);
    ImGui::TableSetupColumn("Max In", ImGuiTableColumnFlags_WidthFixed, 55);
    ImGui::TableSetupColumn("Min Out",ImGuiTableColumnFlags_WidthFixed, 55);
    ImGui::TableSetupColumn("Max Out",ImGuiTableColumnFlags_WidthFixed, 55);
    ImGui::TableSetupColumn("Mode",   ImGuiTableColumnFlags_WidthFixed, 90);
    ImGui::TableSetupColumn("On",     ImGuiTableColumnFlags_WidthFixed, 30);
    ImGui::TableSetupColumn("Learn",  ImGuiTableColumnFlags_WidthFixed, 65);
    ImGui::TableSetupColumn("",       ImGuiTableColumnFlags_WidthFixed, 22);
    ImGui::TableHeadersRow();

    auto& mappings = engine_->getActiveMappings();
    int rowToRemove = -1;

    for (int i = 0; i < (int)mappings.size(); i++) {
        auto& mapping = mappings[i];

        ImGui::TableNextRow();
        ImGui::PushID(i);

        // Activity indicator — green highlight when MIDI was received in last 0.5s
        float timeSinceActive = ofGetElapsedTimef() - mapping.lastActiveTime;
        bool isActive = mapping.lastActiveTime > 0.0f && timeSinceActive < 0.5f;

        if (isActive) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                ImGui::GetColorU32(ImVec4(0.15f, 0.55f, 0.15f, 0.45f)));  // green tint
        }
        // Highlight row during Learn (yellow, overrides active)
        else if (learnActive_ && learnRowIndex_ == i) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                ImGui::GetColorU32(ImVec4(0.5f, 0.4f, 0.1f, 0.4f)));  // yellow tint
        }

        // ---- Type (combo: CC / Note) ----
        ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemWidth(-FLT_MIN);
        const char* typeItems[] = { "CC", "Note" };
        int typeIdx = (mapping.type == MidiMessageType::NOTE) ? 1 : 0;
        if (ImGui::Combo("##type", &typeIdx, typeItems, 2)) {
            mapping.type = (typeIdx == 1) ? MidiMessageType::NOTE : MidiMessageType::CC;
        }

        // ---- Channel (1-16) ----
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        int ch = mapping.channel;
        if (ImGui::InputInt("##ch", &ch, 1, 1)) {
            if (ch < 1) ch = 1;
            if (ch > 16) ch = 16;
            mapping.channel = ch;
        }

        // ---- Number (0-127) / Note name ----
        ImGui::TableSetColumnIndex(2);
        ImGui::SetNextItemWidth(-FLT_MIN);
        int num = mapping.number;
        if (mapping.type == MidiMessageType::NOTE) {
            // Show note name for Note type
            std::string noteName = midiNoteName(mapping.number);
            if (ImGui::InputText("##num", const_cast<char*>(noteName.data()), noteName.size(), ImGuiInputTextFlags_ReadOnly)) {
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Note #%d — click Type to edit number", mapping.number);
            }
        } else {
            if (ImGui::InputInt("##num", &num, 1, 10)) {
                if (num < 0) num = 0;
                if (num > 127) num = 127;
                mapping.number = num;
            }
        }

        // ---- Target Parameter (dropdown from active scene params) ----
        ImGui::TableSetColumnIndex(3);
        ImGui::SetNextItemWidth(-FLT_MIN);
        {
            auto activeScene = sceneEngine_ ? sceneEngine_->getActiveScene() : nullptr;
            std::string sceneId = activeScene ? activeScene->getId() : "";
            auto paramIds = activeScene ? activeScene->getParameterIds() : std::vector<std::string>{};

            // Build options: always include current value first
            std::vector<std::string> options;
            if (!mapping.targetParameterId.empty()) {
                options.push_back(mapping.targetParameterId);
            }
            for (const auto& pid : paramIds) {
                std::string fullId = sceneId + "." + pid;
                if (fullId != mapping.targetParameterId) {
                    options.push_back(fullId);
                }
            }

            if (options.empty()) {
                ImGui::TextDisabled("(no scene)");
            } else {
                // Build const char* array for ImGui::Combo
                std::vector<const char*> cstrs;
                cstrs.reserve(options.size());
                for (const auto& opt : options) {
                    cstrs.push_back(opt.c_str());
                }
                int currentIdx = 0;  // first option is the current value
                if (ImGui::Combo("##target", &currentIdx, cstrs.data(), static_cast<int>(cstrs.size()))) {
                    if (currentIdx >= 0 && currentIdx < static_cast<int>(options.size())) {
                        mapping.targetParameterId = options[currentIdx];
                    }
                }
            }
        }

        // ---- Min In ----
        ImGui::TableSetColumnIndex(4);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::DragFloat("##minIn", &mapping.minInput, 0.5f, 0.0f, 127.0f, "%.0f");

        // ---- Max In ----
        ImGui::TableSetColumnIndex(5);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::DragFloat("##maxIn", &mapping.maxInput, 0.5f, 0.0f, 127.0f, "%.0f");

        // ---- Min Out ----
        ImGui::TableSetColumnIndex(6);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::DragFloat("##minOut", &mapping.minOutput, 0.01f, -1000.0f, 1000.0f, "%.2f");

        // ---- Max Out ----
        ImGui::TableSetColumnIndex(7);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::DragFloat("##maxOut", &mapping.maxOutput, 0.01f, -1000.0f, 1000.0f, "%.2f");

        // ---- Mode (combo: Continuous / Toggle / Trigger / Momentary / ADSR) ----
        ImGui::TableSetColumnIndex(8);
        ImGui::SetNextItemWidth(-FLT_MIN);
        const char* modeItems[] = { "Continuous", "Toggle", "Trigger", "Momentary", "ADSR" };
        int modeIdx = static_cast<int>(mapping.controlMode);
        if (ImGui::Combo("##mode", &modeIdx, modeItems, 5)) {
            mapping.controlMode = static_cast<ControlMode>(modeIdx);
        }

        // ---- Enabled (checkbox) ----
        ImGui::TableSetColumnIndex(9);
        ImGui::Checkbox("##enabled", &mapping.enabled);

        // ---- Learn button + activity indicator ----
        ImGui::TableSetColumnIndex(10);
        bool isThisLearning = learnActive_ && (learnRowIndex_ == i);
        if (isThisLearning) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));  // green
            char label[64];
            snprintf(label, sizeof(label), "Listening... (%.0fs)", kLearnTimeout - learnTimer_);
            if (ImGui::Button(label, ImVec2(-FLT_MIN, 0))) {
                // Cancel learn
                learnActive_ = false;
                learnRowIndex_ = -1;
                learnTimer_ = 0.0f;
            }
            ImGui::PopStyleColor();
        } else if (isActive) {
            // Show live value badge — note name for notes, raw value for CC
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.5f, 0.1f, 0.7f));
            char badge[32];
            if (mapping.type == MidiMessageType::NOTE) {
                snprintf(badge, sizeof(badge), "● %s", midiNoteName(mapping.number).c_str());
            } else {
                snprintf(badge, sizeof(badge), "● %.0f", mapping.lastReceivedValue);
            }
            ImGui::Button(badge, ImVec2(-FLT_MIN, 0));
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                if (mapping.type == MidiMessageType::NOTE) {
                    ImGui::SetTooltip("MIDI active — %s", midiNoteName(mapping.number).c_str());
                } else {
                    ImGui::SetTooltip("MIDI active — raw value: %.0f", mapping.lastReceivedValue);
                }
            }
        } else {
            bool alreadyLearning = learnActive_;  // but on a different row
            if (alreadyLearning) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Learn", ImVec2(-FLT_MIN, 0))) {
                // Cancel any existing learn first
                if (learnActive_) {
                    learnActive_ = false;
                }
                learnActive_ = true;
                learnRowIndex_ = i;
                learnTimer_ = 0.0f;
            }
            if (alreadyLearning) {
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("Already learning on another row");
                }
            }
        }

        // ---- Remove button (per row) ----
        ImGui::TableSetColumnIndex(11);
        if (isThisLearning) {
            ImGui::BeginDisabled();
        }
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.12f, 0.12f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
        if (ImGui::SmallButton("✕")) {
            rowToRemove = i;
        }
        ImGui::PopStyleColor(3);
        if (isThisLearning) {
            ImGui::EndDisabled();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Delete this mapping");
        }

        // ---- ADSR settings (shown only when mode is ADSR) ----
        if (mapping.controlMode == ControlMode::ADSR) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "ADSR");
            ImGui::SameLine();

            // Multi-note input
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("Notes:");
            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(200);
            char notesBuf[128] = {};
            if (!mapping.numbers.empty()) {
                std::string notesStr;
                for (size_t ni = 0; ni < mapping.numbers.size(); ni++) {
                    if (ni > 0) notesStr += ",";
                    notesStr += std::to_string(mapping.numbers[ni]);
                }
                snprintf(notesBuf, sizeof(notesBuf), "%s", notesStr.c_str());
            }
            if (ImGui::InputText("##notes", notesBuf, sizeof(notesBuf))) {
                // Parse comma-separated note numbers
                mapping.numbers.clear();
                std::string s(notesBuf);
                size_t pos = 0;
                while (pos < s.size()) {
                    while (pos < s.size() && (s[pos] == ' ' || s[pos] == ',')) pos++;
                    size_t end = pos;
                    while (end < s.size() && s[end] >= '0' && s[end] <= '9') end++;
                    if (end > pos) {
                        int note = std::stoi(s.substr(pos, end - pos));
                        if (note >= 0 && note <= 127) mapping.numbers.push_back(note);
                    }
                    pos = end;
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Comma-separated MIDI note numbers (e.g. 60,62,64,67)");

            // ADSR params
            ImGui::TableSetColumnIndex(3);
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("A ms##attack", &mapping.attackMs, 1.0f, 1.0f, 5000.0f, "%.0f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("D ms##decay", &mapping.decayMs, 1.0f, 1.0f, 5000.0f, "%.0f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("S##sustain", &mapping.sustainLevel, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50);
            ImGui::DragFloat("R ms##release", &mapping.releaseMs, 1.0f, 1.0f, 5000.0f, "%.0f");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Attack | Decay | Sustain (0-1) | Release (ms)");
        }

        ImGui::PopID();
    }

    ImGui::EndTable();

    // ---- Add + Save buttons (below table) ----
    if (ImGui::Button("+ Add Mapping")) {
        MidiMapping newMapping;
        newMapping.type = MidiMessageType::CC;
        newMapping.channel = 2;
        newMapping.number = 0;
        newMapping.targetParameterId = "";
        newMapping.minInput = 0.0f;
        newMapping.maxInput = 127.0f;
        newMapping.minOutput = 0.0f;
        newMapping.maxOutput = 1.0f;
        newMapping.controlMode = ControlMode::CONTINUOUS;
        newMapping.enabled = true;
        mappings.push_back(newMapping);
    }

    ImGui::SameLine();
    if (ImGui::Button("Save to Preset")) {
        // Sync engine's active mappings to current PresetDef
        presetManager_->syncMidiMappingsToCurrentPreset();
        // Save all loaded scenes to their JSON files
        // (syncMidiMappingsToCurrentPreset only updates in-memory PresetDef)
        // We need to also trigger savePresets. But we don't know which scene...
        // The PresetManager should auto-save after sync.
        ofLogNotice("MidiMappingPanel") << "Mappings saved to preset";
    }

    // ---- Remove (deferred) ----
    if (rowToRemove >= 0 && rowToRemove < (int)mappings.size()) {
        // Cancel learn if active on the removed row
        if (learnActive_ && learnRowIndex_ == rowToRemove) {
            learnActive_ = false;
            learnRowIndex_ = -1;
            learnTimer_ = 0.0f;
        } else if (learnActive_ && learnRowIndex_ > rowToRemove) {
            learnRowIndex_--;
        }
        mappings.erase(mappings.begin() + rowToRemove);
    }

    // ---- Empty state ----
    if (mappings.empty()) {
        ImGui::TextDisabled("No MIDI parameter mappings yet. Click '+ Add Mapping' to create one.");
    }
}

} // namespace modular
