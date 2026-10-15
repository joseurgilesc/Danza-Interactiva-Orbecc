#pragma once

#include "midi/MidiConfig.h"
#include <vector>

class ofxMidiMessage;

namespace modular {

// Forward declarations — headers included only in .cpp, not here
class SceneSequencer;
class MidiMappingEngine;

// ============================================================================
// MidiRouter — channel-based MIDI dispatcher
//
// Receives every incoming MIDI message (after Learn gate) and dispatches by
// channel: cueChannel → SceneSequencer cue lookup/trigger, paramChannels →
// MidiMappingEngine processMidiMessage. All other channels are silently ignored.
//
// Constructor accepts nullptr for sequencer/mappingEngine so wiring can be
// gradual across PR slices.
// ============================================================================

class MidiRouter {
public:
    /// @param sequencer     May be nullptr (cue routing skipped until wired).
    /// @param mappingEngine May be nullptr (param dispatch skipped until wired).
    /// @param config        Initial channel assignment (non-owning copy).
    MidiRouter(SceneSequencer* sequencer,
               MidiMappingEngine* mappingEngine,
               const MidiConfig& config);

    // ---- Core Dispatch ----

    /// Dispatches a single MIDI message by channel.
    /// PC/NoteOn(vel>0)/CC(val>0) on cueChannel → findCueByMidi + triggerCue.
    /// Any message on a paramChannel → MidiMappingEngine::processMidiMessage.
    /// All else silently ignored.
    void routeMessage(const ofxMidiMessage& msg);

    // ---- Runtime Channel Overrides ----

    void setCueChannel(int ch);                        // Clamped to 1–16
    void setParamChannels(const std::vector<int>& chs); // Clamped to 1–16, deduped

    /// Hot-reload: applies channel assignments from a new MidiConfig.
    void reloadConfig(const MidiConfig& config);

private:
    static int clampChannel(int ch);

    SceneSequencer* sequencer_;
    MidiMappingEngine* mappingEngine_;
    int cueChannel_ = 1;
    std::vector<int> paramChannels_ = {2};
};

} // namespace modular
