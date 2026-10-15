# Interaction Points Specification

## Purpose

Multi-point interaction layer bridging Kinect joints to scene effects. Replaces single `interactionPoint_` and raw `ofGetMouse()` with a named-point map supporting decay on tracking loss.

## Requirements

### Requirement: InteractionPoint Struct Definition

The system MUST define `InteractionPoint` with fields:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `position` | `glm::vec2` | (0,0) | Pixel coordinates |
| `velocity` | `glm::vec2` | (0,0) | Pixels per second |
| `confidence` | `float` | 0.0 | 0.0–1.0 tracking confidence |
| `decayRate` | `float` | 1.0 | 0.0–1.0 linear fade per second |
| `active` | `bool` | `false` | Whether point contributes to effects |
| `sourceName` | `string` | `""` | e.g. `"hand_left"`, `"mouse"` |

#### Scenario: Default point is inactive

- GIVEN a default-constructed InteractionPoint
- WHEN its fields are inspected
- THEN `active` MUST be `false` and `confidence` MUST be `0.0`

### Requirement: Per-Scene Interaction Point Map

Each scene MUST store `map<string, InteractionPoint>` replacing the single `interactionPoint_` field. Operations: add by key, update (merge position + velocity + confidence), remove by key, iterate active points.

#### Scenario: Add point from VEC2 dispatch

- GIVEN scene has empty interaction map, deltaTime=0.016s
- WHEN `setParameter("hand_left", vec2(320, 240))` is called
- THEN map contains key `"hand_left"` with position=(320,240), active=true, confidence=1.0

#### Scenario: Update computes velocity

- GIVEN map has `"hand_left"` at (320,240) from last frame
- WHEN `setParameter("hand_left", vec2(330, 250))` arrives 0.016s later
- THEN velocity is (625, 625) px/s; position updates to (330,250)

### Requirement: Linear Decay on Tracking Loss

When confidence drops to 0 (tracking lost), the point SHALL enter linear decay: each frame confidence decreases by `decayRate × deltaTime`. Position SHALL continue along last velocity during decay. When confidence reaches 0, `active` becomes `false`.

#### Scenario: Point fades over 2 seconds

- GIVEN point with confidence=0, decayRate=0.5, active=true, velocity=(100, 0)
- WHEN 2.0 seconds elapse at steady 60fps
- THEN after 2s: confidence=0.0, active=false, position advanced by 200px along velocity

### Requirement: Scenes Read Interaction Map in update()

All 3 scenes (Furry, Anemone, PinturaFluorescente) MUST read active points from their interaction map in `update()` instead of calling `ofGetMouseX()`/`ofGetMouseY()`. Each scene SHALL blend all active points into its effect calculation.

#### Scenario: FurryScene uses multi-point map

- GIVEN FurryScene has 2 active points: `"hand_left"` at (200,300), `"hand_right"` at (800,300)
- WHEN `update(deltaTime)` runs
- THEN fur direction blends both point positions
- AND `ofGetMouseX()`/`ofGetMouseY()` are NOT called

### Requirement: Global useMouseInput Toggle

A `useMouseInput` boolean MUST control whether mouse hardware feeds a synthetic `"mouse"` point. When `false`, scenes SHALL NOT receive mouse-driven points and SHALL NOT call `ofGetMouse()`.

#### Scenario: Mouse disabled

- GIVEN `useMouseInput=false` and scene active
- WHEN user moves mouse to (400,300)
- THEN no `"mouse"` key exists in the scene's interaction map

#### Scenario: Mouse enabled

- GIVEN `useMouseInput=true` and scene active
- WHEN user moves mouse to (400,300)
- THEN map contains `"mouse"` key with position=(400,300), active=true

### Requirement: Coordinate Conversion

KinectMappingEngine MUST convert normalized joint coords (0.0–1.0) to pixel coords: `pixelX = normX × screenWidth`, `pixelY = normY × screenHeight`. SHALL use current `ofGetWidth()`/`ofGetHeight()`.

#### Scenario: Normalized to pixel conversion

- GIVEN screen 1920×1080, joint at normalized (0.5, 0.25)
- WHEN VEC2_XY dispatch occurs
- THEN scene receives vec2(960, 270)

### Requirement: Vec2 Parameter Interface

All 3 scenes MUST implement `setParameter(string, glm::vec2)` to populate their interaction point map. The paramId SHALL name the point key. Implementation SHALL be uniform: look up or create point, merge position + confidence, compute velocity from position delta.

#### Scenario: Vec2 dispatch reaches scene

- GIVEN FurryScene implements setParameter(vec2)
- WHEN `setParameter("hand_right", glm::vec2(800, 400))` is called
- THEN FurryScene's interaction map `"hand_right"` point is populated with given position
