# Tasks: preset-system

## Review Workload Forecast

Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: stacked-to-main
400-line budget risk: High

Estimated changed lines: ~1,600 | Delivery: ask-on-risk

### Suggested Work Units

| Unit | Goal | Base | Est. Lines |
|------|------|------|-------------|
| 1 | PresetManager + pinturaFluorescente.json | feature branch | ~400 |
| 2 | SceneSequencer + OSC/MIDI routing | Unit 1 | ~350 |
| 3 | Scene fixes (PinturaFluorescente, TrappedBodyNet, Skeleton) | Unit 2 | ~175 |
| 4 | UI Panels + PanelIds + UIManager | Unit 3 | ~525 |
| 5 | ofApp wiring + remaining preset JSONs | Unit 4 | ~230 |

## Phase 1: Foundation — PresetManager

- [x] 1.1 Create `src/presets/PresetManager.h/.cpp` — PresetDef{name, params map}, MorphState{paramId, start, target, elapsed, duration}; loadPresets(sceneId, path) with degrade on missing/malformed/empty; applyPreset(scene, name, duration) via scene->setParameter(); update(dt) lerps active morphs, most-recent-wins for same-param conflicts; getPresets(sceneId)
- [x] 1.2 Create `config/presets/pinturaFluorescente.json` — array: "lite" (maxParticles=1000, spawnRate=3, trailEnabled=false), "full" (maxParticles=5000, spawnRate=6, trailEnabled=true)

## Phase 2: Sequencer & Routing

- [x] 2.1 Create `src/sequencer/SceneSequencer.h/.cpp` — CueDef{cue, sceneId, presetName, morphDuration}; loadSequence(path) with degrade; triggerCue(n) validates range/scene/preset, applies preset then transitionTo(); current=-1 initial; reject out-of-range/unknown; create `config/sequence.json` with N-cue array
- [x] 2.2 Add setPresetManager()/setSceneSequencer() to `src/mapping/OscMappingEngine.h`; modify `.cpp` processOscMessage() — prefix-check `/preset` → applyPreset, `/cue` → triggerCue BEFORE mapping loop; matched routes skip mapping
- [x] 2.3 Add MIDI PC listener in `src/ofApp.cpp::setup()` — `ofAddListener(midiMessage)`: if cueRoutingEnabled, PC N → triggerCue(N+1); silent ignore out-of-range

## Phase 3: Scene Fixes

- [x] 3.1 Modify `src/scenes/PinturaFluorescenteScene.h/.cpp` — remove constexpr MAX_PARTICLES/MIN_PARTICLES_PER_FRAME; add members maxParticles_, spawnRate_, trailEnabled_; register params (INT 500-5000/5000, INT 1-20/6, BOOL true); update() resizes pools on change; trailEnabled=false skips spawn + draw
- [x] 3.2 Modify `src/scenes/TrappedBodyNetScene.cpp` — register tensionAlphaBoost (0.0-2.0, 0.5); read in sync loop multiply alpha under tension
- [x] 3.3 Modify `src/input/sensors/SkeletonView.h/.cpp` — add setScale(float)/skeletonScale_; read skeleton.scale from registry each frame; multiply jointRadius_×scale, boneThickness_×scale; register skeleton.scale (FLOAT, 0.25-3.0, 1.0) in `src/engine/SceneEngine.cpp` registerGlobalParameters()

## Phase 4: UI Panels

- [x] 4.1 Add kPresets, kSequencer to `src/ui/PanelIds.h`; register both panels in `src/ui/UIManager.cpp` dock layout
- [x] 4.2 Create `src/ui/PresetPanel.h/.cpp` — IUIModule; draw() renders per-scene preset groups, Apply button + morphDuration input, active morph progress bars
- [x] 4.3 Create `src/ui/SequencerPanel.h/.cpp` — IUIModule; draw() renders cue table (Cue#, Scene, Preset, Duration, Status), current highlighted, Trigger/Prev/Next buttons; clamp at boundaries; "No sequence loaded" when empty

## Phase 5: Integration Wiring

- [x] 5.1 Modify `src/ofApp.h/.cpp` — unique_ptr<PresetManager>, SceneSequencer, bool cueRoutingEnabled; instantiate in setup(); register scenes with PresetManager + load presets per scene; wire OSC setters; add MIDI listener; create+register PresetPanel/SequencerPanel; update managers each frame
- [x] 5.2 Verify full integration: GUI Apply → morph animates; OSC `/cue 1` → transitions; MIDI PC 0 → cue 1 triggers

## Phase 6: Remaining Preset Configs

- [x] 6.1 Create `config/presets/{particleSwarm,organicNet,ribbonDance,furry,trappedBodyNet}.json` — 2 presets each with scene-specific params; all load without errors per scene in PresetPanel
