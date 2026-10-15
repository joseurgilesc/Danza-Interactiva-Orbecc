# Delta for midi-cue-routing

## ADDED Requirements

### Requirement: MIDI Channel Configuration
The system MUST load channel and port config from `config/midi.json`. Config SHALL contain `cueChannel` (default: 1), `paramChannel` (default: 2), `portName` (default: "IAC Driver Bus 1"). Channels MUST be 1–16; cue and param channels MUST NOT be equal. Missing or invalid file SHALL use defaults and log a warning. Runtime changes SHALL take effect immediately.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Default config | No midi.json exists | App starts | cueChannel=1, paramChannel=2, portName="IAC Driver Bus 1" |
| Same channel rejected | midi.json sets cue=3, param=3 | Config loaded | Error logged; fallback to defaults (cue=1, param=2) |
| Invalid channel (0) | midi.json: cueChannel=0 | Config loaded | Error logged; fallback to defaults |
| Invalid channel (17) | midi.json: paramChannel=17 | Config loaded | Error logged; fallback to defaults |
| Custom port | midi.json: portName="IAC Driver Bus 2" | App opens MIDI | Port "IAC Driver Bus 2" opened |
| Runtime change | App running with cue=1, param=2 | User edits midi.json, triggers reload | New channels take effect immediately |

### Requirement: Channel-Based MIDI Routing
MidiRouter SHALL dispatch incoming MIDI messages by channel. Messages on `cueChannel` (default 1) MUST route to cue processing (existing findCueByMidi). Messages on `paramChannel` (default 2) MUST route to MidiMappingEngine. All other channels SHALL be silently ignored. Channel filtering SHALL happen BEFORE any cue lookup or parameter mapping.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| CC on cue channel | CC 7 on ch1; cueChannel=1 | MidiRouter processes | Routed to findCueByMidi |
| Note On velocity>0 on cue channel | NoteOn 60 vel=100 ch1 | MidiRouter processes | Routed to findCueByMidi |
| PC on cue channel | PC 0 on ch1 | MidiRouter processes | Routed to findCueByMidi; triggerCue(1) |
| CC on param channel | CC 7 on ch2; paramChannel=2 | MidiRouter processes | Routed to MidiMappingEngine |
| Note On on param channel | NoteOn 60 vel=80 ch2 | MidiRouter processes | Routed to MidiMappingEngine |
| Message on unconfigured channel | NoteOn 60 on ch3; cue=1 param=2 | MidiRouter processes | Silently ignored |

## MODIFIED Requirements

### Requirement: MIDI Learn Gate
`ofApp::newMidiMessage()` MUST check `midiLearnCallbacks_` vector (not single callback) before normal channel routing. Callbacks SHALL be tried in registration order. First callback returning `true` consumes the message entirely; normal routing SHALL be skipped. If all return `false` or vector is empty, normal channel routing proceeds.
(Previously: Single `function<bool(ofxMidiMessage&)>` callback. Now: `vector<function<bool(ofxMidiMessage&)>>` with first-consumer-wins iteration.)

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Both panels Learn active | CueEditor callback registered first, MidiMappingPanel second | PC 5 arrives | CueEditor callback runs first; if it returns true, MidiMapping callback skipped entirely |
| Only param Learn active | MidiMappingPanel Learn callback registered; CueEditor none | CC 7 on param channel arrives | Param callback receives it; returns true |
| No Learn active | Vector empty or all callbacks return false | Any MIDI message | Normal channel routing proceeds to MidiRouter dispatch |
| Cue callback returns false | CueEditor Learn active, MidiMapping Learn also active | CC message (not PC) arrives | Cue callback returns false (only consumes PC); MidiMapping callback runs next |

### Requirement: MIDI Learn Callback Registration
`ofApp` MUST expose `addMidiLearnCallback(fn)` and `removeMidiLearnCallback(fn)`. CueEditorPanel SHALL call `add(...)` on Learn button click and `remove(...)` on capture or cancel. MidiMappingPanel SHALL do the same independently. Multiple callbacks MAY coexist; registration order determines priority.
(Previously: `setLearnCallback(function)` with single-slot overlay. Now: vector with `push_back`/`remove` supporting multi-panel coexistence.)

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| CueEditor registers | No callbacks active | User clicks "Learn" on cue | CueEditor's callback pushed to vector |
| CueEditor unregisters on capture | CueEditor callback active in vector | PC 42 captured | CueEditor calls remove; callback removed from vector |
| MidiMapping registers alongside CueEditor | CueEditor learn already active | User clicks "Learn" on MidiMapping row | MidiMapping callback pushed (second in vector); two callbacks coexist |
| Both unregister independently | Both callbacks registered | CueEditor captures/cancels | Only CueEditor callback removed; MidiMapping callback still active |
