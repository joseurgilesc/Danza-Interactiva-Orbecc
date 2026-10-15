# Parameter Registry Specification

## Purpose

Central registry for all parameters in the system. Provides single point of access for reading and modifying parameters across all scenes.

## Requirements

### Requirement: Parameter Registration

The ParameterRegistry MUST allow registration of parameters with unique IDs.

- GIVEN ParameterRegistry
- WHEN `registerParam(Parameter)` is called
- THEN parameter is stored in internal map by ID

- GIVEN ParameterRegistry with existing parameter "scene.k"
- WHEN `registerParam()` called with same ID
- THEN return error or overwrite based on config

### Requirement: Parameter Access

The ParameterRegistry SHALL provide get and set operations for parameter values.

- GIVEN registered parameter "scene.k" with value 1.0
- WHEN `getParam("scene.k")` is called
- THEN return Parameter with value 1.0

- GIVEN registered parameter "scene.k"
- WHEN `setParam("scene.k", 0.5)` is called
- THEN parameter value is updated to 0.5
- AND all subscribers are notified

- GIVEN unregistered parameter ID
- WHEN `getParam()` or `setParam()` is called
- THEN return error/empty and log warning

### Requirement: Value Range Clamping

The ParameterRegistry MUST enforce min/max bounds on parameter values.

- GIVEN parameter with min=0.0, max=2.0
- WHEN `setParam(id, 3.0)` is called
- THEN value is clamped to 2.0

- GIVEN parameter with min=0.0, max=2.0
- WHEN `setParam(id, -1.0)` is called
- THEN value is clamped to 0.0

### Requirement: Observer Pattern

The ParameterRegistry SHALL notify observers when parameter values change.

- GIVEN ParameterRegistry with observer registered for "scene.k"
- WHEN `setParam("scene.k", newValue)` is called
- THEN observer's `onParamChanged(id, value)` is invoked

- GIVEN ParameterRegistry with multiple observers
- WHEN parameter changes
- THEN all observers are notified in registration order

### Requirement: Parameter Discovery

The ParameterRegistry MUST provide methods to list all registered parameters.

- GIVEN ParameterRegistry with multiple parameters
- WHEN `getAllParams()` is called
- THEN return vector of all Parameter objects

- GIVEN ParameterRegistry
- WHEN `getParamsByScene(sceneId)` is called
- THEN return only parameters belonging to that scene

---

## Verification Scenarios

### Concurrent Access

- GIVEN ParameterRegistry with parameter
- WHEN setParam called from multiple sources
- THEN value updates are atomic (no race condition)

### Notification Flow

- GIVEN subscribed observer
- WHEN setParam triggers change
- THEN observer receives callback with correct ID and value

### Type Safety

- GIVEN ParameterType FLOAT
- WHEN setParam with integer value
- THEN value is converted appropriately