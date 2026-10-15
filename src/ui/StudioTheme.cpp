#include "ui/StudioTheme.h"

#include "ofMain.h"

#include <string>

namespace {

constexpr float kBaseFontSize = 18.0f;
constexpr float kHeadingFontSize = 20.0f;
constexpr char kRobotoRegularPath[] = "fonts/Roboto-Regular.ttf";
constexpr char kRobotoBoldPath[] = "fonts/Roboto-Bold.ttf";

ImFont* LoadFontFromDataPath(const std::string& relativePath, float size, ImGuiIO& io) {
    const std::string absolutePath = ofToDataPath(relativePath, true);
    if (!ofFile::doesFileExist(absolutePath)) {
        ofLogWarning("UIManager") << "Font not found at " << relativePath
                                   << ". Placeholder file detected — TODO: replace with actual Roboto asset.";
        return nullptr;
    }

    ImFont* font = io.Fonts->AddFontFromFileTTF(absolutePath.c_str(), size);
    if (!font) {
        ofLogWarning("UIManager") << "Failed to load font " << relativePath;
    }
    return font;
}

} // namespace

namespace modular {

StudioFonts loadStudioFonts(ImGuiIO& io) {
    StudioFonts fonts;
    fonts.regular = LoadFontFromDataPath(kRobotoRegularPath, kBaseFontSize, io);
    fonts.bold = LoadFontFromDataPath(kRobotoBoldPath, kHeadingFontSize, io);

    if (fonts.regular != nullptr) {
        io.FontDefault = fonts.regular;
    }

    return fonts;
}

void applyStudioTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);

    // Default: opaque windows. Call setWindowAlpha(0.65f) after this to make panels see-through
    setWindowAlpha(1.0f, 1.0f);
}

void setWindowAlpha(float bgAlpha, float controlAlpha) {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // Text - keep visible
    colors[ImGuiCol_Text] = ImVec4(0.92f, 0.95f, 0.98f, controlAlpha);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.47f, 0.52f, 0.58f, controlAlpha);
    
    // Backgrounds - transparent
    colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.08f, 0.11f, bgAlpha);
    colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.10f, 0.14f, bgAlpha);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.11f, 0.15f, bgAlpha);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.05f, 0.05f, 0.07f, bgAlpha);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.09f, 0.12f, bgAlpha);
    colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.07f, bgAlpha);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.11f, 0.13f, 0.18f, bgAlpha);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.03f, bgAlpha);
    
    // Controls - keep visible
    colors[ImGuiCol_Border] = ImVec4(0.15f, 0.17f, 0.24f, controlAlpha);
    colors[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.15f, 0.21f, controlAlpha);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.26f, 0.42f, controlAlpha);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.36f, 0.58f, controlAlpha);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.22f, 0.27f, controlAlpha);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.27f, 0.30f, 0.36f, controlAlpha);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.32f, 0.36f, 0.44f, controlAlpha);
    colors[ImGuiCol_CheckMark] = ImVec4(0.34f, 0.70f, 0.85f, controlAlpha);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.70f, 0.85f, controlAlpha);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.41f, 0.80f, 0.93f, controlAlpha);
    colors[ImGuiCol_Button] = ImVec4(0.16f, 0.33f, 0.52f, controlAlpha);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.44f, 0.70f, controlAlpha);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.25f, 0.50f, 0.80f, controlAlpha);
    colors[ImGuiCol_Header] = ImVec4(0.14f, 0.28f, 0.43f, controlAlpha);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.19f, 0.36f, 0.57f, controlAlpha);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.50f, 0.75f, controlAlpha);
    colors[ImGuiCol_Separator] = ImVec4(0.16f, 0.18f, 0.24f, controlAlpha);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.30f, 0.50f, 0.80f, controlAlpha);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.30f, 0.50f, 0.80f, controlAlpha);
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.22f, 0.32f, controlAlpha);
    colors[ImGuiCol_TabHovered] = ImVec4(0.17f, 0.34f, 0.52f, controlAlpha);
    colors[ImGuiCol_TabSelected] = ImVec4(0.20f, 0.39f, 0.60f, controlAlpha);
    colors[ImGuiCol_TabDimmed] = ImVec4(0.10f, 0.18f, 0.27f, controlAlpha);
    colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.16f, 0.30f, 0.45f, controlAlpha);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.34f, 0.70f, 0.85f, controlAlpha * 0.4f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.13f, 0.14f, 0.20f, controlAlpha);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.19f, 0.21f, 0.29f, controlAlpha);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.11f, 0.12f, 0.18f, controlAlpha);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.34f, 0.70f, 0.85f, controlAlpha);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.80f, 0.80f, 0.80f, controlAlpha * 0.7f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, controlAlpha);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.0f, 0.45f, 0.00f, controlAlpha);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.08f, 0.08f, 0.12f, bgAlpha * 0.88f);
}

} // namespace modular
