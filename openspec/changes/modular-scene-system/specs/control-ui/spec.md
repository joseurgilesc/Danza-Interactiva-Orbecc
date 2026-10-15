# Control UI Specification

## Purpose

Provides visualization of system state, active parameters, and current mappings for debugging and monitoring.

## Requirements

### Requirement: Parameter Display

The ControlUI MUST display current parameter values.

- GIVEN ControlUI with ParameterRegistry connected
- WHEN render() called
- THEN display list of all parameters with current values

- GIVEN parameter "scene.k" with value 1.5
- WHEN displayed
- THEN show: "scene.k: 1.5" (or formatted: "k = 1.50")

### Requirement: Active Scene Indicator

The ControlUI SHALL show which scene is currently active.

- GIVEN SceneEngine with active scene "pinturaFluorescente"
- WHEN render() called
- THEN display "Active: pinturaFluorescente"

- GIVEN SceneEngine with no active scene
- WHEN render() called
- THEN display "Active: (none)"

### Requirement: Mapping Visualization

The ControlUI MUST display active input-to-parameter mappings.

- GIVEN MappingLayer with mapping osc.ch1 → scene.k
- WHEN render() called
- THEN display "OSC/1 → scene.k"

- GIVEN multiple mappings
- WHEN displayed
- THEN show all mappings in list format

### Requirement: Transport Status

The ControlUI SHALL display BPM, beat, and bar information.

- GIVEN Transport at 120 BPM, beat 3, bar 5
- WHEN render() called
- THEN display "BPM: 120 | Beat: 3/4 | Bar: 5"

- GIVEN Transport stopped
- WHEN displayed
- THEN show "STOPPED" or similar indicator

### Requirement: Render Integration

The ControlUI MUST integrate with ofApp draw loop.

- GIVEN ControlUI initialized
- WHEN ofApp::draw() calls controlUI.render()
- THEN draw overlay in corner of screen

- GIVEN ControlUI rendered
- WHEN window is 1920x1080
- THEN UI draws in top-left or configurable position

---

## Verification Scenarios

### Parameter Updates

- GIVEN ControlUI displaying parameter
- WHEN ParameterRegistry updates value
- THEN UI reflects new value on next render

### Minimal State

- GIVEN system with no scenes loaded
- WHEN ControlUI renders
- THEN display appropriate empty state message

### Performance

- GIVEN 60 FPS render loop
- WHEN ControlUI renders each frame
- THEN overhead < 1ms (simple text rendering)