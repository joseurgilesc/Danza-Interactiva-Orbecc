# Mapping Layer Specification

## Purpose

Bridges external input sources (OSC, Keyboard, Kinect) to the ParameterRegistry. Handles mapping configuration, resolution, and value transformation.

## Requirements

### Requirement: Mapping Registration

The MappingLayer MUST allow registration of input-to-parameter mappings.

- GIVEN MappingLayer
- WHEN `addMapping(source, channel, paramId, inRange, outRange)` is called
- THEN mapping is stored and active

- GIVEN MappingLayer with existing mapping
- WHEN duplicate mapping added
- THEN replace existing or reject based on config

### Requirement: Input Resolution

The MappingLayer SHALL resolve input events to parameter updates.

- GIVEN MappingLayer with mapping: osc.ch1 → "scene.k" (in: 0-1, out: 0.1-2.0)
- WHEN `onInput("osc", 1, 0.5)` is called
- THEN ParameterRegistry.setParam("scene.k", 1.05) is called

- GIVEN MappingLayer with no matching mapping
- WHEN `onInput()` is called
- THEN no parameter change occurs (silent)

### Requirement: Value Transformation

The MappingLayer MUST map input values from source range to destination range.

- GIVEN mapping with inRange [0,1] and outRange [0.1, 2.0]
- WHEN input value is 0.0
- THEN output is 0.1 (minimum of outRange)

- GIVEN mapping with inRange [0,1] and outRange [0.1, 2.0]
- WHEN input value is 1.0
- THEN output is 2.0 (maximum of outRange)

- GIVEN mapping with inRange [0,1] and outRange [0.1, 2.0]
- WHEN input value is 0.5
- THEN output is 1.05 (linear interpolation)

### Requirement: Multi-Source Support

The MappingLayer SHALL handle multiple input sources through same interface.

- GIVEN mappings for OSC, Keyboard, and Kinect
- WHEN any source sends input
- THEN routing to correct mapping based on source identifier

### Requirement: Hot-Reload Mappings

The MappingLayer SHOULD support runtime mapping changes without recompilation.

- GIVEN MappingLayer with existing mappings
- WHEN `loadMappings(jsonPath)` is called
- THEN previous mappings are replaced with new configuration

### Requirement: Mapping Query

The MappingLayer MUST provide methods to query active mappings.

- GIVEN MappingLayer with multiple mappings
- WHEN `getMappingsForParam(paramId)` is called
- THEN return all mappings targeting that parameter

- GIVEN MappingLayer
- WHEN `getAllMappings()` is called
- THEN return all active mappings

---

## Verification Scenarios

### Range Transformation Accuracy

- GIVEN mapping in: [0,1], out: [10,20]
- WHEN input is 0.25
- THEN output is 12.5

### Source Filtering

- GIVEN mapping for osc channel 1
- WHEN keyboard input on channel 1 arrives
- THEN mapping is NOT triggered (source mismatch)

### Empty State

- GIVEN MappingLayer with no mappings
- WHEN input arrives
- THEN no errors thrown, operation is no-op