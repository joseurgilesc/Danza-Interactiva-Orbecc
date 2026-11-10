#include "ofApp.h"

#include "mapping/ParameterTypes.h"
#include "scenes/FurryScene.h"
#include "scenes/RibbonDanceScene.h"
#include "scenes/OrganicNetScene.h"
#include "scenes/EnjambreOrganicoScene.h"
#include "scenes/TrappedBodyNetScene.h"
#include "scenes/FluorescentNetScene.h"
#include "scenes/DepthSliceScanScene.h"
#include "scenes/VolumetricLatticeScene.h"
#include "scenes/ElasticDepthMeshScene.h"
#include "scenes/DepthKineticVortexScene.h"
#include "scenes/VoxelFluidDisruptionScene.h"
#include "scenes/GravitationalPointCloudScene.h"
#include "scenes/SilhouetteDepthTunnelScene.h"
#include "scenes/AcousticDepthEchoesScene.h"
#include "scenes/BodySilhouetteScene.h"
#include "input/sensors/DepthStore.h"

#include "ui/InputPortsPanel.h"
#include "ui/MappingPanel.h"
#include "ui/ParamPanel.h"
#include "ui/ScenePanel.h"
#include "ui/TransportPanel.h"
#include "ui/OscMappingPanel.h"
#include "ui/KinectPanel.h"
#include "ui/PresetPanel.h"
#include "ui/SequencerPanel.h"
#include "ui/CueEditorPanel.h"
#include "ui/MidiMappingPanel.h"

#include <string>
#include <algorithm>
#include <unistd.h>

void ofApp::setup(){
    // Lock working directory to project root so relative paths don't break
    // when macOS invalidates the process cwd (common with bundled apps).
    std::string exeDir = ofFilePath::getCurrentExeDir();
    std::string projectRoot = ofFilePath::join(exeDir, "../../../../");
    if (ofFile::doesFileExist(ofFilePath::join(projectRoot, "config"))) {
        chdir(projectRoot.c_str());
        ofLogNotice("ofApp") << "Working directory set to: " << projectRoot;
    } else {
        ofLogWarning("ofApp") << "Could not resolve project root from: " << exeDir;
    }

    ofLogNotice("ofApp") << "=== SETUP START ===";
    ofSetWindowTitle("Pintura con Particulas Fluorescentes");
    ofSetWindowPosition(100, 100);  // Force window → screen mapping (fixes GLFW Cocoa workarea error)
    ofSetFrameRate(60);
    ofEnableAlphaBlending();

    debugOverlayEnabled = false;

    modular::ScenePtr pinturaScene = modular::CreatePinturaFluorescenteScene();
    std::string kSceneId = pinturaScene->getId();  // Use scene's own ID: "pinturaFluorescente"
    sceneEngine.loadScene(kSceneId, pinturaScene);
    sceneEngine.activateScene(kSceneId);
    
    modular::ScenePtr furryScene = modular::CreateFurryScene();
    sceneEngine.loadScene(furryScene->getId(), furryScene);  // "furry"
    
    modular::ScenePtr ribbonScene = modular::CreateRibbonDanceScene();
    sceneEngine.loadScene(ribbonScene->getId(), ribbonScene);  // "ribbonDance" — no auto-activate
    
    modular::ScenePtr netScene = modular::CreateOrganicNetScene();
    sceneEngine.loadScene(netScene->getId(), netScene);  // "organicNet"
    
    modular::ScenePtr trappedScene = modular::CreateTrappedBodyNetScene();
    sceneEngine.loadScene(trappedScene->getId(), trappedScene);  // "trappedBodyNet"
    
    modular::ScenePtr enjambreScene = modular::CreateEnjambreOrganicoScene();
    sceneEngine.loadScene(enjambreScene->getId(), enjambreScene);  // "enjambreOrganico"
    
    modular::ScenePtr fnScene = modular::CreateFluorescentNetScene();
    sceneEngine.loadScene(fnScene->getId(), fnScene);  // "fluorescentNet"
    
    modular::ScenePtr bcpScene = modular::CreateBodyContourPaintScene();
    sceneEngine.loadScene(bcpScene->getId(), bcpScene);  // "bodyContourPaint"

    modular::ScenePtr dssScene = modular::CreateDepthSliceScanScene();
    sceneEngine.loadScene(dssScene->getId(), dssScene);  // "depthSliceScan" (3D)

    modular::ScenePtr vlScene = modular::CreateVolumetricLatticeScene();
    sceneEngine.loadScene(vlScene->getId(), vlScene);  // "volumetricLattice" (3D)

    modular::ScenePtr edmScene = modular::CreateElasticDepthMeshScene();
    sceneEngine.loadScene(edmScene->getId(), edmScene);  // "elasticDepthMesh" (3D)

    modular::ScenePtr dkvScene = modular::CreateDepthKineticVortexScene();
    sceneEngine.loadScene(dkvScene->getId(), dkvScene);  // "depthKineticVortex" (3D)

    modular::ScenePtr vfdScene = modular::CreateVoxelFluidDisruptionScene();
    sceneEngine.loadScene(vfdScene->getId(), vfdScene);  // "voxelFluidDisruption" (3D)

    modular::ScenePtr gpcScene = modular::CreateGravitationalPointCloudScene();
    sceneEngine.loadScene(gpcScene->getId(), gpcScene);  // "gravitationalPointCloud" (3D)

    modular::ScenePtr sdtScene = modular::CreateSilhouetteDepthTunnelScene();
    sceneEngine.loadScene(sdtScene->getId(), sdtScene);  // "silhouetteDepthTunnel" (3D)

    modular::ScenePtr adeScene = modular::CreateAcousticDepthEchoesScene();
    sceneEngine.loadScene(adeScene->getId(), adeScene);  // "acousticDepthEchoes" (3D)

    modular::ScenePtr bsScene = modular::CreateBodySilhouetteScene();
    sceneEngine.loadScene(bsScene->getId(), bsScene);  // "bodySilhouette" (silueta real del depth)
    
    uiManager.setup();

    // Initialize OSC-to-Parameter Mapping Engine
    oscMappingEngine_ = std::make_unique<modular::OscMappingEngine>(&sceneEngine);

    // Initialize Kinect Mapping Engine (must precede PresetManager for preset mapping lifecycles)
    kinectMappingEngine_ = std::make_unique<modular::KinectMappingEngine>(&sceneEngine);

    // Load MIDI config
    midiConfig_.loadFromJson("config/midi.json");
    // If file doesn't exist, save defaults
    if (!ofFile::doesFileExist("config/midi.json")) {
        midiConfig_.saveToJson("config/midi.json");
    }
    ofLogNotice("ofApp") << "MIDI config: port=" << midiConfig_.portName
                         << " cueCh=" << midiConfig_.cueChannel
                         << " paramCh=" << [&](){
                             std::string s;
                             for (size_t i = 0; i < midiConfig_.paramChannels.size(); i++) {
                                 if (i > 0) s += ",";
                                 s += std::to_string(midiConfig_.paramChannels[i]);
                             }
                             return s;
                         }();

    // Create MIDI mapping engine (pass to PresetManager for per-preset activation)
    midiMappingEngine_ = std::make_unique<modular::MidiMappingEngine>(&sceneEngine);

    // Initialize Preset System
    presetManager_ = std::make_unique<modular::PresetManager>(&sceneEngine, kinectMappingEngine_.get(), midiMappingEngine_.get());
    presetManager_->loadPresets("pinturaFluorescente", "config/presets/pinturaFluorescente.json");

    sceneSequencer_ = std::make_unique<modular::SceneSequencer>(presetManager_.get(), &sceneEngine);
    sceneSequencer_->loadSequence("config/sequence.json");

    // Create MIDI router (channel-based dispatch)
    midiRouter_ = std::make_unique<modular::MidiRouter>(
        sceneSequencer_.get(),
        midiMappingEngine_.get(),
        midiConfig_
    );

    // Wire OSC mapping engine to preset/sequencer routing
    oscMappingEngine_->setPresetManager(presetManager_.get());
    oscMappingEngine_->setSceneSequencer(sceneSequencer_.get());

    // Load presets for all scenes
    presetManager_->loadPresets("particleSwarm", "config/presets/particleSwarm.json");
    presetManager_->loadPresets("organicNet", "config/presets/organicNet.json");
    presetManager_->loadPresets("ribbonDance", "config/presets/ribbonDance.json");
    presetManager_->loadPresets("furry", "config/presets/furry.json");
    presetManager_->loadPresets("trappedBodyNet", "config/presets/trappedBodyNet.json");
    presetManager_->loadPresets("enjambreOrganico", "config/presets/enjambreOrganico.json");
    presetManager_->loadPresets("fluorescentNet", "config/presets/fluorescentNet.json");
    presetManager_->loadPresets("bodyContourPaint", "config/presets/bodyContourPaint.json");
    presetManager_->loadPresets("gridDissolve", "config/presets/gridDissolve.json");

    // Open configured MIDI input port by name, with fallback
    midiIn_.listInPorts();
    ofLogNotice("ofApp") << "Available MIDI input ports: " << midiIn_.getNumInPorts();
    for (int i = 0; i < midiIn_.getNumInPorts(); i++) {
        ofLogNotice("ofApp") << "  [" << i << "] " << midiIn_.getInPortName(i);
    }

    bool portOpened = false;
    // Try exact name match first
    for (int i = 0; i < midiIn_.getNumInPorts(); i++) {
        std::string portName = midiIn_.getInPortName(i);
        if (portName == midiConfig_.portName) {
            midiIn_.openPort(i);
            midiIn_.addListener(this);
            portOpened = true;
            ofLogNotice("ofApp") << "MIDI input opened (exact match): " << portName;
            break;
        }
    }
    // Try substring match (e.g., "IAC Driver" matches "IAC Driver Bus 1")
    if (!portOpened) {
        for (int i = 0; i < midiIn_.getNumInPorts(); i++) {
            std::string portName = midiIn_.getInPortName(i);
            if (portName.find("IAC") != std::string::npos
                || portName.find(midiConfig_.portName) != std::string::npos) {
                midiIn_.openPort(i);
                midiIn_.addListener(this);
                portOpened = true;
                ofLogNotice("ofApp") << "MIDI input opened (substring match): " << portName;
                break;
            }
        }
    }
    // Fallback: open first available port
    if (!portOpened && midiIn_.getNumInPorts() > 0) {
        midiIn_.openPort(0);
        midiIn_.addListener(this);
        portOpened = true;
        ofLogWarning("ofApp") << "Configured port '" << midiConfig_.portName
                              << "' not found — falling back to first available: "
                              << midiIn_.getInPortName(0);
    }
    if (!portOpened) {
        ofLogWarning("ofApp") << "No MIDI input ports available — cue routing via keyboard only";
    }

    auto& registry = modular::ParameterRegistry::getGlobal();

    // Auto-load saved parameters on startup using direct lookup (no fragile matching)
    std::string paramFile = "config/parameters.json";
    if (registry.loadFromJson(paramFile)) {
        ofLogNotice("ofApp") << "Loaded saved parameters from: " << paramFile;
        
        auto params = registry.getAllParams();
        int synced = 0;
        
        for (const auto& param : params) {
            if (param.scope == modular::ParameterScope::SCENE) {
                size_t dotPos = param.id.find('.');
                if (dotPos != std::string::npos) {
                    std::string sceneId = param.id.substr(0, dotPos);
                    std::string localParamId = param.id.substr(dotPos + 1);
                    
                    auto scene = sceneEngine.getScene(sceneId);
                    if (scene) {
                        bool success = scene->setParameter(localParamId, param.currentValue);
                        if (success) {
                            synced++;
                        } else {
                            ofLogWarning("ofApp") << "Failed to sync: " << param.id << " = " << param.currentValue;
                        }
                    } else {
                        ofLogWarning("ofApp") << "Scene not found for param: " << param.id;
                    }
                }
            }
        }
        
        ofLogNotice("ofApp") << "Synced " << synced << " parameters to all scenes";
    }

    modular::ParameterBinding mappingA;
    mappingA.sourceSceneId = "global";
    mappingA.sourceParameterId = "bpm";
    mappingA.sourceChannel = "CC1";
    mappingA.inputDevice = "APC40";
    mappingA.targetSceneId = kSceneId;
    mappingA.targetParameterId = std::string(kSceneId) + ".k";
    mappingLayer.addMapping(mappingA);

    modular::ParameterBinding mappingB;
    mappingB.sourceSceneId = kSceneId;
    mappingB.sourceParameterId = "flockSpeed";
    mappingB.sourceChannel = "CC2";
    mappingB.inputDevice = "IAC Bus 1";
    mappingB.targetSceneId = kSceneId;
    mappingB.targetParameterId = std::string(kSceneId) + ".flockWaveX";
    mappingB.enabled = false;
    mappingLayer.addMapping(mappingB);

    midiPorts.addPort("APC MINI", "Akai APC MINI", false, 1);
    midiPorts.addPort("Launchpad X", "Novation Launchpad X", false, 2);
    midiPorts.addPort("Loopback", "IAC Driver Bus 1", true, 1);

    auto paramPanel = std::make_unique<modular::ParamPanel>(&sceneEngine, &registry);
    auto transportPanel = std::make_unique<modular::TransportPanel>(&transport);
    auto scenePanel = std::make_unique<modular::ScenePanel>(&sceneEngine);
    auto mappingPanel = std::make_unique<modular::MappingPanel>(&mappingLayer);
    auto inputPortsPanel = std::make_unique<modular::InputPortsPanel>(&midiPorts);

    uiManager.addModule(std::move(scenePanel));
    uiManager.addModule(std::move(transportPanel));
    uiManager.addModule(std::move(paramPanel));
    uiManager.addModule(std::move(mappingPanel));
    uiManager.addModule(std::move(inputPortsPanel));

    // OSC Mapping Panel
    auto oscMappingPanel = std::make_unique<modular::OscMappingPanel>(oscMappingEngine_.get(), &sceneEngine);
    uiManager.addModule(std::move(oscMappingPanel));

    // Preset System Panels
    auto presetPanel = std::make_unique<modular::PresetPanel>(presetManager_.get(), &sceneEngine);
    uiManager.addModule(std::move(presetPanel));

    auto sequencerPanel = std::make_unique<modular::SequencerPanel>(sceneSequencer_.get());
    uiManager.addModule(std::move(sequencerPanel));

    // Cue Editor Panel — edit/delete/reorder cues + MIDI Learn
    auto cueEditorPanel = std::make_unique<modular::CueEditorPanel>(
        sceneSequencer_.get(), &sceneEngine, presetManager_.get(), &midiIn_,
        [this](std::function<bool(ofxMidiMessage&)> cb) {
            addMidiLearnCallback(std::move(cb));
        },
        [this](std::function<bool(ofxMidiMessage&)> cb) {
            removeMidiLearnCallback(std::move(cb));
        }
    );
    uiManager.addModule(std::move(cueEditorPanel));

    // MIDI Mapping Panel — per-preset MIDI-to-parameter mappings with Learn
    auto midiMappingPanel = std::make_unique<modular::MidiMappingPanel>(
        midiMappingEngine_.get(),
        presetManager_.get(),
        &sceneEngine,
        [this](std::function<bool(ofxMidiMessage&)> cb) {
            addMidiLearnCallback(std::move(cb));
        },
        [this](std::function<bool(ofxMidiMessage&)> cb) {
            removeMidiLearnCallback(std::move(cb));
        }
    );
    uiManager.addModule(std::move(midiMappingPanel));

    // ============================================
    // Kinect Integration (Orbbec Gemini 2 native)
    // ============================================
    
    // Orbbec adapter: color + depth D2C + MediaPipe pose (pose_detector.py) → 18 joints con z real.
    kinectAdapter_ = std::make_unique<modular::OrbbecAdapter>();
    // kinectAdapter_ = std::make_unique<modular::MediaPipePoseAdapter>();  // 2D OSC bridge (webcam)
    // kinectAdapter_ = std::make_unique<modular::MockKinectAdapter>(30.0f);  // Swap for testing without hardware
     if (kinectAdapter_->initialize()) {
        ofLogNotice("Kinect") << "Orbbec Adapter initialized successfully";
    } else {
        ofLogError("Kinect") << "Failed to initialize Orbbec Adapter";
    }
    
    // Wire Kinect adapter callbacks to mapping engine
    kinectAdapter_->setJointCallback(
        [this](int bodyId, modular::JointType jointType, const modular::Joint& joint) {
            kinectMappingEngine_->processJointData(bodyId, jointType, joint);
        }
    );
    
    // Load saved kinect mappings
    kinectMappingEngine_->loadFromJson("config/kinect-mappings.json");
    
    // Create Kinect UI panel
    auto kinectPanel = std::make_unique<modular::KinectPanel>(
        kinectAdapter_.get(), kinectMappingEngine_.get(), &sceneEngine, presetManager_.get());
    uiManager.addModule(std::move(kinectPanel));
    
    // Create Skeleton View (OF renderer for main viewport)
    skeletonView_ = std::make_unique<modular::SkeletonView>();
    skeletonView_->setVisible(true);
    skeletonAlpha_ = 0.0f;

    inputRouter = std::make_unique<modular::InputRouter>(&sceneEngine, &uiManager);
    inputRouter->addKeyboardBypass('k');
    inputRouter->addKeyboardBypass('K');

    // Initialize OSC adapter
    oscSettings_ = modular::OSCSettingsManager("config/osc.json");
    if (oscSettings_.load()) {
        oscAdapter_ = std::make_unique<modular::OSCAdapter>(oscSettings_, transport);
        auto status = oscAdapter_->startListening();
        if (status == modular::OSCAdapterStatus::OK) {
            ofLogNotice("OSC") << "OSC Adapter started successfully";

            // Wire OSC → Parameter mapping
            oscAdapter_->setOscMessageCallback(
                [this](const std::string& addr, float val) {
                    oscMappingEngine_->processOscMessage(addr, val);
                }
            );
            oscMappingEngine_->loadFromJson("config/osc-mappings.json");
        } else {
            ofLogWarning("OSC") << "OSC Adapter failed to start: " << static_cast<int>(status);
        }
    } else {
        ofLogError("OSC") << "Failed to load OSC settings";
    }

    // Set background after window is fully realized (avoids GLFW workarea Cocoa error)
    sceneEngine.setBackgroundColor(10);

    ofLogNotice("ofApp") << "=== SETUP COMPLETE ===";
}

void ofApp::update(){
    const float delta = ofGetLastFrameTime();
    
    // Update OSC adapter (hot reload + polling)
    if (oscAdapter_) {
        oscAdapter_->update();
    }
    
    if (oscMappingEngine_) {
        oscMappingEngine_->update(delta);
    }

    if (presetManager_) {
        presetManager_->update(delta);
    }
    
    // Update Kinect adapter (skeleton tracking)
    if (kinectAdapter_) {
        kinectAdapter_->update();
    }
    
    if (kinectMappingEngine_) {
        kinectMappingEngine_->update(delta);
    }

    if (midiMappingEngine_) {
        midiMappingEngine_->update(delta);
    }
    
    sceneEngine.update(delta);
    transport.update(delta);
    uiManager.update(delta);

    if (thresholdToastTimer_ > 0.0f) thresholdToastTimer_ -= delta;
}

void ofApp::draw(){
    // Vista de cámara RGB (V) — semitransparente y en espejo (como un espejo real).
    if (showRgb_ && kinectAdapter_) {
        auto* ob = dynamic_cast<modular::OrbbecAdapter*>(kinectAdapter_.get());
        if (ob && ob->hasColorTexture()) {
            ofPushStyle();
            ofEnableBlendMode(OF_BLENDMODE_ALPHA);
            ofSetColor(255, 255, 255, 110);   // ~43% opacidad
            // Espejo horizontal: tu izquierda queda a la izquierda de la pantalla.
            ofPushMatrix();
            ofTranslate(ofGetWidth(), 0);
            ofScale(-1, 1, 1);
            ob->getColorTexture().draw(0, 0, ofGetWidth(), ofGetHeight());
            ofPopMatrix();
            ofDisableBlendMode();
            ofPopStyle();
        }
    }

    sceneEngine.draw();
    
    // Draw skeleton overlay (after scene, before UI)
    if (skeletonView_ && sceneEngine.isShowSkeleton()) {
        // Smoothly track presence for fade transitions
        float targetPresence = kinectAdapter_ ? kinectAdapter_->getTrackingPresence() : 0.0f;
        skeletonAlpha_ = ofLerp(skeletonAlpha_, targetPresence, 0.1f);
        skeletonView_->setAlpha(skeletonAlpha_);

        if (skeletonAlpha_ > 0.01f) {
            auto bodies = kinectAdapter_->getAllTrackedBodies();
            if (!bodies.empty()) {
                skeletonView_->draw(bodies[0], ofGetWidth(), ofGetHeight());
            }
        }
    }
    
    uiManager.draw();
    drawDebugOverlay();

    // Toast del umbral de silueta (feedback al ajustar con , / .)
    if (thresholdToastTimer_ > 0.0f) {
        ofPushStyle();
        std::string msg = "Silueta: " + std::to_string(thresholdToastValue_) + " mm  (',' baja / '.' sube)";
        ofDrawBitmapStringHighlight(msg, ofGetWidth() / 2 - 160, 40, ofColor(0, 0, 0, 220), ofColor::yellow);
        ofPopStyle();
    }

    // Scene transition fade overlay (black rectangle with alpha)
    if (sceneEngine.isTransitioning()) {
        float alpha = sceneEngine.getTransitionAlpha();
        if (alpha > 0.005f) {
            ofPushStyle();
            ofSetColor(0, 0, 0, static_cast<int>(alpha * 255.0f));
            ofFill();
            ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
            ofPopStyle();
        }
    }
}

void ofApp::exit(){
    // Stop OSC adapter and persist settings
    if (oscAdapter_) {
        oscAdapter_->stopListening();
    }
    oscSettings_.save();
    
    // Persist OSC mappings
    if (oscMappingEngine_) {
        oscMappingEngine_->saveToJson("config/osc-mappings.json");
    }
    
    // Persist Kinect mappings
    if (kinectMappingEngine_) {
        kinectMappingEngine_->saveToJson("config/kinect-mappings.json");
    }

    // Guardar umbral de silueta (persistencia entre sesiones).
    {
        ofJson dj;
        dj["silhouetteThreshold"] = modular::DepthStore::instance().getThreshold();
        ofSaveJson("config/depth.json", dj);
    }
    
    // Shutdown Kinect adapter
    if (kinectAdapter_) {
        kinectAdapter_->shutdown();
    }
    
    // Close MIDI input
    midiIn_.closePort();
    
    sceneEngine.deactivateAll();
}

// ── MIDI Message Handler (ofxMidiListener) ──────────────────────────────────

void ofApp::newMidiMessage(ofxMidiMessage &message) {
    // MIDI Learn gate — iterate callbacks, first-consumer-wins
    // Copy vector before iterating: a callback may unregister itself
    // during invocation (e.g., Learn capture), which would invalidate
    // iterators if we walked the original vector.
    auto callbacks = midiLearnCallbacks_;
    for (auto& cb : callbacks) {
        if (cb && cb(message)) return;  // consumed — skip normal routing
    }

    // Route through MidiRouter (channel-based dispatch)
    if (cueRoutingEnabled_ && midiRouter_) {
        midiRouter_->routeMessage(message);
    }
}

void ofApp::addMidiLearnCallback(std::function<bool(ofxMidiMessage&)> cb) {
    midiLearnCallbacks_.push_back(std::move(cb));
}

void ofApp::removeMidiLearnCallback(std::function<bool(ofxMidiMessage&)> cb) {
    if (!cb) return;
    // Remove first callback whose target_type matches.
    // Each panel stores its callback as a member and passes it to both
    // add/remove — the type identity discriminates between panels.
    // Panels never register two callbacks of the same type simultaneously.
    auto it = std::find_if(midiLearnCallbacks_.begin(), midiLearnCallbacks_.end(),
        [&cb](const auto& f) {
            return f && f.target_type() == cb.target_type();
        });
    if (it != midiLearnCallbacks_.end()) {
        midiLearnCallbacks_.erase(it);
    }
}

void ofApp::keyPressed(int key){
    uiManager.keyPressed(key);
    // Route Escape to cancel MIDI Learn if active
    if (cueEditorPanel_ && cueEditorPanel_->handleKeyPressed(key)) {
        return;  // key consumed by CueEditorPanel (e.g., Escape cancelled Learn)
    }
    if (inputRouter) {
        inputRouter->keyPressed(key);
    } else {
        sceneEngine.keyPressed(key);
    }

    if (key == 'u' || key == 'U') {
        uiManager.toggleGlobalVisible();
        // Cursor follows UI visibility
        if (uiManager.isGlobalVisible()) ofShowCursor();
        else ofHideCursor();
    }
    if (key == 't' || key == 'T') {
        uiManager.toggleWindowAlpha();
    }
    if (key == 's' || key == 'S') {
        // Sync active scene parameters into registry before saving
        auto activeScene = sceneEngine.getActiveScene();
        if (activeScene) {
            auto paramIds = activeScene->getParameterIds();
            for (const auto& id : paramIds) {
                auto param = activeScene->getParameter(id);
                if (param) {
                    // Build full ID and write current value into registry
                    std::string fullId = activeScene->getId() + "." + id;
                    auto& registry = modular::ParameterRegistry::getGlobal();
                    registry.setValue(fullId, param->currentValue);
                }
            }
        }
        // Save all parameters and mappings
        auto& registry = modular::ParameterRegistry::getGlobal();
        registry.saveToJson("config/parameters.json");
        if (kinectMappingEngine_) {
            kinectMappingEngine_->saveToJson("config/kinect-mappings.json");
        }
        if (oscMappingEngine_) {
            oscMappingEngine_->saveToJson("config/osc-mappings.json");
        }
        ofLogNotice("ofApp") << "Parameters saved (S)";
    }
    if (key == 'l' || key == 'L') {
        // Load all parameters and mappings
        auto& registry = modular::ParameterRegistry::getGlobal();
        registry.loadFromJson("config/parameters.json");
        if (kinectMappingEngine_) {
    kinectMappingEngine_->loadFromJson("config/kinect-mappings.json");

    // Cargar umbral de silueta guardado (persistencia entre sesiones).
    if (ofFile::doesFileExist("config/depth.json")) {
        ofJson dj = ofLoadJson("config/depth.json");
        if (dj.contains("silhouetteThreshold") && dj["silhouetteThreshold"].is_number()) {
            modular::DepthStore::instance().setThreshold(
                static_cast<uint16_t>(dj["silhouetteThreshold"].get<int>()));
        }
    }
        }
        if (oscMappingEngine_) {
            oscMappingEngine_->loadFromJson("config/osc-mappings.json");
        }
        // Re-sync loaded values to active scene
        auto activeScene = sceneEngine.getActiveScene();
        if (activeScene) {
            auto params = registry.getAllParams();
            std::string sceneId = activeScene->getId();
            for (const auto& param : params) {
                if (param.scope == modular::ParameterScope::SCENE) {
                    size_t dotPos = param.id.find('.');
                    if (dotPos != std::string::npos) {
                        std::string paramSceneId = param.id.substr(0, dotPos);
                        std::string localId = param.id.substr(dotPos + 1);
                        if (paramSceneId == sceneId) {
                            activeScene->setParameter(localId, param.currentValue);
                        }
                    }
                }
            }
        }
        ofLogNotice("ofApp") << "Parameters loaded (L)";
    }
    if (key == 'd' || key == 'D') {
        debugOverlayEnabled = !debugOverlayEnabled;
        ofLogNotice("DebugOverlay") << (debugOverlayEnabled ? "Enabled" : "Disabled");
    }
    if (key == 'j' || key == 'J') {
        auto activeScene = sceneEngine.getActiveScene();
        auto pinturaScene = std::dynamic_pointer_cast<modular::PinturaFluorescenteScene>(activeScene);
        if (pinturaScene) {
            bool current = pinturaScene->isDebugMarkersVisible();
            pinturaScene->setDebugMarkers(!current);
            ofLogNotice("DebugMarkers") << (current ? "Hidden" : "Shown") << " (J)";
        }
        auto enjambreScene = std::dynamic_pointer_cast<modular::EnjambreOrganicoScene>(activeScene);
        if (enjambreScene) {
            bool current = enjambreScene->isDebugMarkersVisible();
            enjambreScene->setDebugMarkers(!current);
            ofLogNotice("DebugMarkers") << (current ? "Hidden" : "Shown") << " (J — Enjambre)";
        }
        auto fnScene = std::dynamic_pointer_cast<modular::FluorescentNetScene>(activeScene);
        if (fnScene) {
            bool current = fnScene->isDebugMarkersVisible();
            fnScene->setDebugMarkers(!current);
            ofLogNotice("DebugMarkers") << (current ? "Hidden" : "Shown") << " (J — FluorescentNet)";
        }
    }
    if (key == 'e' || key == 'E') {
        bool current = sceneEngine.isShowSkeleton();
        sceneEngine.setShowSkeleton(!current);
        ofLogNotice("Skeleton") << (current ? "Hidden" : "Shown") << " (E)";
    }
    if (key == 'f' || key == 'F') {
        ofToggleFullscreen();
        ofLogNotice("ofApp") << "Fullscreen toggled (F)";
    }
    
    // Toggle mouse-driven interaction points (M)
    if (key == 'm' || key == 'M') {
        bool current = sceneEngine.isMouseInputEnabled();
        sceneEngine.setUseMouseInput(!current);
        ofLogNotice("MouseInput") << (current ? "Disabled" : "Enabled") << " (M)";
    }

    // Ajustar umbral de la silueta de profundidad (, / .)
    if (key == ',' || key == '.') {
        auto& depth = modular::DepthStore::instance();
        int t = depth.getThreshold();
        t += (key == ',') ? -200 : 200;
        t = ofClamp(t, 500, 6000);
        depth.setThreshold(static_cast<uint16_t>(t));
        thresholdToastTimer_ = 2.5f;
        thresholdToastValue_ = t;
        ofLogNotice("DepthStore") << "Umbral silueta: " << t << " mm";
    }

    // Vista de cámara RGB (V)
    if (key == 'v' || key == 'V') {
        showRgb_ = !showRgb_;
        ofLogNotice("ofApp") << "Vista RGB: " << (showRgb_ ? "ON" : "OFF") << " (V)";
    }
    
    // Reset Kinect/MediaPipe tracking (R) — only when ImGui is not capturing keyboard
    if (key == 'r' || key == 'R') {
        if (!ImGui::GetIO().WantCaptureKeyboard) {
            if (kinectAdapter_) {
                kinectAdapter_->shutdown();
                kinectAdapter_->initialize();
                ofLogNotice("Kinect") << "Tracking reset (R)";
            }
        }
    }
    
    // Cycle scenes with arrow keys
    if (key == OF_KEY_RIGHT || key == OF_KEY_LEFT) {
        auto scenes = sceneEngine.getLoadedScenes();
        if (scenes.empty()) return;
        
        std::string activeId = sceneEngine.getActiveSceneId();
        int currentIdx = -1;
        for (int i = 0; i < static_cast<int>(scenes.size()); i++) {
            if (scenes[i] == activeId) { currentIdx = i; break; }
        }
        
        int nextIdx;
        if (key == OF_KEY_RIGHT) {
            nextIdx = (currentIdx + 1) % scenes.size();
        } else {
            nextIdx = (currentIdx - 1 + scenes.size()) % scenes.size();
        }
        
        sceneEngine.transitionTo(scenes[nextIdx]);
        ofLogNotice("SceneSwitch") << "Arrow key → " << scenes[nextIdx];
    }
    
    // Teclas 1-9 para cambiar de escena (solo si UI no está activo)
    if (key >= '1' && key <= '9' && !uiManager.isGlobalVisible()) {
        auto scenes = sceneEngine.getLoadedScenes();
        int index = key - '1';  // 0 para '1', 1 para '2', etc
        if (index >= 0 && index < static_cast<int>(scenes.size())) {
            sceneEngine.transitionTo(scenes[index]);
            ofLogNotice("ofApp") << "Scene switched to: " << scenes[index];
        }
    }

    // ── Cue Routing (MIDI PC simulation via keyboard) ──────────────────────
    // Shift + NUMBER → trigger cue N (1-9)
    // [ → previous cue    ] → next cue
    if (cueRoutingEnabled_ && sceneSequencer_ && sceneSequencer_->hasSequence()) {
        if (ofGetKeyPressed(OF_KEY_SHIFT) && key >= '1' && key <= '9') {
            int cueNum = key - '0';  // '1' → 1
            sceneSequencer_->triggerCue(cueNum);
            ofLogNotice("ofApp") << "Cue triggered: " << cueNum;
        }
        else if (key == '[') {
            sceneSequencer_->triggerPrev();
        }
        else if (key == ']') {
            sceneSequencer_->triggerNext();
        }
    }

    // ── MIDI Config Reload ───────────────────────────────────────────────────
    if (key == OF_KEY_F5) {
        if (midiConfig_.loadFromJson("config/midi.json")) {
            midiRouter_->reloadConfig(midiConfig_);
            ofLogNotice("ofApp") << "MIDI config reloaded (F5)";
        } else {
            ofLogWarning("ofApp") << "Failed to reload MIDI config (F5)";
        }
    }

    // ── Sequence Toggle ─────────────────────────────────────────────────────
    if (key == OF_KEY_F1) {
        cueRoutingEnabled_ = !cueRoutingEnabled_;
        ofLogNotice("ofApp") << "Cue routing: " << (cueRoutingEnabled_ ? "ENABLED" : "DISABLED");
    }
}

void ofApp::mouseDragged(int x, int y, int button){
    uiManager.mouseDragged(x, y, button);
    if (inputRouter) {
        inputRouter->mouseDragged(x, y, button);
    } else {
        sceneEngine.mouseDragged(x, y, button);
    }
}

void ofApp::mousePressed(int x, int y, int button){
    uiManager.mousePressed(x, y, button);
    if (inputRouter) {
        inputRouter->mousePressed(x, y, button);
    } else {
        sceneEngine.mousePressed(x, y, button);
    }
}

void ofApp::mouseReleased(int x, int y, int button){
    uiManager.mouseReleased(x, y, button);
    if (inputRouter) {
        inputRouter->mouseReleased(x, y, button);
    } else {
        sceneEngine.mouseReleased(x, y, button);
    }
}

void ofApp::mouseMoved(int x, int y){
    uiManager.mouseMoved(x, y);
    if (inputRouter) {
        inputRouter->mouseMoved(x, y);
    } else {
        sceneEngine.mouseMoved(x, y);
    }
}

void ofApp::windowResized(int w, int h){
    sceneEngine.windowResized(w, h);
}

void ofApp::drawDebugOverlay() {
    if (!debugOverlayEnabled) {
        return;
    }

    std::ostringstream overlay;
    overlay << "[D] Scene Debug Overlay" << std::endl;
    overlay << "FPS: " << ofToString(ofGetFrameRate(), 1) << std::endl;
    overlay << "Silueta (umbral): " << modular::DepthStore::instance().getThreshold()
            << " mm  (ajusta con ',' y '.')" << std::endl;

    auto activeScene = sceneEngine.getActiveScene();
    if (activeScene) {
        overlay << "Scene: " << activeScene->getName() << std::endl;
        auto pinturaScene = std::dynamic_pointer_cast<modular::PinturaFluorescenteScene>(activeScene);
        if (pinturaScene) {
            modular::SceneDebugStats stats = pinturaScene->getDebugStats();
            overlay << "Main Pool: " << stats.activeParticles << " / " << modular::MAX_PARTICLES;
            if (stats.activeParticles >= modular::MAX_PARTICLES) {
                overlay << " (SATURATED)";
            }
            overlay << std::endl;
            overlay << "Trail Pool: " << stats.trailParticles << " / " << modular::MAX_PARTICLES;
            if (stats.trailParticles >= modular::MAX_PARTICLES) {
                overlay << " (SATURATED)";
            }
            overlay << std::endl;
            overlay << "Spawned last frame: " << stats.spawnedMainLastFrame << " main, "
                    << stats.spawnedTrailLastFrame << " trail" << std::endl;
            overlay << "Attractor: " << (stats.attractorActive ? "ON" : "OFF");
            if (stats.attractorActive) {
                overlay << " @ (" << static_cast<int>(stats.attractorPoint.x)
                        << ", " << static_cast<int>(stats.attractorPoint.y) << ")";
            }
            overlay << std::endl;
            overlay << "Flock mode: " << (stats.flockMode ? "ON" : "OFF")
                    << " | Clear mode: " << (stats.clearEachFrame ? "AUTO" : "TRAIL") << std::endl;
        } else {
            overlay << "Active scene does not expose debug stats." << std::endl;
        }
    } else {
        overlay << "No active scene." << std::endl;
    }

    ofPushStyle();
    ofDrawBitmapStringHighlight(overlay.str(), 20, 30, ofColor(0, 0, 0, 200), ofColor::white);
    ofPopStyle();
}
