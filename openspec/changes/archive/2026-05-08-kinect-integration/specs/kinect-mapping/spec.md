# KinectMappingEngine Specification

## Purpose

Maps joint positions and gestures to scene parameters via ParameterRegistry, following the same pattern as OscMappingEngine. Scenes NEVER know Kinect — they only receive parameter updates.

## Requirements

### Requirement: Mapping CRUD Operations

MUST support `addMapping`, `removeMapping(id)`, `findMapping(id)`, and `getAllMappings()`. Each mapping MUST have: `mappingId`, `jointSource` (e.g. `"hand_right.position.y"`), `targetParameterId`, `condition` (enum), `threshold` (float), `minInput/maxInput`, `minOutput/maxOutput`, `transformType`, `enabled`, `smoothing`. IDs MUST use format `"kinect-map-N"` with auto-incrementing N.

#### Scenario: Add mapping
- GIVEN no mappings exist
- WHEN `addMapping("hand_right.position.y", "pinturaFluorescente.flockSpeed", ...)` is called
- THEN `getAllMappings()` returns one entry with `mappingId="kinect-map-1"`

#### Scenario: Duplicate rejection
- GIVEN a mapping exists for joint `"head.position.y"` → `"anemone.wave"`
- WHEN identical joint+target pair is added again
- THEN operation is rejected; warning logged

### Requirement: Condition Types

MUST support five conditions: `ABOVE_THRESHOLD`, `BELOW_THRESHOLD`, `IN_RANGE` (with min/max), `GESTURE` (hand state match), `CONTINUOUS` (always dispatch). Unmet conditions SHALL skip dispatch for that mapping.

#### Scenario: ABOVE_THRESHOLD triggers
- GIVEN condition is ABOVE_THRESHOLD at 0.5, joint source value is 0.7
- WHEN `processJointData()` is called
- THEN mapped value is dispatched to the scene parameter

#### Scenario: BELOW_THRESHOLD skips
- GIVEN condition is ABOVE_THRESHOLD at 0.5, joint source value is 0.3
- WHEN `processJointData()` is called
- THEN no dispatch occurs for this mapping

### Requirement: Transform Functions

MUST support four transform types: `LINEAR` (passthrough), `EXPONENTIAL`, `LOGARITHMIC`, `INVERT` (1.0 - value). Transform SHALL be applied after range scaling and before dispatch.

#### Scenario: INVERT transform
- GIVEN input 0.2, range [0,1]→[0,1], transform INVERT
- WHEN value is processed
- THEN output is 0.8

### Requirement: Smooth Interpolation

MUST interpolate between previous and current mapped value using `smoothing` factor (0.0 = instant, 0.9 = heavy smoothing). Applied per-mapping.

#### Scenario: Smoothing at 0.5
- GIVEN previous dispatched value was 0.0, new raw value maps to 1.0, smoothing = 0.5
- WHEN `processJointData()` runs
- THEN dispatched value is 0.5 (midpoint lerp)

### Requirement: Scene Dispatch

MUST dispatch via `scene->setParameter(localParamId, value)` on the active scene from `SceneEngine`. SHALL NOT expose Kinect types to scenes.

#### Scenario: Dispatch to active scene
- GIVEN scene "pinturaFluorescente" is active; mapping `hand_right.position.y` → `pinturaFluorescente.flockSpeed` exists
- WHEN right hand y-coordinate is 0.8
- THEN `scene->setParameter("flockSpeed", scaledValue)` is called

### Requirement: JSON Persistence

MUST save mappings via `saveToJson(filepath)` and load via `loadFromJson(filepath)`. Runtime-only fields (`lastValue`, `frameCounter`) MUST NOT be persisted.

#### Scenario: Round-trip
- GIVEN 3 mappings are saved to `kinect-mappings.json`
- WHEN `loadFromJson("kinect-mappings.json")` is called
- THEN all 3 mappings are restored with identical fields
