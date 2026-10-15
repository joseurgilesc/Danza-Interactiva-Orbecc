#pragma once

// openFrameworks
#include "ofMain.h"
#include <memory>
#include <sstream>
#include <functional>
#include <vector>

#include "engine/SceneEngine.h"
#include "engine/Transport.h"
#include "input/InputRouter.h"
#include "input/MidiPortsStore.h"
#include "mapping/MappingLayer.h"
#include "scenes/PinturaFluorescenteScene.h"
#include "scenes/BodyContourPaintScene.h"
#include "scenes/GridDissolveScene.h"
#include "registry/ParameterRegistry.h"
#include "ui/UIManager.h"
#include "adapters/OSCSettings.h"
#include "adapters/OSCAdapter.h"
#include "mapping/OscMappingEngine.h"
#include "presets/PresetManager.h"
#include "sequencer/SceneSequencer.h"
#include "ofxMidi.h"
#include "midi/MidiConfig.h"
#include "midi/MidiRouter.h"
#include "midi/MidiMappingEngine.h"
#include "adapters/kinect/IKinectAdapter.h"
#include "adapters/kinect/OrbbecAdapter.h"
// #include "adapters/kinect/MediaPipePoseAdapter.h"  // 2D OSC bridge (webcam/Kinect)
// #include "adapters/kinect/MockKinectAdapter.h"  // Swap for testing without hardware
#include "mapping/KinectMappingEngine.h"
#include "input/sensors/SkeletonView.h"
#include "ui/CueEditorPanel.h"

class ofApp : public ofBaseApp, public ofxMidiListener {

    public:
        void setup() override;
        void update() override;
        void draw() override;
        void exit() override;

        void keyPressed(int key) override;
        void newMidiMessage(ofxMidiMessage &message) override;
        void mouseDragged(int x, int y, int button) override;
        void mousePressed(int x, int y, int button) override;
        void mouseReleased(int x, int y, int button) override;
        void mouseMoved(int x, int y) override;
        void windowResized(int w, int h) override;

    private:
        void drawDebugOverlay();

        modular::SceneEngine sceneEngine;
        modular::TransportController transport;
        modular::MappingLayer mappingLayer;
        modular::MidiPortsStore midiPorts;
        std::unique_ptr<modular::InputRouter> inputRouter;
        modular::UIManager uiManager;
        bool debugOverlayEnabled;
        
        // Kinect Integration
        std::unique_ptr<modular::IKinectAdapter> kinectAdapter_;
        std::unique_ptr<modular::KinectMappingEngine> kinectMappingEngine_;
        std::unique_ptr<modular::SkeletonView> skeletonView_;
        float skeletonAlpha_;  // Lerped toward tracking presence for smooth fade
        
        // OSC Adapter
        modular::OSCSettingsManager oscSettings_;
        std::unique_ptr<modular::OSCAdapter> oscAdapter_;
        
        // OSC-to-Parameter Mapping Engine
        std::unique_ptr<modular::OscMappingEngine> oscMappingEngine_;

        // Preset System
        std::unique_ptr<modular::PresetManager> presetManager_;
        std::unique_ptr<modular::SceneSequencer> sceneSequencer_;
        std::unique_ptr<modular::CueEditorPanel> cueEditorPanel_;
        bool cueRoutingEnabled_ = true;

        // MIDI Routing
        modular::MidiConfig midiConfig_;
        std::unique_ptr<modular::MidiRouter> midiRouter_;
        std::unique_ptr<modular::MidiMappingEngine> midiMappingEngine_;

        // MIDI Learn
        std::vector<std::function<bool(ofxMidiMessage&)>> midiLearnCallbacks_;
        void addMidiLearnCallback(std::function<bool(ofxMidiMessage&)> cb);
        void removeMidiLearnCallback(std::function<bool(ofxMidiMessage&)> cb);

        // MIDI Input
        ofxMidiIn midiIn_;
    };
