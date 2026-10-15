# Transport / Sync Engine Specification

## Purpose

Manages BPM, beat tracking, bar counting, and provides clock functionality for internal and external synchronization.

## Requirements

### Requirement: Internal Clock

The Transport MUST generate beats internally based on configurable BPM.

- GIVEN Transport with BPM set to 120
- WHEN running with internal clock
- THEN generate 120 beats per minute (2 beats per second)

- GIVEN Transport with BPM set to 40
- WHEN running
- THEN minimum 40 BPM is enforced

- GIVEN Transport with BPM set to 200
- WHEN running
- THEN maximum 200 BPM is enforced

### Requirement: BPM Control

The Transport SHALL allow BPM to be changed at runtime.

- GIVEN Transport playing at 120 BPM
- WHEN `setBPM(140)` is called
- THEN new BPM is 140 and beat timing adjusts immediately

### Requirement: Beat and Bar Tracking

The Transport MUST track current beat (1-4) and bar count.

- GIVEN Transport running
- WHEN beat counter reaches 4
- THEN increment bar counter
- AND reset beat to 1

- GIVEN Transport stopped
- WHEN `play()` is called
- THEN beat and bar reset to 1

### Requirement: Play/Stop Control

The Transport MUST provide play and stop functionality.

- GIVEN Transport in stopped state
- WHEN `play()` is called
- THEN internal clock starts generating beats

- GIVEN Transport in playing state
- WHEN `stop()` is called
- THEN clock stops
- AND beat/bar counters are preserved for resume

### Requirement: Beat Callback System

The Transport SHALL notify observers on each beat and bar change.

- GIVEN Transport with registered beat observer
- WHEN each beat occurs
- THEN `onBeat(beatNumber)` callback is invoked

- GIVEN Transport with registered bar observer
- WHEN each bar completes (beat 4 → 1 transition)
- THEN `onBar(barNumber)` callback is invoked

### Requirement: External Clock Reception

The Transport SHOULD accept external clock via OSC.

- GIVEN Transport receiving `/clock` OSC message
- THEN synchronize internal clock to external source

- GIVEN Transport receiving `/bpm N` OSC message
- THEN update internal BPM to N

---

## Verification Scenarios

### BPM Range Enforcement

- GIVEN Transport
- WHEN setBPM(30) called
- THEN BPM clamped to minimum 40

- GIVEN Transport
- WHEN setBPM(250) called
- THEN BPM clamped to maximum 200

### Beat Callback Timing

- GIVEN Transport at 120 BPM
- WHEN 1 second passes
- THEN onBeat called exactly 2 times

### Stop/Play Preserves Position

- GIVEN Transport at beat 3, bar 5, stopped
- WHEN play() called
- THEN resume from beat 3, bar 5