#pragma once

#include "ofMain.h"
#include <string>
#include <vector>

namespace modular {

// Forward declarations
class PresetManager;
class SceneEngine;

// ============================================================================
// CueDef — a single cue in the performance sequence
// ============================================================================

struct CueDef {
    int cue;                    // 1-indexed cue number (from JSON)
    std::string sceneId;        // Target scene ID
    std::string presetName;     // Preset to apply (empty = skip preset)
    float morphDuration;        // 0.0 = instant preset apply
    std::string midiType;       // "" = unassigned, "pc", "note", "cc"
    int midiValue = -1;         // 0-127, -1 = unassigned
    int midiChannel = -1;       // 1-16, -1 = not captured (display only, routing is by channel elsewhere)
};

// ============================================================================
// SceneSequencer — cue-based sequence combining preset + scene transition
// ============================================================================

class SceneSequencer {
public:
    /// @param presetManager May be nullptr (triggerCue skips preset apply).
    /// @param sceneEngine   Must be non-null.
    SceneSequencer(PresetManager* presetManager, SceneEngine* sceneEngine);

    // ---- Sequence Loading ----

    /// Loads cues from config/sequence.json.
    /// Degrades gracefully: missing file → no cues (log warning),
    /// malformed JSON → no cues (log error), empty array → no cues.
    void loadSequence(const std::string& path);

    // ---- Cue Navigation ----

    /// Triggers a cue by its 1-indexed cue number.
    /// Applies preset via PresetManager (if valid) then calls
    /// SceneEngine::transitionTo(). Returns false if cue is invalid.
    bool triggerCue(int cueNumber);

    /// Triggers the cue at currentCueIndex_ + 1. Clamps at max (does not wrap).
    bool triggerNext();

    /// Triggers the cue at currentCueIndex_ - 1. Clamps at 0.
    bool triggerPrev();

    // ---- Accessors ----

    /// Returns the 0-indexed position of the last triggered cue, or -1 if none.
    int getCurrentCueIndex() const;

    /// Returns the number of loaded cues.
    int getCueCount() const;

    /// Returns a pointer to the cue at a given 0-based index, or nullptr.
    const CueDef* getCue(int index) const;

    /// Returns true if at least one cue is loaded.
    bool hasSequence() const;

    /// Find cue index (0-based) by MIDI trigger type+value, or -1 if not found.
    int findCueByMidi(const std::string& type, int value) const;

    // ---- Cue Mutation ----

    /// Append a new cue. Fails (returns false) if cue number already exists.
    bool addCue(const CueDef& cue);

    /// Remove the cue with the given 1-indexed cue number.
    /// Returns false if not found.
    bool removeCue(int cueNumber);

    /// Replace the cue matching cueNumber with newDef.
    /// Fails if cue not found, or if new cue number conflicts with another cue.
    /// Self-update (same cue number) is allowed.
    bool updateCue(int cueNumber, const CueDef& newDef);

    /// Serialize current cue list to path as {"cues": [...]} JSON.
    /// Returns false on write error.
    bool saveSequence(const std::string& path) const;

    /// Move cue at fromIndex to toIndex (0-based positions).
    /// Returns false if either index is out of range.
    bool reorderCues(int fromIndex, int toIndex);

private:
    /// Find a cue by its 1-indexed cue number. Returns index into cues_ or -1.
    int findCueIndex(int cueNumber) const;

    /// Check whether a cue number already exists in cues_, ignoring the entry
    /// at excludeIdx (pass -1 to check all). Used for uniqueness validation.
    bool isDuplicateCueNumber(int cueNumber, int excludeIdx = -1) const;

    std::vector<CueDef> cues_;
    int currentCueIndex_ = -1;      // -1 = no cue triggered yet
    PresetManager* presetManager_;
    SceneEngine* sceneEngine_;
};

} // namespace modular
