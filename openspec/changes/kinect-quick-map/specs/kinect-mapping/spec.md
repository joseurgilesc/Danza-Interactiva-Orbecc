# Delta for kinect-mapping

## ADDED Requirements

### Requirement: Scene-Filtered Mapping Removal
KinectMappingEngine MUST provide `removeMappingsByScene(const std::string& sceneId)` that erases all mappings whose `targetParameterId` starts with `sceneId + "."`.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Remove scene mappings | 3 mappings for "pinturaFluorescente.x", 2 for "furry.y" | removeMappingsByScene("pinturaFluorescente") | 2 "furry" mappings remain; 3 removed |
| No match | No mappings for "ribbonDance" | removeMappingsByScene("ribbonDance") | Mappings unchanged; no error |

### Requirement: Batch Mapping Addition
KinectMappingEngine MUST provide `addMappings(const std::vector<KinectMapping>& mappings)` that adds all entries, skipping duplicates via existing duplicate detection.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Batch add | 3 mappings, no duplicates | addMappings(vec) | All 3 appear in getMappings() |
| Partial duplicate | 2 mappings; 1 duplicates existing | addMappings(vec) | 1 added; duplicate skipped with warning |

### Requirement: Serialization Helper Functions
Free functions `kinectMappingToJson(const KinectMapping&)` and `kinectMappingFromJson(const ofJson&)` MUST be extracted from engine's existing saveToJson/loadFromJson. These SHALL be declared in KinectMappingTypes.h, callable without engine instance, and backward-compatible with existing JSON output.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Round-trip single mapping | Valid KinectMapping with all fields set | kinectMappingFromJson(kinectMappingToJson(m)) | Returns identical KinectMapping |
| Backward-compatible save | Existing mappings saved via engine | saveToJson output | Identical to output using new free functions |
| Backward-compatible load | JSON from older engine version | loadFromJson | Parses all fields; new fields (if any) default |
