# Apply Progress: preset-system — Units 1-6 (COMPLETE)

**Date**: 2026-05-26
**Delivery strategy**: stacked-to-main (chained PRs)
**Mode**: Standard (strict_tdd: false)

## Completed Tasks

### Phase 1: Foundation — PresetManager

- [x] **1.1** Create `src/presets/PresetManager.h/.cpp`
- [x] **1.2** Create `config/presets/pinturaFluorescente.json`

### Phase 2: Sequencer & Routing

- [x] **2.1** Create `src/sequencer/SceneSequencer.h/.cpp`
- [x] **2.2** Create `config/sequence.json`
- [x] **2.3** OSC routing in OscMappingEngine
- [x] **2.4** Wire PresetManager + SceneSequencer into ofApp

### Phase 3: Scene Fixes

- [x] **3.1** PinturaFluorescenteScene — Dynamic ParticlePool capacity
- [x] **3.2** TrappedBodyNetScene — tensionAlphaBoost param
- [x] **3.3** SkeletonView — skeletonScale_ member, reads from registry

### Phase 4: UI Panels

- [x] **4.1** PanelIds + UIManager dock registration for kPresets/kSequencer
- [x] **4.2** PresetPanel — IUIModule with scene selector, preset list, Apply, morph progress
- [x] **4.3** SequencerPanel — IUIModule with cue table, prev/next, trigger

### Phase 5: Integration Wiring

- [x] **5.1** ofApp wiring — includes, loadPresets for all 6 scenes, PresetPanel + SequencerPanel registration
- [x] **5.2** Build verification — `make -j6` succeeds with zero errors

### Phase 6: Remaining Preset Configs

- [x] **6.1** Created particleSwarm.json, organicNet.json, ribbonDance.json, furry.json, trappedBodyNet.json

## Files Changed (Unit 5-6)

| File | Action | Description |
|------|--------|-------------|
| `src/ofApp.cpp` | Modified | Added PresetPanel.h/SequencerPanel.h includes; loadPresets for 5 remaining scenes; PresetPanel + SequencerPanel registration via UIManager |
| `config/presets/particleSwarm.json` | Created | lite/full presets: swarm, visual, push params |
| `config/presets/organicNet.json` | Created | lite/full presets: grid, line, node, phys, noise, wind |
| `config/presets/ribbonDance.json` | Created | lite/full presets: ribbon, particle params |
| `config/presets/furry.json` | Created | lite/full presets: density, fiber, stiffness, wind, breath |
| `config/presets/trappedBodyNet.json` | Created | lite/full presets: rings, line, node, noise, elastic, open |

## Verification

- ✅ `make -j6` succeeds — zero new errors
- ✅ All 6 preset JSON files valid (python3 json.tool)
- ✅ PresetPanel and SequencerPanel compile and link into binary

## Deviations

- ParticlePool (Unit 3): changed from static constexpr CAPACITY to dynamic capacity_ with resize()

## ALL TASKS COMPLETE — preset-system is ready for verify phase.
