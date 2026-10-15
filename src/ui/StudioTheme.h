#pragma once

#include "imgui.h"

namespace modular {

struct StudioFonts {
    ImFont* regular = nullptr;
    ImFont* bold = nullptr;
};

StudioFonts loadStudioFonts(ImGuiIO& io);
void applyStudioTheme();
void setWindowAlpha(float bgAlpha, float controlAlpha);

} // namespace modular
