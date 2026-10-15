#include "ui/TransportPanel.h"

#include "imgui.h"
#include "ui/PanelIds.h"

#include <cstdio>

namespace modular {

namespace {
constexpr float kProgressBarHeight = 16.0f;
}

TransportPanel::TransportPanel(TransportController* transport)
    : transport_(transport)
    , visible_(true) {}

void TransportPanel::setVisible(bool visible) {
    visible_ = visible;
}

bool TransportPanel::isVisible() const {
    return visible_;
}

void TransportPanel::draw() {
    if (!visible_ || !transport_) {
        return;
    }

    if (!ImGui::Begin(panel_ids::kTransport, &visible_)) {
        ImGui::End();
        return;
    }

    drawPlaybackButtons();
    drawTimeline();

    ImGui::End();
}

void TransportPanel::drawPlaybackButtons() {
    const bool isOnline = transport_->isOnline();
    const bool isPlaying = transport_->isPlaying();

    if (!isOnline) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Transport offline");
    }

    ImGui::BeginDisabled(!isOnline);

    if (ImGui::Button(isPlaying ? "Pause" : "Play")) {
        if (isPlaying) {
            transport_->pause();
        } else {
            transport_->play();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        transport_->stop();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Phase")) {
        transport_->resetPhase();
    }

    float bpm = transport_->getBpm();
    if (ImGui::DragFloat("BPM", &bpm, 0.1f, 30.0f, 250.0f, "%.1f")) {
        transport_->setBpm(bpm);
    }

    ImGui::EndDisabled();
}

void TransportPanel::drawTimeline() {
    const float normalized = transport_->getNormalizedPhase();
    char overlay[32];
    std::snprintf(overlay, sizeof(overlay), "%.1f beats", transport_->getBeatsElapsed());
    ImGui::ProgressBar(normalized, ImVec2(-1.0f, kProgressBarHeight), overlay);
}

} // namespace modular
