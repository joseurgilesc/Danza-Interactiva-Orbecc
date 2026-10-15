#include "ui/InputPortsPanel.h"

#include "ui/PanelIds.h"

namespace modular {

InputPortsPanel::InputPortsPanel(MidiPortsStore* store)
    : store_(store)
    , visible_(true) {}

void InputPortsPanel::setVisible(bool visible) {
    visible_ = visible;
}

bool InputPortsPanel::isVisible() const {
    return visible_;
}

void InputPortsPanel::draw() {
    if (!visible_) {
        return;
    }

    if (!ImGui::Begin(panel_ids::kInputPorts, &visible_)) {
        ImGui::End();
        return;
    }

    drawPortsTable();

    ImGui::End();
}

void InputPortsPanel::drawPortsTable() {
    if (!store_) {
        ImGui::TextDisabled("No hay MidiPortsStore configurado.");
        return;
    }

    const auto& ports = store_->getPorts();
    if (ports.empty()) {
        ImGui::TextDisabled("No se detectaron puertos MIDI.");
        return;
    }

    if (ImGui::BeginTable("InputPortsTable", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Nombre", ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableSetupColumn("Dispositivo", ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableSetupColumn("Canal", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Activo", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Learn", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableHeadersRow();

        for (const auto& port : ports) {
            ImGui::TableNextRow();
            ImGui::PushID(port.id.c_str());

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(port.name.c_str());
            if (port.isVirtual) {
                ImGui::SameLine();
                ImGui::TextDisabled("(Virtual)");
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(port.device.c_str());

            ImGui::TableSetColumnIndex(2);
            int channel = port.channel;
            if (ImGui::InputInt("##channel", &channel, 1, 0)) {
                store_->setChannel(port.id, channel);
            }

            ImGui::TableSetColumnIndex(3);
            bool enabled = port.enabled;
            if (ImGui::Checkbox("##enabled", &enabled)) {
                store_->setEnabled(port.id, enabled);
            }

            ImGui::TableSetColumnIndex(4);
            bool learning = port.learning;
            if (ImGui::Checkbox("##learning", &learning)) {
                store_->setLearning(port.id, learning);
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

} // namespace modular
