#pragma once

#include "ui/IUIModule.h"
#include "imgui.h"
#include <cstdint>
#include <string>
#include <vector>

namespace modular {

class IKinectAdapter;
class KinectMappingEngine;
class SceneEngine;
class PresetManager;
struct TrackedBody;
enum class JointType : uint8_t;

class KinectPanel : public IUIModule {
public:
    KinectPanel(IKinectAdapter* adapter, KinectMappingEngine* mappingEngine, 
               SceneEngine* sceneEngine, PresetManager* presetManager);

    void setup() override;
    void update(float deltaTime) override;
    void draw() override;

    void setVisible(bool visible) override;
    bool isVisible() const override;

    std::string getPanelId() const override { return "Kinect"; }

private:
    // === Tab drawers ===
    void drawDeviceTab();
    void drawJointsTab();
    void drawMappingsTab();
    void drawQuickMapTab();

    // === Device sub-sections ===
    void drawStatusBar();
    void drawSourceSelector();
    void drawBridgeControls();
    void drawSkeletonToggle();

    // === Joints sub-sections ===
    void drawJointTable(const struct TrackedBody& body);

    // === Mappings sub-sections ===
    void drawMappingToolbar();
    void drawMappingList();
    void drawMappingEditorRight();

    // === Helpers ===
    const char* jointTypeToLabel(JointType type) const;
    const char* connectionStateLabel() const;
    ImVec4 connectionStateColor() const;

    // === Bridge launcher ===
    void launchPoseBridge();
    void stopPoseBridge();
    bool isBridgeRunning() const;

    // === Camera scanning ===
    void scanCameras();

    // === Data ===
    IKinectAdapter* adapter_;
    KinectMappingEngine* mappingEngine_;
    SceneEngine* sceneEngine_;
    PresetManager* presetManager_;
    bool visible_;

    // Bridge state
    bool bridgeRunning_;
    int cameraId_;
    int sourceType_;             // 0 = webcam, 1 = kinect
    std::vector<std::string> cameraNames_;
    bool camerasScanned_;
    float adapterSmoothing_;     // C++ ofLerp smoothing factor for joint interpolation
    float skeletonScale_ = 1.0f; // global skeleton.scale for overlay zoom

    // Joints tab state
    int selectedBodyIndex_;

    // Mappings tab state
    int selectedMappingIndex_;
    ImGuiTextFilter mappingFilter_;
    char editJointSourceBuf_[64];
    char editTargetParamBuf_[128];
    int editJointType_;          // JointType enum index
    int editJointAxis_;          // 0=X, 1=Y, 2=Z, 3=Confidence
    int editConditionType_;
    int editTransformType_;
    float editThreshold_;
    float editMinInput_, editMaxInput_;
    float editMinOutput_, editMaxOutput_;
    float editSmoothing_;
    bool editorIsNew_;           // true = creating new, false = editing existing

    // Quick Map tab state
    int quickSceneIndex_ = 0;
    int quickSceneIndexPrev_ = 0;   // detect scene changes to sync engine
    int quickPresetIndex_ = -1;
    int quickPresetIndexPrev_ = -1;
    std::string toastMessage_;
    float toastTimer_ = 0.0f;
    int lastConnectionState_ = 0;  // cached ConnectionState enum value for change detection
};

} // namespace modular
