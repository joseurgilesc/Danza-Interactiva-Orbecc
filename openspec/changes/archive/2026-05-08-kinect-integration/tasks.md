# Tasks: Kinect Integration

## Phase 1: Foundation — Types & Contract (no hardware)
- [x] 1.1 Create `src/adapters/kinect/KinectTypes.h` — JointType enum (18 joints, COUNT), Vector3D, Joint, TrackedBody, HandState, ConnectionState. Zero SDK deps.
- [x] 1.2 Create `src/adapters/kinect/IKinectAdapter.h` — Abstract interface: initialize/shutdown/getConnectionState/update, 5 std::function callback setters, getAllTrackedBodies/getRawJointPosition polling, config (setSmoothing/setConfidenceThreshold/setZoneGrid). Zero SDK deps.

## Phase 2: Mock Adapter (dev without hardware)
- [x] 2.1 Create `src/adapters/kinect/MockKinectAdapter.h` — Implements IKinectAdapter. SimMode(IDLE/WALKING/WAVING), setMode(), setMockJointPosition(). Zero SDK.
- [x] 2.2 Create `src/adapters/kinect/MockKinectAdapter.cpp` — init always true. update(): bodyId=1 all joints 0.5. IDLE=static, WALKING=y-sin 0.3-0.7, WAVING=HAND_RIGHT xy-osc. Fire callbacks each frame.
- [x] 2.3 Verify `make` — Types, interface, mock compile without OpenNI.

## Phase 3: Skeleton Visualization
- [x] 3.1 Create `src/input/sensors/SkeletonView.h` — draw(), setVisible(bool), isVisible(), confidenceColor(), 17 bone connections table.
- [x] 3.2 Create `src/input/sensors/SkeletonView.cpp` — ofDrawCircle joints (3-12px radius by conf), ofDrawLine bones (avg conf color: green>0.7, yel 0.3-0.7, red<0.3), mirror H (screenX=1-normX×vpW), "Tracking: ON/LOST" overlay.

## Phase 4: Mapping Engine
- [x] 4.1 Create `src/mapping/KinectMappingTypes.h` — KinectCondition (ABOVE/BELOW_THRESHOLD, IN_RANGE, GESTURE, CONTINUOUS), KinectTransformType (LINEAR/EXP/LOG/INVERT), KinectMapping struct.
- [x] 4.2 Create `src/mapping/KinectMappingEngine.h` — CRUD (addMapping rejects dup jointSource+target, removeMapping, findMapping, getMappings), processJointData(), update(delta), saveToJson/loadFromJson. Ctor(SceneEngine*).
- [x] 4.3 Create `src/mapping/KinectMappingEngine.cpp` — processJointData: parse axis from source string→eval condition→apply transform→EMA smooth→scene->setParameter(localId,value). JSON: array of mappings, excludes lastValue. Auto-create config/, handle missing/corrupt gracefully.

## Phase 5: UI Panel & Dock Registration
- [x] 5.1 Create `src/ui/KinectPanel.h` — Implements IUIModule. Ctor(IKinectAdapter*, KinectMappingEngine*, SceneEngine*). Return panel_ids::kKinect.
- [x] 5.2 Create `src/ui/KinectPanel.cpp` — Status bar (color+Connect/Disconnect), bodies table (selectable), joint detail (18 rows X/Y/Z/conf colored), mapping editor (dropdowns, condition/threshold/transform, Add/Remove/Save/Load), mapping list (toggles), skeleton mini-preview. Controls disabled when !connected.
- [x] 5.3 Modify `src/ui/PanelIds.h` — Add `static const char* kKinect = "Kinect";`.
- [x] 5.4 Modify `src/ui/UIManager.h` — Add `ImGuiID kinectDockId_ = 0;`.
- [x] 5.5 Modify `src/ui/UIManager.cpp` — `buildDefaultDockLayout()`: split paramsDockId_ ImGuiDir_Right 0.4f→kinectDockId_; dock `panel_ids::kKinect`.

## Phase 6: ofApp Wiring
- [x] 6.1 Modify `src/ofApp.h` — #include Kinect headers; add unique_ptrs: kinectAdapter_, kinectMappingEngine_, skeletonView_.
- [x] 6.2 Modify `src/ofApp.cpp` — setup: create engine+MockAdapter after OSC, wire JointCallback→processJointData, init, loadFromJson, create SkeletonView+KinectPanel→addModule. update: adapter→engine→scenes. draw: skeletonView.draw() between scene+UI. exit: saveToJson, shutdown.

## Phase 7: Hardware Adapter (Nuitrack — replaces OpenNI fallback)
- [x] 7.1 Create `src/adapters/kinect/NuitrackKinectAdapter.h` — IKinectAdapter with PIMPL, Nuitrack forward-declarations, 18-joint mapping, coordinate conversion. Zero Nuitrack SDK in header.
- [x] 7.2 Create `src/adapters/kinect/NuitrackKinectAdapter.cpp` — `#ifdef HAS_NUITRACK` full impl: init/shutdown/update via Nuitrack SDK, processNuitrackSkeletons with joint filter+mapping+coordinate conversion, thread-safe body cache, PIMPL. Stub without SDK logs warning.
- [x] 7.3 Modify `config.make` — Add commented Nuitrack SDK paths (`-DHAS_NUITRACK`, lib paths). Stub compiles by default without Nuitrack.

## Phase 8: Documentation
- [x] 8.1 Create `docs/kinect-setup.md` — macOS step-by-step: Nuitrack SDK install, license activation, Kinect v1 hardware connection, config.make setup, build/run instructions. Troubleshooting table.

## Phase 9: Integration Verification
- [x] 9.1 `make` — Zero errors/warnings with MockAdapter active.
- [x] 9.2 Compile-time verification — MockKinectAdapter, SkeletonView, KinectPanel, KinectMappingEngine all compile+link. ofApp wiring verifies. config.make Nuitrack commented (mock-only build).
- [x] 9.3 Final status — All task checkmarks updated. Build verified (zero errors, zero warnings). Apply progress saved to engram.
