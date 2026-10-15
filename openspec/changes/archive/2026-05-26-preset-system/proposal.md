# Proposal: Scene Preset System + OSC/MIDI Scene Sequencing

## Intent

Live performers need to trigger scene transitions and parameter overrides from Ableton Live via OSC/MIDI. Currently scenes have individual params but no preset mechanism, no cue-based sequencing, and two scenes have missing/unregistered parameters. This proposal adds a preset system, an OSC/MIDI trigger layer, and fixes the gaps.

## Scope

### In Scope
- **Preset engine**: Load/apply named presets (e.g., "lite", "full") per scene from JSON
- **Preset morphing/interpolation**: Smooth parameter transitions when applying presets, with configurable duration per cue
- **UI panels**: PresetManager panel (browse presets, apply, morph progress) and SceneSequencer panel (cue list, trigger, current/next display)
- **OSC preset/cue triggers**: `/preset <sceneId> <presetName>` and `/cue <number>` that apply preset + transition
- **MIDI program change triggers**: MIDI PC messages → cue numbers
- **New PinturaFluorescenteScene params**: `maxParticles`, `spawnRate`, `trailEnabled` as registered parameters
- **Bug fix**: Register `tensionAlphaBoost` in TrappedBodyNetScene (exists in struct, never `addParam`-ed)
- **Skeleton scale**: Global `skeleton.scale` parameter (0.25–3.0, default 1.0) in ParameterRegistry; `SkeletonView` reads it
- **Flexible cue sequence**: JSON file defining N cues (not hardcoded to 11); SceneSequencer adapts to array length

### Out of Scope
- Preset editor GUI (presets are JSON-edited for v1)
- Auto-advance/timeline-based sequencing (cues are external-trigger-only)
- Per-scene skeleton scale (global only for v1)
- Saving current param state as a preset from UI

## Capabilities

### New Capabilities
- `preset-system`: Named parameter presets per scene, JSON storage, apply on load or OSC trigger. Includes morphing engine that interpolates parameters over time.
- `scene-sequencer`: Cue definitions that combine preset + scene transition + morph duration. Triggered by OSC `/cue N`, MIDI PC, or UI panel.
- `preset-ui`: ImGui panel for PresetManager (list presets, apply, show morph progress)
- `sequencer-ui`: ImGui panel for SceneSequencer (cue list, current/next, trigger buttons)

### Modified Capabilities
- `skeleton-view`: New global `skeleton.scale` parameter affects joint radius and bone thickness scaling
- `osc-scene-param-mapping`: Engine must handle `/preset` and `/cue` addresses before delegating to existing mapping dispatch
- `ui-manager`: Register new PresetPanel and SequencerPanel modules

## Approach

1. **PresetManager** (new class in `src/presets/`): Loads `config/presets/<sceneId>.json`. Each file contains an array of presets with name + parameter overrides. `applyPreset(scene, presetName, morphDuration=0.0f)` calls `scene->setParameter(key, value)` for each override. When `morphDuration > 0`, starts a morph: lerps each parameter from current value to target over the duration. `update(deltaTime)` advances all active morphs each frame.

2. **PresetMorph** (internal to PresetManager): Tracks per-parameter start → target → current values with elapsed time. Supports easing (linear for v1). Multiple morphs can run concurrently (e.g., morph A at 80% when morph B starts). Reports `isComplete()`.

3. **SceneSequencer** (new class in `src/sequencer/`): Loads `config/sequence.json` with N ordered cues (`{cue, sceneId, presetName, morphDuration}`). `triggerCue(n)` calls `PresetManager::applyPreset()` with the morph duration, then `SceneEngine::transitionTo()`. Adapts to any array length — no hardcoded limit.

4. **UI Panels** (`src/ui/PresetPanel.h/.cpp`, `src/ui/SequencerPanel.h/.cpp`): ImGui panels registered in UIManager. PresetPanel lists loaded presets per scene, shows active morph progress bar. SequencerPanel shows cue table with current/next highlighting, trigger buttons per cue, and next/prev navigation.

5. **OscMappingEngine** extension: Process `/preset` and `/cue` addresses before existing mapping dispatch. `/preset sceneId presetName [duration]` → `PresetManager::applyPreset()`. `/cue N` → `SceneSequencer::triggerCue(N)`.

6. **MIDI PC routing**: `MidiPortsStore` or a thin adapter maps MIDI Program Change → cue number → `SceneSequencer::triggerCue()`.

7. **PinturaFluorescenteScene**: Move `MAX_PARTICLES` to a registered param `maxParticles` (500–5000), `spawnRate` (1–20), `trailEnabled` (bool). Particle pool resizes on param change.

8. **TrappedBodyNetScene fix**: Add `addParam("line.tensionAlphaBoost", ...)` and `readFloat("line.tensionAlphaBoost", netParams_.tensionAlphaBoost)`.

9. **SkeletonView**: Read `skeleton.scale` from `ParameterRegistry::getGlobal()`. Multiply `jointRadius_` and `boneThickness_` by scale before draw.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/presets/PresetManager.h/.cpp` | New | Preset load, store, apply, morph engine |
| `src/sequencer/SceneSequencer.h/.cpp` | New | Cue sequence execution |
| `src/ui/PresetPanel.h/.cpp` | New | ImGui panel: preset browser, morph progress |
| `src/ui/SequencerPanel.h/.cpp` | New | ImGui panel: cue list, triggers, current/next |
| `src/mapping/OscMappingEngine.h/.cpp` | Modified | Route `/preset`, `/cue` |
| `src/mapping/MidiPortsStore.h/.cpp` | Modified | MIDI PC → cue routing |
| `src/scenes/PinturaFluorescenteScene.h/.cpp` | Modified | New registered params |
| `src/scenes/TrappedBodyNetScene.h/.cpp` | Modified | Register tensionAlphaBoost |
| `src/input/sensors/SkeletonView.h/.cpp` | Modified | Read scale param |
| `src/engine/SceneEngine.h/.cpp` | Modified | Expose scene access for preset apply |
| `src/ui/UIManager.h/.cpp` | Modified | Register PresetPanel, SequencerPanel |
| `src/ofApp.cpp` | Modified | Wire PresetManager, SceneSequencer, new panels |
| `config/presets/*.json` | New | Per-scene preset files |
| `config/sequence.json` | New | Performance cue sequence (N cues)

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Particle pool resize on maxParticles change causes frame hitch | Low | Defer resize to next frame; pre-allocate with `std::vector` pattern |
| OSC `/preset`/`/cue` collision with Ableton M4L address space | Med | Use configurable prefix in `config.yaml`; default `/preset`, `/cue` |
| Preset JSON malformed → silent failure on stage | Med | Validate on load; fallback to default preset; log error to console |
| Morph interpolation on high-count params (e.g., 27 RibbonDance params) causes frame drops | Low | Batch lerp in PresetManager::update(); O(n) per morph with n≈30 is negligible |

## Rollback Plan

- PresetManager/SceneSequencer are opt-in: if JSON files missing, system falls through to existing behavior
- New PinturaFluorescente params default to current hardcoded values — no visual change
- `skeleton.scale` defaults to 1.0 — identical to current rendering
- All new code in new files or additive in existing; revert by deleting new files + reverting modified ones

## Dependencies

- `ParameterRegistry` singleton (already exists)
- `SceneEngine::transitionTo()` (already exists)
- `OscMappingEngine` callback chain (already exists)
- `ofxOsc` addon (already linked)

## Success Criteria

- [ ] `/preset pinturaFluorescente lite` reduces particle density below default
- [ ] `/preset pinturaFluorescente lite 3.0` morphs parameters over 3 seconds
- [ ] `/cue 1` transitions to PinturaFluorescenteScene with lite preset and morph applied
- [ ] MIDI PC 1 triggers cue 1 (same as `/cue 1`)
- [ ] PinturaFluorescenteScene `maxParticles=2000` halves active particle count
- [ ] TrappedBodyNetScene `tensionAlphaBoost` appears in OSC mapping panel
- [ ] `skeleton.scale=2.0` doubles skeleton joint size on screen
- [ ] PresetPanel shows loaded presets with morph progress bar
- [ ] SequencerPanel shows N cues, highlights current, triggers on click
- [ ] All N cues in sequence.json execute without crash
- [ ] Build succeeds with `make -j$(nproc)`
