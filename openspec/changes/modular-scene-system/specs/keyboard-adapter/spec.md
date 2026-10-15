# Keyboard Trigger Adapter Specification

## Purpose

Converts keyboard key presses into scene activation events and parameter changes.

## Requirements

### Requirement: Key-to-Scene Mapping

The KeyboardTriggerAdapter MUST map keys to scene activation.

- GIVEN KeyboardTriggerAdapter with mapping: key '1' → scene "pinturaFluorescente"
- WHEN key '1' is pressed
- THEN SceneEngine.activate("pinturaFluorescente") is called

- GIVEN KeyboardTriggerAdapter with mapping: key '2' → scene "flockMode"
- WHEN key '2' is pressed
- THEN SceneEngine.activate("flockMode") is called

### Requirement: Parameter Adjustment Keys

The KeyboardTriggerAdapter SHALL support keys for parameter increment/decrement.

- GIVEN mapping: key 'w' → param "scene.physics.k" +0.1
- WHEN 'w' pressed
- THEN ParameterRegistry.setParam("scene.physics.k", currentValue + 0.1)

- GIVEN mapping: key 's' → param "scene.physics.k" -0.1
- WHEN 's' pressed
- THEN ParameterRegistry.setParam("scene.physics.k", currentValue - 0.1)

### Requirement: Transport Control Keys

The KeyboardTriggerAdapter MUST map keys to transport controls.

- GIVEN mapping: space → Transport.play()
- WHEN space pressed
- THEN transport starts playing

- GIVEN mapping: escape → Transport.stop()
- WHEN escape pressed
- THEN transport stops

### Requirement: Configurable Key Bindings

The KeyboardTriggerAdapter SHALL allow runtime configuration of key bindings.

- GIVEN KeyboardTriggerAdapter
- WHEN `mapKey(key, sceneId)` is called
- THEN key is bound to scene activation

- GIVEN KeyboardTriggerAdapter with existing mapping
- WHEN new mapping added for same key
- THEN old mapping is replaced

---

## Verification Scenarios

### Scene Activation

- GIVEN key '1' mapped to scene "sceneA"
- WHEN key pressed
- THEN sceneA activates

### Parameter Modification

- GIVEN 'w' increments param by 0.1
- WHEN 'w' pressed 5 times
- THEN param increased by 0.5 total

### Key Release Handling

- GIVEN key held down
- WHEN key released
- THEN no additional action (only press triggers)