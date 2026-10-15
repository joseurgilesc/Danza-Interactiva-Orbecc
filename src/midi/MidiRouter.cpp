#include "midi/MidiRouter.h"
#include "midi/MidiMappingEngine.h"
#include "sequencer/SceneSequencer.h"
#include "ofxMidi.h"
#include "ofLog.h"
#include <algorithm>
#include <sstream>

namespace modular {

// ============================================================================
// Constructor
// ============================================================================

MidiRouter::MidiRouter(SceneSequencer* sequencer,
                       MidiMappingEngine* mappingEngine,
                       const MidiConfig& config)
    : sequencer_(sequencer)
    , mappingEngine_(mappingEngine)
    , cueChannel_(config.cueChannel)
    , paramChannels_(config.paramChannels)
{}

// ============================================================================
// routeMessage — channel-based dispatch (mirrors ofApp::newMidiMessage logic)
// ============================================================================

void MidiRouter::routeMessage(const ofxMidiMessage& msg) {
    // ── Cue Channel ───────────────────────────────────────────────────────
    if (msg.channel == cueChannel_ && sequencer_) {
        int cueIdx = -1;

        if (msg.status == MIDI_PROGRAM_CHANGE) {
            // Lookup by assigned trigger first, fall back to sequential
            cueIdx = sequencer_->findCueByMidi("pc", msg.pitch);
            if (cueIdx < 0) {
                // Backward compat: PC N triggers cue N+1
                cueIdx = msg.pitch;
            }
        } else if (msg.status == MIDI_NOTE_ON && msg.velocity > 0) {
            cueIdx = sequencer_->findCueByMidi("note", msg.pitch);
        } else if (msg.status == MIDI_CONTROL_CHANGE && msg.value > 0) {
            cueIdx = sequencer_->findCueByMidi("cc", msg.control);
        }

        if (cueIdx >= 0) {
            const auto* cue = sequencer_->getCue(cueIdx);
            if (cue) {
                bool ok = sequencer_->triggerCue(cue->cue);
                if (ok) {
                    ofLogNotice("MidiRouter")
                        << "MIDI " << static_cast<int>(msg.status)
                        << " → cue " << cue->cue << " triggered";
                }
            }
        }
    }
    // ── Param Channels (all configured) ───────────────────────────────────
    else if (std::find(paramChannels_.begin(), paramChannels_.end(), msg.channel) != paramChannels_.end()
             && mappingEngine_) {
        mappingEngine_->processMidiMessage(msg);
    }
    // else: silently ignored
}

// ============================================================================
// Runtime Channel Overrides (clamped 1–16)
// ============================================================================

void MidiRouter::setCueChannel(int ch) {
    cueChannel_ = clampChannel(ch);
    ofLogNotice("MidiRouter") << "cueChannel set to: " << cueChannel_;
}

void MidiRouter::setParamChannels(const std::vector<int>& chs) {
    paramChannels_.clear();
    for (int ch : chs) {
        int clamped = clampChannel(ch);
        if (std::find(paramChannels_.begin(), paramChannels_.end(), clamped) == paramChannels_.end()) {
            paramChannels_.push_back(clamped);
        }
    }
    // Log
    std::ostringstream oss;
    for (size_t i = 0; i < paramChannels_.size(); i++) {
        if (i > 0) oss << ",";
        oss << paramChannels_[i];
    }
    ofLogNotice("MidiRouter") << "paramChannels set to: [" << oss.str() << "]";
}

// ============================================================================
// reloadConfig — hot-reload channel assignments
// ============================================================================

void MidiRouter::reloadConfig(const MidiConfig& config) {
    cueChannel_    = config.cueChannel;
    paramChannels_ = config.paramChannels;

    std::ostringstream oss;
    for (size_t i = 0; i < paramChannels_.size(); i++) {
        if (i > 0) oss << ",";
        oss << paramChannels_[i];
    }
    ofLogNotice("MidiRouter")
        << "Config reloaded: cueChannel=" << cueChannel_
        << " paramChannels=[" << oss.str() << "]";
}

// ============================================================================
// clampChannel — bounds check for MIDI channel (1–16)
// ============================================================================

int MidiRouter::clampChannel(int ch) {
    if (ch < 1)  return 1;
    if (ch > 16) return 16;
    return ch;
}

} // namespace modular
