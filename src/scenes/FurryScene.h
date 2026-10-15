#pragma once

#include "scenes/BaseScene.h"
#include "registry/ParameterRegistry.h"
#include "ofMain.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vector>
#include <string>
#include <memory>
#include <map>

namespace modular {

// ============================================================================
// FurryScene - Escena de fibras vivas con efecto de pelo/pelaje dinámico
// ============================================================================

class FurryScene : public IBaseScene {
public:
    FurryScene();
    ~FurryScene() override;

    // ============================================
    // IBaseScene Implementation
    // ============================================

    void setup() override;
    void update(float deltaTime) override;
    void draw() override;

    void activate() override;
    void deactivate() override;
    void pause() override;
    void resume() override;

    std::string getId() const override;
    std::string getName() const override;

    std::vector<std::string> getParameterIds() const override;
    std::vector<std::string> getInteractionPointIds() const override;
    std::shared_ptr<Parameter> getParameter(const std::string& paramId) override;
    bool setParameter(const std::string& paramId, float value) override;
    bool setParameter(const std::string& paramId, const glm::vec2& value) override;
    bool setParameter(const std::string& paramId, const glm::vec3& value) override;

    std::vector<SuggestedMapping> getSuggestedMappings() const override;
    Preset getDefaultPreset() const override;
    SceneMetadata getSceneMetadata() const override;

    void onBeat(int beatNumber) override;
    void onBar(int barNumber) override;

    bool isValid() const override;
    std::string getInvalidReason() const override;

    void unloadResources() override;
    void reloadResources() override;

    // ============================================
    // Input Handlers
    // ============================================

    void keyPressed(int key) override;
    void keyReleased(int key) override;
    void mousePressed(int x, int y, int button) override;
    void mouseReleased(int x, int y, int button) override;
    void mouseDragged(int x, int y, int button) override;
    void mouseMoved(int x, int y) override;
    void windowResized(int w, int h) override;

private:
    // ============================================
    // Internal helpers
    // ============================================

    void setupPalettes();
    void initializeParameterDefinitions();
    void registerParameters();
    void unregisterParameters();
    void rebuildGrid();
    void drawContour();  // draws body outline from active mapped joints

    // ============================================
    // Palettes (same 5 as PinturaFluorescenteScene)
    // ============================================

    std::vector<std::vector<ofFloatColor>> palettes_;
    int currentPalette_;

    // ============================================
    // Grid & Fiber data
    // ============================================

    int gridCols_;
    int gridRows_;
    std::vector<glm::vec2> rootPoints_;  // root positions on screen

    // VBO rendering
    ofVbo vbo_;
    std::vector<glm::vec3> vertices_;    // GL_LINES pairs: root→mid, mid→tip
    std::vector<ofFloatColor> colors_;

    // ============================================
    // Interaction
    // ============================================

    std::map<std::string, InteractionPoint> interactionPoints_;

    // Body contour from mapped joints — becomes more visible as more joints are tracked
    float contourOpacity_;  // 0.0-1.0, how visible the body outline is
    std::vector<glm::vec2> bodyHull_;  // convex hull of active joints (pixels)

    // ============================================
    // Time & State
    // ============================================

    float timeAccum_;   // deltaTime accumulator for noise animation
    bool isActive_;
    bool isPaused_;
    bool isValid_;
    std::string invalidReason_;

    // ============================================
    // Scene parameters (exposed via registry)
    // ============================================

    float density_;       // 0.3-1.0, grid density multiplier
    float fiberLength_;   // 5.0-40.0, base fiber length in pixels
    float stiffness_;     // 0.0-1.0, resistance to bending
    float windStrength_;  // 0.0-2.0, noise-driven wind intensity
    float windSpeed_;     // 0.1-3.0, speed of wind animation
    float mouseRadius_;   // 50.0-400.0, radius of mouse influence
    float mouseForce_;    // 0.0-3.0, strength of mouse deflection
    float breathAmount_;  // 0.0-1.0, amplitude of breathing oscillation
    float paletteParam_;  // 0.0-4.0, palette index (cast to int)
    float fiberThickness_; // 1.0-8.0, thickness of each fiber in pixels

    // ============================================
    // Parameter storage (registered in registry)
    // ============================================

    std::map<std::string, std::shared_ptr<Parameter>> parameters_;
    bool parametersRegistered_;
};

// ============================================================================
// Factory function
// ============================================================================

ScenePtr CreateFurryScene();

} // namespace modular
