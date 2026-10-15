# Delta for osc-scene-param-mapping

## ADDED Requirements

### Requirement: Preset and Cue Address Routing
OscMappingEngine MUST process `/preset` and `/cue` addresses BEFORE existing mapping dispatch. These addresses SHALL NOT match stored OscMapping entries.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| /preset route | Scene loaded, preset "lite" exists | `/preset pinturaFluorescente lite` | `PresetManager::applyPreset("lite")` called; mapping dispatch skipped |
| /preset with duration | Scene loaded | `/preset pinturaFluorescente full 3.0` | `applyPreset("full", 3.0f)` called |
| /cue route | Sequencer loaded | `/cue 5` | `SceneSequencer::triggerCue(5)` called |

### Requirement: Configurable Prefix
The address prefix SHALL be configurable (default: `/preset`, `/cue`). When customized, the original default addresses SHALL be ignored.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Default prefix | Config uses defaults | `/cue 2` arrives | Routed to SceneSequencer |
| Custom prefix | Config sets prefix to `/show/cue` | `/show/cue 2` arrives | Routed; `/cue 2` ignored |

### Requirement: Error Handling for Preset/Cue Addresses
Invalid scene, preset name, or cue number in OSC messages SHALL log errors without crashing.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Unknown scene | No "ghost" scene loaded | `/preset ghost lite` | Error logged; no crash |
| Unknown preset | pinturaFluorescente loaded | `/preset pinturaFluorescente badPreset` | Error logged; no crash |
| Bad cue number | 5 cues loaded | `/cue 99` | Error logged; no crash |

## MODIFIED Requirements

### Requirement: OSC Message Processing
(Previously: "Engine SHALL match mappings and dispatch scaled values to active scene via SceneEngine::setParameter().")

OSCAdapter MUST invoke callback for every message AFTER built-in routing. Engine SHALL first check `/preset` and `/cue` addresses — if matched, route to PresetManager/SceneSequencer and skip mapping dispatch. Otherwise, proceed with existing mapping match and scaled dispatch to active scene via `SceneEngine::setParameter()`.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Existing dispatch | `pintura` active; `/XPos`→`pintura.k` mapped | `/XPos` 0.42 | Engine dispatches scaled value to `setParameter("k", scaled)` |
| /preset bypasses mapping | `/preset furry full` arrives | Engine processes | PresetManager invoked; mapping matcher NOT invoked |
| /cue bypasses mapping | `/cue 3` arrives | Engine processes | SceneSequencer invoked; mapping matcher NOT invoked |
| Inactive scene | `anemone` NOT active | `/Y`→`anemone.wave` mapped | Silently skipped (unchanged) |
| Fader coexistence | `osc.json` prefix `rig`; `/rig/fader/2` mapped | `/rig/fader/2` 0.8 | Fader handler AND engine dispatch both run (unchanged) |
