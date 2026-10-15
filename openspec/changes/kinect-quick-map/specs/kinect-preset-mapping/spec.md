# kinect-preset-mapping Specification

## Purpose
Per-preset Kinect mapping lifecycle: store mappings inside PresetDef, apply/replace on preset switch, persist through JSON.

## Requirements

### Requirement: Preset-Bound Mapping Storage
PresetDef MUST include `std::vector<KinectMapping> kinectMappings`. Quick Map operations SHALL modify both the active PresetDef's vector and the KinectMappingEngine.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Store mapping | Active preset "lite" has no mappings | User clicks joint in Quick Map | Mapping added to preset "lite".kinectMappings and to engine |
| Old preset JSON | File lacks `kinectMappings` key | loadPresets() parses file | kinectMappings defaults to empty vector; no error |

### Requirement: Preset Switch Mapping Lifecycle
applyPreset() MUST call `engine->removeMappingsByScene(sceneId)` then `engine->addMappings(preset.kinectMappings)` before parameter application.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Replace mappings | Scene has 5 mappings; preset "lite" has 2 | applyPreset(scene, "lite") | Scene has exactly 2 mappings from preset |
| Empty preset mappings | Preset "clean" has empty kinectMappings | applyPreset(scene, "clean") | Scene has 0 Kinect mappings |

### Requirement: JSON Round-Trip for Mappings
loadPresets() SHALL parse optional `kinectMappings` array. savePresets() SHALL serialize it. Free functions `kinectMappingToJson/ofJsonToKinectMapping` MUST be extracted from engine's existing serialization.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Save with mappings | Preset "lite" has 3 kinectMappings | savePresets(sceneId) | JSON contains `kinectMappings` array with 3 entries |
| Load with mappings | JSON has `kinectMappings` with 2 entries | loadPresets(sceneId, path) | PresetDef.kinectMappings has 2 entries |
| Old file backward-compat | JSON from older version without kinectMappings | loadPresets() | Preset loads normally; kinectMappings empty |
