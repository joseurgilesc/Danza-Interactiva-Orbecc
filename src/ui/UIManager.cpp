#include "ui/UIManager.h"

#include "imgui_internal.h"

#include <utility>

namespace {
constexpr char kDockspaceWindowName[] = "StudioDockspaceHost";
constexpr char kDockspaceIdName[] = "StudioDockspace";
}

namespace modular {

UIManager::UIManager()
    : initialized_(false)
    , visible_(true)
    , layoutInitialized_(false)
    , wantsMouseCapture_(false)
    , wantsKeyboardCapture_(false)
    , dockspaceId_(0)
    , scenesDockId_(0)
    , paramsDockId_(0)
    , transportDockId_(0)
    , mappingDockId_(0)
    , inputDockId_(0)
    , oscMappingDockId_(0)
    , kinectDockId_(0)
    , presetsDockId_(0)
    , sequencerDockId_(0)
    , cueEditorDockId_(0)
    , midiMappingDockId_(0)
    , windowAlphaTransparent_(true) {}

void UIManager::setup() {
    if (initialized_) {
        ofLogWarning("UIManager") << "setup() called more than once";
        return;
    }

    // The ImGui context already exists (gui_ member constructor created it).
    // Load fonts and configure BEFORE gui_.setup(), because that call builds
    // and uploads the font atlas texture. Adding fonts afterwards leaves them
    // without baked glyphs → segfault in ImFont::CalcTextSizeA.
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    fonts_ = loadStudioFonts(io);

    gui_.setup(nullptr, /*autoDraw*/ false);

    applyStudioTheme();

    // Compact UI style — smaller elements for dense information
    ImGui::GetStyle().ScaleAllSizes(0.85f);
    io.FontGlobalScale = 0.9f;

    initialized_ = true;

    // Default: transparent UI background
    setWindowAlpha(0.0f, 0.9f);

    for (auto& module : modules_) {
        module->setup();
        module->setVisible(visible_);
    }
}

void UIManager::update(float deltaTime) {
    if (!initialized_) {
        return;
    }

    for (auto& module : modules_) {
        module->update(deltaTime);
    }
}

void UIManager::draw() {
    if (!initialized_) {
        updateCaptureState();
        return;
    }

    if (!visible_) {
        updateCaptureState();
        return;
    }

    gui_.begin();

    drawDockspace();

    for (auto& module : modules_) {
        if (module && module->isVisible()) {
            module->draw();
        }
    }

    drawShortcutsOverlay();

    gui_.end();
    gui_.draw();

    updateCaptureState();
}

void UIManager::drawShortcutsOverlay() {
    // Compact, non-interactive shortcut reference shown only while the control
    // UI is visible. Hiding the UI (U) also hides this overlay → clean stage.
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(12.0f, io.DisplaySize.y - 10.0f), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.55f);

    ImGui::Begin("##shortcuts", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing);

    ImGui::TextUnformatted(
        "U: paneles   T: transparencia   <-/->: escena   1-9: escena   F: pantalla   "
        "M: raton   R: reset   S: guardar   L: cargar   E: esqueleto   V: camara   ,/.: silueta");

    ImGui::End();
}

void UIManager::addModule(std::unique_ptr<IUIModule> module) {
    if (!module) {
        ofLogWarning("UIManager") << "Attempted to register null UI module";
        return;
    }

    if (initialized_) {
        module->setup();
        module->setVisible(visible_);
    }

    modules_.push_back(std::move(module));
}

void UIManager::keyPressed(int /*key*/) {}

void UIManager::keyReleased(int /*key*/) {}

void UIManager::mousePressed(int /*x*/, int /*y*/, int /*button*/) {}

void UIManager::mouseReleased(int /*x*/, int /*y*/, int /*button*/) {}

void UIManager::mouseDragged(int /*x*/, int /*y*/, int /*button*/) {}

void UIManager::mouseMoved(int /*x*/, int /*y*/) {}

void UIManager::toggleGlobalVisible() {
    setGlobalVisible(!visible_);
}

void UIManager::setGlobalVisible(bool visible) {
    visible_ = visible;
    for (auto& module : modules_) {
        module->setVisible(visible_);
    }
}

bool UIManager::isGlobalVisible() const {
    return visible_;
}

void UIManager::toggleWindowAlpha() {
    windowAlphaTransparent_ = !windowAlphaTransparent_;
    // When transparent: background fully transparent, only text/sliders/buttons visible
    float bgAlpha = windowAlphaTransparent_ ? 0.0f : 1.0f;
    float controlAlpha = windowAlphaTransparent_ ? 0.9f : 1.0f;
    setWindowAlpha(bgAlpha, controlAlpha);
    ofLogNotice("UIManager") << "Window alpha: " << (windowAlphaTransparent_ ? "transparent" : "opaque");
}

bool UIManager::isWindowAlphaTransparent() const {
    return windowAlphaTransparent_;
}

bool UIManager::wantsMouseCapture() const {
    return wantsMouseCapture_;
}

bool UIManager::wantsKeyboardCapture() const {
    return wantsKeyboardCapture_;
}

void UIManager::drawDockspace() {
    ImGuiIO& io = ImGui::GetIO();
    if ((io.ConfigFlags & ImGuiConfigFlags_DockingEnable) == 0) {
        return;
    }

    const ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                  ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                  ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground |
                                  ImGuiWindowFlags_MenuBar;

    ImGui::Begin(kDockspaceWindowName, nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    drawMenuBar();

    dockspaceId_ = ImGui::GetID(kDockspaceIdName);
    ImGui::DockSpace(dockspaceId_, ImVec2(0.0f, 0.0f), dockspaceFlags);

    if (!layoutInitialized_) {
        buildDefaultDockLayout();
    }

    ImGui::End();
}

void UIManager::buildDefaultDockLayout() {
    if (dockspaceId_ == 0) {
        return;
    }

    layoutInitialized_ = true;

    ImGui::DockBuilderRemoveNode(dockspaceId_);
    ImGui::DockBuilderAddNode(dockspaceId_, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::DockBuilderSetNodeSize(dockspaceId_, ImGui::GetMainViewport()->Size);

    // Clean layout with tabs:
    // +------------------------------------------+
    // | [View]  Menu Bar                         |
    // +-------------------+----------+-----------+
    // | OSC + Kinect      | Scenes   | Params    |
    // | (left 28%)        | + MIDI   | + Presets |
    // |                   | (center) | + Cues    |
    // +-------------------+----------+-----------+
    // | Transport + Sequencer (bottom)           |
    // +------------------------------------------+

    ImGuiID dockMainId = dockspaceId_;

    // Left dock: OSC Mappings + Kinect (28% width)
    ImGuiID dockLeftId = ImGui::DockBuilderSplitNode(dockMainId, ImGuiDir_Left, 0.28f, nullptr, &dockMainId);

    // Right dock: Parameters + Presets + Cue Editor (22% width)
    ImGuiID dockRightId = ImGui::DockBuilderSplitNode(dockMainId, ImGuiDir_Right, 0.22f, nullptr, &dockMainId);

    // Bottom strip: Transport + Sequencer (20% height)
    ImGuiID dockBottomId = ImGui::DockBuilderSplitNode(dockMainId, ImGuiDir_Down, 0.20f, nullptr, &dockMainId);

    // Center = Scenes + MIDI Mappings
    scenesDockId_      = dockMainId;
    midiMappingDockId_ = dockMainId;

    paramsDockId_      = dockRightId;
    presetsDockId_     = dockRightId;
    cueEditorDockId_   = dockRightId;

    transportDockId_   = dockBottomId;
    sequencerDockId_   = dockBottomId;

    oscMappingDockId_  = dockLeftId;
    kinectDockId_      = dockLeftId;
    mappingDockId_     = dockLeftId;
    inputDockId_       = dockBottomId;

    ImGui::DockBuilderDockWindow(panel_ids::kScenes,     scenesDockId_);
    ImGui::DockBuilderDockWindow(panel_ids::kParameters, paramsDockId_);
    ImGui::DockBuilderDockWindow(panel_ids::kTransport,  transportDockId_);
    ImGui::DockBuilderDockWindow(panel_ids::kOscMapping, oscMappingDockId_);
    ImGui::DockBuilderDockWindow(panel_ids::kKinect,     kinectDockId_);
    ImGui::DockBuilderDockWindow(panel_ids::kMidiMapping, midiMappingDockId_);
    ImGui::DockBuilderDockWindow(panel_ids::kPresets,   presetsDockId_);
    ImGui::DockBuilderDockWindow(panel_ids::kSequencer, sequencerDockId_);
    ImGui::DockBuilderDockWindow(panel_ids::kCueEditor,  cueEditorDockId_);
    ImGui::DockBuilderDockWindow(panel_ids::kMapping,    mappingDockId_);
    ImGui::DockBuilderDockWindow(panel_ids::kInputPorts, inputDockId_);

    ImGui::DockBuilderFinish(dockspaceId_);

    // Hide less common panels by default
    for (auto& module : modules_) {
        std::string id = module->getPanelId();
        if (id == panel_ids::kMapping || id == panel_ids::kInputPorts || id == panel_ids::kCueEditor) {
            module->setVisible(false);
        }
    }
}

void UIManager::updateCaptureState() {
    if (!initialized_ || !visible_) {
        wantsMouseCapture_ = false;
        wantsKeyboardCapture_ = false;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    wantsMouseCapture_ = io.WantCaptureMouse;
    wantsKeyboardCapture_ = io.WantCaptureKeyboard;
}

// ============================================================
// Menu Bar
// ============================================================
void UIManager::drawMenuBar() {
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("View")) {
        for (auto& module : modules_) {
            bool visible = module->isVisible();
            std::string label = module->getPanelId();
            if (ImGui::MenuItem(label.c_str(), nullptr, &visible)) {
                module->setVisible(visible);
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Show All")) {
            for (auto& module : modules_) {
                module->setVisible(true);
            }
        }
        if (ImGui::MenuItem("Hide All")) {
            for (auto& module : modules_) {
                module->setVisible(false);
            }
        }

        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

void UIManager::toggleModuleVisibility(const std::string& panelId) {
    for (auto& module : modules_) {
        if (module->getPanelId() == panelId) {
            module->setVisible(!module->isVisible());
            return;
        }
    }
}

bool UIManager::isModuleVisible(const std::string& panelId) const {
    for (const auto& module : modules_) {
        if (module->getPanelId() == panelId) {
            return module->isVisible();
        }
    }
    return false;
}

} // namespace modular
