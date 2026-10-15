# midi-param-mapping Specification

## Purpose
Per-preset MIDI-to-parameter mapping engine with MIDI Learn. Receives messages dispatched by MidiRouter on the configured param channel, matches against active per-preset mappings, scales values to parameter ranges, and dispatches to the active scene via `SceneEngine::setParameter()`. Managed through an ImGui panel and persisted inside `PresetDef`.

## Requirements

### Requirement: MIDI Mapping Structure
Each MidiMapping MUST contain: `type` (Note or CC), `channel` (matching paramChannel), `number` (0–127 for CC number or note number), `targetParameterId` ("sceneId.paramId"), `minInput`/`maxInput` (0–127 for CC/velocity; 0–1 for Note on/off), `minOutput`/`maxOutput` (mapped to parameter's actual range), `controlMode` (CONTINUOUS | TOGGLE | TRIGGER | MOMENTARY), and `enabled` (bool).

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| CONTINUOUS scaling | CC7: minIn=0, maxIn=127, minOut=0, maxOut=100 | CC7 value 64 arrives | Output=50.0 (50% linear interpolation) |
| TOGGLE 1st press | Note60 TOGGLE: minOut=0, maxOut=1, param currently 0 | NoteOn 60 velocity>0 | Param set to 1.0 |
| TOGGLE 2nd press | Note60 TOGGLE, param currently 1.0 | NoteOn 60 velocity>0 | Param flipped to 0.0 |
| TRIGGER press→release | Note60 TRIGGER: minOut=0, maxOut=1 | NoteOn 60 then NoteOff 60 | Param=1.0 on press, 0.0 on release |
| MOMENTARY press→release | Note60 MOMENTARY: minOut=0, maxOut=1 | NoteOn 60 then NoteOff 60 | Same as TRIGGER for notes without hold |
| Disabled mapping ignored | Mapping enabled=false | CC7 on param channel arrives | Message discarded; no dispatch |

### Requirement: Per-Preset MIDI Mappings
PresetDef SHALL store `vector<MidiMapping> midiMappings`. `applyPreset()` MUST call `MidiMappingEngine::setActiveMappings(preset.midiMappings)`, replacing the entire active set. Presets with no `midiMappings` field SHALL default to empty vector (backward-compatible; no parse errors).

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Preset with mappings | Preset "lite" has {CC7→intensity} | applyPreset("lite") | CC7 controls intensity; engine active set = 1 mapping |
| Preset replaces prior | "lite" active (CC7→intensity) | applyPreset("full", mappings={CC8→speed}) | CC7 stops responding; CC8 controls speed |
| Preset without mappings | Preset "empty" has no midiMappings field | applyPreset("empty") | Active mapping set becomes empty; no param control via MIDI |
| Old JSON backward-compat | Existing preset JSON lacks midiMappings key | PresetManager loads preset | midiMappings defaults to empty vector; no error or warning |

### Requirement: MIDI Mapping Engine
MidiMappingEngine SHALL receive messages dispatched from MidiRouter on the param channel. SHALL match each message against active mappings by type, channel, and number. Matched messages SHALL scale input via linear interpolation: `out = minOut + (in - minIn)/(maxIn - minIn) * (maxOut - minOut)`. If `minInput > maxInput`, range SHALL invert. Unmatched messages SHALL be silently ignored. Control mode SHALL govern dispatch behavior (CONTINUOUS dispatches every message; TOGGLE/TRIGGER/MOMENTARY track note state).

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Matched CC dispatch | Active mapping CC7 ch2→pintura.k [0,127]→[0,5] | CC7 val=64 ch2 arrives | setParameter("pintura.k", ≈2.5) called |
| Inverted range | Mapping minIn=127 maxIn=0→param[0,1] | CC7 val=64 | setParameter(≈0.496) called |
| Unmatched CC | Only CC7 mapping active | CC8 ch2 arrives | Silently ignored; no dispatch |
| Note tracking | Note60 TOGGLE active, param at 0 | NoteOn 60, then NoteOn 60 | Toggles: 1.0, then 0.0 |

### Requirement: MIDI Learn for Parameters
MidiMappingPanel SHALL support single-slot Learn mode. On activation, the next MIDI message on the param channel auto-fills the mapping's type, channel, and number fields. SHALL timeout after 10 seconds with no message, canceling Learn and showing "Timeout". Only one Learn slot active at a time; activating a second SHALL be rejected.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Learn captures CC | New mapping row, fields empty; Learn activated | CC7 ch2 arrives | Row auto-fills: type=CC, channel=2, number=7; Learn exits |
| Learn captures Note | New mapping row; Learn activated | NoteOn 60 ch2 | Row auto-fills: type=Note, channel=2, number=60 |
| Learn timeout | Learn active on a row | 10s elapse, no MIDI message | Learn cancels; row unchanged; UI shows "Timeout" |
| Double Learn rejected | Learn active on row 1 | User clicks Learn on row 2 | Rejected; row 1 Learn continues |
| PC ignored during param Learn | MidiMapping Learn active | PC 42 arrives | Not a param-type message; Learn ignores it (but CueEditor may consume via its own callback) |

### Requirement: MIDI Mapping Panel UI
MidiMappingPanel SHALL implement `IUIModule`, display active preset mappings in an ImGui table. Columns: Type, Channel, Number, Target Parameter, Min In/Max In, Min Out/Max Out, Mode, Enabled, Learn. SHALL support: add row, remove row, edit inline, enable/disable toggle. SHALL refresh displayed mappings when preset changes, via `PresetManager::OnPresetApplied` callback.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Display mappings | Preset "lite" has 2 mappings active | Panel draws | 2 table rows with all columns populated |
| Edit CC number inline | Row shows CC number=7 | User edits to 8 | Mapping updated in engine immediately |
| Disable mapping | Row enabled=true | User toggles Enabled column | Mapping disabled; MIDI messages for it ignored |
| Preset change refresh | Preset A active; panel shows 3 mappings | Preset B applied (1 mapping) | Panel refreshes showing 1 mapping; old rows gone |
| Add empty row | Panel open | User clicks "Add" | New row with defaults (type=CC, enabled=true); Learn button ready |
| Remove row | Row for CC7 exists in table | User clicks "Remove" | Row deleted from panel and engine active set |
