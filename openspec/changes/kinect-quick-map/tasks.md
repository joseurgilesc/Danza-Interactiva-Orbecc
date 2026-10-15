# Tasks: Kinect Quick Map

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~430–470 |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Suggested split | PR 1 → PR 2 → PR 3 |
| Delivery strategy | ask-on-risk |
| Chain strategy | pending |

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: pending
400-line budget risk: High

### Suggested Work Units

| Unit | Goal | Likely PR | Notes |
|------|------|-----------|-------|
| 1 | Engine + serialization helpers (~80 lines) | PR 1 | Foundation — no UI; pure engine/types changes |
| 2 | PresetDef + morph easing + applyPreset (~170 lines) | PR 2 | Depends on PR 1 engine methods; no UI |
| 3 | Quick Map tab + PresetPanel morph UI (~200 lines) | PR 3 | Depends on PR 2 PresetDef fields + engine |

## Phase 1: Foundation — Engine & Type Helpers

- [x] 1.1 `src/mapping/KinectMappingTypes.h`: Declare `kinectMappingToJson(const KinectMapping&)` and `kinectMappingFromJson(const ofJson&)` as free functions
- [x] 1.2 `src/mapping/KinectMappingEngine.h`: Declare `removeMappingsByScene(const std::string&)` and `addMappings(const std::vector<KinectMapping>&)`
- [x] 1.3 `src/mapping/KinectMappingEngine.cpp`: Extract serialization bodies from `saveToJson`/`loadFromJson` into the free functions; implement `removeMappingsByScene` (erase-if `targetParameterId` starts with `sceneId+"."`); implement `addMappings` (loop `addMapping`, skip duplicates)

## Phase 2: Preset Core — PresetDef + Morph Easing + applyPreset

- [x] 2.1 `src/presets/PresetManager.h`: Add `enum class MorphEasing { LINEAR, EASE_IN, EASE_OUT, EASE_IN_OUT }` and free function `applyEasing(float t, MorphEasing)`. `PresetDef` gains `vector<KinectMapping> kinectMappings`, `float morphDuration = 0.0f`, `MorphEasing morphEasing = LINEAR`. `MorphState` gains `MorphEasing easing`
- [x] 2.2 `src/presets/PresetManager.cpp`: Implement easing math (t², 1−(1−t)², 3t²−2t³) in `applyEasing()`. In `loadPresets()`: parse optional `kinectMappings` array via `kinectMappingFromJson`, `morphDuration`, `morphEasing` string; use defaults if missing. In `savePresets()`: serialize `kinectMappings` via `kinectMappingToJson`, `morphDuration`, `morphEasing` string. In `applyPreset()`: duration fallback (0 → preset->morphDuration), `engine->removeMappingsByScene` + `engine->addMappings` before params, capture `preset->morphEasing` in `MorphState`. In `update()`: apply `applyEasing(t)` before lerp

## Phase 3: UI Wiring — Quick Map Tab + PresetPanel + ofApp

- [x] 3.1 `src/ui/KinectPanel.h`: Add `PresetManager*` 4th param to constructor; add `presetManager_`, `drawQuickMapTab()` declaration, state vars: `quickSceneIndex_`, `quickPresetIndex_`, `toastMessage_`, `toastTimer_`
- [x] 3.2 `src/ui/KinectPanel.cpp`: Add 4th `BeginTabItem("Quick Map")`. Implement `drawQuickMapTab()`: scene dropdown from `sceneEngine_->getLoadedScenes()`, preset dropdown from `presetManager_->getPresets(sceneId)`, skeleton adapt `kJointBonePairs` and `toScreen` from Joints tab (L355–411), hit-test via `ImGui::IsMouseClicked(0)` + distance check, gold ring via `AddCircle` on mapped joints. On click: build `KinectMapping{jSource="{joint}.x", target="{sceneId}.{joint}", VEC2_XY, CONTINUOUS}`, dispatch to engine, write to `PresetDef.kinectMappings`. Guard states: ghost skeleton + "Start tracking" when no bodies; "No active scene" message + disabled selector when null scene
- [x] 3.3 `src/ui/PresetPanel.h/.cpp`: Add `morphDuration` slider (0–10s) + `MorphEasing` combo in preset editor section, writing to selected `PresetDef` and calling `savePresets()`. Wire Apply button to pass `0.0f` when preset has own duration
- [x] 3.4 `src/ofApp.cpp`: Add `presetManager_.get()` as 4th argument to `KinectPanel` constructor (line 214–215)
