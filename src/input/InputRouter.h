#pragma once

#include <unordered_set>

namespace modular {

class SceneEngine;
class UIManager;

class InputRouter {
public:
    InputRouter(SceneEngine* sceneEngine, UIManager* uiManager);

    void keyPressed(int key);
    void keyReleased(int key);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void mouseDragged(int x, int y, int button);
    void mouseMoved(int x, int y);

    void addKeyboardBypass(int key);
    void addMouseBypass(int button);

private:
    bool shouldBlockKeyboard(int key) const;
    bool shouldBlockMouse(int button) const;

    void forwardMousePressed(int x, int y, int button);
    void forwardMouseReleased(int x, int y, int button);
    void forwardMouseDragged(int x, int y, int button);
    void forwardMouseMoved(int x, int y);

    SceneEngine* sceneEngine_;
    UIManager* uiManager_;
    std::unordered_set<int> keyboardBypass_;
    std::unordered_set<int> mouseBypass_;
};

} // namespace modular
