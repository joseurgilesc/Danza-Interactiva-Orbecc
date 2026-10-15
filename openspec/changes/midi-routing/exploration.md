# Exploration: MIDI Routing — Separación Cues vs Parámetros por Preset

> **Change**: `midi-routing` | **Date**: 2026-06-02 | **Executor**: sdd-explore (deepseek-v4-pro)

## Current State

### MIDI Entry Point (`ofApp.cpp:347-382`)

`ofApp` implements `ofxMidiListener`. `newMidiMessage()` is the single MIDI entry point:

1. **MIDI Learn gate**: If `midiLearnCallback_` is set (from CueEditorPanel), the message is routed there first. If consumed, normal routing is skipped.
2. **Cue routing**: All unconsumed messages go to `sceneSequencer_->findCueByMidi()` — PC, Note On, CC.
3. **NO channel filtering exists** anywhere in the current MIDI flow. All channels arrive at the same handler.

`midiLearnCallback_` is a single `std::function<bool(ofxMidiMessage&)>` slot — currently used exclusively by CueEditorPanel.

### MIDI Port Configuration (`ofApp.cpp:90-98`)

- Opens port 0 (first detected MIDI port).
- No mechanism to select specific ports or channels.
- `MidiPortsStore` exists but is metadata-only (name, device, channel, enabled). Not used for routing.
- `InputPortsPanel` shows the store's data in ImGui but has no routing logic.

### Preset System (`PresetManager`, `PresetDef`)

- `PresetDef` has: `name`, `params` (map), `kinectMappings` (vector), `morphDuration`, `morphEasing`.
- **No `midiMappings` field exists.** This is the insertion point for per-preset MIDI parameter mappings.
- `applyPreset()` does:
  1. Kinect mapping lifecycle (clear + set scene's preset mappings)
  2. Clear interaction points
  3. Morph or instant-apply params
- Adding `setActiveMidiMappings()` would follow the same pattern as `mappingEngine_->removeMappingsByScene()` + `mappingEngine_->addMappings()` (line 164-166).

### Cue MIDI Learn Pattern (`CueEditorPanel.cpp`)

The Cue MIDI Learn is the existing pattern to reuse:

1. **Registration**: Panel receives a `registerLearn_` callback (`std::function<void(std::function<bool(ofxMidiMessage&)>)>`) in constructor.
2. **Learn mode**: When user clicks "Learn", panel calls `registerLearn_(lambda)` passing a lambda that processes the next MIDI message.
3. **Learning state**: Panel tracks `learningRow_` (cue number of row being learned). Button shows "Learning..." with green color.
4. **Message processing**: Lambda reads `msg.status`, extracts type (`"pc"`/`"note"`/`"cc"`) and value (pitch/control number), updates the cue's `midiType`/`midiValue`.
5. **Cancel**: Escape key via `handleKeyPressed()` clears learn state. Panel calls `registerLearn_(nullptr)` to unregister.
6. **Consumption**: Lambda returns `true` to consume the message, preventing normal routing.
7. **Manual editing**: EditMIDITrigger popup allows explicit assignment without Learn.
8. **Port availability guard**: Learn button is disabled when `midiIn_->getNumInPorts() == 0`.

### CueDef MIDI Fields

```cpp
// From SceneSequencer.h (verified via existing MIDI cue routing)
std::string midiType;   // "pc", "note", "cc", or empty
int         midiValue;  // 0–127
```

These are **not channel-specific** — the CueEditorPanel does not filter by channel.

### Parameter System

- `IBaseScene::setParameter(paramId, float)` — sets a scene parameter.
- Parameter IDs follow `sceneId.localParamId` format (e.g., `pinturaFluorescente.k`, `particleSwarm.swarm.maxParticles`).
- `PresetManager::applyPreset()` calls `scene->setParameter(paramId, value)` for each param in the preset.

### Config Pattern

All config files live in `config/` as JSON:
- `config/sequence.json` — sequencer cues
- `config/osc.json` — OSC settings
- `config/parameters.json` — parameter persistence
- `config/osc-mappings.json` — OSC mappings
- `config/kinect-mappings.json` — Kinect mappings
- `config/presets/*.json` — per-scene presets

A new `config/midi-mappings.json` would follow this established pattern.

## Per-Scene Parameter Inventory

All scenes register parameters with ID format `sceneId.localParamId`.

### pinturaFluorescente (14 params)
| Param Key | Label | Type | Range |
|-----------|-------|------|-------|
| `k` | atraccion | FLOAT | 0.0–2.5 |
| `A` | amplitud | FLOAT | 0.0–100.0 |
| `w` | frecuencia | FLOAT | 0.1–5.0 |
| `q` | densidad | FLOAT | 0.0–0.5 |
| `beta` | friccion | FLOAT | 0.9–1.0 |
| `S` | remolino | FLOAT | 0.0–50.0 |
| `ws` | velRemolino | FLOAT | 0.0–5.0 |
| `qs` | tamRemolino | FLOAT | 0.0–0.5 |
| `flockSpeed` | velocidad | FLOAT | 0.1–3.0 |
| `flockWaveX` | ondaX | FLOAT | 1.0–30.0 |
| `flockWaveY` | ondaY | FLOAT | 1.0–30.0 |
| `flockCohesion` | cohesion | FLOAT | 0.0–0.01 |
| `flockLife` | duracion | FLOAT | 0.01–0.5 |
| `flockTransition` | transicion | FLOAT | 0.5–3.0 |
| `maxParticles` | Max Particulas | INT | 500–5000 |
| `spawnRate` | Tasa Spawn | INT | 1–20 |
| `trailEnabled` | Estelas | BOOL | 0.0–1.0 |

### furry (9 params)
| Param Key | Label | Type | Range |
|-----------|-------|------|-------|
| `density` | densidad | FLOAT | 0.3–1.0 |
| `fiberLength` | largoFibra | FLOAT | 5.0–40.0 |
| `stiffness` | rigidez | FLOAT | 0.0–1.0 |
| `windStrength` | viento | FLOAT | 0.0–2.0 |
| `windSpeed` | velViento | FLOAT | 0.1–3.0 |
| `mouseRadius` | radioMouse | FLOAT | 50.0–400.0 |
| `mouseForce` | fuerzaMouse | FLOAT | 0.0–3.0 |
| `breathAmount` | respiracion | FLOAT | 0.0–1.0 |
| `palette` | paleta | FLOAT | 0.0–4.0 |

### particleSwarm (15 params — namespaced)
| Param Key | Label | Type | Range |
|-----------|-------|------|-------|
| `swarm.maxParticles` | Particulas Max | INT | 50–800 |
| `swarm.spawnRate` | Tasa Spawn | INT | 1–10 |
| `swarm.massMin` | Masa Min | FLOAT | 0.001–0.02 |
| `swarm.massMax` | Masa Max | FLOAT | 0.005–0.06 |
| `swarm.lifeMin` | Vida Min | FLOAT | 1.0–10.0 |
| `swarm.lifeMax` | Vida Max | FLOAT | 3.0–20.0 |
| `swarm.restDistance` | Distancia Reposo | FLOAT | 50.0–800.0 |
| `swarm.damping` | Amortiguacion | FLOAT | 0.9–0.999 |
| `swarm.gravity` | Gravedad | FLOAT | 0.0–0.2 |
| `swarm.forceScale` | Escala Fuerza | FLOAT | 0.1–2.0 |
| `push.radius` | Radio Empuje Joint | FLOAT | 30.0–500.0 |
| `push.strength` | Fuerza Empuje Joint | FLOAT | 0.01–0.2 |
| `color.cycles` | Ciclos Color | INT | 1–5 |
| `visual.radiusScale` | Escala Radio | FLOAT | 200.0–2000.0 |
| `visual.radiusMin` | Radio Min | FLOAT | 1.0–10.0 |
| `visual.radiusMax` | Radio Max | FLOAT | 15.0–60.0 |

### ribbonDance (20 params — namespaced)
| Param Key | Category |
|-----------|----------|
| `ribbon.maxPoints`, `ribbon.maxWidth`, `ribbon.minWidth` | Geometry |
| `ribbon.gravity`, `ribbon.damping`, `ribbon.fadeSpeed`, `ribbon.life` | Physics |
| `ribbon.velocityInfluence`, `ribbon.emaFactor` | Velocity |
| `ribbon.waveAmount`, `ribbon.waveSpeed` | Wave |
| `ribbon.maxDistanceJump`, `ribbon.maxActiveRibbons`, `ribbon.maxJointVelocity` | Limits |
| `ribbon.colorCycles`, `ribbon.orbitRadius`, `ribbon.orbitStrength` | Color & Orbital |
| `particle.maxCount`, `particle.emissionThreshold`, `particle.perBurst` | Particles |
| `particle.velocityInfluence`, `particle.gravity`, `particle.damping` | Particle Physics |
| `particle.lifeMin`, `particle.lifeMax`, `particle.sizeMin`, `particle.sizeMax` | Particle Life |

### organicNet (18 params — namespaced)
| Param Key | Category |
|-----------|----------|
| `grid.rows`, `grid.cols`, `grid.spacing`, `grid.randomOffset`, `grid.omitChance` | Grid |
| `phys.gravity`, `phys.damping`, `phys.springStiffness`, `phys.springIterations` | Physics |
| `noise.strength`, `noise.scale` | Noise |
| `wind.strength`, `wind.frequency`, `wind.bias` | Wind |
| `lift.radius`, `lift.strength` | Lift |
| `return.force` | Return |
| `line.segments`, `line.noiseAmount`, `line.width`, `line.alpha` | Line Render |
| `node.radius`, `node.alpha` | Node Render |

### trappedBodyNet (22 params — namespaced)
| Param Key | Category |
|-----------|----------|
| `rings.count`, `rings.points`, `rings.baseRadius`, `rings.spacing` | Rings |
| `open.strength`, `open.maxExpansion`, `open.radius` | Opening |
| `elastic.return`, `elastic.damping`, `elastic.spring`, `elastic.springIter` | Elasticity |
| `noise.strength`, `noise.scale` | Noise |
| `dynamics.openSpeed`, `dynamics.closeSpeed` | Dynamics |
| `line.segments`, `line.noiseAmount`, `line.tensionAlphaBoost`, `line.width`, `line.alpha` | Line Render |
| `node.radius` | Node Render |

**Total: ~95 unique scene parameters** across 6 scenes. MIDI mapping must work with `sceneId.localParamId` format.

### Preset JSON Format (current)

```json
{
  "sceneId": "pinturaFluorescente",
  "presets": [
    {
      "name": "suspiro",
      "params": {
        "maxParticles": 300,
        "flockSpeed": 0.1
      },
      "kinectMappings": [...],
      "morphDuration": 2.0,
      "morphEasing": "ease_in_out"
    }
  ]
}
```

**No `midiMappings` field exists.** This is the insertion point.

## Current MIDI Flow: The Gap

```
┌──────────────┐     ┌─────────────────┐     ┌───────────────────┐
│ MIDI Device  │ ──▶ │ ofApp::newMidi  │ ──▶ │ midiLearnCallback │ (if set)
│ (IAC Bus 1)  │     │ Message()       │     │ (Cue Learn only)  │
└──────────────┘     └─────────────────┘     └──────┬────────────┘
                                                    │ not consumed
                                                    ▼
                                           ┌───────────────────┐
                                           │ sceneSequencer_   │
                                           │ findCueByMidi()   │
                                           │ NO CHANNEL FILTER │
                                           └───────────────────┘
```

**What's missing:**
1. **Channel filtering**: No distinction between channel 1 (cues) and channel 2 (params).
2. **MIDI parameter mapping engine**: No component exists to map MIDI messages → scene parameters.
3. **Per-preset MIDI mappings**: No storage or lifecycle for MIDI mappings in presets.
4. **MIDI Learn for parameters**: Only cue MIDI Learn exists.

**What exists and works well:**
- Cue MIDI routing (via `findCueByMidi`) including PC/Note/CC with assigned values.
- Cue MIDI Learn (via `CueEditorPanel` + `midiLearnCallback_`).
- Preset lifecycle (load, apply with morphing, per-scene storage).
- Parameter system with `IBaseScene::setParameter()` for direct value changes.
- Config JSON pattern for persistence.

## Integration Points

### Where each new component plugs in:

| Component | Integration Point | Why |
|-----------|------------------|-----|
| **MidiRouter** | `ofApp::newMidiMessage()` | Replace current monolithic routing with channel-dispatching |
| **MidiMappingEngine** | `PresetManager::applyPreset()` | Hook `setActiveMappings()` after Kinect mapping setup (same pattern) |
| **MidiMappingEngine::processMidi()** | `ofApp::newMidiMessage()` | After channel routing, maps to `IBaseScene::setParameter()` |
| **MidiMappingPanel** | `ofApp::setup()` via `uiManager.addModule()` | Same pattern as OscMappingPanel, KinectPanel |
| **MidiConfig (persistence)** | `applyPreset()` / `savePresets()` | Add `midiMappings` to PresetDef, serialize in JSON, load in `loadPresets()` |

## Recommended File Structure

```
src/
├── midi/
│   ├── MidiRouter.h          — Channel-dispatching engine (cues→ch1, params→ch2)
│   ├── MidiMappingEngine.h   — MIDI→parameter mapping with Learn mode
│   └── MidiMappingEngine.cpp
├── ui/
│   ├── MidiMappingPanel.h    — ImGui panel for mapping table + Learn
│   └── MidiMappingPanel.cpp
config/
└── midi-mappings.json        — Global MIDI config (default channels, IAC port)
```

**Why separate `midi/` directory**: The `mapping/` directory is for internal parameter-to-parameter binding (`MappingLayer`, `ParameterBinding`). MIDI routing is fundamentally external input processing — distinct concerns. The `input/` directory currently has `InputRouter` and `MidiPortsStore`; MIDI routing fits the `midi/` namespace better to group all MIDI-specific code.

### Data structures to add:

```cpp
// In MidiMappingEngine.h (or in a shared types header)
struct MidiMapping {
    std::string midiType;         // "note" or "cc"
    int         channel;          // 1-16
    int         noteOrCc;         // 0-127
    std::string targetParamId;    // e.g., "pinturaFluorescente.k"
    float       minIn;            // MIDI input range min (0)
    float       maxIn;            // MIDI input range max (127)
    float       minOut;           // mapped output min
    float       maxOut;           // mapped output max
    std::string controlMode;      // "continuous", "toggle", "trigger", "momentary"
};

// Add to PresetDef (in PresetManager.h)
struct PresetDef {
    // ... existing fields ...
    std::vector<MidiMapping> midiMappings;  // per-preset MIDI mappings
};
```

### PanelIds.h addition:

```cpp
static const char* kMidiMapping = "MIDI Mappings";
```

## Approaches to Channel Routing

### Approach 1: Hardcoded Channels (Simplest)

| Pros | Cons | Effort |
|------|------|--------|
| Immediate, no config overhead | Inflexible; changing channels requires recompile | Low |
| Matches user's stated defaults (ch1=cues, ch2=params) | No UI to reconfigure | |
| Minimal code — if/else in `newMidiMessage()` | | |

### Approach 2: Configurable Channels via `config/midi.json`

| Pros | Cons | Effort |
|------|------|--------|
| User can reconfigure at runtime | Needs config file + loading + UI | Medium |
| Follows existing `config/*.json` pattern | Adds complexity if user never changes defaults | |
| MidiMappingPanel could include channel config | | |

**Recommendation**: Approach 2 (configurable). The user explicitly mentioned "Configurable channels from UI or config file." This is a core requirement.

### Approach 3: Port-Based Routing (Multiple MIDI Ports)

Not applicable — user uses a single IAC Driver Bus for both cue and parameter data on different channels.

## The `midiLearnCallback_` Concurrency Problem

`ofApp` has a **single** `midiLearnCallback_` slot. Currently used by `CueEditorPanel`. For MIDI parameter Learn to coexist:

**Solution**: Use a **vector** of callbacks (or priority chain) so both panels can register simultaneously without conflict. The first callback that returns `true` consumes the message.

```cpp
// Instead of:
std::function<bool(ofxMidiMessage&)> midiLearnCallback_;

// Use:
std::vector<std::function<bool(ofxMidiMessage&)>> midiLearnCallbacks_;
```

Or keep the single slot but change the registration API to support multiple panels by wrapping in a dispatcher. However, the simpler vector approach works because:
- Only one Learn is active at a time per panel (mutual exclusion within each panel)
- Two panels could be in Learn mode simultaneously (rare but possible)
- PC/Note On/CC overlap: CueEditorPanel consumes PC, Note On (velocity>0), CC (value>0). MidiMappingPanel consumes Note On, CC (with channel filtering). If both want Note On, the first registered callback wins. This is acceptable because it's improbable both Learn modes are active at once.

## Risks

1. **Single `midiLearnCallback_` slot**: Needs refactoring to support multiple Learn sources (CueEditor + MidiMappingPanel). Low-risk — change to vector with iteration pattern.
2. **PresetDef field bloat**: Adding `midiMappings` to an existing struct requires updating `savePresets()` serialization. All preset JSON files on disk are backward-compatible (missing `midiMappings` → empty vector).
3. **Parameter ID resolution at MIDI time**: `MidiMappingEngine` needs access to `SceneEngine` to resolve `sceneId.localParamId` → `IBaseScene::setParameter()`. Same pattern as `oscMappingEngine_->setSceneEngine(&sceneEngine)`.
4. **Channel 1 cue routing conflict**: If a MIDI Note on channel 1 matches a cue's assigned trigger, it currently routes to the cue. After channel routing, channel 1 messages go ONLY to cues and channel 2+ goes to params. This is the intended behavior. But it means MIDI mappings should NOT use channel 1.
5. **Learn mode UX for parameters vs cues**: Cue Learn auto-detects type (PC/Note/CC). Parameter Learn also needs channel detection from the message. Both use the same MIDI entry point — channel filtering in the Learn callback distinguishes them.

## Affected Areas (Summary)

| File | Impact | Type |
|------|--------|------|
| `src/ofApp.cpp` / `ofApp.h` | Channel routing in `newMidiMessage()`, multiple Learn callbacks, instantiate new engine | MODIFY |
| `src/presets/PresetManager.h` / `.cpp` | Add `midiMappings` to `PresetDef`, serialize/deserialize, hook in `applyPreset()` | MODIFY |
| `src/ui/PanelIds.h` | Add `kMidiMapping` constant | MODIFY |
| `src/ui/CueEditorPanel.cpp` | Refactor to use callback vector instead of single slot (minor) | MODIFY |
| `config/presets/*.json` | None initially (backward compatible), later: add `midiMappings` entries | ADD |
| `src/midi/MidiRouter.h` | New channel-dispatching engine | CREATE |
| `src/midi/MidiMappingEngine.h` / `.cpp` | New mapping engine with Learn mode | CREATE |
| `src/ui/MidiMappingPanel.h` / `.cpp` | New ImGui panel for mapping table | CREATE |
| `config/midi-mappings.json` | New config: default channels, maybe port name | CREATE |

## Ready for Proposal

**Yes.** The architecture is well understood, the integration points are clear, and the existing patterns (OscMappingEngine for mapping, CueEditorPanel for Learn, PresetManager for per-preset lifecycle) provide strong templates for the new components. The main decision needed is whether to refactor `midiLearnCallback_` to a vector (recommended) or keep a single slot with priority dispatch.
