#include "ui/KinectPanel.h"
#include "adapters/kinect/IKinectAdapter.h"
#include "mapping/KinectMappingEngine.h"
#include "mapping/KinectMappingTypes.h"
#include "ui/PanelIds.h"
#include "engine/SceneEngine.h"
#include "presets/PresetManager.h"
#include "registry/ParameterRegistry.h"
#include "scenes/BaseScene.h"

#include "imgui.h"
#include "imgui_internal.h"
#include <ofLog.h>
#include <ofFileUtils.h>
#include <ofVideoGrabber.h>
#include <cstring>
#include <cmath>
#include <set>
#include <algorithm>

namespace modular {

// ============================================================
// Constructor
// ============================================================
KinectPanel::KinectPanel(IKinectAdapter* adapter, KinectMappingEngine* mappingEngine, SceneEngine* sceneEngine, PresetManager* presetManager)
    : adapter_(adapter)
    , mappingEngine_(mappingEngine)
    , sceneEngine_(sceneEngine)
    , presetManager_(presetManager)
    , visible_(true)
    , bridgeRunning_(false)
    , cameraId_(0)
    , sourceType_(0)          // 0 = webcam
    , camerasScanned_(false)
    , adapterSmoothing_(0.3f)
    , selectedBodyIndex_(0)
    , selectedMappingIndex_(-1)
    , editJointType_(0)
    , editJointAxis_(0)       // X
    , editConditionType_(0)
    , editTransformType_(0)
    , editThreshold_(0.5f)
    , editMinInput_(0.0f)
    , editMaxInput_(1.0f)
    , editMinOutput_(0.0f)
    , editMaxOutput_(1.0f)
    , editSmoothing_(0.0f)
    , editorIsNew_(true)
    , quickSceneIndex_(0)
    , quickPresetIndex_(-1)
    , toastTimer_(0.0f)
{
    memset(editJointSourceBuf_, 0, sizeof(editJointSourceBuf_));
    memset(editTargetParamBuf_, 0, sizeof(editTargetParamBuf_));
}

// ============================================================
// Lifecycle
// ============================================================
void KinectPanel::setup() {
    scanCameras();
    
    // Load saved joint smoothing
    ofJson json = ofLoadJson("config/joint-smoothing.json");
    if (!json.empty() && json.contains("jointSmoothing")) {
        adapterSmoothing_ = json["jointSmoothing"].get<float>();
    }
    adapter_->setSmoothing(adapterSmoothing_);

    // Load skeleton scale from global registry
    auto scaleParam = ParameterRegistry::getGlobal().getParam("skeleton.scale");
    if (scaleParam.has_value()) {
        skeletonScale_ = scaleParam->currentValue;
    }
}

void KinectPanel::update(float deltaTime) {
    if (toastTimer_ > 0.0f) toastTimer_ -= deltaTime;
}

// ============================================================
// Main Draw — Tabbed Layout
// ============================================================
void KinectPanel::draw() {
    ImGui::SetNextWindowSize(ImVec2(650, 500), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(panel_ids::kKinect, &visible_)) {
        ImGui::End();
        return;
    }

    // Status bar always visible at top
    drawStatusBar();

    if (ImGui::BeginTabBar("KinectTabs")) {

        if (ImGui::BeginTabItem("Device")) {
            drawDeviceTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Joints")) {
            drawJointsTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Mappings")) {
            drawMappingsTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Quick Map")) {
            drawQuickMapTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ============================================================
// Status Bar (shared across all tabs)
// ============================================================
void KinectPanel::drawStatusBar() {
    ConnectionState state = adapter_->getConnectionState();
    
    // Debug: detect connection state changes
    int stateInt = static_cast<int>(state);
    if (stateInt != lastConnectionState_) {
        static const char* names[] = {"Disconnected", "Connecting", "Connected", "Error"};
        int oldIdx = lastConnectionState_;
        int newIdx = stateInt;
        ofLogWarning("KinectPanel") << "CONNECTION STATE CHANGED: "
            << names[oldIdx] << " -> " << names[newIdx]
            << " (tab switching or key event may have triggered this)";
        lastConnectionState_ = stateInt;
    }
    
    ImVec4 color = connectionStateColor();

    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::Text("\u25CF");  // ● bullet
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::Text("%s", connectionStateLabel());

    ImGui::SameLine();
    auto bodies = adapter_->getAllTrackedBodies();
    ImGui::Text("| Bodies: %zu", bodies.size());

    ImGui::SameLine();
    if (state == ConnectionState::CONNECTED) {
        ImGui::Text("| FPS: %.1f", ImGui::GetIO().Framerate);
    } else {
        ImGui::TextDisabled("| FPS: N/A");
    }
}

// ============================================================
// Tab 1: Device
// ============================================================
void KinectPanel::drawDeviceTab() {
    drawSourceSelector();
    ImGui::Separator();
    drawBridgeControls();
    ImGui::Separator();
    drawSkeletonToggle();
}

// ============================================================
// Source Selector (Webcam / Kinect)
// ============================================================
void KinectPanel::drawSourceSelector() {
    ConnectionState state = adapter_->getConnectionState();
    bool connected = (state == ConnectionState::CONNECTED);
    bool disconnected = (state == ConnectionState::DISCONNECTED || state == ConnectionState::ERROR);

    // Source type dropdown
    ImGui::SeparatorText("Source");
    {
        const char* sources[] = {"Webcam (OpenCV)", "Kinect v1 (libfreenect)"};
        ImGui::SetNextItemWidth(250);
        if (ImGui::Combo("##sourceType", &sourceType_, sources, 2)) {
            if (bridgeRunning_) {
                stopPoseBridge();
            }
        }
    }

    // Camera selector (only for webcam)
    if (sourceType_ == 0) {
        ImGui::SeparatorText("Camera");

        if (!camerasScanned_) {
            scanCameras();
        }

        if (cameraNames_.empty()) {
            ImGui::TextDisabled("No cameras detected");
        } else {
            ImGui::SetNextItemWidth(250);
            if (ImGui::Combo("##cameraId", &cameraId_, 
                [](void* data, int idx) -> const char* {
                    auto& names = *static_cast<std::vector<std::string>*>(data);
                    return names[idx].c_str();
                }, &cameraNames_, cameraNames_.size())) {
                ofLogNotice("KinectPanel") << "Selected camera " << cameraId_;
            }
        }
    }

    // Adapter control — small buttons, right-aligned, below source/camera
    ImGui::Spacing();
    ImGui::SeparatorText("Adapter");
    ImGui::TextDisabled("Connect / Disconnect / Reset");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 320);
    if (disconnected) {
        if (ImGui::SmallButton("Connect")) {
            adapter_->initialize();
        }
    }
    if (connected) {
        if (disconnected) ImGui::SameLine();
        if (ImGui::SmallButton("Disconnect")) {
            adapter_->shutdown();
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset Tracking")) {
        adapter_->shutdown();
        adapter_->initialize();
    }
}

// ============================================================
// Bridge Controls
// ============================================================
void KinectPanel::drawBridgeControls() {
    ImGui::SeparatorText(sourceType_ == 1 ? "Pose Bridge (Kinect v1 → OSC)" : "Pose Bridge (Webcam → OSC)");

    if (!bridgeRunning_) {
        if (ImGui::Button("Start Pose Bridge", ImVec2(150, 0))) {
            launchPoseBridge();
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
        if (ImGui::Button("Stop Pose Bridge", ImVec2(150, 0))) {
            stopPoseBridge();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Bridge running");
    }
    ImGui::TextDisabled("Runs mediapipe_pose_bridge.py with %s", sourceType_ == 1 ? "Kinect v1" : "webcam");
}

// ============================================================
// Skeleton Toggle
// ============================================================
void KinectPanel::drawSkeletonToggle() {
    ImGui::SeparatorText("Skeleton");
    
    bool skeletonVisible = sceneEngine_->isShowSkeleton();
    if (ImGui::Checkbox("Show Skeleton", &skeletonVisible)) {
        sceneEngine_->setShowSkeleton(skeletonVisible);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(Shift+E)");

    // Adapter smoothing: controls ofLerp interpolation speed in C++ adapter.
    // Lower = smoother but more latency. Higher = more responsive but more jitter.
    if (ImGui::SliderFloat("Joint Smoothing", &adapterSmoothing_, 0.05f, 0.8f, "%.2f")) {
        adapter_->setSmoothing(adapterSmoothing_);
        // Persist to disk so it survives restarts
        ofJson json;
        json["jointSmoothing"] = adapterSmoothing_;
        ofSaveJson("config/joint-smoothing.json", json);
    }
    ImGui::TextDisabled("0.1 = smooth/slow | 0.3 = balanced | 0.6 = fast/jittery");

    // Skeleton scale: global zoom for skeleton overlay
    ImGui::Spacing();
    if (ImGui::SliderFloat("Skeleton Scale", &skeletonScale_, 0.25f, 3.0f, "%.2f")) {
        ParameterRegistry::getGlobal().setValue("skeleton.scale", skeletonScale_);
    }
    ImGui::TextDisabled("0.5 = half | 1.0 = normal | 2.0 = double");
}

// ============================================================
// Tab 2: Joints
// ============================================================
void KinectPanel::drawJointsTab() {
    auto bodies = adapter_->getAllTrackedBodies();

    if (bodies.empty() || !bodies[0].isTracked) {
        ImGui::TextDisabled("No skeleton data — start tracking to see joints");
        return;
    }

    // Body selector (if multiple)
    if (bodies.size() > 1) {
        if (selectedBodyIndex_ >= (int)bodies.size()) {
            selectedBodyIndex_ = 0;
        }
        ImGui::SetNextItemWidth(150);
        std::string bodyLabel = "Body " + std::to_string(bodies[selectedBodyIndex_].bodyId);
        if (ImGui::BeginCombo("##bodyCombo", bodyLabel.c_str())) {
            for (int i = 0; i < (int)bodies.size(); i++) {
                bool isSel = (selectedBodyIndex_ == i);
                std::string label = "Body " + std::to_string(bodies[i].bodyId);
                if (ImGui::Selectable(label.c_str(), isSel)) {
                    selectedBodyIndex_ = i;
                }
                if (isSel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    } else {
        selectedBodyIndex_ = 0;
    }

    const TrackedBody& body = bodies[selectedBodyIndex_];

    // Joint table (top half)
    drawJointTable(body);

    ImGui::Separator();

    // Skeleton preview (bottom half)
    ImGui::SeparatorText("Skeleton Preview");
    ImGui::BeginChild("SkeletonPreview", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);

    ImDrawList* drawList   = ImGui::GetWindowDrawList();
    ImVec2      canvasPos  = ImGui::GetCursorScreenPos();
    ImVec2      canvasSize = ImGui::GetContentRegionAvail();

    // Dark background
    drawList->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(20, 20, 30, 255));

    float marginX = canvasSize.x * 0.1f;
    float marginY = canvasSize.y * 0.1f;
    float scaleX  = canvasSize.x - 2.0f * marginX;
    float scaleY  = canvasSize.y - 2.0f * marginY;

    auto toScreen = [&](const Vector3D& pos) -> ImVec2 {
        return ImVec2(
            canvasPos.x + marginX + pos.x * scaleX,
            canvasPos.y + marginY + pos.y * scaleY);
    };

    // 17 bone connections
    static const JointType kJointBonePairs[17][2] = {
        {JointType::HEAD,           JointType::NECK},
        {JointType::NECK,           JointType::SHOULDER_LEFT},
        {JointType::NECK,           JointType::SHOULDER_RIGHT},
        {JointType::NECK,           JointType::SPINE_MID},
        {JointType::SHOULDER_LEFT,  JointType::ELBOW_LEFT},
        {JointType::SHOULDER_RIGHT, JointType::ELBOW_RIGHT},
        {JointType::ELBOW_LEFT,     JointType::WRIST_LEFT},
        {JointType::ELBOW_RIGHT,    JointType::WRIST_RIGHT},
        {JointType::WRIST_LEFT,     JointType::HAND_LEFT},
        {JointType::WRIST_RIGHT,    JointType::HAND_RIGHT},
        {JointType::SPINE_MID,      JointType::SPINE_BASE},
        {JointType::SPINE_BASE,     JointType::HIP_LEFT},
        {JointType::SPINE_BASE,     JointType::HIP_RIGHT},
        {JointType::HIP_LEFT,       JointType::KNEE_LEFT},
        {JointType::HIP_RIGHT,      JointType::KNEE_RIGHT},
        {JointType::KNEE_LEFT,      JointType::ANKLE_LEFT},
        {JointType::KNEE_RIGHT,     JointType::ANKLE_RIGHT},
    };

    // Draw bones
    for (const auto& pair : kJointBonePairs) {
        int pi = static_cast<int>(pair[0]);
        int ci = static_cast<int>(pair[1]);
        float avgConf = (body.joints[pi].confidence + body.joints[ci].confidence) * 0.5f;

        ImU32 color;
        if (avgConf > 0.7f)
            color = IM_COL32(0, 255, 0, 255);
        else if (avgConf > 0.3f)
            color = IM_COL32(255, 255, 0, 255);
        else
            color = IM_COL32(255, 0, 0, 255);

        drawList->AddLine(
            toScreen(body.joints[pi].position),
            toScreen(body.joints[ci].position),
            color, 2.0f);
    }

    // Draw joints
    for (int i = 0; i < static_cast<int>(JointType::COUNT); i++) {
        const Joint& joint = body.joints[i];
        float radius = 3.0f + joint.confidence * 9.0f;

        ImU32 color;
        if (joint.confidence > 0.7f)
            color = IM_COL32(0, 255, 0, 255);
        else if (joint.confidence > 0.3f)
            color = IM_COL32(255, 255, 0, 255);
        else
            color = IM_COL32(255, 0, 0, 255);

        drawList->AddCircleFilled(toScreen(joint.position), radius, color);
    }

    ImGui::EndChild();
}

// ============================================================
// Joint Table (inside Joints tab)
// ============================================================
void KinectPanel::drawJointTable(const TrackedBody& body) {
    ImGui::SeparatorText("Joint Data");

    if (!ImGui::BeginTable("JointTable", 5,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
        ImVec2(0, 200))) {
        return;
    }

    ImGui::TableSetupColumn("Joint", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("X",     ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("Y",     ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("Z",     ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("Conf",  ImGuiTableColumnFlags_WidthFixed, 50);
    ImGui::TableHeadersRow();

    for (int i = 0; i < static_cast<int>(JointType::COUNT); i++) {
        JointType type = static_cast<JointType>(i);
        const Joint& joint = body.joints[i];

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%s", jointTypeToLabel(type));

        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%.3f", joint.position.x);

        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.3f", joint.position.y);

        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.3f", joint.position.z);

        ImGui::TableSetColumnIndex(4);
        if (joint.confidence > 0.7f)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        else if (joint.confidence > 0.3f)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        else
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::Text("%.2f", joint.confidence);
        ImGui::PopStyleColor();
    }

    ImGui::EndTable();
}

// ============================================================
// Tab 3: Mappings (Master-Detail layout)
// ============================================================
void KinectPanel::drawMappingsTab() {
    drawMappingToolbar();
    ImGui::Separator();

    // Two-column master-detail: list (40%) | editor (60%)
    float totalWidth = ImGui::GetContentRegionAvail().x;
    ImGui::Columns(2, "kinectMappingCols", true);
    ImGui::SetColumnWidth(0, totalWidth * 0.38f);

    drawMappingList();

    ImGui::NextColumn();

    drawMappingEditorRight();

    ImGui::Columns(1);
}

// ============================================================
// Mapping Toolbar
// ============================================================
void KinectPanel::drawMappingToolbar() {
    if (ImGui::Button("+ Add")) {
        // Set editor to "new" mode with defaults
        editorIsNew_ = true;
        selectedMappingIndex_ = -1;
        memset(editJointSourceBuf_, 0, sizeof(editJointSourceBuf_));
        memset(editTargetParamBuf_, 0, sizeof(editTargetParamBuf_));
        editJointType_      = 0;
        editJointAxis_      = 0;
        editConditionType_  = 0;
        editTransformType_  = 0;
        editThreshold_      = 0.5f;
        editMinInput_       = 0.0f;
        editMaxInput_       = 1.0f;
        editMinOutput_      = 0.0f;
        editMaxOutput_      = 1.0f;
        editSmoothing_      = 0.0f;

        // Auto-populate joint source with default (head.x) so the user
        // can click "Add Mapping" immediately without touching the dropdown.
        snprintf(editJointSourceBuf_, sizeof(editJointSourceBuf_), "head.x");
    }
    ImGui::SameLine();

    if (ImGui::Button("- Delete") && selectedMappingIndex_ >= 0) {
        auto& mappings = mappingEngine_->getMappings();
        if (selectedMappingIndex_ < (int)mappings.size()) {
            mappingEngine_->removeMapping(mappings[selectedMappingIndex_].mappingId);
            selectedMappingIndex_ = -1;
            editorIsNew_ = true;
        }
    }
    ImGui::SameLine();

    if (ImGui::Button("Save")) {
        if (mappingEngine_->saveToJson("config/kinect-mappings.json")) {
            ofLogNotice("KinectPanel") << "Mappings saved";
        }
    }
    ImGui::SameLine();

    if (ImGui::Button("Load")) {
        mappingEngine_->loadFromJson("config/kinect-mappings.json");
    }

    ImGui::SameLine();
    mappingFilter_.Draw("Filter", 150);
}

// ============================================================
// Mapping List (left column)
// ============================================================
void KinectPanel::drawMappingList() {
    auto& mappings = mappingEngine_->getMappings();

    if (!ImGui::BeginTable("KinectMappingsList", 5,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
        return;
    }

    ImGui::TableSetupColumn("On",            ImGuiTableColumnFlags_WidthFixed, 25);
    ImGui::TableSetupColumn("Joint Source",  ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Target",        ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Cond",          ImGuiTableColumnFlags_WidthFixed, 60);
    ImGui::TableSetupColumn("Del",           ImGuiTableColumnFlags_WidthFixed, 25);
    ImGui::TableHeadersRow();

    for (int i = 0; i < (int)mappings.size(); i++) {
        auto& mapping = mappings[i];

        // Apply filter
        if (mappingFilter_.IsActive()) {
            if (!mappingFilter_.PassFilter(mapping.jointSource.c_str()) &&
                !mappingFilter_.PassFilter(mapping.targetParameterId.c_str())) {
                continue;
            }
        }

        ImGui::TableNextRow();

        // Highlight selected row
        if (selectedMappingIndex_ == i) {
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                ImGui::GetColorU32(ImVec4(0.3f, 0.5f, 0.8f, 0.4f)));
        }

        ImGui::PushID(mapping.mappingId.c_str());

        // Enabled checkbox (column 0)
        ImGui::TableSetColumnIndex(0);
        ImGui::Checkbox("##enabled", &mapping.enabled);

        // Joint Source (column 1) — click to select
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Selectable(mapping.jointSource.c_str(), false,
                ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
            selectedMappingIndex_ = i;
            editorIsNew_ = false;
            // Populate editor from mapping
            strncpy(editJointSourceBuf_, mapping.jointSource.c_str(), sizeof(editJointSourceBuf_) - 1);
            editJointSourceBuf_[sizeof(editJointSourceBuf_) - 1] = '\0';
            strncpy(editTargetParamBuf_, mapping.targetParameterId.c_str(), sizeof(editTargetParamBuf_) - 1);
            editTargetParamBuf_[sizeof(editTargetParamBuf_) - 1] = '\0';
            editConditionType_ = static_cast<int>(mapping.condition);
            editTransformType_ = static_cast<int>(mapping.transformType);
            editThreshold_     = mapping.threshold;
            editMinInput_      = mapping.minInput;
            editMaxInput_      = mapping.maxInput;
            editMinOutput_     = mapping.minOutput;
            editMaxOutput_     = mapping.maxOutput;
            editSmoothing_     = mapping.smoothing;
        }

        // Target Param (column 2)
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%s", mapping.targetParameterId.c_str());

        // Condition (column 3)
        ImGui::TableSetColumnIndex(3);
        const char* condLabel = "???";
        switch (mapping.condition) {
            case KinectCondition::ABOVE_THRESHOLD: condLabel = "ABOVE";     break;
            case KinectCondition::BELOW_THRESHOLD: condLabel = "BELOW";     break;
            case KinectCondition::IN_RANGE:        condLabel = "RANGE";     break;
            case KinectCondition::GESTURE:         condLabel = "GEST";      break;
            case KinectCondition::CONTINUOUS:      condLabel = "CONT";      break;
        }
        ImGui::Text("%s", condLabel);

        // Delete (column 4)
        ImGui::TableSetColumnIndex(4);
        if (ImGui::Button("X")) {
            mappingEngine_->removeMapping(mapping.mappingId);
            if (selectedMappingIndex_ == i) {
                selectedMappingIndex_ = -1;
                editorIsNew_ = true;
            } else if (selectedMappingIndex_ > i) {
                selectedMappingIndex_--;
            }
        }

        ImGui::PopID();
    }

    ImGui::EndTable();

    if (mappings.empty()) {
        ImGui::TextDisabled("No mappings. Click '+ Add' to create one.");
    }
}

// ============================================================
// Mapping Editor (right column)
// ============================================================
void KinectPanel::drawMappingEditorRight() {
    ImGui::BeginChild("KinectMappingEditor", ImVec2(0, 0), true);

    ImGui::Text("%s", editorIsNew_ ? "New Mapping" : "Edit Mapping");
    ImGui::Separator();

    // ---- Joint Source: type dropdown + axis dropdown ----
    ImGui::Text("Joint:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##jointType", jointTypeToLabel(static_cast<JointType>(editJointType_)))) {
        for (int i = 0; i < static_cast<int>(JointType::COUNT); i++) {
            bool selected = (i == editJointType_);
            if (ImGui::Selectable(jointTypeToLabel(static_cast<JointType>(i)), selected)) {
                editJointType_ = i;
                // Build joint source string using authoritative jointTypeToName
                std::string jointName = jointTypeToName(static_cast<JointType>(i));
                const char* axisLabels[] = {".x", ".y", ".z", ".conf"};
                std::string fullSource = jointName + axisLabels[editJointAxis_];
                strncpy(editJointSourceBuf_, fullSource.c_str(), sizeof(editJointSourceBuf_) - 1);
                editJointSourceBuf_[sizeof(editJointSourceBuf_) - 1] = '\0';
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Axis selector
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    const char* axisLabels[] = {"X", "Y", "Z", "Conf"};
    if (ImGui::Combo("##jointAxis", &editJointAxis_, axisLabels, 4)) {
        // Rebuild joint source string with new axis
        std::string jointName = jointTypeToName(static_cast<JointType>(editJointType_));
        const char* axLabels[] = {".x", ".y", ".z", ".conf"};
        std::string fullSource = jointName + axLabels[editJointAxis_];
        strncpy(editJointSourceBuf_, fullSource.c_str(), sizeof(editJointSourceBuf_) - 1);
        editJointSourceBuf_[sizeof(editJointSourceBuf_) - 1] = '\0';
    }

    // Manual override for joint source
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##jointSourceManual", editJointSourceBuf_, sizeof(editJointSourceBuf_));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Manual joint source, e.g. 'wrist_right.y' or 'head.conf'");
    }

    ImGui::Spacing();

    // ---- Target Parameter: dropdown from active scene ----
    bool isVec2XYMode = (editTransformType_ >= 4);  // VEC2_XY (4) / VEC3_XYZ (5)
    ImGui::Text("Target Parameter:");
    ImGui::SetNextItemWidth(-1);
    std::string comboPreview = editTargetParamBuf_[0] ? editTargetParamBuf_ : "(select parameter)";
    if (ImGui::BeginCombo("##targetCombo", comboPreview.c_str())) {
        auto activeScene = sceneEngine_->getActiveScene();
        if (activeScene) {
            std::string sceneId = activeScene->getId();
            
            // Section 1: FLOAT parameters
            auto paramIds = activeScene->getParameterIds();
            if (!paramIds.empty()) {
                ImGui::TextDisabled("Parameters");
                for (const auto& paramId : paramIds) {
                    std::string fullId = sceneId + "." + paramId;
                    bool isSel = (fullId == editTargetParamBuf_);
                    if (ImGui::Selectable(fullId.c_str(), isSel)) {
                        strncpy(editTargetParamBuf_, fullId.c_str(), sizeof(editTargetParamBuf_) - 1);
                        editTargetParamBuf_[sizeof(editTargetParamBuf_) - 1] = '\0';
                        auto param = activeScene->getParameter(paramId);
                        if (param) {
                            editMinOutput_ = param->min;
                            editMaxOutput_ = param->max;
                        }
                    }
                    if (isSel) ImGui::SetItemDefaultFocus();
                }
            }
            
            // Section 2: Interaction Points (Vec2 XY targets)
            auto ipIds = activeScene->getInteractionPointIds();
            if (!ipIds.empty()) {
                ImGui::Separator();
                ImGui::TextDisabled("Interaction Points (Vec2 XY)");
                for (const auto& ipId : ipIds) {
                    std::string fullId = sceneId + "." + ipId;
                    bool isSel = (fullId == editTargetParamBuf_);
                    if (ImGui::Selectable(fullId.c_str(), isSel)) {
                        strncpy(editTargetParamBuf_, fullId.c_str(), sizeof(editTargetParamBuf_) - 1);
                        editTargetParamBuf_[sizeof(editTargetParamBuf_) - 1] = '\0';
                    }
                    if (isSel) ImGui::SetItemDefaultFocus();
                }
            }
            
            // Section 3: Quick-add for Vec2 XY (common joint names)
            if (isVec2XYMode) {
                ImGui::Separator();
                ImGui::TextDisabled("Quick Names (Vec2 XY)");
                const char* commonNames[] = {
                    "head", "neck",
                    "shoulder_right", "shoulder_left",
                    "elbow_right", "elbow_left",
                    "wrist_right", "wrist_left",
                    "hand_right", "hand_left",
                    "spine_mid", "spine_base",
                    "hip_right", "hip_left",
                    "knee_right", "knee_left",
                    "ankle_right", "ankle_left"
                };
                for (const char* name : commonNames) {
                    std::string fullId = sceneId + "." + name;
                    bool isSel = (fullId == editTargetParamBuf_);
                    if (ImGui::Selectable(fullId.c_str(), isSel)) {
                        strncpy(editTargetParamBuf_, fullId.c_str(), sizeof(editTargetParamBuf_) - 1);
                        editTargetParamBuf_[sizeof(editTargetParamBuf_) - 1] = '\0';
                    }
                    if (isSel) ImGui::SetItemDefaultFocus();
                }
            }
        } else {
            ImGui::TextDisabled("(no active scene)");
        }
        ImGui::EndCombo();
    }
    if (isVec2XYMode && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Vec2 XY target: type a name (e.g. 'wrist_right').\n"
                         "The interaction point is created automatically when a joint maps to it.");
    }

    // Manual override for target
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##targetManual", editTargetParamBuf_, sizeof(editTargetParamBuf_));

    ImGui::Separator();

    // ---- Transform (includes VEC2_XY / VEC3_XYZ) ----
    ImGui::Text("Transform:");
    ImGui::SetNextItemWidth(-1);
    const char* xformLabels[] = {"Linear", "Exponential", "Logarithmic", "Invert", "Vec2 XY", "Vec3 XYZ"};
    ImGui::Combo("##xformType", &editTransformType_, xformLabels, 6);

    bool isVec2XY = (editTransformType_ >= 4);  // VEC2_XY (4) / VEC3_XYZ (5)

    // ---- Condition (hidden for VEC2_XY) ----
    if (!isVec2XY) {
        ImGui::Separator();
        ImGui::Text("Condition:");
        ImGui::SetNextItemWidth(-1);
        const char* condLabels[] = {"Above Threshold", "Below Threshold", "In Range", "Gesture", "Continuous"};
        ImGui::Combo("##condType", &editConditionType_, condLabels, 5);

        KinectCondition cond = static_cast<KinectCondition>(editConditionType_);

        if (cond == KinectCondition::ABOVE_THRESHOLD || cond == KinectCondition::BELOW_THRESHOLD) {
            ImGui::SliderFloat("Threshold", &editThreshold_, 0.0f, 1.0f, "%.2f");
        }

        if (cond == KinectCondition::IN_RANGE) {
            ImGui::SliderFloat("Range Min", &editMinInput_, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Range Max", &editMaxInput_, 0.0f, 1.0f, "%.2f");
            if (editMinInput_ > editMaxInput_) editMinInput_ = editMaxInput_;
        }

        if (cond == KinectCondition::CONTINUOUS) {
            ImGui::SliderFloat("Input Min", &editMinInput_, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Input Max", &editMaxInput_, 0.0f, 1.0f, "%.2f");
            if (editMinInput_ > editMaxInput_) editMinInput_ = editMaxInput_;
        }
    }

    // ---- Output Range (hidden for VEC2_XY) ----
    if (!isVec2XY) {
        ImGui::Separator();
        ImGui::Text("Output Range:");
        ImGui::SliderFloat("Output Min", &editMinOutput_, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Output Max", &editMaxOutput_, 0.0f, 1.0f, "%.2f");
        if (editMinOutput_ > editMaxOutput_) editMinOutput_ = editMaxOutput_;
    }

    // ---- Smoothing (hidden for VEC2_XY) ----
    if (!isVec2XY) {
        ImGui::SliderFloat("Smoothing", &editSmoothing_, 0.0f, 1.0f, "%.2f");
    }

    ImGui::Separator();

    // ---- Add / Update button ----
    if (editorIsNew_) {
        if (ImGui::Button("Add Mapping", ImVec2(-1, 0))) {
            if (strlen(editJointSourceBuf_) > 0 && strlen(editTargetParamBuf_) > 0) {
                KinectMapping mapping;
                mapping.jointSource       = editJointSourceBuf_;
                mapping.targetParameterId = editTargetParamBuf_;
                mapping.condition         = static_cast<KinectCondition>(editConditionType_);
                mapping.threshold         = editThreshold_;
                mapping.minRange          = editMinInput_;
                mapping.maxRange          = editMaxInput_;
                mapping.minInput          = editMinInput_;
                mapping.maxInput          = editMaxInput_;
                mapping.minOutput         = editMinOutput_;
                mapping.maxOutput         = editMaxOutput_;
                mapping.transformType     = static_cast<KinectTransformType>(editTransformType_);
                mapping.smoothing         = editSmoothing_;
                mapping.enabled           = true;

                mappingEngine_->addMapping(mapping);

                // Reset editor
                memset(editJointSourceBuf_, 0, sizeof(editJointSourceBuf_));
                memset(editTargetParamBuf_, 0, sizeof(editTargetParamBuf_));
                editJointType_      = 0;
                editJointAxis_      = 0;
                editConditionType_  = 0;
                editTransformType_  = 0;
                editThreshold_      = 0.5f;
                editMinInput_       = 0.0f;
                editMaxInput_       = 1.0f;
                editMinOutput_      = 0.0f;
                editMaxOutput_      = 1.0f;
                editSmoothing_      = 0.0f;
            }
        }
    } else {
        if (ImGui::Button("Update Mapping", ImVec2(-1, 0)) && selectedMappingIndex_ >= 0) {
            auto& mappings = mappingEngine_->getMappings();
            if (selectedMappingIndex_ < (int)mappings.size()) {
                auto& mapping = mappings[selectedMappingIndex_];
                mapping.jointSource       = editJointSourceBuf_;
                mapping.targetParameterId = editTargetParamBuf_;
                mapping.condition         = static_cast<KinectCondition>(editConditionType_);
                mapping.threshold         = editThreshold_;
                mapping.minRange          = editMinInput_;
                mapping.maxRange          = editMaxInput_;
                mapping.minInput          = editMinInput_;
                mapping.maxInput          = editMaxInput_;
                mapping.minOutput         = editMinOutput_;
                mapping.maxOutput         = editMaxOutput_;
                mapping.transformType     = static_cast<KinectTransformType>(editTransformType_);
                mapping.smoothing         = editSmoothing_;
            }
        }
    }

    ImGui::EndChild();
}

// ============================================================
// Tab 4: Quick Map — click-to-assign visual skeleton
// ============================================================
void KinectPanel::drawQuickMapTab() {
    // --- Guard: no active scene ---
    auto activeScene = sceneEngine_->getActiveScene();
    if (!activeScene) {
        ImGui::TextDisabled("No active scene — activate a scene first");
        return;
    }

    auto sceneIds = sceneEngine_->getLoadedScenes();
    if (sceneIds.empty()) {
        ImGui::TextDisabled("No scenes loaded");
        return;
    }

    // --- Scene selector ---
    if (quickSceneIndex_ >= (int)sceneIds.size()) quickSceneIndex_ = 0;
    ImGui::SetNextItemWidth(250);
    if (ImGui::BeginCombo("Scene", sceneIds[quickSceneIndex_].c_str())) {
        for (int i = 0; i < (int)sceneIds.size(); i++) {
            bool sel = (quickSceneIndex_ == i);
            if (ImGui::Selectable(sceneIds[i].c_str(), sel)) quickSceneIndex_ = i;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    std::string selectedSceneId = sceneIds[quickSceneIndex_];

    // --- Preset selector ---
    auto presets = presetManager_->getPresets(selectedSceneId);
    std::vector<const char*> presetNames;
    presetNames.push_back("(none)");
    for (const auto& p : presets) presetNames.push_back(p.name.c_str());

    int presetComboIdx = quickPresetIndex_ + 1;  // +1 for "(none)" offset
    if (presetComboIdx >= (int)presetNames.size()) {
        presetComboIdx = 0;
        quickPresetIndex_ = -1;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::BeginCombo("Preset", presetNames[presetComboIdx])) {
        for (int i = 0; i < (int)presetNames.size(); i++) {
            bool sel = (presetComboIdx == i);
            if (ImGui::Selectable(presetNames[i], sel)) {
                quickPresetIndex_ = i - 1;  // -1 for "(none)"
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (quickPresetIndex_ < 0) {
        ImGui::SameLine();
        ImGui::TextDisabled("(no preset — maps engine only)");
    }

    // --- Sync engine mappings when preset or scene changes ---
    bool sceneChanged = (quickSceneIndex_ != quickSceneIndexPrev_);
    bool presetChanged = (quickPresetIndex_ != quickPresetIndexPrev_);

    if (presetChanged || sceneChanged) {
        quickSceneIndexPrev_ = quickSceneIndex_;
        quickPresetIndexPrev_ = quickPresetIndex_;

        if (sceneChanged && !presetChanged) {
            // Scene changed but preset index is the same (e.g., both scenes have
            // at least one preset and index 0 is valid). Force a full reload.
            mappingEngine_->removeMappingsByScene(selectedSceneId);
            auto presetsNow = presetManager_->getPresets(selectedSceneId);
            if (quickPresetIndex_ >= 0 && quickPresetIndex_ < (int)presetsNow.size()) {
                auto scene = sceneEngine_->getScene(selectedSceneId);
                if (scene) scene->clearInteractionPoints();
                mappingEngine_->addMappings(presetsNow[quickPresetIndex_].kinectMappings);
                ofLogNotice("KinectPanel")
                    << "Scene sync: scene='" << selectedSceneId << "'"
                    << " preset[" << quickPresetIndex_ << "]='"
                    << presetsNow[quickPresetIndex_].name << "'"
                    << " presaved=" << presetsNow[quickPresetIndex_].kinectMappings.size();
            }
        } else if (presetChanged) {
            // Count engine mappings for this scene before sync
            size_t before = 0;
            for (const auto& m : mappingEngine_->getMappings()) {
                if (m.enabled && m.targetParameterId.compare(0, selectedSceneId.size(), selectedSceneId) == 0) {
                    before++;
                }
            }
            
            if (quickPresetIndex_ >= 0 && quickPresetIndex_ < (int)presets.size()) {
                // Clear old interaction points before loading new preset's mappings
                auto scene = sceneEngine_->getScene(selectedSceneId);
                if (scene) {
                    scene->clearInteractionPoints();
                }
                
                mappingEngine_->removeMappingsByScene(selectedSceneId);
                mappingEngine_->addMappings(presets[quickPresetIndex_].kinectMappings);
                
                // Count engine mappings after sync
                size_t after = 0;
                for (const auto& m : mappingEngine_->getMappings()) {
                    if (m.enabled && m.targetParameterId.compare(0, selectedSceneId.size(), selectedSceneId) == 0) {
                        after++;
                    }
                }
                
                ofLogNotice("KinectPanel") 
                    << "Preset sync: preset[" << quickPresetIndex_ << "]='"
                    << presets[quickPresetIndex_].name << "'"
                    << " presaved=" << presets[quickPresetIndex_].kinectMappings.size()
                    << " engineBefore=" << before
                    << " engineAfter=" << after;
            } else {
                ofLogNotice("KinectPanel") 
                    << "Preset sync: NONE selected"
                    << " engineBefore=" << before;
            }
        }
    }

    ImGui::Separator();

    // --- Static default pose for config mode (no tracking) ---
    // Same normalized positions as MockKinectAdapter::kDefaultPose.
    static const Vector3D kDefaultPose[] = {
        {0.50f, 0.10f, 0.50f}, // HEAD
        {0.50f, 0.18f, 0.50f}, // NECK
        {0.30f, 0.20f, 0.48f}, // SHOULDER_LEFT  (open)
        {0.70f, 0.20f, 0.52f}, // SHOULDER_RIGHT (open)
        {0.18f, 0.24f, 0.47f}, // ELBOW_LEFT     (extended)
        {0.82f, 0.24f, 0.53f}, // ELBOW_RIGHT    (extended)
        {0.10f, 0.28f, 0.46f}, // WRIST_LEFT     (extended)
        {0.90f, 0.28f, 0.54f}, // WRIST_RIGHT    (extended)
        {0.06f, 0.28f, 0.45f}, // HAND_LEFT      (fully open)
        {0.94f, 0.28f, 0.55f}, // HAND_RIGHT     (fully open)
        {0.44f, 0.52f, 0.48f}, // HIP_LEFT
        {0.56f, 0.52f, 0.52f}, // HIP_RIGHT
        {0.38f, 0.72f, 0.48f}, // KNEE_LEFT      (spread)
        {0.62f, 0.72f, 0.52f}, // KNEE_RIGHT     (spread)
        {0.30f, 0.93f, 0.49f}, // ANKLE_LEFT     (spread)
        {0.70f, 0.93f, 0.51f}, // ANKLE_RIGHT    (spread)
        {0.50f, 0.52f, 0.50f}, // SPINE_BASE
        {0.50f, 0.35f, 0.50f}, // SPINE_MID
    };

    // --- Build static reference body (Quick Map always uses fixed pose) ---
    static const int kJointCount = static_cast<int>(JointType::COUNT);
    TrackedBody refBody;
    for (int i = 0; i < kJointCount; i++) {
        refBody.joints[i].type       = static_cast<JointType>(i);
        refBody.joints[i].position   = kDefaultPose[i];
        refBody.joints[i].confidence = 1.0f;
    }
    const TrackedBody& body = refBody;

    // --- Skeleton canvas ---
    ImGui::BeginChild("QuickMapSkeleton", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);

    ImDrawList* drawList   = ImGui::GetWindowDrawList();
    ImVec2      canvasPos  = ImGui::GetCursorScreenPos();
    ImVec2      canvasSize = ImGui::GetContentRegionAvail();

    // Dark background
    drawList->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(20, 20, 30, 255));

    float marginX = canvasSize.x * 0.1f;
    float marginY = canvasSize.y * 0.1f;
    float scaleX  = canvasSize.x - 2.0f * marginX;
    float scaleY  = canvasSize.y - 2.0f * marginY;

    auto toScreen = [&](const Vector3D& pos) -> ImVec2 {
        return ImVec2(
            canvasPos.x + marginX + pos.x * scaleX,
            canvasPos.y + marginY + pos.y * scaleY);
    };

    // --- Build set of mapped joint names for gold-ring detection ---
    // Filter by selected scene so we don't show mappings from other scenes.
    std::string scenePrefix = selectedSceneId + ".";
    std::set<std::string> mappedJoints;
    for (const auto& m : mappingEngine_->getMappings()) {
        if (m.transformType == KinectTransformType::VEC2_XY && m.enabled) {
            size_t dot = m.targetParameterId.rfind('.');
            if (dot != std::string::npos) {
                std::string jointName = m.targetParameterId.substr(dot + 1);
                // Only count if mapping belongs to the currently selected scene
                if (m.targetParameterId.compare(0, scenePrefix.size(), scenePrefix) == 0) {
                    mappedJoints.insert(jointName);
                }
            }
        }
    }

    // --- 17 bone connections (same as Joints tab) ---
    static const JointType kJointBonePairs[17][2] = {
        {JointType::HEAD,           JointType::NECK},
        {JointType::NECK,           JointType::SHOULDER_LEFT},
        {JointType::NECK,           JointType::SHOULDER_RIGHT},
        {JointType::NECK,           JointType::SPINE_MID},
        {JointType::SHOULDER_LEFT,  JointType::ELBOW_LEFT},
        {JointType::SHOULDER_RIGHT, JointType::ELBOW_RIGHT},
        {JointType::ELBOW_LEFT,     JointType::WRIST_LEFT},
        {JointType::ELBOW_RIGHT,    JointType::WRIST_RIGHT},
        {JointType::WRIST_LEFT,     JointType::HAND_LEFT},
        {JointType::WRIST_RIGHT,    JointType::HAND_RIGHT},
        {JointType::SPINE_MID,      JointType::SPINE_BASE},
        {JointType::SPINE_BASE,     JointType::HIP_LEFT},
        {JointType::SPINE_BASE,     JointType::HIP_RIGHT},
        {JointType::HIP_LEFT,       JointType::KNEE_LEFT},
        {JointType::HIP_RIGHT,      JointType::KNEE_RIGHT},
        {JointType::KNEE_LEFT,      JointType::ANKLE_LEFT},
        {JointType::KNEE_RIGHT,     JointType::ANKLE_RIGHT},
    };

    // Draw bones
    for (const auto& pair : kJointBonePairs) {
        int pi = static_cast<int>(pair[0]);
        int ci = static_cast<int>(pair[1]);

        drawList->AddLine(
            toScreen(body.joints[pi].position),
            toScreen(body.joints[ci].position),
            IM_COL32(100, 100, 120, 255), 2.0f);
    }

    // Draw joints + invisible buttons for easy clicking.
    // Each joint gets a styled circle, gold ring if mapped,
    // then an InvisibleButton (28x28) with left-click assign / right-click unassign.
    const float kButtonSize = 28.0f;
    const float kButtonHalf  = kButtonSize * 0.5f;
    const float kJointR    = 7.0f;
    const float kGoldRingR = 11.0f;

    for (int i = 0; i < static_cast<int>(JointType::COUNT); i++) {
        const Joint& joint = body.joints[i];
        ImVec2 jointScreen = toScreen(joint.position);
        std::string jointName = jointTypeToName(static_cast<JointType>(i));
        bool isMapped = (mappedJoints.find(jointName) != mappedJoints.end());

        // Invisible button for easy hit-testing (place BEFORE drawing)
        ImGui::SetCursorScreenPos(
            ImVec2(jointScreen.x - kButtonHalf, jointScreen.y - kButtonHalf));
        std::string btnId = "##joint_" + jointName;
        ImGui::InvisibleButton(btnId.c_str(), ImVec2(kButtonSize, kButtonSize));

        bool hovered = ImGui::IsItemHovered();

        // --- Draw: hover highlight ring ---
        if (hovered) {
            drawList->AddCircleFilled(jointScreen, kJointR + 5.0f,
                IM_COL32(255, 255, 255, 80));
            drawList->AddCircle(jointScreen, kJointR + 5.0f,
                IM_COL32(255, 255, 255, 200), 0, 2.0f);
        }

        // --- Draw: gold ring for mapped joints ---
        if (isMapped) {
            drawList->AddCircleFilled(jointScreen, kGoldRingR,
                IM_COL32(255, 215, 0, 100));
            drawList->AddCircle(jointScreen, kGoldRingR,
                IM_COL32(255, 215, 0, 255), 0, 2.5f);
            drawList->AddCircleFilled(jointScreen, kJointR,
                IM_COL32(255, 215, 0, 255));
        } else {
            drawList->AddCircleFilled(jointScreen, kJointR,
                IM_COL32(100, 200, 255, 255));
        }

        // Tooltip on hover
        if (hovered) {
            ImGui::SetTooltip("%s%s",
                jointName.c_str(),
                isMapped ? " (mapped — right-click to remove)" : " (click to map)");
        }

        // Left click → assign
        if (ImGui::IsItemClicked(0)) {
            std::string targetId = selectedSceneId + "." + jointName;

            KinectMapping mapping;
            mapping.jointSource       = jointName + ".x";
            mapping.targetParameterId = targetId;
            mapping.condition         = KinectCondition::CONTINUOUS;
            mapping.transformType     = KinectTransformType::VEC2_XY;
            mapping.enabled           = true;

            std::string id = mappingEngine_->addMapping(mapping);
            if (!id.empty()) {
                if (quickPresetIndex_ >= 0 && quickPresetIndex_ < (int)presets.size()) {
                    auto mutablePresets = presetManager_->getPresets(selectedSceneId);
                    if (quickPresetIndex_ < (int)mutablePresets.size()) {
                        mutablePresets[quickPresetIndex_].kinectMappings.push_back(mapping);
                        presetManager_->addOrUpdatePreset(selectedSceneId,
                            mutablePresets[quickPresetIndex_]);
                        presetManager_->savePresets(selectedSceneId);
                    }
                }
                toastMessage_ = "Mapped: " + targetId;
                toastTimer_ = 2.0f;
            } else {
                toastMessage_ = "Already mapped: " + targetId;
                toastTimer_ = 2.0f;
            }
        }

        // Right click → unassign
        if (ImGui::IsItemClicked(1)) {
            // Remove from engine: match any scene's mapping ending with .jointName
            std::string suffix = "." + jointName;
            auto& mappings = mappingEngine_->getMappings();
            for (auto it = mappings.begin(); it != mappings.end(); ++it) {
                if (it->transformType == KinectTransformType::VEC2_XY &&
                    it->targetParameterId.size() >= suffix.size() &&
                    it->targetParameterId.compare(it->targetParameterId.size() - suffix.size(),
                                                   suffix.size(), suffix) == 0) {
                    mappingEngine_->removeMapping(it->mappingId);
                    // Also remove from selected preset
                    if (quickPresetIndex_ >= 0 && quickPresetIndex_ < (int)presets.size()) {
                        auto mutablePresets = presetManager_->getPresets(selectedSceneId);
                        if (quickPresetIndex_ < (int)mutablePresets.size()) {
                            auto& km = mutablePresets[quickPresetIndex_].kinectMappings;
                            km.erase(std::remove_if(km.begin(), km.end(),
                                [&](const KinectMapping& m) {
                                    return m.targetParameterId.size() >= suffix.size() &&
                                           m.targetParameterId.compare(
                                               m.targetParameterId.size() - suffix.size(),
                                               suffix.size(), suffix) == 0;
                                }),
                                km.end());
                            presetManager_->addOrUpdatePreset(selectedSceneId,
                                mutablePresets[quickPresetIndex_]);
                            presetManager_->savePresets(selectedSceneId);
                        }
                    }
                    toastMessage_ = "Removed: " + jointName;
                    toastTimer_ = 2.0f;
                    break;
                }
            }
        }
    }

    ImGui::EndChild();

    // --- Mapped joints summary ---
    if (!mappedJoints.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "Mapped:");
        ImGui::SameLine();
        std::string mappedList;
        for (const auto& name : mappedJoints) {
            if (!mappedList.empty()) mappedList += ", ";
            mappedList += name;
        }
        ImGui::TextWrapped("%s", mappedList.c_str());
    }

    // --- Toast message ---
    if (toastTimer_ > 0.0f) {
        ImGui::Spacing();
        float alpha = std::min(1.0f, toastTimer_);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, alpha));
        ImGui::Text("%s", toastMessage_.c_str());
        ImGui::PopStyleColor();
    }
}

// ============================================================
// Camera Scanner
// ============================================================
void KinectPanel::scanCameras() {
    if (camerasScanned_ && !cameraNames_.empty()) return;

    ofVideoGrabber grabber;
    auto devices = grabber.listDevices();
    cameraNames_.clear();
    for (const auto& dev : devices) {
        cameraNames_.push_back(dev.deviceName + " [" + std::to_string(dev.id) + "]");
    }
    if (cameraNames_.empty()) {
        cameraNames_.push_back("No cameras found");
    }
    camerasScanned_ = true;
}

// ============================================================
// Bridge Launcher
// ============================================================
void KinectPanel::launchPoseBridge() {
    if (bridgeRunning_) return;

    // Resolve project root: try exe-relative first, fallback to cwd
    std::string exeDir = ofFilePath::getCurrentExeDir();
    std::string projectRoot = ofFilePath::join(exeDir, "../../../..");
    ofFile testScript(ofFilePath::join(projectRoot, "mediapipe_pose_bridge.py"));
    if (!testScript.exists()) {
        projectRoot = ofFilePath::getCurrentWorkingDirectory();
    }

    std::string python = ofFilePath::join(projectRoot, "venv_mediapipe/bin/python3");
    std::string script = ofFilePath::join(projectRoot, "mediapipe_pose_bridge.py");

    ofFile pyFile(python);
    ofFile scrFile(script);
    if (!pyFile.exists()) {
        ofLogError("KinectPanel") << "Python not found: " << python
                                  << " (projectRoot=" << projectRoot << ")";
        return;
    }
    if (!scrFile.exists()) {
        ofLogError("KinectPanel") << "Bridge script not found: " << script
                                  << " (projectRoot=" << projectRoot << ")";
        return;
    }

    std::string sourceArg = (sourceType_ == 1) ? "--source kinect" : "--source webcam";
    std::string cameraArg = (sourceType_ == 0) ? " --camera " + std::to_string(cameraId_) : "";

    // Launch via osascript → Terminal.app so macOS TCC allows camera access.
    // Direct system() runs Python as child of ProyectoDanza2026.app which
    // has no camera permission and crashes with SIGABRT (TCC violation).
    std::string fullCmd = "cd \"" + projectRoot + "\" && "
                        + "\"" + python + "\" \"" + script + "\" "
                        + sourceArg + cameraArg
                        + " --detection-confidence 0.50 --tracking-confidence 0.50";
    // Escape double-quotes and backslashes for AppleScript string literal
    ofStringReplace(fullCmd, "\\", "\\\\");
    ofStringReplace(fullCmd, "\"", "\\\"");
    std::string cmd = "osascript -e 'tell application \"Terminal\" to do script \""
                    + fullCmd + "\"' > /dev/null 2>&1 &";

    system(cmd.c_str());
    bridgeRunning_ = true;
    ofLogNotice("KinectPanel") << "Pose bridge launched in Terminal window | " << sourceArg;
}

void KinectPanel::stopPoseBridge() {
    if (!bridgeRunning_) return;
    system("pkill -f mediapipe_pose_bridge.py 2>/dev/null");
    bridgeRunning_ = false;
    ofLogNotice("KinectPanel") << "Pose bridge stopped";
}

bool KinectPanel::isBridgeRunning() const {
    return bridgeRunning_;
}

// ============================================================
// Visibility
// ============================================================
void KinectPanel::setVisible(bool visible) {
    visible_ = visible;
}

bool KinectPanel::isVisible() const {
    return visible_;
}

// ============================================================
// Helpers
// ============================================================
const char* KinectPanel::jointTypeToLabel(JointType type) const {
    switch (type) {
        case JointType::HEAD:           return "Head";
        case JointType::NECK:           return "Neck";
        case JointType::SHOULDER_LEFT:  return "Shoulder L";
        case JointType::SHOULDER_RIGHT: return "Shoulder R";
        case JointType::ELBOW_LEFT:     return "Elbow L";
        case JointType::ELBOW_RIGHT:    return "Elbow R";
        case JointType::WRIST_LEFT:     return "Wrist L";
        case JointType::WRIST_RIGHT:    return "Wrist R";
        case JointType::HAND_LEFT:      return "Hand L";
        case JointType::HAND_RIGHT:     return "Hand R";
        case JointType::HIP_LEFT:       return "Hip L";
        case JointType::HIP_RIGHT:      return "Hip R";
        case JointType::KNEE_LEFT:      return "Knee L";
        case JointType::KNEE_RIGHT:     return "Knee R";
        case JointType::ANKLE_LEFT:     return "Ankle L";
        case JointType::ANKLE_RIGHT:    return "Ankle R";
        case JointType::SPINE_BASE:     return "Spine Base";
        case JointType::SPINE_MID:      return "Spine Mid";
        default:                        return "Unknown";
    }
}

const char* KinectPanel::connectionStateLabel() const {
    switch (adapter_->getConnectionState()) {
        case ConnectionState::DISCONNECTED: return "Disconnected";
        case ConnectionState::CONNECTING:   return "Connecting...";
        case ConnectionState::CONNECTED:    return "Connected";
        case ConnectionState::ERROR:        return "Error";
        default:                            return "Unknown";
    }
}

ImVec4 KinectPanel::connectionStateColor() const {
    switch (adapter_->getConnectionState()) {
        case ConnectionState::CONNECTED:    return ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        case ConnectionState::CONNECTING:   return ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
        case ConnectionState::DISCONNECTED: return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        case ConnectionState::ERROR:        return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        default:                            return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

} // namespace modular
