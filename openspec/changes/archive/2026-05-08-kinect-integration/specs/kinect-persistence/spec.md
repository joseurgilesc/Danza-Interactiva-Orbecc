# Kinect Persistence Specification

## Purpose

Save and load Kinect mapping configurations to/from JSON. Ensures mapping configurations persist across application restarts.

## Requirements

### Requirement: Save Format

Mappings MUST be saved to `config/kinect-mappings.json`. JSON format MUST be an array of objects with fields: `mappingId`, `jointSource`, `targetParameterId`, `condition`, `threshold`, `minRange`, `maxRange`, `minInput`, `maxInput`, `minOutput`, `maxOutput`, `transformType`, `enabled`, `smoothing`.

#### Scenario: Save structure
- GIVEN one mapping exists: id="kinect-map-1", source="hand_right.position.y", target="pinturaFluorescente.flockSpeed"
- WHEN `saveToJson()` is called
- THEN file contains a JSON array with one object containing all schema fields

### Requirement: Runtime Fields Excluded

Fields `lastValue` and `frameCounter` MUST NOT appear in the saved JSON. Only configuration fields SHALL be persisted.

#### Scenario: Save excludes runtime state
- GIVEN a mapping has `lastValue=0.4` and `frameCounter=120` internally
- WHEN saved to JSON
- THEN `lastValue` and `frameCounter` are absent from file; only config fields present

### Requirement: Auto-Create Config Directory

Save operation MUST create the `config/` directory if it does not exist, using `ofDirectory::createDirectory`.

#### Scenario: First save
- GIVEN `config/` directory does not exist
- WHEN `saveToJson("config/kinect-mappings.json")` is called
- THEN `config/` is created; file is written successfully

### Requirement: Graceful Load — Missing File

`loadFromJson()` MUST handle missing file gracefully: return `true`, log info, start with empty mappings list.

#### Scenario: First run
- GIVEN `config/kinect-mappings.json` does not exist
- WHEN `loadFromJson()` is called
- THEN returns `true`; `ofLogNotice` emitted; `getAllMappings()` returns empty

### Requirement: Graceful Load — Corrupt JSON

`loadFromJson()` MUST handle corrupt JSON gracefully: return `false`, log error via `ofLogError`, keep existing mappings unchanged (no corruption of in-memory state).

#### Scenario: Corrupt file
- GIVEN `config/kinect-mappings.json` contains invalid JSON (missing bracket)
- WHEN `loadFromJson()` is called
- THEN returns `false`; `ofLogError` emitted; existing mappings remain intact

### Requirement: Complete Round-Trip

A save followed by a load across app restart MUST produce identical mappings (all fields match).

#### Scenario: Persistence round-trip
- GIVEN 3 mappings configured with various conditions, transforms, and thresholds
- WHEN saved, app restarted, and loaded
- THEN all 3 mappings restored with identical `mappingId`, `jointSource`, `targetParameterId`, `condition`, `threshold`, ranges, `transformType`, `enabled`, `smoothing`
