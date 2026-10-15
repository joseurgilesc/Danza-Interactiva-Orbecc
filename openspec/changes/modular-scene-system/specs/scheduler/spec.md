# Scene Scheduler Specification

## Purpose

Manages scene activation through timeline-based scheduling, external triggers, or hybrid mode combining both approaches.

## Requirements

### Requirement: Timeline Activation Mode

The SceneScheduler SHALL activate scenes at specific beats or bars based on a pre-defined schedule.

- GIVEN SceneScheduler in timeline mode with schedule `{ beat: 4, bar: 1, sceneId: "sceneA" }`
- WHEN Transport reports beat 4 of bar 1
- THEN SceneEngine.activate("sceneA") is invoked

- GIVEN SceneScheduler in timeline mode
- WHEN schedule ends (no more cues)
- THEN remain in playing state without triggering

### Requirement: Trigger Activation Mode

The SceneScheduler SHALL activate scenes immediately upon receiving external trigger events.

- GIVEN SceneScheduler in trigger mode
- WHEN `onTrigger(sceneId)` callback is invoked
- THEN SceneEngine.activate(sceneId) is called immediately

- GIVEN SceneScheduler in trigger mode
- WHEN trigger arrives while scene is transitioning
- THEN queue the trigger or ignore based on configuration

### Requirement: Hybrid Activation Mode

The SceneScheduler SHALL combine timeline cues with trigger overrides in hybrid mode.

- GIVEN SceneScheduler in hybrid mode with timeline cue at bar 4
- WHEN bar 4 is reached via timeline
- THEN activate scheduled scene

- GIVEN SceneScheduler in hybrid mode with active timeline
- WHEN external trigger arrives
- THEN override timeline and activate triggered scene immediately

### Requirement: Quantization Support

The SceneScheduler SHOULD support quantization to grid (beat, 2 beats, bar, 4 bars).

- GIVEN SceneScheduler with quantization set to "bar"
- WHEN trigger arrives at beat 3
- THEN activation occurs at next bar boundary (beat 4)

### Requirement: Schedule Management

The SceneScheduler MUST provide methods to add, remove, and clear scheduled cues.

- GIVEN SceneScheduler with empty schedule
- WHEN `addCue(beat, bar, sceneId)` is called
- THEN cue is added to internal schedule sorted by time

- GIVEN SceneScheduler with existing cues
- WHEN `removeCue(cueId)` is called
- THEN cue is removed from schedule

- GIVEN SceneScheduler
- WHEN `clearSchedule()` is called
- THEN all cues are removed

---

## Verification Scenarios

### Timeline Execution

- GIVEN schedule with cue at beat 1, bar 1
- WHEN Transport fires onBar(1)
- THEN scene activates at correct time

### Hybrid Override

- GIVEN hybrid mode with scheduled scene at bar 4
- WHEN external trigger for different scene arrives at bar 3
- THEN triggered scene activates immediately, timeline cue is skipped

### Schedule Persistence

- GIVEN schedule with multiple cues
- WHEN application restarts
- THEN schedule can be restored from PresetPersistence