#include "ui/MappingPanel.h"

#include "ui/PanelIds.h"

namespace modular {

MappingPanel::MappingPanel(MappingLayer* mappingLayer)
    : mappingLayer_(mappingLayer)
    , visible_(true) {}

void MappingPanel::setVisible(bool visible) {
    visible_ = visible;
}

bool MappingPanel::isVisible() const {
    return visible_;
}

void MappingPanel::draw() {
    if (!visible_) {
        return;
    }

    if (!ImGui::Begin(panel_ids::kMapping, &visible_)) {
        ImGui::End();
        return;
    }

    filter_.Draw("Filtrar", 200.0f);
    drawMappingTable();

    ImGui::End();
}

void MappingPanel::drawMappingTable() {
    if (!mappingLayer_) {
        ImGui::TextDisabled("No hay MappingLayer configurado.");
        return;
    }

    const auto& mappings = mappingLayer_->getAllMappings();
    if (mappings.empty()) {
        ImGui::TextDisabled("No hay mapeos registrados.");
        return;
    }

    if (ImGui::BeginTable("MappingTable", 6, ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Fuente", ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableSetupColumn("Dispositivo", ImGuiTableColumnFlags_WidthStretch, 0.2f);
        ImGui::TableSetupColumn("Canal", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Destino", ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableSetupColumn("Transform", ImGuiTableColumnFlags_WidthStretch, 0.2f);
        ImGui::TableSetupColumn("Activo", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableHeadersRow();

        for (const auto& mapping : mappings) {
            const std::string rowLabel = mapping.sourceSceneId + "." + mapping.sourceParameterId;
            if (!filter_.PassFilter(rowLabel.c_str()) && !filter_.PassFilter(mapping.targetParameterId.c_str())) {
                continue;
            }

            ImGui::TableNextRow();
            ImGui::PushID(mapping.bindingId.c_str());

            const bool isSelected = (selectedId_ == mapping.bindingId);
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(rowLabel.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedId_ = mapping.bindingId;
            }

            ImGui::TableSetColumnIndex(1);
            std::string device = mapping.inputDevice;
            if (isSelected && ImGui::InputText("##device", &device)) {
                mappingLayer_->setMappingDevice(mapping.bindingId, device);
            } else {
                ImGui::TextUnformatted(device.empty() ? "—" : device.c_str());
            }

            ImGui::TableSetColumnIndex(2);
            std::string channel = mapping.sourceChannel;
            if (isSelected && ImGui::InputText("##channel", &channel)) {
                mappingLayer_->setMappingChannel(mapping.bindingId, channel);
            } else {
                ImGui::TextUnformatted(channel.empty() ? "—" : channel.c_str());
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s.%s", mapping.targetSceneId.c_str(), mapping.targetParameterId.c_str());

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%d", static_cast<int>(mapping.transform.type));

            ImGui::TableSetColumnIndex(5);
            bool enabled = mapping.enabled;
            if (ImGui::Checkbox("##enabled", &enabled)) {
                mappingLayer_->setMappingEnabled(mapping.bindingId, enabled);
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

} // namespace modular
