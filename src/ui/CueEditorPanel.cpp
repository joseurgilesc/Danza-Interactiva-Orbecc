#include "ui/CueEditorPanel.h"
#include "ui/PanelIds.h"
#include "imgui.h"
#include <algorithm>
#include <set>

namespace modular {

CueEditorPanel::CueEditorPanel(SceneSequencer* seq, SceneEngine* engine,
                               PresetManager* presets, ofxMidiIn* midiIn,
                               std::function<void(std::function<bool(ofxMidiMessage&)>)> registerLearn,
                               std::function<void(std::function<bool(ofxMidiMessage&)>)> unregisterLearn)
    : seq_(seq), engine_(engine), presets_(presets), midiIn_(midiIn)
    , registerLearn_(std::move(registerLearn))
    , unregisterLearn_(std::move(unregisterLearn))
{}

void CueEditorPanel::setVisible(bool v) { visible_ = v; }
bool CueEditorPanel::isVisible() const { return visible_; }

bool CueEditorPanel::handleKeyPressed(int key) {
    if (learningRow_ >= 0 && key == OF_KEY_ESC) {
        learningRow_ = -1;
        unregisterLearn_(learnCallback_);
        learnCallback_ = nullptr;
        return true;  // consumed
    }
    return false;
}

// ---- Helpers ----------------------------------------------------------------

std::vector<int> CueEditorPanel::findDuplicateCueNumbers() const {
    if (!seq_) return {};
    std::vector<int> duplicates;
    int count = seq_->getCueCount();
    for (int i = 0; i < count; ++i) {
        const auto* a = seq_->getCue(i);
        if (!a) continue;
        for (int j = i + 1; j < count; ++j) {
            const auto* b = seq_->getCue(j);
            if (!b) continue;
            if (a->cue == b->cue) {
                if (std::find(duplicates.begin(), duplicates.end(), a->cue) == duplicates.end()) {
                    duplicates.push_back(a->cue);
                }
                break;
            }
        }
    }
    return duplicates;
}

bool CueEditorPanel::isPresetValid(const std::string& sceneId,
                                   const std::string& presetName) const {
    if (presetName.empty() || presetName == "(none)") return true;
    if (!presets_) return true;  // can't validate without PresetManager
    auto presets = presets_->getPresets(sceneId);
    for (const auto& p : presets) {
        if (p.name == presetName) return true;
    }
    return false;
}

std::vector<std::string> CueEditorPanel::buildPresetNames(const std::string& sceneId) const {
    std::vector<std::string> names;
    names.push_back("(none)");
    if (!presets_) return names;
    auto presets = presets_->getPresets(sceneId);
    for (const auto& p : presets) {
        names.push_back(p.name);
    }
    return names;
}

// ---- draw -------------------------------------------------------------------

void CueEditorPanel::draw() {
    if (!visible_ || !seq_ || !engine_) return;

    if (!ImGui::Begin(panel_ids::kCueEditor, &visible_)) {
        ImGui::End();
        return;
    }

    // ---- Empty state ----
    if (!seq_->hasSequence()) {
        ImGui::TextDisabled("No cues defined");
        if (ImGui::Button("Add Cue")) {
            auto scenes = engine_->getLoadedScenes();
            CueDef cue;
            cue.cue = 1;
            cue.sceneId = scenes.empty() ? "" : scenes[0];
            cue.presetName = "(none)";
            cue.morphDuration = 0.0f;
            seq_->addCue(cue);
        }
        ImGui::End();
        return;
    }

    // ---- Pre-compute state for rendering ----
    auto duplicates = findDuplicateCueNumbers();
    auto sceneIds = engine_->getLoadedScenes();
    bool hasMidiPorts = midiIn_ && midiIn_->getNumInPorts() > 0;
    int cueCount = seq_->getCueCount();

    // ---- Toolbar: Add + Save ----
    if (ImGui::Button("+ Add Cue")) {
        // Find max existing cue number
        int maxCue = 0;
        for (int i = 0; i < cueCount; ++i) {
            const auto* c = seq_->getCue(i);
            if (c && c->cue > maxCue) maxCue = c->cue;
        }
        CueDef cue;
        cue.cue = maxCue + 1;
        cue.sceneId = sceneIds.empty() ? "" : sceneIds[0];
        cue.presetName = "(none)";
        cue.morphDuration = 0.0f;
        if (!seq_->addCue(cue)) {
            // If duplicate somehow, try next number
            cue.cue = maxCue + 2;
            seq_->addCue(cue);
        }
    }
    ImGui::SameLine();

    if (ImGui::Button("Save")) {
        // Pre-save validation: check for duplicates
        auto dups = findDuplicateCueNumbers();
        if (!dups.empty()) {
            saveError_ = true;
            saveErrorMessage_ = "Cannot save: duplicate cue numbers exist.";
        } else {
            bool ok = seq_->saveSequence("config/sequence.json");
            if (!ok) {
                saveError_ = true;
                saveErrorMessage_ = "Failed to write config/sequence.json";
            } else {
                saveError_ = false;
                saveErrorMessage_.clear();
            }
        }
    }

    // Save error feedback
    if (saveError_) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", saveErrorMessage_.c_str());
    }

    ImGui::Separator();

    // ---- Cue Table ----
    ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
    if (!ImGui::BeginTable("CueEditorTable", 7, flags)) {
        ImGui::End();
        return;
    }

    ImGui::TableSetupColumn("#",     ImGuiTableColumnFlags_WidthFixed, 50);
    ImGui::TableSetupColumn("Scene", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Preset",ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Morph", ImGuiTableColumnFlags_WidthFixed, 65);
    ImGui::TableSetupColumn("MIDI",  ImGuiTableColumnFlags_WidthFixed, 75);
    ImGui::TableSetupColumn("Learn", ImGuiTableColumnFlags_WidthFixed, 70);
    ImGui::TableSetupColumn("",      ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableHeadersRow();

    for (int row = 0; row < cueCount; ++row) {
        const auto* cue = seq_->getCue(row);
        if (!cue) continue;

        ImGui::TableNextRow();
        ImGui::PushID(row);

        bool isDuplicate = std::find(duplicates.begin(), duplicates.end(), cue->cue) != duplicates.end();
        bool presetOrphan = !isPresetValid(cue->sceneId, cue->presetName);

        // Row background tint for duplicate
        if (isDuplicate) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                ImGui::GetColorU32(ImVec4(0.45f, 0.1f, 0.1f, 0.4f)));
        }

        // ---- Column 0: Cue# ----
        ImGui::TableSetColumnIndex(0);
        int cueNum = cue->cue;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputInt("##cue", &cueNum, 1, 100, ImGuiInputTextFlags_None)) {
            if (cueNum > 0 && cueNum != cue->cue) {
                CueDef updated = *cue;
                updated.cue = cueNum;
                seq_->updateCue(cue->cue, updated);
            }
        }

        // ---- Column 1: Scene combo ----
        ImGui::TableSetColumnIndex(1);
        int sceneIdx = -1;
        for (int s = 0; s < (int)sceneIds.size(); ++s) {
            if (sceneIds[s] == cue->sceneId) { sceneIdx = s; break; }
        }
        std::string scenePreview = sceneIdx >= 0 ? sceneIds[sceneIdx] : cue->sceneId;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##scene", scenePreview.c_str())) {
            for (int s = 0; s < (int)sceneIds.size(); ++s) {
                bool isSel = (sceneIds[s] == cue->sceneId);
                if (ImGui::Selectable(sceneIds[s].c_str(), isSel)) {
                    CueDef updated = *cue;
                    updated.sceneId = sceneIds[s];
                    // Reset preset to (none) when switching scenes
                    updated.presetName = "(none)";
                    seq_->updateCue(cue->cue, updated);
                }
                if (isSel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // ---- Column 2: Preset combo ----
        ImGui::TableSetColumnIndex(2);
        auto presetNames = buildPresetNames(cue->sceneId);
        int presetIdx = -1;
        std::string displayPreset = cue->presetName.empty() ? "(none)" : cue->presetName;
        for (int p = 0; p < (int)presetNames.size(); ++p) {
            if (presetNames[p] == displayPreset) { presetIdx = p; break; }
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##preset", presetIdx >= 0 ? presetNames[presetIdx].c_str() : displayPreset.c_str())) {
            for (int p = 0; p < (int)presetNames.size(); ++p) {
                bool isSel = (presetNames[p] == displayPreset);
                if (ImGui::Selectable(presetNames[p].c_str(), isSel)) {
                    CueDef updated = *cue;
                    updated.presetName = (presetNames[p] == "(none)") ? "" : presetNames[p];
                    seq_->updateCue(cue->cue, updated);
                }
                if (isSel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        // Orphan preset warning
        if (presetOrphan && !cue->presetName.empty() && cue->presetName != "(none)") {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f), " (!)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Preset '%s' not found for scene '%s'",
                                  cue->presetName.c_str(), cue->sceneId.c_str());
            }
        }

        // ---- Column 3: Morph ----
        ImGui::TableSetColumnIndex(3);
        float morph = cue->morphDuration;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::DragFloat("##morph", &morph, 0.1f, 0.0f, 30.0f, "%.1f")) {
            if (morph != cue->morphDuration) {
                CueDef updated = *cue;
                updated.morphDuration = morph;
                seq_->updateCue(cue->cue, updated);
            }
        }

        // ---- Column 4: MIDI Trigger ----
        ImGui::TableSetColumnIndex(4);
        {
            std::string midiLabel = "-";
            if (!cue->midiType.empty() && cue->midiValue >= 0) {
                std::string typeLabel;
                if (cue->midiType == "pc")   typeLabel = "PC";
                else if (cue->midiType == "note") typeLabel = "Note";
                else if (cue->midiType == "cc")   typeLabel = "CC";
                if (!typeLabel.empty()) {
                    midiLabel = typeLabel + " " + std::to_string(cue->midiValue);
                    if (cue->midiChannel > 0) {
                        midiLabel += " ch" + std::to_string(cue->midiChannel);
                    }
                }
            }

            // Clickable cell: click to open assignment popup
            bool hasAssignment = !cue->midiType.empty() && cue->midiValue >= 0;
            ImGui::PushID("midi");
            if (hasAssignment) {
                ImGui::Text("%s", midiLabel.c_str());
            } else {
                ImGui::TextDisabled("%s", midiLabel.c_str());
            }
            ImGui::PopID();
            if (ImGui::IsItemClicked()) {
                pendingEditCue_ = cue->cue;
                editMidiTypeIdx_ = 0;  // "None"
                if (cue->midiType == "pc")      editMidiTypeIdx_ = 1;
                else if (cue->midiType == "note") editMidiTypeIdx_ = 2;
                else if (cue->midiType == "cc")   editMidiTypeIdx_ = 3;
                editMidiValue_ = cue->midiValue >= 0 ? cue->midiValue : 0;
                editMidiChannel_ = cue->midiChannel > 0 ? cue->midiChannel : -1;
                pendingOpenMidiPopup_ = true;  // defer OpenPopup to correct ID stack level
            }
        }

        // ---- Column 5: Learn button ----
        ImGui::TableSetColumnIndex(5);
        bool isLearning = (learningRow_ == cue->cue);
        if (isLearning) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            if (ImGui::Button("Learning...", ImVec2(-FLT_MIN, 0))) {
                // Cancel learn
                learningRow_ = -1;
                unregisterLearn_(learnCallback_);
                learnCallback_ = nullptr;
            }
            ImGui::PopStyleColor();
        } else {
            if (!hasMidiPorts) {
                ImGui::BeginDisabled();
                ImGui::Button("Learn", ImVec2(-FLT_MIN, 0));
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("No MIDI input ports available");
                }
            } else {
                if (ImGui::Button("Learn", ImVec2(-FLT_MIN, 0))) {
                    // Cancel any existing learn first
                    if (learningRow_ >= 0) {
                        unregisterLearn_(learnCallback_);
                        learnCallback_ = nullptr;
                    }
                    int targetCue = cue->cue;
                    learningRow_ = targetCue;

                    // Create learn callback and register with ofApp
                    learnCallback_ = [this, targetCue](ofxMidiMessage& msg) -> bool {
                        std::string type;
                        int value = -1;

                        if (msg.status == MIDI_PROGRAM_CHANGE) {
                            type = "pc";
                            value = msg.pitch;
                        } else if (msg.status == MIDI_NOTE_ON && msg.velocity > 0) {
                            type = "note";
                            value = msg.pitch;
                        } else if (msg.status == MIDI_CONTROL_CHANGE && msg.value > 0) {
                            type = "cc";
                            value = msg.control;
                        } else {
                            return false;  // not consumed
                        }

                        // Find the cue by number and update its midiType/midiValue
                        int idx = -1;
                        int count = seq_->getCueCount();
                        for (int i = 0; i < count; ++i) {
                            const auto* c = seq_->getCue(i);
                            if (c && c->cue == targetCue) { idx = i; break; }
                        }
                        if (idx >= 0) {
                            const auto* existing = seq_->getCue(idx);
                            CueDef updated = *existing;
                            updated.midiType = type;
                            updated.midiValue = value;
                            updated.midiChannel = msg.channel + 1;  // 0-based → 1-based
                            seq_->updateCue(targetCue, updated);
                        }
                        // Unregister and stop learning
                        learningRow_ = -1;
                        unregisterLearn_(learnCallback_);
                        learnCallback_ = nullptr;
                        return true;  // message consumed
                    };
                    registerLearn_(learnCallback_);
                }
            }
        }

        // ---- Column 6: Delete ----
        ImGui::TableSetColumnIndex(6);
        if (ImGui::Button("Del", ImVec2(-FLT_MIN, 0))) {
            cueToDelete_ = cue->cue;
        }

        ImGui::PopID();
    }

    ImGui::EndTable();

    // Deferred OpenPopup: must happen at this ID stack level (outside PushID/table scope)
    // so it matches the BeginPopupModal below. See Dear ImGui FAQ: "OpenPopup needs to be
    // called at the same ID stack level as BeginPopup."
    if (pendingOpenMidiPopup_) {
        ImGui::OpenPopup("EditMIDITrigger");
        pendingOpenMidiPopup_ = false;
    }

    // ---- MIDI Trigger Edit Popup ----
    if (ImGui::BeginPopupModal("EditMIDITrigger", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static const char* midiTypes[] = { "None", "Program Change", "Note On", "Control Change" };
        ImGui::Text("MIDI Trigger for Cue %d", pendingEditCue_);
        ImGui::Separator();

        ImGui::Combo("Type", &editMidiTypeIdx_, midiTypes, IM_ARRAYSIZE(midiTypes));
        ImGui::InputInt("Value (0-127)", &editMidiValue_, 1, 100);
        ImGui::InputInt("Channel (1-16)", &editMidiChannel_, 1, 1);
        if (editMidiChannel_ < 1) editMidiChannel_ = -1;
        if (editMidiChannel_ > 16) editMidiChannel_ = 16;

        // Clamp value
        if (editMidiValue_ < 0) editMidiValue_ = 0;
        if (editMidiValue_ > 127) editMidiValue_ = 127;

        if (ImGui::Button("Apply")) {
            // Find cue and update
            int idx = -1;
            int count = seq_->getCueCount();
            for (int i = 0; i < count; ++i) {
                const auto* c = seq_->getCue(i);
                if (c && c->cue == pendingEditCue_) { idx = i; break; }
            }
            if (idx >= 0) {
                const auto* existing = seq_->getCue(idx);
                CueDef updated = *existing;
                if (editMidiTypeIdx_ == 0) {
                    // None
                    updated.midiType = "";
                    updated.midiValue = -1;
                    updated.midiChannel = -1;
                } else if (editMidiTypeIdx_ == 1) {
                    updated.midiType = "pc";
                    updated.midiValue = editMidiValue_;
                    updated.midiChannel = editMidiChannel_;
                } else if (editMidiTypeIdx_ == 2) {
                    updated.midiType = "note";
                    updated.midiValue = editMidiValue_;
                    updated.midiChannel = editMidiChannel_;
                } else if (editMidiTypeIdx_ == 3) {
                    updated.midiType = "cc";
                    updated.midiValue = editMidiValue_;
                    updated.midiChannel = editMidiChannel_;
                }
                seq_->updateCue(pendingEditCue_, updated);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ---- Delete confirmation popup ----
    if (cueToDelete_ >= 0) {
        ImGui::OpenPopup("Delete Cue");
    }
    if (ImGui::BeginPopupModal("Delete Cue", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // cueToDelete_ persists across frames so the popup stays open
        ImGui::Text("Delete cue %d?", cueToDelete_);
        ImGui::Separator();
        if (ImGui::Button("Yes, delete")) {
            seq_->removeCue(cueToDelete_);
            // If we were learning on the deleted row, cancel
            if (learningRow_ == cueToDelete_) {
                learningRow_ = -1;
                unregisterLearn_(learnCallback_);
                learnCallback_ = nullptr;
            }
            cueToDelete_ = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            cueToDelete_ = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace modular
