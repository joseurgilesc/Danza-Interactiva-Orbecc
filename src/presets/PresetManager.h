#pragma once

#include "ofMain.h"
#include "scenes/BaseScene.h"
#include "mapping/KinectMappingTypes.h"
#include "midi/MidiMapping.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace modular {

// Forward declarations
class SceneEngine;
class KinectMappingEngine;
class MidiMappingEngine;

// ============================================================================
// MorphEasing — easing curve for preset morph transitions
// ============================================================================

enum class MorphEasing : uint8_t {
    LINEAR,
    EASE_IN,
    EASE_OUT,
    EASE_IN_OUT
};

/// Apply an easing curve to normalized time t (0.0–1.0).
/// LINEAR→t, EASE_IN→t², EASE_OUT→1−(1−t)², EASE_IN_OUT→smoothstep (3t²−2t³).
float applyEasing(float t, MorphEasing easing);

// ============================================================================
// PresetDef — a named collection of parameter overrides and Kinect mappings
// ============================================================================

struct PresetDef {
    std::string name;
    std::unordered_map<std::string, float> params;       // paramKey → targetValue
    std::vector<KinectMapping> kinectMappings;            // per-preset Kinect mappings
    std::vector<MidiMapping> midiMappings;                // per-preset MIDI param mappings
    float morphDuration = 0.0f;                           // 0 = instant
    MorphEasing morphEasing = MorphEasing::LINEAR;        // easing curve
};

// ============================================================================
// MorphState — tracks a single lerping parameter during a morph
// ============================================================================

struct MorphState {
    std::string sceneId;
    std::string presetName;
    std::string paramId;
    float start;
    float target;
    float elapsed;
    float duration;
    bool active;
    MorphEasing easing;

    MorphState()
        : start(0.0f), target(0.0f), elapsed(0.0f), duration(0.0f)
        , active(false), easing(MorphEasing::LINEAR) {}
};

// ============================================================================
// PresetManager — loads JSON presets per scene, applies with optional morphing
// ============================================================================

class PresetManager {
public:
    explicit PresetManager(SceneEngine* sceneEngine, KinectMappingEngine* mappingEngine = nullptr,
                           MidiMappingEngine* midiMappingEngine = nullptr);

    /// Load presets from a JSON file for a given sceneId.
    /// Missing file → warning, no presets.
    /// Malformed JSON → error logged, no presets.
    /// Empty array → info logged, no presets.
    void loadPresets(const std::string& sceneId, const std::string& path);

    /// Apply a preset to a scene.
    /// If morphDuration > 0, current parameter values are captured as start
    /// and MorphState entries are created. Previous morphs for overlapping
    /// params in this scene are cancelled (most-recent-wins).
    void applyPreset(ScenePtr scene, const std::string& presetName, float morphDuration = 0.0f);

    /// Advance all active morphs by deltaTime seconds.
    /// Each param is lerped: start + (target - start) * (elapsed / duration).
    /// Completed morphs are removed at the end of the frame.
    void update(float deltaTime);

    /// Returns the preset list for a given sceneId (empty vector if none loaded).
    std::vector<PresetDef> getPresets(const std::string& sceneId) const;

    /// True when at least one morph is still active.
    bool hasActiveMorph() const;

    /// Returns the average progress (0.0–1.0) of all morphing params for the
    /// given scene+preset pair. Returns 0.0 if no matching morph exists.
    float getMorphProgress(const std::string& sceneId, const std::string& presetName) const;

    /// Returns unique scene IDs that currently have active morphs.
    std::vector<std::string> getActiveMorphSceneIds() const;

    /// Save presets for a scene back to its JSON file.
    /// Returns false if no path was recorded for this sceneId.
    bool savePresets(const std::string& sceneId);

    /// Add a new preset or overwrite an existing one. Does NOT auto-save.
    void addOrUpdatePreset(const std::string& sceneId, const PresetDef& preset);

    /// Delete a preset by name. Does NOT auto-save.
    bool deletePreset(const std::string& sceneId, const std::string& presetName);

    /// Capture current scene parameter values into a PresetDef.
    PresetDef captureCurrentParams(ScenePtr scene, const std::string& presetName);

    /// Sync the MidiMappingEngine's active mappings back to the current preset's
    /// PresetDef and save to JSON. Call this before switching presets or on explicit save.
    void syncMidiMappingsToCurrentPreset();

private:
    std::unordered_map<std::string, std::vector<PresetDef>> presets_;  // sceneId → presets
    std::unordered_map<std::string, std::string> presetPaths_;          // sceneId → JSON file path
    std::vector<MorphState> morphs_;                                    // active morph states
    SceneEngine* sceneEngine_;                                          // for scene lookup during morph update
    KinectMappingEngine* mappingEngine_;                                // for Kinect mapping lifecycle on applyPreset (nullable)
    MidiMappingEngine* midiMappingEngine_;                              // for MIDI mapping lifecycle on applyPreset (nullable)
    std::string currentPresetSceneId_;                                  // sceneId of last applied preset
    std::string currentPresetName_;                                     // name of last applied preset
};

} // namespace modular
