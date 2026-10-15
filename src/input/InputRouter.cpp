#include "input/InputRouter.h"

#include "engine/SceneEngine.h"
#include "ui/UIManager.h"

namespace modular {

InputRouter::InputRouter(SceneEngine* sceneEngine, UIManager* uiManager)
    : sceneEngine_(sceneEngine)
    , uiManager_(uiManager) {}

void InputRouter::keyPressed(int key) {
    if (shouldBlockKeyboard(key)) {
        return;
    }
    if (sceneEngine_) {
        sceneEngine_->keyPressed(key);
    }
}

void InputRouter::keyReleased(int key) {
    if (shouldBlockKeyboard(key)) {
        return;
    }
    if (sceneEngine_) {
        sceneEngine_->keyReleased(key);
    }
}

void InputRouter::mousePressed(int x, int y, int button) {
    if (shouldBlockMouse(button)) {
        return;
    }
    forwardMousePressed(x, y, button);
}

void InputRouter::mouseReleased(int x, int y, int button) {
    if (shouldBlockMouse(button)) {
        return;
    }
    forwardMouseReleased(x, y, button);
}

void InputRouter::mouseDragged(int x, int y, int button) {
    if (shouldBlockMouse(button)) {
        return;
    }
    forwardMouseDragged(x, y, button);
}

void InputRouter::mouseMoved(int x, int y) {
    // Mouse tracking always passes through — it's position data, not interaction.
    // Blocking it here prevents SceneEngine from feeding synthetic interaction
    // points (setParameter("mouse", glm::vec2(...))) while the UI is visible.
    forwardMouseMoved(x, y);
}

void InputRouter::addKeyboardBypass(int key) {
    keyboardBypass_.insert(key);
}

void InputRouter::addMouseBypass(int button) {
    mouseBypass_.insert(button);
}

bool InputRouter::shouldBlockKeyboard(int key) const {
    if (keyboardBypass_.count(key) > 0) {
        return false;
    }
    return uiManager_ && uiManager_->wantsKeyboardCapture();
}

bool InputRouter::shouldBlockMouse(int button) const {
    if (mouseBypass_.count(button) > 0) {
        return false;
    }
    return uiManager_ && uiManager_->wantsMouseCapture();
}

void InputRouter::forwardMousePressed(int x, int y, int button) {
    if (sceneEngine_) {
        sceneEngine_->mousePressed(x, y, button);
    }
}

void InputRouter::forwardMouseReleased(int x, int y, int button) {
    if (sceneEngine_) {
        sceneEngine_->mouseReleased(x, y, button);
    }
}

void InputRouter::forwardMouseDragged(int x, int y, int button) {
    if (sceneEngine_) {
        sceneEngine_->mouseDragged(x, y, button);
    }
}

void InputRouter::forwardMouseMoved(int x, int y) {
    if (sceneEngine_) {
        sceneEngine_->mouseMoved(x, y);
    }
}

} // namespace modular
