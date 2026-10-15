#include "ui/OscMappingPanel.h"
#include "mapping/OscMappingEngine.h"
#include "ui/PanelIds.h"
#include "engine/SceneEngine.h"
#include "scenes/BaseScene.h"

#include "imgui.h"
#include "imgui_stdlib.h"
#include <ofLog.h>
#include <algorithm>

namespace modular {

OscMappingPanel::OscMappingPanel(OscMappingEngine* engine, SceneEngine* sceneEngine)
    : engine_(engine)
    , sceneEngine_(sceneEngine)
    , visible_(true)
    , selectedRow_(-1)
{
    memset(newOscAddrBuf_, 0, sizeof(newOscAddrBuf_));
    memset(newTargetParamBuf_, 0, sizeof(newTargetParamBuf_));
}

void OscMappingPanel::update(float /*deltaTime*/) {
    // No per-frame state needed — engine handles its own update
}

void OscMappingPanel::draw() {
    ImGui::SetNextWindowSize(ImVec2(900, 500), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(panel_ids::kOscMapping, &visible_)) {
        ImGui::End();
        return;
    }

    drawLearningIndicator();
    ImGui::Separator();
    drawToolbar();
    ImGui::Separator();

    // Two-column master-detail layout: list (40%) | editor (60%)
    float totalWidth = ImGui::GetContentRegionAvail().x;
    ImGui::Columns(2, "oscMappingCols", true);
    ImGui::SetColumnWidth(0, totalWidth * 0.40f);

    drawMappingList();

    ImGui::NextColumn();

    drawMappingEditor();

    ImGui::Columns(1);

    ImGui::End();
}

void OscMappingPanel::drawLearningIndicator() {
    auto state = engine_->getLearnState();
    if (state == LearnState::LEARNING) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); // Yellow/orange
        float remaining = engine_->getLearnTimeRemaining();
        ImGui::Text("● LISTENING for: %s (%.1fs)", engine_->getLearningTarget().c_str(), remaining);
        ImGui::PopStyleColor();

        ImGui::SameLine();
        if (ImGui::SmallButton("Cancel")) {
            engine_->cancelLearning();
        }
    } else if (state == LearnState::TIMEOUT) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f)); // Red
        ImGui::Text("✗ TIMEOUT — no OSC message received");
        ImGui::PopStyleColor();
    } else {
        ImGui::TextDisabled("Ready — click [MAP] next to a parameter to start learning");
    }
}

void OscMappingPanel::drawToolbar() {
    if (ImGui::Button("+ Add")) {
        OscMapping mapping;
        mapping.oscAddress = "/new/address";
        mapping.targetParameterId = "scene.param";
        engine_->addMapping(mapping);
    }
    ImGui::SameLine();

    if (ImGui::Button("- Delete") && selectedRow_ >= 0) {
        auto& mappings = engine_->getMappings();
        if (selectedRow_ < (int)mappings.size()) {
            engine_->removeMapping(mappings[selectedRow_].mappingId);
            selectedRow_ = -1;
        }
    }
    ImGui::SameLine();

    if (ImGui::Button("Save")) {
        if (engine_->saveToJson("config/osc-mappings.json")) {
            ofLogNotice("OscMappingPanel") << "Mappings saved";
        }
    }
    ImGui::SameLine();

    if (ImGui::Button("Load")) {
        engine_->loadFromJson("config/osc-mappings.json");
    }

    ImGui::SameLine();
    filter_.Draw("Filter", 150);
}

void OscMappingPanel::drawMappingList() {
    auto& mappings = engine_->getMappings();
    bool isLearning = (engine_->getLearnState() == LearnState::LEARNING);
    std::string learningTarget = engine_->getLearningTarget();

    if (!ImGui::BeginTable("OscMappingsList", 5,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        return;
    }

    ImGui::TableSetupColumn("OSC Address", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 25);
    ImGui::TableSetupColumn("MAP", ImGuiTableColumnFlags_WidthFixed, 55);
    ImGui::TableSetupColumn("Del", ImGuiTableColumnFlags_WidthFixed, 25);
    ImGui::TableHeadersRow();

    for (int i = 0; i < (int)mappings.size(); i++) {
        auto& mapping = mappings[i];

        // Apply filter
        if (filter_.IsActive()) {
            if (!filter_.PassFilter(mapping.oscAddress.c_str()) &&
                !filter_.PassFilter(mapping.targetParameterId.c_str())) {
                continue;
            }
        }

        ImGui::TableNextRow();

        // Highlight selected row
        if (selectedRow_ == i) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                ImGui::GetColorU32(ImVec4(0.3f, 0.5f, 0.8f, 0.4f)));
        }

        // Check if this mapping points to the active scene
        bool matchesActiveScene = false;
        {
            auto activeScene = sceneEngine_->getActiveScene();
            if (activeScene && !mapping.targetParameterId.empty()) {
                size_t dotPos = mapping.targetParameterId.find('.');
                if (dotPos != std::string::npos) {
                    std::string mappedSceneId = mapping.targetParameterId.substr(0, dotPos);
                    matchesActiveScene = (mappedSceneId == activeScene->getId());
                }
            }
        }

        ImGui::PushID(mapping.mappingId.c_str());

        // OSC Address (column 0) — editable
        ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemWidth(-1);
        char addrBuf[128];
        strncpy(addrBuf, mapping.oscAddress.c_str(), sizeof(addrBuf) - 1);
        addrBuf[sizeof(addrBuf) - 1] = '\0';
        if (ImGui::InputText("##addr", addrBuf, sizeof(addrBuf))) {
            mapping.oscAddress = addrBuf;
        }
        if (ImGui::IsItemClicked()) {
            selectedRow_ = i;
        }

        // Target (column 1) — compact combo
        ImGui::TableSetColumnIndex(1);

        if (!matchesActiveScene && !mapping.targetParameterId.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.5f, 0.2f, 0.3f));
        }

        ImGui::SetNextItemWidth(-1);
        std::string comboPreview = mapping.targetParameterId.empty() ? "(none)" : mapping.targetParameterId;
        if (ImGui::BeginCombo("##targetCombo", comboPreview.c_str(), ImGuiComboFlags_None)) {
            selectedRow_ = i;
            auto activeScene = sceneEngine_->getActiveScene();
            if (activeScene) {
                auto paramIds = activeScene->getParameterIds();
                std::string sceneId = activeScene->getId();

                for (const auto& paramId : paramIds) {
                    std::string fullId = sceneId + "." + paramId;
                    bool isSel = (mapping.targetParameterId == fullId);

                    if (ImGui::Selectable(paramId.c_str(), isSel)) {
                        mapping.targetParameterId = fullId;
                        auto param = activeScene->getParameter(paramId);
                        if (param) {
                            mapping.minOutput = param->min;
                            mapping.maxOutput = param->max;
                        }
                        ofLogNotice("OscMappingPanel") << "Target param: " << fullId
                            << " (output range: " << mapping.minOutput << " - " << mapping.maxOutput << ")";
                    }

                    if (isSel) {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                if (paramIds.empty()) {
                    ImGui::TextDisabled("No parameters");
                }
            } else {
                ImGui::TextDisabled("No active scene");
            }
            ImGui::EndCombo();
        }

        if (!matchesActiveScene && !mapping.targetParameterId.empty()) {
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("⚠ Target scene is not the active scene — param may not receive values");
            }
            ImGui::PopStyleColor(2);
        }

        // Enabled (column 2)
        ImGui::TableSetColumnIndex(2);
        if (ImGui::Checkbox("##enabled", &mapping.enabled)) {
            selectedRow_ = i;
        }

        // MAP button (column 3)
        ImGui::TableSetColumnIndex(3);
        bool isThisLearning = isLearning && (learningTarget == mapping.targetParameterId);
        if (isThisLearning) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.6f, 0.0f, 1.0f));
            ImGui::Button("Listening...");
            ImGui::PopStyleColor();
        } else {
            ImGui::BeginDisabled(isLearning);
            if (ImGui::Button("MAP")) {
                selectedRow_ = i;
                engine_->startLearning(mapping.targetParameterId);
            }
            ImGui::EndDisabled();
        }

        // Delete button (column 4)
        ImGui::TableSetColumnIndex(4);
        if (isThisLearning) {
            ImGui::BeginDisabled(true);
        }
        if (ImGui::Button("X")) {
            engine_->removeMapping(mapping.mappingId);
            if (selectedRow_ == i) {
                selectedRow_ = -1;
            } else if (selectedRow_ > i) {
                selectedRow_--;
            }
        }
        if (isThisLearning) {
            ImGui::EndDisabled();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Delete this mapping");
        }

        ImGui::PopID();
    }

    ImGui::EndTable();

    if (mappings.empty()) {
        ImGui::TextDisabled("No mappings yet. Click '+ Add' to create one.");
    }
}

void OscMappingPanel::drawMappingEditor() {
    auto& mappings = engine_->getMappings();

    ImGui::BeginChild("MappingEditor", ImVec2(0, 0), true);

    if (selectedRow_ >= 0 && selectedRow_ < (int)mappings.size()) {
        auto& mapping = mappings[selectedRow_];

        // OSC Address
        ImGui::Text("OSC Address:");
        ImGui::SetNextItemWidth(-1);
        char addrBuf[128];
        strncpy(addrBuf, mapping.oscAddress.c_str(), sizeof(addrBuf) - 1);
        addrBuf[sizeof(addrBuf) - 1] = '\0';
        if (ImGui::InputText("##editorAddr", addrBuf, sizeof(addrBuf))) {
            mapping.oscAddress = addrBuf;
        }

        ImGui::Spacing();

        // Target Parameter
        ImGui::Text("Target:");
        ImGui::SetNextItemWidth(-1);

        // Color hint for mismatched scene
        bool matchesActiveScene = false;
        {
            auto activeScene = sceneEngine_->getActiveScene();
            if (activeScene && !mapping.targetParameterId.empty()) {
                size_t dotPos = mapping.targetParameterId.find('.');
                if (dotPos != std::string::npos) {
                    std::string mappedSceneId = mapping.targetParameterId.substr(0, dotPos);
                    matchesActiveScene = (mappedSceneId == activeScene->getId());
                }
            }
        }

        if (!matchesActiveScene && !mapping.targetParameterId.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.5f, 0.2f, 0.3f));
        }

        std::string comboPreview = mapping.targetParameterId.empty() ? "(none)" : mapping.targetParameterId;
        if (ImGui::BeginCombo("##editorTargetCombo", comboPreview.c_str(), ImGuiComboFlags_None)) {
            auto activeScene = sceneEngine_->getActiveScene();
            if (activeScene) {
                auto paramIds = activeScene->getParameterIds();
                std::string sceneId = activeScene->getId();

                for (const auto& paramId : paramIds) {
                    std::string fullId = sceneId + "." + paramId;
                    bool isSel = (mapping.targetParameterId == fullId);

                    if (ImGui::Selectable(paramId.c_str(), isSel)) {
                        mapping.targetParameterId = fullId;
                        auto param = activeScene->getParameter(paramId);
                        if (param) {
                            mapping.minOutput = param->min;
                            mapping.maxOutput = param->max;
                        }
                        ofLogNotice("OscMappingPanel") << "Target param: " << fullId
                            << " (output range: " << mapping.minOutput << " - " << mapping.maxOutput << ")";
                    }

                    if (isSel) {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                if (paramIds.empty()) {
                    ImGui::TextDisabled("No parameters");
                }
            } else {
                ImGui::TextDisabled("No active scene");
            }
            ImGui::EndCombo();
        }

        if (!matchesActiveScene && !mapping.targetParameterId.empty()) {
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("⚠ Target scene is not the active scene — param may not receive values");
            }
            ImGui::PopStyleColor(2);
        }

        ImGui::Separator();

        // Input Range
        ImGui::Text("Input Range");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
        ImGui::DragFloat("##editorMinIn", &mapping.minInput, 0.01f, -1000.0f, 1000.0f, "Min %.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::DragFloat("##editorMaxIn", &mapping.maxInput, 0.01f, -1000.0f, 1000.0f, "Max %.2f");

        ImGui::Separator();

        // Output Range
        ImGui::Text("Output Range");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
        ImGui::DragFloat("##editorMinOut", &mapping.minOutput, 0.01f, -1000.0f, 1000.0f, "Min %.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::DragFloat("##editorMaxOut", &mapping.maxOutput, 0.01f, -1000.0f, 1000.0f, "Max %.2f");

        ImGui::Separator();

        // Transform Type
        ImGui::Text("Transform:");
        ImGui::SetNextItemWidth(-1);
        const char* transformTypes[] = { "↗ Linear", "^ Exp", "log Log", "↓ Invert"};
        int currentTransform = static_cast<int>(mapping.transformType);
        if (ImGui::Combo("##editorTransform", &currentTransform, transformTypes, 4)) {
            mapping.transformType = static_cast<OscTransformType>(currentTransform);
        }

        ImGui::Spacing();

        // Frequency Mode
        ImGui::Text("Frequency:");
        ImGui::SetNextItemWidth(-1);
        const char* freqModes[] = {"Every Frame", "On Change", "Throttled"};
        int currentFreq = static_cast<int>(mapping.frequencyMode);
        if (ImGui::Combo("##editorFreq", &currentFreq, freqModes, 3)) {
            mapping.frequencyMode = static_cast<OscFrequencyMode>(currentFreq);
        }

        ImGui::Spacing();

        // Enabled
        ImGui::Checkbox("Enabled", &mapping.enabled);

    } else {
        // Centered placeholder text
        ImVec2 childSize = ImGui::GetContentRegionAvail();
        ImVec2 textSize = ImGui::CalcTextSize("Select a mapping from the list to edit");
        ImGui::SetCursorPos(ImVec2(
            (childSize.x - textSize.x) * 0.5f,
            (childSize.y - textSize.y) * 0.5f
        ));
        ImGui::TextDisabled("Select a mapping from the list to edit");
    }

    ImGui::EndChild();
}

void OscMappingPanel::setVisible(bool visible) {
    visible_ = visible;
}

bool OscMappingPanel::isVisible() const {
    return visible_;
}

} // namespace modular
