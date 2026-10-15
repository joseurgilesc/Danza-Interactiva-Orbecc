# Archive Report — preset-system

**Archived**: 2026-05-26
**Status**: COMPLETE — all verification passed
**Delivery strategy**: stacked-to-main

## Change Summary

The preset-system change adds a complete preset + sequencing layer for live performance:
- **PresetManager** (`src/presets/`) — JSON-loaded parameter presets per scene with morphing interpolation
- **SceneSequencer** (`src/sequencer/`) — ordered cue sequence combining preset + scene transition
- **PresetPanel + SequencerPanel** (`src/ui/`) — ImGui panels for preset browsing and cue control
- **OSC `/preset` and `/cue` routing** — integrated into OscMappingEngine prefix dispatch
- **MIDI Program Change → cue routing** — via ofxMidi listener in ofApp
- **3 new params** in PinturaFluorescenteScene (maxParticles, spawnRate, trailEnabled)
- **tensionAlphaBoost** registered in TrappedBodyNetScene (was missing)
- **skeleton.scale** global parameter in SkeletonView + SceneEngine
- **6 preset JSON configs + 1 sequence JSON** (11 cues)

## Specs Synced

| Domain | Action | Details |
|--------|--------|---------|
| `preset-system` | Created | 5 requirements (loading, apply, morphing, concurrent morphs, fallback) |
| `scene-sequencer` | Created | 4 requirements (loading, triggering, tracking, error handling) |
| `preset-ui` | Created | 4 requirements (lifecycle, display, apply, progress) |
| `sequencer-ui` | Created | 4 requirements (lifecycle, table, trigger, navigation) |
| `midi-cue-routing` | Created | 3 requirements (mapping, toggle, error handling) |
| `pintura-fluorescente-params` | Created | 4 requirements (maxParticles, spawnRate, trailEnabled, backward compat) |
| `trapped-body-net-fix` | Created | 2 requirements (registration, readback) |
| `scene-engine-preset-access` | Created | 2 requirements (access, no new API) |
| `osc-scene-param-mapping` | Updated | 3 added (R-10/11/12: routing, prefix, error handling) + 1 modified (R-3: message processing with preset/cue dispatch) |
| `skeleton-view` | Updated | 2 added (global scale param, ParamPanel visibility) + 1 modified (joint rendering with scale multiplication) |

## Archive Contents

```
openspec/changes/archive/2026-05-26-preset-system/
├── proposal.md         ✅
├── design.md           ✅
├── tasks.md            ✅ (14/14 complete)
├── apply-progress.md   ✅ (Units 1-6)
├── verify-report.md    ✅ (PASS)
├── archive-report.md   ✅ (this file)
└── specs/
    ├── preset-system/spec.md
    ├── scene-sequencer/spec.md
    ├── preset-ui/spec.md
    ├── sequencer-ui/spec.md
    ├── midi-cue-routing/spec.md
    ├── pintura-fluorescente-params/spec.md
    ├── trapped-body-net-fix/spec.md
    ├── scene-engine-preset-access/spec.md
    ├── osc-scene-param-mapping/spec.md
    └── skeleton-scale/spec.md
```

## Engram Artifact Observation IDs

| Artifact | Observation ID |
|----------|---------------|
| Proposal | #439 |
| Specs | #440 |
| Design | #441 |
| Tasks | #442 |
| Apply Progress | #443 |
| Verify Report | #448 |
| Archive Report | (this save) |

## Verification Summary

- **Build**: PASS — `make -j6` compiled + linked zero errors
- **Files**: 18/18 present
- **Params**: all 5 registrations confirmed (maxParticles, spawnRate, trailEnabled, tensionAlphaBoost, skeleton.scale)
- **Integration**: 17/17 wiring points verified
- **JSON**: 7/7 valid (6 presets + sequence.json)
- **Spec Compliance**: 17/17 requirements compliant across 10 domains
- **Design Coherence**: 8/8 architecture decisions followed
- **Deviation**: 1 low-impact (ParticlePool resize uses raw arrays vs vector-based — functionally correct)

## Source of Truth Updated

Main specs now reflect all preset-system behavior:
- `openspec/specs/preset-system/spec.md`
- `openspec/specs/scene-sequencer/spec.md`
- `openspec/specs/preset-ui/spec.md`
- `openspec/specs/sequencer-ui/spec.md`
- `openspec/specs/midi-cue-routing/spec.md`
- `openspec/specs/pintura-fluorescente-params/spec.md`
- `openspec/specs/trapped-body-net-fix/spec.md`
- `openspec/specs/scene-engine-preset-access/spec.md`
- `openspec/specs/osc-scene-param-mapping/spec.md` (merged: R-3 modified + R-10/11/12 added)
- `openspec/specs/skeleton-view/spec.md` (merged: Joint Rendering modified + 2 added)

## SDD Cycle Complete

The preset-system change has been fully planned, implemented, verified, and archived. Ready for the next change.
