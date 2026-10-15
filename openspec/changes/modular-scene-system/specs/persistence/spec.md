# State Persistence Specification

## Purpose

Handles saving and loading of mappings, presets, and system configuration to/from JSON files.

## Requirements

### Requirement: Mapping Persistence

The PresetPersistence MUST save and load mapping configurations.

- GIVEN MappingLayer with active mappings
- WHEN `saveMappings("config.json")` is called
- THEN write JSON with all mappings to file

- GIVEN mappings saved to "config.json"
- WHEN `loadMappings("config.json")` is called
- THEN restore all mappings in MappingLayer

### Requirement: JSON Format

The PresetPersistence SHALL use JSON for serialization.

- GIVEN mappings saved
- THEN JSON structure is:
```json
{
  "mappings": [
    { "source": "osc", "channel": 1, "param": "scene.k", "in": [0, 1], "out": [0.1, 2.0] },
    { "source": "keyboard", "key": "1", "action": "activate", "target": "sceneA" }
  ]
}
```

### Requirement: Full Preset Persistence

The PresetPersistence MUST save complete system state.

- GIVEN full system configuration
- WHEN `savePreset("preset.json")` is called
- THEN save: mappings, transport BPM, scheduler cues, key bindings

- GIVEN preset file exists
- WHEN `loadPreset("preset.json")` is called
- THEN restore all saved configuration

### Requirement: Export/Import

The PresetPersistence SHALL support exporting and importing presets.

- GIVEN system with current state
- WHEN `exportConfig(path)` called
- THEN create portable config file

- GIVEN valid config file
- WHEN `importConfig(path)` called
- THEN apply configuration to system

### Requirement: Error Handling

The PresetPersistence MUST handle file I/O errors gracefully.

- GIVEN non-existent file
- WHEN `loadMappings("missing.json")` called
- THEN return error code, log warning, no crash

- GIVEN invalid JSON
- WHEN `loadMappings("corrupt.json")` called
- THEN return error, log error details

---

## Verification Scenarios

### Round-trip Persistence

- GIVEN mappings saved to file
- WHEN file loaded in new session
- THEN mappings are identical to original

### Partial Load

- GIVEN config with 10 mappings, 2 invalid
- WHEN loaded
- THEN 8 valid mappings applied, 2 errors logged

### File Permissions

- GIVEN read-only directory
- WHEN save attempted
- THEN error returned, no crash