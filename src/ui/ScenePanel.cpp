#include "ui/ScenePanel.h"

#include "imgui.h"
#include "ui/PanelIds.h"

namespace modular {

ScenePanel::ScenePanel(SceneEngine* sceneEngine)
    : sceneEngine_(sceneEngine)
    , visible_(true) {}

void ScenePanel::setVisible(bool visible) {
    visible_ = visible;
}

bool ScenePanel::isVisible() const {
    return visible_;
}

void ScenePanel::draw() {
    if (!visible_) {
        return;
    }

    if (!sceneEngine_) {
        return;
    }

    if (!ImGui::Begin(panel_ids::kScenes, &visible_)) {
        ImGui::End();
        return;
    }

    drawSceneTable();

    ImGui::End();
}

void ScenePanel::drawSceneTable() {
    std::vector<std::string> scenes = sceneEngine_->getLoadedScenes();
    if (scenes.empty()) {
        ImGui::TextDisabled("No hay escenas cargadas. Usa SceneEngine::loadScene para registrar escenas.");
        return;
    }

    const std::string activeSceneId = sceneEngine_->getActiveSceneId();

    if (ImGui::BeginTable("ScenePanelTable", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuterH)) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("Estado", ImGuiTableColumnFlags_WidthStretch, 0.4f);
        ImGui::TableHeadersRow();

        for (const auto& sceneId : scenes) {
            ImGui::TableNextRow();
            const bool isActive = (sceneId == activeSceneId);

            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(sceneId.c_str());
            if (ImGui::Selectable(sceneId.c_str(), isActive, ImGuiSelectableFlags_SpanAllColumns)) {
                if (!isActive) {
                    sceneEngine_->transitionTo(sceneId);
                }
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(1);
            if (isActive) {
                ImGui::TextUnformatted("Activa");
            } else {
                ImGui::TextDisabled("Inactiva");
            }
        }
        ImGui::EndTable();
    }
}

} // namespace modular
