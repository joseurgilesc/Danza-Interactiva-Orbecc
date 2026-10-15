#include "engine/SceneEngine.h"
#include "scenes/BaseScene.h"
#include "../mapping/ParameterTypes.h"

namespace modular {

SceneEngine::SceneEngine()
    : isPaused_(false)
    , backgroundColor_(10.0f) {
    // Registrar parámetros globales en el ParameterRegistry
    registerGlobalParameters();
}

SceneEngine::~SceneEngine() {
    deactivateAll();
    loadedScenes_.clear();
}

// ============================================================================
// Private: Global Parameter Registration
// ============================================================================

void SceneEngine::registerGlobalParameters() {
    auto& registry = ParameterRegistry::getGlobal();
    
    // BPM - Beats Per Minute
    Parameter bpm;
    bpm.id = "global.bpm";
    bpm.label = "BPM";
    bpm.type = ParameterType::FLOAT;
    bpm.scope = ParameterScope::GLOBAL;
    bpm.min = 30.0f;
    bpm.max = 300.0f;
    bpm.defaultValue = 120.0f;
    bpm.step = 1.0f;
    bpm.unit = "BPM";
    bpm.group = "global";
    bpm.description = "Beats per minute for timing";
    registry.registerParam(bpm);
    
    // timeScale - Escala de tiempo global
    Parameter timeScale;
    timeScale.id = "global.timeScale";
    timeScale.label = "Time Scale";
    timeScale.type = ParameterType::FLOAT;
    timeScale.scope = ParameterScope::GLOBAL;
    timeScale.min = 0.0f;
    timeScale.max = 5.0f;
    timeScale.defaultValue = 1.0f;
    timeScale.step = 0.1f;
    timeScale.unit = "x";
    timeScale.group = "global";
    timeScale.description = "Global time scale multiplier";
    registry.registerParam(timeScale);
    
    // clearEachFrame - Limpiar cada frame
    Parameter clearEachFrame;
    clearEachFrame.id = "global.clearEachFrame";
    clearEachFrame.label = "Clear Each Frame";
    clearEachFrame.type = ParameterType::BOOL;
    clearEachFrame.scope = ParameterScope::GLOBAL;
    clearEachFrame.min = 0.0f;
    clearEachFrame.max = 1.0f;
    clearEachFrame.defaultValue = 1.0f;
    clearEachFrame.step = 1.0f;
    clearEachFrame.group = "global";
    clearEachFrame.description = "Clear background each frame";
    registry.registerParam(clearEachFrame);
    
    // currentPalette - Paleta de colores actual
    Parameter currentPalette;
    currentPalette.id = "global.currentPalette";
    currentPalette.label = "Color Palette";
    currentPalette.type = ParameterType::INT;
    currentPalette.scope = ParameterScope::GLOBAL;
    currentPalette.min = 0.0f;
    currentPalette.max = 5.0f;  // Ajustar según número de paletas
    currentPalette.defaultValue = 0.0f;
    currentPalette.step = 1.0f;
    currentPalette.unit = "";
    currentPalette.group = "global";
    currentPalette.description = "Current color palette index";
    registry.registerParam(currentPalette);
    
    // useMouseInput - Toggle para habilitar/deshabilitar entrada de mouse
    Parameter useMouseInput;
    useMouseInput.id = "global.useMouseInput";
    useMouseInput.label = "Use Mouse Input";
    useMouseInput.type = ParameterType::BOOL;
    useMouseInput.scope = ParameterScope::GLOBAL;
    useMouseInput.min = 0.0f;
    useMouseInput.max = 1.0f;
    useMouseInput.defaultValue = 1.0f;  // true por defecto
    useMouseInput.step = 1.0f;
    useMouseInput.group = "global";
    useMouseInput.description = "Enable/disable mouse-driven interaction points";
    registry.registerParam(useMouseInput);
    
    // showSkeleton - Toggle para mostrar/ocultar el esqueleto
    Parameter showSkeleton;
    showSkeleton.id = "global.showSkeleton";
    showSkeleton.label = "Show Skeleton";
    showSkeleton.type = ParameterType::BOOL;
    showSkeleton.scope = ParameterScope::GLOBAL;
    showSkeleton.min = 0.0f;
    showSkeleton.max = 1.0f;
    showSkeleton.defaultValue = 1.0f;  // true por defecto
    showSkeleton.step = 1.0f;
    showSkeleton.group = "global";
    showSkeleton.description = "Show/hide the skeleton overlay. Toggle with Shift+E.";
    registry.registerParam(showSkeleton);
    
    // backgroundColor - Color de fondo global (0-255 grayscale)
    Parameter backgroundColor;
    backgroundColor.id = "global.backgroundColor";
    backgroundColor.label = "Background";
    backgroundColor.type = ParameterType::INT;
    backgroundColor.scope = ParameterScope::GLOBAL;
    backgroundColor.min = 0.0f;
    backgroundColor.max = 255.0f;
    backgroundColor.defaultValue = 10.0f;
    backgroundColor.step = 1.0f;
    backgroundColor.group = "global";
    backgroundColor.description = "Global background color (0=black, 255=white)";
    registry.registerParam(backgroundColor);

    // skeleton.scale - Escala del overlay de esqueleto
    Parameter skeletonScale;
    skeletonScale.id = "skeleton.scale";
    skeletonScale.label = "Skeleton Scale";
    skeletonScale.type = ParameterType::FLOAT;
    skeletonScale.scope = ParameterScope::GLOBAL;
    skeletonScale.min = 0.25f;
    skeletonScale.max = 3.0f;
    skeletonScale.defaultValue = 1.0f;
    skeletonScale.step = 0.05f;
    skeletonScale.group = "global";
    skeletonScale.description = "Scale factor for skeleton overlay (joints and bones)";
    registry.registerParam(skeletonScale);
}

// ============================================================================
// Scene Management - Carga/Descarga
// ============================================================================

bool SceneEngine::loadScene(const std::string& sceneId, ScenePtr scene) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!scene || sceneId.empty()) {
        return false;
    }
    
    if (loadedScenes_.find(sceneId) != loadedScenes_.end()) {
        // Scene already loaded
        return false;
    }
    
    // Call setup when loading the scene
    scene->setup();
    
    loadedScenes_[sceneId] = scene;
    return true;
}

bool SceneEngine::unloadScene(const std::string& sceneId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = loadedScenes_.find(sceneId);
    if (it == loadedScenes_.end()) {
        return false;
    }
    
    // Desactivar si está activa
    if (activeSceneId_ == sceneId) {
        auto scene = it->second;
        if (scene) {
            scene->deactivate();
            scene->unloadResources();
        }
        activeSceneId_.clear();
    }
    
    // Limpiar prepared si está preparada
    if (preparedSceneId_ == sceneId) {
        preparedSceneId_.clear();
    }
    
    loadedScenes_.erase(it);
    return true;
}

bool SceneEngine::isSceneLoaded(const std::string& sceneId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loadedScenes_.find(sceneId) != loadedScenes_.end();
}

// ============================================================================
// ISceneActivator Implementation
// ============================================================================

bool SceneEngine::activateScene(const std::string& sceneId) {
    std::lock_guard<std::mutex> lock(mutex_);
    return activateSceneLocked(sceneId);
}

bool SceneEngine::activateSceneLocked(const std::string& sceneId) {
    auto it = loadedScenes_.find(sceneId);
    if (it == loadedScenes_.end()) {
        return false;
    }
    
    ScenePtr newScene = it->second;
    if (!newScene) {
        return false;
    }
    
    // Desactivar escena anterior
    if (!activeSceneId_.empty() && activeSceneId_ != sceneId) {
        auto oldIt = loadedScenes_.find(activeSceneId_);
        if (oldIt != loadedScenes_.end()) {
            auto oldScene = oldIt->second;
            if (oldScene) {
                oldScene->deactivate();
            }
        }
    }
    
    // Activar nueva escena
    newScene->activate();
    activeSceneId_ = sceneId;
    
    return true;
}

bool SceneEngine::deactivateScene(const std::string& sceneId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (activeSceneId_ != sceneId) {
        return false;
    }
    
    auto it = loadedScenes_.find(sceneId);
    if (it == loadedScenes_.end()) {
        return false;
    }
    
    auto scene = it->second;
    if (scene) {
        scene->deactivate();
    }
    
    activeSceneId_.clear();
    return true;
}

bool SceneEngine::prepareScene(const std::string& sceneId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Verificar que la escena existe
    auto it = loadedScenes_.find(sceneId);
    if (it == loadedScenes_.end()) {
        return false;
    }
    
    // La preparación en modo hybrid implica que la escena ya está cargada
    // pero no activada. Marcamos cual está preparada.
    preparedSceneId_ = sceneId;
    return true;
}

std::string SceneEngine::getActiveSceneId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeSceneId_;
}

bool SceneEngine::deactivateAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!activeSceneId_.empty()) {
        auto it = loadedScenes_.find(activeSceneId_);
        if (it != loadedScenes_.end()) {
            auto scene = it->second;
            if (scene) {
                scene->deactivate();
            }
        }
        activeSceneId_.clear();
    }
    
    preparedSceneId_.clear();
    isPaused_ = false;
    return true;
}

// ============================================================================
// Scene Lifecycle - Pausa/Reanudación
// ============================================================================

void SceneEngine::pauseScene() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!activeSceneId_.empty() && !isPaused_) {
        auto it = loadedScenes_.find(activeSceneId_);
        if (it != loadedScenes_.end()) {
            auto scene = it->second;
            if (scene) {
                scene->pause();
                isPaused_ = true;
            }
        }
    }
}

void SceneEngine::resumeScene() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!activeSceneId_.empty() && isPaused_) {
        auto it = loadedScenes_.find(activeSceneId_);
        if (it != loadedScenes_.end()) {
            auto scene = it->second;
            if (scene) {
                scene->resume();
                isPaused_ = false;
            }
        }
    }
}

bool SceneEngine::hasActiveScene() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !activeSceneId_.empty();
}

bool SceneEngine::isScenePaused() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return isPaused_;
}

// ============================================================================
// Update & Draw - Rendering
// ============================================================================

void SceneEngine::update(float deltaTime) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Handle scene transition
    if (transitionState_ == TransitionState::FADING_OUT) {
        transitionTimer_ += deltaTime;
        transitionAlpha_ = std::min(1.0f, transitionTimer_ / (transitionDuration_ * 0.5f));
        if (transitionAlpha_ >= 1.0f) {
            // Switch scene at peak darkness (mutex already held by update)
            activateSceneLocked(transitionTargetId_);
            transitionState_ = TransitionState::FADING_IN;
            transitionTimer_ = 0.0f;
        }
    } else if (transitionState_ == TransitionState::FADING_IN) {
        transitionTimer_ += deltaTime;
        transitionAlpha_ = std::max(0.0f, 1.0f - transitionTimer_ / (transitionDuration_ * 0.5f));
        if (transitionAlpha_ <= 0.0f) {
            transitionState_ = TransitionState::NONE;
            transitionAlpha_ = 0.0f;
        }
    }

    if (activeSceneId_.empty() || isPaused_) {
        return;
    }
    
    auto it = loadedScenes_.find(activeSceneId_);
    if (it != loadedScenes_.end()) {
        auto scene = it->second;
        if (scene) {
            scene->update(deltaTime);
        }
    }
}

void SceneEngine::draw() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (activeSceneId_.empty()) {
        return;
    }
    
    auto it = loadedScenes_.find(activeSceneId_);
    if (it != loadedScenes_.end()) {
        auto scene = it->second;
        if (scene) {
            scene->draw();
        }
    }
}

// ============================================================================
// Scene Access - Consultas
// ============================================================================

ScenePtr SceneEngine::getActiveScene() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (activeSceneId_.empty()) {
        return nullptr;
    }
    
    auto it = loadedScenes_.find(activeSceneId_);
    if (it != loadedScenes_.end()) {
        return it->second;
    }
    
    return nullptr;
}

ScenePtr SceneEngine::getScene(const std::string& sceneId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = loadedScenes_.find(sceneId);
    if (it != loadedScenes_.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::vector<std::string> SceneEngine::getLoadedScenes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> sceneIds;
    sceneIds.reserve(loadedScenes_.size());
    
    for (const auto& pair : loadedScenes_) {
        sceneIds.push_back(pair.first);
    }
    
    return sceneIds;
}

size_t SceneEngine::getLoadedSceneCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loadedScenes_.size();
}

// ============================================================================
// Global Toggles
// ============================================================================

void SceneEngine::setUseMouseInput(bool enabled) {
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.useMouseInput")) {
        param->currentValue = enabled ? 1.0f : 0.0f;
    }
}

bool SceneEngine::isMouseInputEnabled() const {
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.useMouseInput")) {
        return param->currentValue > 0.5f;
    }
    return true;  // default: enabled
}

void SceneEngine::setShowSkeleton(bool visible) {
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.showSkeleton")) {
        param->currentValue = visible ? 1.0f : 0.0f;
    }
}

bool SceneEngine::isShowSkeleton() const {
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.showSkeleton")) {
        return param->currentValue > 0.5f;
    }
    return true;  // default: visible
}

void SceneEngine::setBackgroundColor(float value) {
    backgroundColor_ = value;
    auto& registry = ParameterRegistry::getGlobal();
    if (auto param = registry.getParam("global.backgroundColor")) {
        param->currentValue = value;
    }
    ofBackground(static_cast<int>(value));
}

float SceneEngine::getBackgroundColor() const {
    return backgroundColor_;
}

// ============================================================================
// Input Handling - Reenvío a escena activa
// ============================================================================

// Keys that should be handled by UI/ofApp, NOT passed to scenes
// These are global hotkeys that take priority
static bool isGlobalKey(int key) {
    ofLogNotice("SceneEngine::isGlobalKey") << "checking key=" << key;
    switch (key) {
        case 'u': case 'U':  // UI toggle
        case '1':           // UI panel toggle
        case 'c': case 'C': // Global clear
        case 's': case 'S': // Global save
        case 'm': case 'M': // Global mode toggle
        case 'p': case 'P': // Global palette
        case 'b': case 'B': // Global flock mode
            return true;
        default:
            ofLogNotice("SceneEngine::isGlobalKey") << "key=" << key << " NOT global, will pass to scene";
            return false;
    }
}

void SceneEngine::keyPressed(int key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Filter out global keys - they should be handled by ofApp, not scenes
    if (isGlobalKey(key)) {
        ofLogNotice("SceneEngine") << "Filtered global key " << key << " from reaching scene";
        return;
    }
    
    // Pass other keys to active scene
    if (!activeSceneId_.empty()) {
        auto it = loadedScenes_.find(activeSceneId_);
        if (it != loadedScenes_.end() && it->second) {
            it->second->keyPressed(key);
        }
    }
}

void SceneEngine::keyReleased(int key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeSceneId_.empty()) {
        auto it = loadedScenes_.find(activeSceneId_);
        if (it != loadedScenes_.end() && it->second) {
            it->second->keyReleased(key);
        }
    }
}

void SceneEngine::mousePressed(int x, int y, int button) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeSceneId_.empty()) {
        auto it = loadedScenes_.find(activeSceneId_);
        if (it != loadedScenes_.end() && it->second) {
            it->second->mousePressed(x, y, button);
        }
    }
}

void SceneEngine::mouseReleased(int x, int y, int button) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeSceneId_.empty()) {
        auto it = loadedScenes_.find(activeSceneId_);
        if (it != loadedScenes_.end() && it->second) {
            it->second->mouseReleased(x, y, button);
        }
    }
}

void SceneEngine::mouseDragged(int x, int y, int button) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeSceneId_.empty()) {
        auto it = loadedScenes_.find(activeSceneId_);
        if (it != loadedScenes_.end() && it->second) {
            it->second->mouseDragged(x, y, button);
        }
    }
}

void SceneEngine::mouseMoved(int x, int y) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeSceneId_.empty()) {
        auto it = loadedScenes_.find(activeSceneId_);
        if (it != loadedScenes_.end() && it->second) {
            // Forward raw event to scene (deprecated no-op in scenes)
            it->second->mouseMoved(x, y);

            // If mouse input is enabled, feed "mouse" synthetic interaction point
            // Normalize pixel coords → 0-1 for InteractionPoint::setTarget conversion
            if (isMouseInputEnabled()) {
                float nx = static_cast<float>(x) / ofGetWidth();
                float ny = static_cast<float>(y) / ofGetHeight();
                it->second->setParameter("mouse", glm::vec2(nx, ny));
            }
        }
    }
}

void SceneEngine::windowResized(int w, int h) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeSceneId_.empty()) {
        auto it = loadedScenes_.find(activeSceneId_);
        if (it != loadedScenes_.end() && it->second) {
            it->second->windowResized(w, h);
        }
    }
}

// ============================================================================
// Scene Transition — Fade In/Out
// ============================================================================

bool SceneEngine::transitionTo(const std::string& sceneId, float duration) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Validate target scene exists
    if (loadedScenes_.find(sceneId) == loadedScenes_.end()) {
        ofLogWarning("SceneEngine") << "transitionTo: scene not found: " << sceneId;
        return false;
    }

    // If already transitioning to the same scene, ignore
    if (transitionState_ != TransitionState::NONE && transitionTargetId_ == sceneId) {
        return true;
    }

    // If target is already active, skip
    if (activeSceneId_ == sceneId && transitionState_ == TransitionState::NONE) {
        return true;
    }

    transitionState_ = TransitionState::FADING_OUT;
    transitionTargetId_ = sceneId;
    transitionDuration_ = std::max(0.2f, duration);
    transitionTimer_ = 0.0f;
    transitionAlpha_ = 0.0f;

    ofLogNotice("SceneEngine") << "Transition started → " << sceneId
                               << " (duration: " << transitionDuration_ << "s)";
    return true;
}

bool SceneEngine::isTransitioning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transitionState_ != TransitionState::NONE;
}

float SceneEngine::getTransitionAlpha() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return transitionAlpha_;
}

} // namespace modular
