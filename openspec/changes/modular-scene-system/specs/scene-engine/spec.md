# Scene Engine Specification

## Purpose

Central module for loading, activating, pausing, and managing the lifecycle of scenes. Acts as the orchestrator for all scene operations.

## Requirements

### Requirement: Scene Lifecycle Management

The SceneEngine MUST provide complete lifecycle control for all scenes: setup, activation, deactivation, pause, resume, update, and draw.

- GIVEN SceneEngine initialized with loaded scenes
- WHEN `activate(sceneId)` is called
- THEN the scene's `setup()` and `activate()` methods are executed
- AND the scene enters the active state

- GIVEN SceneEngine with active scene
- WHEN `deactivate(sceneId)` is called
- THEN the scene's `deactivate()` method is executed
- AND the scene returns to loaded state

- GIVEN SceneEngine with active scene
- WHEN `pause(sceneId)` is called
- THEN the scene's `update()` and `draw()` are suspended
- AND the scene retains its internal state

- GIVEN SceneEngine with paused scene
- WHEN `resume(sceneId)` is called
- THEN the scene resumes normal `update()` and `draw()` operations

### Requirement: Scene Registration

The SceneEngine SHALL maintain a registry of all available scenes by ID and name.

- GIVEN SceneEngine initialized
- WHEN `loadScene(unique_ptr<BaseScene>)` is called
- THEN the scene is stored in the internal registry
- AND a SceneInfo with id, name, and parameter list is returned

- GIVEN SceneEngine with registered scenes
- WHEN `getScene(sceneId)` is called
- THEN return pointer to the scene if found
- AND return nullptr if not found

### Requirement: Single Active Scene

The SceneEngine MUST support exactly one active scene at a time, or zero (idle state).

- GIVEN SceneEngine with active scene A
- WHEN `activate(sceneB)` is called
- THEN scene A's `deactivate()` is invoked
- AND scene B's `activate()` is invoked

### Requirement: Scene Update Loop

The SceneEngine MUST call `update()` and `draw()` on the active scene every frame.

- GIVEN SceneEngine with active scene
- WHEN `update()` is called on the engine
- THEN active scene's `update(deltaTime)` is called

- GIVEN SceneEngine with active scene
- WHEN `draw()` is called on the engine
- THEN active scene's `draw()` is called

---

## Verification Scenarios

### Activation Flow

- GIVEN SceneEngine with two loaded scenes (A, B)
- WHEN activate(A) is called, then activate(B)
- THEN scene A receives deactivate(), scene B receives activate()

### Pause/Resume

- GIVEN active scene processing updates
- WHEN pause() then resume() called
- THEN scene resumes from exact state before pause (params preserved)