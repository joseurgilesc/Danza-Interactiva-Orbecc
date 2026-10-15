# Design: Kinect Quick Map

## Technical Approach

Three loosely-coupled components: (1) **Quick Map tab** adds click-to-assign VEC2_XY via hit-tested skeleton joints, (2) **PresetDef** stores `kinectMappings` + `morphDuration` + `morphEasing` with full JSON round-trip, and (3) **applyPreset** replaces scene mappings on preset switch. Morph easing is a pure math layer injected into `update()`.

## Architecture Decisions

| Decision | Option A | Option B | Choice | Rationale |
|---|---|---|---|---|
| Hit-testing | ImGui InvisibleButton per joint | Manual distance check on `ImGui::IsMouseClicked(0)` | Distance check | Skeleton positions are dynamic (tracking data); InvisibleButton requires fixed rects. Distance check against `toScreen()` output is simpler and accurate. |
| KinectPanel access to PresetManager | Inject via constructor | Global/singleton access | Constructor injection | Follows existing pattern — KinectPanel already receives `adapter`, `mappingEngine`, `sceneEngine` via constructor. |
| Serialization helpers location | `KinectMappingTypes.h` as free functions | Static methods on engine | Free functions in `KinectMappingTypes.h` | Reusable without engine instance; keeps format definition in types header alongside the struct. |
| Easing functions | Inline lambdas in `update()` | Named free function `applyEasing(t, easing)` | Named free function | Testable, self-documenting, shared between morph update and any future consumer. |
| Easing per MorphState | Store at morph creation time | Look up from PresetDef every frame | Store at creation | Consistent behavior if preset changes mid-morph; snapshot semantics. |

## Data Flow

```
User clicks joint in Quick Map
  │
  ▼
drawQuickMapTab() → ImGui::IsMouseClicked(0) + distance check against 18 joints
  │
  ▼
Create KinectMapping { jointSource="{jointName}.x",
  targetParameterId="{sceneId}.{jointName}", transform=VEC2_XY, condition=CONTINUOUS }
  │
  ├──▶ engine->addMapping(mapping)              // immediate live dispatch
  │
  └──▶ Write to selected PresetDef.kinectMappings  // persistence source-of-truth
```

```
Preset switch (SceneSequencer or PresetPanel Apply)
  │
  ▼
applyPreset(scene, name, morphDuration)
  │
  ├─ 1. engine->removeMappingsByScene(sceneId)       // sweep old
  ├─ 2. engine->addMappings(preset.kinectMappings)   // load new
  ├─ 3. If morphDuration ≤ 0: use preset->morphDuration (fallback)
  ├─ 4. Create MorphState per param with preset->morphEasing captured
  └─ 5. Instant-set or morph-start params
```

## File Changes

| File | Action | Description |
|---|---|---|
| `src/mapping/KinectMappingTypes.h` | Modify | Add `kinectMappingToJson(const KinectMapping&)` and `kinectMappingFromJson(const ofJson&)` declarations |
| `src/mapping/KinectMappingEngine.h` | Modify | Add `removeMappingsByScene(sceneId)`, `addMappings(vector)` method declarations |
| `src/mapping/KinectMappingEngine.cpp` | Modify | Extract serialization bodies to free functions; implement `removeMappingsByScene` (erase-if `startsWith(sceneId + ".")`) and `addMappings` (loop `addMapping` with duplicate skip) |
| `src/presets/PresetManager.h` | Modify | Add `enum class MorphEasing { LINEAR, EASE_IN, EASE_OUT, EASE_IN_OUT }` and free function `applyEasing(float t, MorphEasing)`. `PresetDef` gains `kinectMappings`, `morphDuration`, `morphEasing`. `MorphState` gains `MorphEasing easing`. Forward-declare `KinectMapping` |
| `src/presets/PresetManager.cpp` | Modify | Easing math: `LINEAR→t`, `EASE_IN→t²`, `EASE_OUT→1−(1−t)²`, `EASE_IN_OUT→3t²−2t³`. `loadPresets`: parse new fields with defaults. `savePresets`: serialize new fields. `applyPreset`: `removeMappingsByScene` + `addMappings` before params; duration fallback; capture easing in `MorphState`. `update`: eased lerp via `applyEasing()` |
| `src/ui/KinectPanel.h` | Modify | Add `PresetManager*` param to constructor; add `presetManager_` member; add `drawQuickMapTab()` declaration; add quick-map state vars: `quickSceneIndex_`, `quickPresetIndex_`, toast message timer |
| `src/ui/KinectPanel.cpp` | Modify | Add 4th `BeginTabItem("Quick Map")` in `draw()`. Implement `drawQuickMapTab`: scene dropdown, preset dropdown, skeleton render (adapted from JointsTab L331–411 using same `kJointBonePairs` and `toScreen`) with hit-testing via `ImGui::IsMouseClicked(0)` + distance-to-joint check. Gold-ring overlay (`AddCircle`) on mapped joints. On click: build `KinectMapping`, dispatch to engine + preset |
| `src/ui/PresetPanel.h/.cpp` | Modify | Add per-preset editor controls: `morphDuration` slider + `MorphEasing` combo that write to selected PresetDef and persist via `savePresets`. Keep existing global `morphDuration_` slider as apply-time override |
| `src/ofApp.cpp` | Modify | Add `presetManager_.get()` as 4th argument to `KinectPanel` constructor |

## Interfaces / Contracts

```cpp
// PresetManager.h additions
enum class MorphEasing : uint8_t { LINEAR, EASE_IN, EASE_OUT, EASE_IN_OUT };
float applyEasing(float t, MorphEasing easing);

struct PresetDef {
    std::string name;
    std::unordered_map<std::string, float> params;
    std::vector<KinectMapping> kinectMappings;               // NEW
    float morphDuration = 0.0f;                              // NEW
    MorphEasing morphEasing = MorphEasing::LINEAR;          // NEW
};

struct MorphState {
    // ... existing fields: sceneId, presetName, paramId, start, target, elapsed, duration, active
    MorphEasing easing = MorphEasing::LINEAR;               // NEW
};

// Engine additions
void KinectMappingEngine::removeMappingsByScene(const std::string& sceneId);
void KinectMappingEngine::addMappings(const std::vector<KinectMapping>& mappings);

// KinectMappingTypes.h additions
ofJson kinectMappingToJson(const KinectMapping& m);
KinectMapping kinectMappingFromJson(const ofJson& j);
```

## JSON Schema (Backward-Compatible)

Missing `kinectMappings` → empty vector. Missing `morphDuration` → 0.0f. Missing `morphEasing` → `"linear"`.

```json
{
  "sceneId": "pinturaFluorescente",
  "presets": [
    {
      "name": "lite",
      "morphDuration": 2.0,
      "morphEasing": "ease_in_out",
      "kinectMappings": [
        {
          "mappingId": "kinect-map-1",
          "jointSource": "wrist_right.x",
          "targetParameterId": "pinturaFluorescente.wrist_right",
          "condition": 4, "threshold": 0.5,
          "minRange": 0.0, "maxRange": 1.0, "gestureType": "",
          "minInput": 0.0, "maxInput": 1.0,
          "minOutput": 0.0, "maxOutput": 1.0,
          "transformType": 4,
          "enabled": true, "smoothing": 0.0
        }
      ],
      "params": { "brush_size": 0.5, "opacity": 0.8 }
    }
  ]
}
```

## Error & Guard States

| State | Guard | Behavior |
|---|---|---|
| No tracking data | `getAllTrackedBodies().empty()` | Ghost skeleton (low alpha), message "Start tracking to see the skeleton", clicks disabled |
| No active scene | `sceneEngine_->getActiveScene() == nullptr` | Message "No active scene", scene selector disabled |
| No preset selected in Quick Map | `quickPresetIndex_ < 0` | Mapping added to engine only; toast "Select a preset to store mapping" |
| Duplicate click | Engine `findDuplicate()` rejects | `addMapping` returns ""; toast "Already mapped" shown for 2s |
| Old JSON lacks new fields | `entry.contains("kinectMappings")` false | Default empty vector and 0.0f/LINEAR; no error logged |
| Unknown preset in applyPreset | Preset not found | Log error, leave scene unchanged |

## Testing Strategy

| Layer | What to Test | Approach |
|---|---|---|
| Unit | `applyEasing()` correct at t=0, 0.5, 1.0 | Inline math assertion |
| Unit | `removeMappingsByScene` exact match, no match, partial match | Engine unit test with pre-seeded mappings |
| Unit | `kinectMappingToJson/FromJson` round-trip | JSON string compare |
| Unit | `loadPresets` backward-compat (old JSON without new fields) | Fixture with old-format JSON |
| Integration | Click skeleton joint → engine has mapping + preset has mapping | In-app manual test (no test framework) |
| Integration | applyPreset replaces engine mappings | In-app: create mappings manually, switch preset, verify |
| Build | `make` compiles cleanly | `make` command |

## Open Questions

None.
