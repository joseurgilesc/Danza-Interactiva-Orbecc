# OSC Adapter Specification

## Purpose

This spec defines the behavior of the OSCAdapter module, which receives OSC messages from external controllers (e.g., lighting rigs) and maps them to the Scene Engine's MappingLayer and Transport layers.

## Requirements

### Requirement: OSC Receiver Lifecycle

The system MUST initialize, start, stop, and restart the OSC receiver without memory leaks or port conflicts.

#### Scenario: Successful startup

- GIVEN `osc.json` has `enabled: true` and `port: 9000`
- WHEN `OSCAdapter::startListening()` is called
- THEN the receiver binds to port 9000 and returns `ConfigStatus::OK`

#### Scenario: Port already in use

- GIVEN port 9000 is occupied by another process
- WHEN `OSCAdapter::startListening()` is called
- THEN the adapter logs an error, returns `ConfigStatus::ERROR_PORT_IN_USE`, and does NOT crash

#### Scenario: Hot reload port change

- GIVEN the adapter is listening on port 9000
- WHEN `osc.json` changes to `"port": 9001` and `refreshIfChanged()` is called
- THEN the adapter restarts on port 9001 and logs "OSC port changed to 9001"

### Requirement: Message Parsing

The system MUST strip the configured address prefix and map OSC addresses to channel indices.

#### Scenario: Fader message with prefix

- GIVEN `prefix: "rig"` and incoming message `/rig/fader/3`
- WHEN the message is processed
- THEN the channel index is 2 (0-based) and value is normalized to 0.0-1.0

#### Scenario: Toggle message

- GIVEN incoming message `/rig/toggle/5`
- WHEN the message is processed
- THEN the channel index is 4 and value is boolean (0 or 1)

#### Scenario: Malformed address

- GIVEN incoming message `/invalid/no/channel`
- WHEN the message is processed
- THEN a warning is logged and the message is skipped

### Requirement: Transport Integration

The system MUST route transport OSC addresses to the Transport layer.

#### Scenario: BPM update

- GIVEN incoming message `/bpm` with float argument 120.0
- WHEN the message is processed
- THEN Transport::setBpm(120.0) is called once

#### Scenario: Play command

- GIVEN incoming message `/play`
- WHEN the message is processed
- THEN Transport::play() is called once

#### Scenario: Stop command

- GIVEN incoming message `/stop`
- WHEN the message is processed
- THEN Transport::stop() is called once

### Requirement: Frame Budget Throttling

The system MUST limit the number of messages processed per frame to prevent UI freezing.

#### Scenario: High message volume

- GIVEN 500 OSC messages arrive in a single frame and `maxMessagesPerFrame: 64`
- WHEN `update()` is called
- THEN only 64 messages are processed and a backlog warning is logged

### Requirement: Configuration Persistence

The system MUST load settings from `osc.json` on startup and persist changes back to the file.

#### Scenario: Missing config file

- GIVEN `osc.json` does not exist
- WHEN `OSCSettingsManager` initializes
- THEN a default config is created at `bin/data/config/osc.json`

#### Scenario: Runtime override persists

- GIVEN the operator changes `enabled` to false via UI
- WHEN the application exits
- THEN the change is saved to `osc.json`