# Design: preset-system

## Technical Approach

Add a PresetManager (JSON-loaded parameter presets per scene, with morphing interpolation), SceneSequencer (cue-based sequence combining preset+transition), two ImGui panels, and route `/preset`/`/cue` OSC addresses + MIDI Program Change to this layer. Zero new SceneEngine API — use existing `getScene()` and `transitionTo()`.

## Architecture Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Preset storage | `std::unordered_map<std::string, std::vector<PresetDef>>` keyed by sceneId | O(1) lookup; presets are always queried by scene |
| Morph engine placement | Internal struct `PresetMorph` inside PresetManager | Tightly coupled — morphs only exist within preset apply flow |
| Morph lerp targets | Per-param lerp: `start + (target - start) * (elapsed / duration)` | Linear for v1; add easing later without API break |
| Concurrent morph conflict | Most-recent-wins: latest `applyPreset()` cancels previous morphs for same param | Simple, predictable for live performance |
| Sequence storage | `std::vector<CueDef>` (auto-sized) | Loads from JSON array, adapts to any N |
| Morph vs transition timing | Both start simultaneously on `triggerCue()` | Morph runs in parallel with fade; params are already settling when scene appears |
| OSC routing | Check `/preset`/`/cue` prefix BEFORE mapping loop in `OscMappingEngine::processOscMessage()` | Minimal change; zero new classes |
| MIDI PC routing | Lambda in `ofApp::setup()` via `ofAddListener(ofEvents().midiMessage)` | Zero new classes; MidiPortsStore is a port registry, not a message dispatcher |
| Panel pattern | Follow `ScenePanel` pattern: constructor receives pointer, `draw()` wraps `ImGui::Begin/End` | Consistent with existing 6 panels |
| PresetManager ownership | `std::unique_ptr` member in `ofApp` | Matches `oscMappingEngine_`, `kinectMappingEngine_` pattern |

## Data Flow

```
OSC /preset sceneId name [duration]
   │
   ▼
OscMappingEngine::processOscMessage()
   │ prefix match? /preset or /cue
   ├── YES → PresetManager::applyPreset() / SceneSequencer::triggerCue()
   │          → scene->setParameter(key, val) for each override
   └── NO  → existing mapping loop (unchanged)

MIDI PC N (0-127)
   │
   ▼
ofApp lambda → if (cueRoutingEnabled) SceneSequencer::triggerCue(N+1)

PresetManager::update(dt)
   │ iterates active morphs
   ▼ scene→setParameter(key, lerpedValue) each frame
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/presets/PresetManager.h` | Create | Preset load/store/apply/morph engine |
| `src/presets/PresetManager.cpp` | Create | Implementation |
| `src/sequencer/SceneSequencer.h` | Create | Cue sequence load+trigger |
| `src/sequencer/SceneSequencer.cpp` | Create | Implementation |
| `src/ui/PresetPanel.h` | Create | ImGui IUIModule: preset list, apply, morph progress |
| `src/ui/PresetPanel.cpp` | Create | Implementation |
| `src/ui/SequencerPanel.h` | Create | ImGui IUIModule: cue table, prev/next, trigger buttons |
| `src/ui/SequencerPanel.cpp` | Create | Implementation |
| `src/ui/PanelIds.h` | Modify | Add `kPresets`, `kSequencer` constants |
| `src/ui/UIManager.cpp` | Modify | Register PresetPanel, SequencerPanel in dock layout |
| `src/mapping/OscMappingEngine.h` | Modify | Add `setPresetManager()`, `setSceneSequencer()` setters |
| `src/mapping/OscMappingEngine.cpp` | Modify | Prefix check before mapping loop in `processOscMessage()` |
| `src/scenes/PinturaFluorescenteScene.h` | Modify | Add `maxParticles_`, `spawnRate_`, `trailEnabled_` members; remove constexpr `MAX_PARTICLES`, `MAX/MIN_PARTICLES_PER_FRAME` (replaced by registered params) |
| `src/scenes/PinturaFluorescenteScene.cpp` | Modify | Register 3 new params; read+resize pool in update(); skip trail spawn when disabled |
| `src/scenes/TrappedBodyNetScene.cpp` | Modify | Add `addParam("line.tensionAlphaBoost", 0.0f, 2.0f, 0.5f)`; add read in sync loop |
| `src/input/sensors/SkeletonView.h` | Modify | Add `setScale(float)` method |
| `src/input/sensors/SkeletonView.cpp` | Modify | Read `skeleton.scale` from registry; multiply `jointRadius_`/`boneThickness_` before draw |
| `src/engine/SceneEngine.cpp` | Modify | Register global `skeleton.scale` in `registerGlobalParameters()` |
| `src/ofApp.h` | Modify | Add `presetManager_`, `sceneSequencer_`, `cueRoutingEnabled_` members |
| `src/ofApp.cpp` | Modify | Instantiate PresetManager, SceneSequencer; update in loop; OSC lambda modified; MIDI PC listener added; new panels registered |
| `config/presets/pinturaFluorescente.json` | Create | 2 presets (lite, full) |
| `config/sequence.json` | Create | N-cue performance sequence |

## Key Data Structures

```cpp
// PresetManager internal
struct PresetDef {
    std::string name;
    std::unordered_map<std::string, float> params; // paramKey → targetValue
};
struct MorphState {
    std::string paramId; float start, target; float elapsed, duration; bool active;
};
std::unordered_map<std::string, std::vector<PresetDef>> presets_;  // sceneId → presets
std::vector<MorphState> activeMorphs_;

// SceneSequencer internal
struct CueDef { int cue; std::string sceneId, presetName; float morphDuration; };
std::vector<CueDef> cues_;
int currentCueIndex_ = -1;
```

## PinturaFluorescenteScene: Particle Pool Resize

```
update():
  if (maxParticles_ != particlePool_->getCapacity())
    particlePool_->resize(maxParticles_);   // realloc + copy active
    trailPool_->resize(maxParticles_);
```

`trailEnabled_=false` skips the trail spawn block and trail draw pass entirely.

## Testing Strategy

| Layer | What | Approach |
|-------|------|----------|
| Build | Compilation | `make -j$(nproc)` — all new files compile + link |
| Unit | PresetManager JSON load | Manual: drop preset files, check `ofLog` output |
| Unit | Morph lerp math | Verify midpoint value ≈ (start+target)/2 |
| Integration | `/cue` OSC → scene switch | Send OSC from Ableton/TouchOSC, verify transition |
| Integration | MIDI PC → cue | Send PC from MIDI controller, verify cue triggers |
| Visual | Params affect visuals | Adjust `maxParticles` slider, confirm particle count drops |

## Migration / Rollout

No migration required. If `config/presets/` or `config/sequence.json` are missing, PresetManager/SceneSequencer degrade gracefully (empty, log warnings). Default param values match current hardcoded constants exactly — visual output unchanged on first launch.

## Open Questions

- [ ] Should cue navigation wrap (1 → N → 1) or clamp at boundaries? (Spec says clamp or wrap; design defaults to **clamp** for safety — no accidental cue on overshoot)
- [ ] Easing curves for v2? Linear only in v1 per spec.
