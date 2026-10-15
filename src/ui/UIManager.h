#pragma once

#include "ofMain.h"
#include "ofxImGui.h"
#include "ui/IUIModule.h"
#include "ui/PanelIds.h"
#include "ui/StudioTheme.h"

#include <memory>
#include <vector>

namespace modular {

class UIManager {
public:
    UIManager();

    void setup();
    void update(float deltaTime);
    void draw();

    void addModule(std::unique_ptr<IUIModule> module);

    void keyPressed(int key);
    void keyReleased(int key);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void mouseDragged(int x, int y, int button);
    void mouseMoved(int x, int y);

    void toggleGlobalVisible();
    void setGlobalVisible(bool visible);
    bool isGlobalVisible() const;

    // Panel visibility toggle (for View menu)
    void toggleModuleVisibility(const std::string& panelId);
    bool isModuleVisible(const std::string& panelId) const;

    void toggleWindowAlpha();
    bool isWindowAlphaTransparent() const;

    bool wantsMouseCapture() const;
    bool wantsKeyboardCapture() const;

private:
    void drawDockspace();
    void drawMenuBar();
    void buildDefaultDockLayout();
    void drawPlaceholderPanels();
    void drawShortcutsOverlay();
    void updateCaptureState();

    ofxImGui::Gui gui_;
    std::vector<std::unique_ptr<IUIModule>> modules_;
    bool initialized_;
    bool visible_;
    bool layoutInitialized_;
    bool wantsMouseCapture_;
    bool wantsKeyboardCapture_;
    StudioFonts fonts_;
    ImGuiID dockspaceId_;
    ImGuiID scenesDockId_;
    ImGuiID paramsDockId_;
    ImGuiID transportDockId_;
    ImGuiID mappingDockId_;
    ImGuiID inputDockId_;
    ImGuiID oscMappingDockId_;
    ImGuiID kinectDockId_;
    ImGuiID presetsDockId_;
    ImGuiID sequencerDockId_;
    ImGuiID cueEditorDockId_;
    ImGuiID midiMappingDockId_;

    // Window transparency
    bool windowAlphaTransparent_ = false;
};

} // namespace modular
