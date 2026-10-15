#include "ui/SequencerPanel.h"
#include "ui/PanelIds.h"
#include "imgui.h"

namespace modular {

SequencerPanel::SequencerPanel(SceneSequencer* seq) : sequencer_(seq) {}

void SequencerPanel::setVisible(bool v) { visible_ = v; }
bool SequencerPanel::isVisible() const { return visible_; }

void SequencerPanel::draw() {
    if (!visible_ || !sequencer_) return;

    if (!ImGui::Begin(panel_ids::kSequencer, &visible_)) {
        ImGui::End();
        return;
    }

    if (!sequencer_->hasSequence()) {
        ImGui::TextDisabled("No sequence loaded");
        ImGui::End();
        return;
    }

    int cueCount = sequencer_->getCueCount();
    int currentIdx = sequencer_->getCurrentCueIndex();

    // Navigation buttons
    if (ImGui::Button("< Prev")) sequencer_->triggerPrev();
    ImGui::SameLine();
    ImGui::Text("Cue %d / %d", currentIdx >= 0 ? currentIdx + 1 : 1, cueCount);
    ImGui::SameLine();
    if (ImGui::Button("Next >")) sequencer_->triggerNext();

    ImGui::Separator();

    // Cue table
    if (ImGui::BeginTable("CueTable", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::TableSetupColumn("Scene", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Preset", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Morph", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableHeadersRow();

        for (int i = 0; i < cueCount; i++) {
            const auto* cue = sequencer_->getCue(i);
            if (!cue) continue;

            ImGui::TableNextRow();

            bool isCurrent = (i == currentIdx);
            if (isCurrent) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                    ImGui::GetColorU32(ImVec4(0.2f, 0.5f, 0.2f, 0.5f)));
            }

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", cue->cue);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(cue->sceneId.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(cue->presetName.c_str());

            ImGui::TableSetColumnIndex(3);
            if (cue->morphDuration > 0.0f) {
                ImGui::Text("%.1fs", cue->morphDuration);
            }

            // Click row to trigger
            ImGui::PushID(i);
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
                sequencer_->triggerCue(cue->cue);
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace modular
