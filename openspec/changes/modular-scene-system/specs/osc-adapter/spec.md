# OSC Adapter Specification

## Purpose

Receives and parses OSC messages from external controllers (faders, buttons, BPM, transport controls) and forwards them to the MappingLayer.

## Requirements

### Requirement: OSC Message Reception

The OSCAdapter MUST listen on a UDP port and receive OSC messages.

- GIVEN OSCAdapter listening on port 8000
- WHEN OSC message arrives at port 8000
- THEN message is parsed and processed

- GIVEN OSCAdapter
- WHEN malformed OSC message arrives
- THEN message is logged and discarded
- AND parsing continues for next message

### Requirement: Standard OSC Address Handling

The OSCAdapter SHALL handle standard OSC address patterns.

- GIVEN OSC message `/fader/1` with float value 0.5
- WHEN message received
- THEN forward to MappingLayer as `onInput("osc", 1, 0.5)`

- GIVEN OSC message `/button/2` with value 1.0
- WHEN message received
- THEN forward as `onInput("osc", 2, 1.0)`

- GIVEN OSC message `/bpm` with value 140
- WHEN message received
- THEN forward to Transport.setBPM(140)

- GIVEN OSC message `/play`
- WHEN message received
- THEN forward to Transport.play()

- GIVEN OSC message `/stop`
- WHEN message received
- THEN forward to Transport.stop()

### Requirement: Fader Range Normalization

The OSCAdapter MUST normalize fader values to 0.0-1.0 range.

- GIVEN fader sending values 0-127 (MIDI-style)
- WHEN value 64 received
- THEN normalize to 0.5 and forward

- GIVEN fader sending values 0-1023
- WHEN value 512 received
- THEN normalize to ~0.5 and forward

### Requirement: Configuration

The OSCAdapter SHALL allow configuration of listening port.

- GIVEN OSCAdapter initialized
- WHEN `setPort(9000)` is called
- THEN listen on port 9000

- GIVEN OSCAdapter already listening
- WHEN port change requested
- THEN close old port and open new one

---

## Verification Scenarios

### Message Parsing

- GIVEN OSC message "/fader/5 0.75"
- WHEN received
- THEN onInput("osc", 5, 0.75) called

### Malformed Message

- GIVEN invalid OSC packet
- WHEN received
- THEN no crash, error logged

### Port Reconfiguration

- GIVEN OSCAdapter on port 8000
- WHEN setPort(9000) called
- THEN port 8000 closed, 9000 opened