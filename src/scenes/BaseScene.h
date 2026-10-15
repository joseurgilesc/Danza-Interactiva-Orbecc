#pragma once

#include "ofMain.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>

// Include shared parameter types - NO redefinir aquí
#include "../mapping/ParameterTypes.h"

// Include InteractionPoint struct — used by all scenes for multi-point interaction
#include "InteractionPoint.h"

namespace modular {

// ============================================================================
// Scene Metadata - información de metadatos de una escena
// ============================================================================

struct SceneMetadata {
    std::string id;
    std::string name;
    std::string description;
    std::string author;
    std::string version;
    std::vector<std::string> tags;
    std::string category;
    bool hasInternalState;
    bool isTimeBased;
    float estimatedDuration;  // en segundos, 0 si infinito
    std::string presetPath;
};

// ============================================================================
// IBaseScene - Interfaz abstracta para todas las escenas
// ============================================================================

class IBaseScene {
public:
    virtual ~IBaseScene() = default;
    
    // ============================================
    // Lifecycle Methods
    // ============================================
    
    virtual void setup() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void draw() = 0;
    
    // ============================================
    // Activation State
    // ============================================
    
    virtual void activate() = 0;
    virtual void deactivate() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    
    // ============================================
    // Identification
    // ============================================
    
    virtual std::string getId() const = 0;
    virtual std::string getName() const = 0;
    
    // ============================================
    // Parameter Management
    // ============================================
    
    virtual std::vector<std::string> getParameterIds() const = 0;
    virtual std::vector<std::string> getInteractionPointIds() const { return {}; }
    virtual void clearInteractionPoints() {}  // instantly deactivate all IPs
    virtual std::shared_ptr<Parameter> getParameter(const std::string& paramId) = 0;
    virtual bool setParameter(const std::string& paramId, float value) = 0;
    virtual bool setParameter(const std::string& paramId, const glm::vec2& value) = 0;
    virtual bool setParameter(const std::string& paramId, const glm::vec3& value) = 0;
    
    // ============================================
    // Preset & Metadata
    // ============================================
    
    virtual std::vector<SuggestedMapping> getSuggestedMappings() const = 0;
    virtual Preset getDefaultPreset() const = 0;
    virtual SceneMetadata getSceneMetadata() const = 0;
    
    // ============================================
    // Timing & Synchronization
    // ============================================
    
    virtual void onBeat(int beatNumber) = 0;
    virtual void onBar(int barNumber) = 0;
    
    // ============================================
    // Validation
    // ============================================
    
    virtual bool isValid() const = 0;
    virtual std::string getInvalidReason() const = 0;
    
    // ============================================
    // Resource Management
    // ============================================
    
    virtual void unloadResources() = 0;
    virtual void reloadResources() = 0;
    
    // ============================================
    // Input Handling
    // ============================================
    
    virtual void keyPressed(int key) = 0;
    virtual void keyReleased(int key) = 0;
    virtual void mousePressed(int x, int y, int button) = 0;
    virtual void mouseReleased(int x, int y, int button) = 0;
    virtual void mouseDragged(int x, int y, int button) = 0;
    virtual void mouseMoved(int x, int y) = 0;
    virtual void windowResized(int w, int h) = 0;
    
    // ============================================
    // Optional: Internal State for Serialization
    // ============================================
    
    virtual std::string getInternalState() const { return ""; }
    virtual bool setInternalState(const std::string& state) { return false; }
};

// ============================================================================
// Typedef para punteros a escenas
// ============================================================================

using ScenePtr = std::shared_ptr<IBaseScene>;
using SceneWeakPtr = std::weak_ptr<IBaseScene>;

// ============================================================================
// Scene Factory Function Type
// ============================================================================

using SceneFactory = std::function<ScenePtr()>;

} // namespace modular