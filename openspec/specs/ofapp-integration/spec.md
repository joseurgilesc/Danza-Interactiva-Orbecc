# ofApp Integration Specification

## Purpose

Wiring of Kinect components into the main application. Defines instantiation order, callback connections, lifecycle hooks, and architectural boundaries.

## Requirements

### Requirement: Component Instantiation in setup()

ofApp MUST instantiate `OpenNIKinectAdapter` (or `MockKinectAdapter`), `KinectMappingEngine` (with `SceneEngine` pointer), and `SkeletonView` in `setup()`. Order MUST be: SceneEngine created → KinectMappingEngine created → Kinect adapter created → SkeletonView created.

#### Scenario: Full instantiation
- GIVEN app enters setup()
- WHEN all components are created
- THEN `kinectAdapter_`, `kinectMappingEngine_`, and `skeletonView_` are valid pointers

### Requirement: Callback Wiring

ofApp MUST wire kinect adapter callbacks to `KinectMappingEngine::processJointData()`. `setJointCallback` SHALL forward joint data. `setBodyCallback` SHALL handle body tracking state. `setHandCallback` SHALL forward hand state.

#### Scenario: Callback chain
- GIVEN all components instantiated
- WHEN kinect adapter fires `jointCallback(bodyId=1, type=WRIST_RIGHT, data)`
- THEN `KinectMappingEngine::processJointData(1, WRIST_RIGHT, data)` is called

### Requirement: UI Panel Registration

ofApp MUST add `KinectPanel` to UIManager via `addModule()` passing the `IKinectAdapter` and `KinectMappingEngine` pointers.

#### Scenario: Panel registration
- GIVEN UIManager exists and KinectPanel is created
- WHEN `uiManager.addModule(kinectPanel)` is called
- THEN panel appears in UIManager dock layout

### Requirement: Per-Frame Update

ofApp MUST call `kinectAdapter->update()` and `kinectMappingEngine->update()` each frame in `ofApp::update()`. Mapping engine update SHALL come after adapter update.

#### Scenario: Update order
- GIVEN app is running
- WHEN `ofApp::update()` executes
- THEN adapter update processes skeleton → mapping engine update dispatches parameters → scenes receive values

### Requirement: SkeletonView Drawing

ofApp MUST call `skeletonView->setTrackedBodies(adapter->getAllTrackedBodies())` before `skeletonView->draw()` in `ofApp::draw()`. SkeletonView SHALL render with `ofPushView/ofPopView` to avoid coordinate interference.

#### Scenario: Draw pipeline
- GIVEN adapter returns one tracked body
- WHEN `ofApp::draw()` executes
- THEN SkeletonView draws the tracked body over the scene viewport

### Requirement: Architectural Boundary — No Direct Kinect in Scenes

ofApp MUST NOT pass Kinect data directly to any scene. Parameter transmission MUST exclusively use `ParameterRegistry`. `KinectMappingEngine` SHALL be the sole bridge.

#### Scenario: Scene isolation
- GIVEN a scene receives parameter "flockSpeed" = 1.5
- WHEN inspecting scene code
- THEN scene has no `#include` of Kinect headers and no knowledge of Kinect source

### Requirement: Persistence Lifecycle

ofApp MUST load mappings via `kinectMappingEngine->loadFromJson()` after scene registration in `setup()`. ofApp MUST save mappings via `kinectMappingEngine->saveToJson()` in `ofApp::exit()`.

#### Scenario: Full lifecycle
- GIVEN app starts with saved mappings
- WHEN setup() completes and exit() is called
- THEN saved mappings are loaded at startup and current mappings are saved on exit
