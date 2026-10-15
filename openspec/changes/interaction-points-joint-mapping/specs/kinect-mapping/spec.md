# Delta for kinect-mapping

## ADDED Requirements

### Requirement: VEC2_XY Transform Type

`KinectTransformType` MUST add `VEC2_XY` as a fifth enum value. When selected, the mapping SHALL dispatch the joint's X and Y as `glm::vec2` to the scene via `setParameter(paramId, vec2)`. Float range scaling and smoothing SHALL NOT apply. Condition evaluation SHALL be skipped (VEC2_XY implies always dispatch).

#### Scenario: VEC2 dispatch with coordinate conversion

- GIVEN mapping: transformType=VEC2_XY, targetParamId="furry.hand_right", screen 1920×1080
- WHEN hand_right joint at normalized (0.5, 0.3) is processed
- THEN `scene->setParameter("hand_right", glm::vec2(960, 324))` is called

#### Scenario: VEC2 skips range scaling

- GIVEN mapping: VEC2_XY with minOutput=0.2, maxOutput=0.8 configured
- WHEN dispatched
- THEN output range settings are ignored; raw pixel coords are dispatched

#### Scenario: Float dispatch unaffected by VEC2_XY addition

- GIVEN mapping with transformType=LINEAR, targetParamId="pinturaFluorescente.flockSpeed"
- WHEN processJointData() runs
- THEN existing `setParameter("flockSpeed", floatValue)` path still works unchanged

### Requirement: Joint-to-Vec2 Coordinate Conversion

When dispatching VEC2_XY, the engine MUST extract X and Y axes from the joint's `Vector3D` position. Normalized coordinates (0.0–1.0) SHALL be converted to pixel-space: `pixelX = joint.position.x × ofGetWidth()`, `pixelY = joint.position.y × ofGetHeight()`. Origin SHALL match screen coordinate system (top-left = 0,0).

#### Scenario: Edge coordinates

- GIVEN screen 1920×1080, joint at normalized (0.0, 0.0)
- WHEN VEC2_XY dispatch
- THEN scene receives vec2(0, 0)

#### Scenario: Joint with depth ignored

- GIVEN joint at (0.5, 0.3, 2.0) normalized
- WHEN VEC2_XY dispatch
- THEN only X and Y are used; Z is discarded

## MODIFIED Requirements

### Requirement: Transform Functions

MUST support five transform types: `LINEAR` (passthrough), `EXPONENTIAL`, `LOGARITHMIC`, `INVERT` (1.0 - value), `VEC2_XY` (raw (X,Y) dispatch with coordinate conversion). For VEC2_XY: transform is identity; range scaling and smoothing SHALL NOT apply. All other transforms SHALL apply after range scaling and before dispatch.

(Previously: four transform types — LINEAR, EXPONENTIAL, LOGARITHMIC, INVERT)

#### Scenario: INVERT transform

- GIVEN input 0.2, range [0,1]→[0,1], transform INVERT
- WHEN value is processed
- THEN output is 0.8

### Requirement: Scene Dispatch

MUST dispatch via `scene->setParameter(localParamId, value)` on the active scene. For float transforms SHALL call `setParameter(string, float)`. For VEC2_XY SHALL call `setParameter(string, glm::vec2)`. SHALL NOT expose Kinect types to scenes.

(Previously: dispatched only float values)

#### Scenario: Float dispatch to active scene

- GIVEN scene "pinturaFluorescente" active; mapping hand_right.position.y → pinturaFluorescente.flockSpeed
- WHEN right hand y-coordinate is 0.8
- THEN `scene->setParameter("flockSpeed", scaledValue)` is called

#### Scenario: VEC2 dispatch to active scene

- GIVEN scene "furry" active; mapping VEC2_XY targetParamId="furry.hand_right"
- WHEN hand_right joint at (0.5, 0.3) is processed
- THEN `scene->setParameter("hand_right", glm::vec2(960, 324))` is called
