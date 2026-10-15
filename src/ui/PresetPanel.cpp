#include "ui/PresetPanel.h"
#include "ui/PanelIds.h"
#include "imgui.h"

namespace modular {

PresetPanel::PresetPanel(PresetManager* pm, SceneEngine* se)
    : presetManager_(pm), sceneEngine_(se) {}

void PresetPanel::setVisible(bool v) { visible_ = v; }
bool PresetPanel::isVisible() const { return visible_; }

void PresetPanel::draw() {
    if (!visible_ || !presetManager_ || !sceneEngine_) return;

    if (!ImGui::Begin(panel_ids::kPresets, &visible_)) {
        ImGui::End();
        return;
    }

    // Get loaded scenes
    auto sceneIds = sceneEngine_->getLoadedScenes();
    if (sceneIds.empty()) {
        ImGui::TextDisabled("No scenes loaded");
        ImGui::End();
        return;
    }

    // Scene selector combo
    if (selectedSceneIndex_ >= (int)sceneIds.size()) selectedSceneIndex_ = 0;
    if (ImGui::BeginCombo("Scene", sceneIds[selectedSceneIndex_].c_str())) {
        for (int i = 0; i < (int)sceneIds.size(); i++) {
            bool isSelected = (selectedSceneIndex_ == i);
            if (ImGui::Selectable(sceneIds[i].c_str(), isSelected))
                selectedSceneIndex_ = i;
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const std::string& sceneId = sceneIds[selectedSceneIndex_];
    auto presets = presetManager_->getPresets(sceneId);
    auto scene = sceneEngine_->getScene(sceneId);

    if (!scene) {
        ImGui::TextDisabled("Scene not available");
        ImGui::End();
        return;
    }

    // ── Morph duration ──────────────────────────────────────────────────────
    ImGui::SliderFloat("Morph (s)", &morphDuration_, 0.0f, 10.0f, "%.1f");

    ImGui::Separator();

    // ── Save Current as Preset ───────────────────────────────────────────────
    ImGui::TextUnformatted("Save Current as:");
    ImGui::InputText("##newName", newPresetName_, sizeof(newPresetName_));
    ImGui::SameLine();
    if (ImGui::Button("Save") && strlen(newPresetName_) > 0) {
        auto captured = presetManager_->captureCurrentParams(scene, newPresetName_);
        presetManager_->addOrUpdatePreset(sceneId, captured);
        presetManager_->savePresets(sceneId);
        newPresetName_[0] = '\0';
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(overwrites if exists)");

    ImGui::Separator();

    // ── Preset list ──────────────────────────────────────────────────────────
    if (presets.empty()) {
        ImGui::TextDisabled("No presets — type a name and click Save above");
    }

    for (size_t i = 0; i < presets.size(); i++) {
        auto& preset = presets[i];  // mutable — per-preset controls write to this
        ImGui::PushID(preset.name.c_str());

        // --- Preset row: Apply | Name (active?) | Update | Del ---
        
        // Apply button — prominent, green
        bool isActive = (activePresetPerScene_[sceneId] == preset.name);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.75f, 0.25f, 1.0f));
        float applyDuration = (preset.morphDuration > 0.0f) ? 0.0f : morphDuration_;
        if (ImGui::Button("Apply")) {
            presetManager_->applyPreset(scene, preset.name, applyDuration);
            activePresetPerScene_[sceneId] = preset.name;
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();

        // Preset name + active badge
        ImGui::AlignTextToFramePadding();
        if (isActive) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s  (active)", preset.name.c_str());
        } else {
            ImGui::TextUnformatted(preset.name.c_str());
        }

        // Morph progress bar
        float progress = presetManager_->getMorphProgress(sceneId, preset.name);
        if (progress > 0.0f && progress < 1.0f) {
            ImGui::SameLine();
            ImGui::ProgressBar(progress, ImVec2(80, 0), "");
        }

        // Tooltip with params
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            for (const auto& [key, val] : preset.params) {
                ImGui::Text("%s: %.2f", key.c_str(), val);
            }
            ImGui::EndTooltip();
        }

        // Right-aligned: Dup | Rnm | Update | Del
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 210);
        if (ImGui::SmallButton("Dup")) {
            PresetDef copy = preset;
            // Generate unique name: "suspiro" → "suspiro (copy)" → "suspiro (copy 2)" ...
            std::string baseName = preset.name + " (copy)";
            std::string candidate = baseName;
            int suffix = 1;
            bool conflict = true;
            while (conflict) {
                conflict = false;
                for (const auto& p : presets) {
                    if (p.name == candidate) {
                        conflict = true;
                        suffix++;
                        candidate = baseName + " " + std::to_string(suffix);
                        break;
                    }
                }
            }
            copy.name = candidate;
            // midiMappings already copied via struct copy above
            presetManager_->addOrUpdatePreset(sceneId, copy);
            presetManager_->savePresets(sceneId);
            ofLogNotice("PresetPanel") << "Duplicated preset '" << preset.name
                                       << "' as '" << candidate << "'";
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Rnm")) {
            renamePendingIdx_ = static_cast<int>(i);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Update")) {
            auto captured = presetManager_->captureCurrentParams(scene, preset.name);
            captured.kinectMappings = preset.kinectMappings;
            captured.midiMappings = preset.midiMappings;        // preserve MIDI mappings
            captured.morphDuration = preset.morphDuration;
            captured.morphEasing = preset.morphEasing;
            presetManager_->addOrUpdatePreset(sceneId, captured);
            presetManager_->savePresets(sceneId);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Del")) {
            deletePendingIdx_ = static_cast<int>(i);
        }

        // --- Per-Preset Morph Controls ---
        float morphDur = preset.morphDuration;
        ImGui::SliderFloat("Duration", &morphDur, 0.0f, 10.0f, "%.1fs");
        if (morphDur != preset.morphDuration) {
            preset.morphDuration = morphDur;
            presetManager_->addOrUpdatePreset(sceneId, preset);
            presetManager_->savePresets(sceneId);
        }

        int easingIdx = static_cast<int>(preset.morphEasing);
        const char* easingLabels[] = {"Linear", "Ease In", "Ease Out", "Ease In/Out"};
        ImGui::SetNextItemWidth(180);
        if (ImGui::Combo("Easing", &easingIdx, easingLabels, 4)) {
            preset.morphEasing = static_cast<MorphEasing>(easingIdx);
            presetManager_->addOrUpdatePreset(sceneId, preset);
            presetManager_->savePresets(sceneId);
        }

        ImGui::Separator();

        // Handle delete with confirmation
        if (static_cast<int>(i) == deletePendingIdx_) {
            ImGui::OpenPopup("Delete Preset");
        }
        if (ImGui::BeginPopupModal("Delete Preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Delete preset '%s' from '%s'?", preset.name.c_str(), sceneId.c_str());
            ImGui::Separator();
            if (ImGui::Button("Yes, delete")) {
                presetManager_->deletePreset(sceneId, preset.name);
                presetManager_->savePresets(sceneId);
                deletePendingIdx_ = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                deletePendingIdx_ = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Handle rename with text input
        if (static_cast<int>(i) == renamePendingIdx_) {
            ImGui::OpenPopup("Rename Preset");
        }
        if (ImGui::BeginPopupModal("Rename Preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Rename preset '%s':", preset.name.c_str());
            static char newName[64] = "";
            // Pre-fill with current name when popup opens
            snprintf(newName, sizeof(newName), "%s", preset.name.c_str());
            ImGui::InputText("##renameInput", newName, sizeof(newName));
            ImGui::Separator();

            bool nameValid = strlen(newName) > 0;
            bool nameConflict = false;
            if (nameValid) {
                for (const auto& p : presets) {
                    if (&p != &preset && p.name == newName) {
                        nameConflict = true;
                        break;
                    }
                }
            }

            if (!nameValid) {
                ImGui::TextDisabled("Name cannot be empty");
            } else if (nameConflict) {
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Name already exists");
            }

            if (ImGui::Button("Rename") && nameValid && !nameConflict) {
                preset.name = newName;
                presetManager_->addOrUpdatePreset(sceneId, preset);
                presetManager_->savePresets(sceneId);
                // If this was the active preset, update tracking
                if (activePresetPerScene_[sceneId] == newName || activePresetPerScene_[sceneId] == preset.name) {
                    activePresetPerScene_[sceneId] = newName;
                }
                renamePendingIdx_ = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                renamePendingIdx_ = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    ImGui::End();
}

} // namespace modular
