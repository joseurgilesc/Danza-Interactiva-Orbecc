# Proposal: Kinect Quick Map

## Intent

Reduce VEC2_XY mapping creation from 8-step/6-widget flow to 2-click visual skeleton. Store Kinect mappings per-preset so switching presets reconfigures joint-to-parameter assignments. Add per-preset morph easing curves (duration + easing function) for smooth, non-linear parameter transitions.

## Scope

### In Scope
- 4th "Quick Map" tab: interactive skeleton, click-to-assign VEC2_XY, preset selector, gold-ring indicator
- Per-preset Kinect mappings: `PresetDef` gains `kinectMappings` vector; PresetManager load/save/apply includes them
- `KinectMappingEngine`: `removeMappingsByScene()`, batch `addMappings()`, free-function JSON serialization helpers
- Configurable morph transitions per preset: `morphDuration` + `morphEasing` (LINEAR, EASE_IN, EASE_OUT, EASE_IN_OUT) stored in `PresetDef`, applied via easing function in `MorphState::update()`

### Out of Scope
- Body filtering, axis/transform/condition selection (Quick Map ≡ VEC2_XY + CONTINUOUS)
- Auto-save, right-click, cross-preset mapping merging

## Capabilities

### New Capabilities
- `kinect-preset-mapping`: Per-preset Kinect mapping lifecycle (serialize, load, apply, replace-on-switch)

### Modified Capabilities
- `kinect-panel`: Quick Map tab with preset-scoped skeleton interaction
- `preset-system`: PresetDef + kinectMappings + morphDuration/morphEasing; applyPreset reads preset's own duration when caller passes 0; update() applies easing curve to lerp
- `preset-ui`: Morph duration slider + easing dropdown in PresetPanel preset editor
- `kinect-mapping`: Engine gains scene-filtered remove + batch-add + serialization helpers

## Approach

**Quick Map tab** (~200 lines, KinectPanel): Adapt skeleton loop from Joints tab (L331–411). Hit-test joints via `InvisibleButton` or distance check. On click: `addMapping()` targeting `{sceneId}.{jointName}`. Preset selector via `presetManager->getPresets()`. Gold ring on mapped joints.

**Preset integration** (~150 lines):
1. Free functions `kinectMappingToJson/fromJson` extracted from engine's existing `saveToJson/loadFromJson` body
2. `PresetDef` + `std::vector<KinectMapping> kinectMappings`
3. `loadPresets()` parses optional `kinectMappings` array; `savePresets()` serializes it
4. `applyPreset()`: `engine->removeMappingsByScene(sceneId)` → `engine->addMappings(preset.kinectMappings)`
5. `removeMappingsByScene(id)` erases mappings where `targetParameterId` starts with `id + "."`
6. Quick Map writes to both engine (dispatch) and active PresetDef (persistence source-of-truth)

**Morph Easing** (~80 lines, PresetManager + PresetPanel):
1. `PresetDef` gains `float morphDuration = 0.0f` and `MorphEasing morphEasing = LINEAR`
2. `MorphEasing` enum: LINEAR, EASE_IN (quad in), EASE_OUT (quad out), EASE_IN_OUT (smoothstep)
3. `applyPreset()`: when `morphDuration == 0.0f`, use `preset->morphDuration` as fallback
4. `update()`: `t = applyEasing(elapsed/duration, morph.easing)` before lerp — easing stored per-MorphState at creation
5. `MorphState` gains `MorphEasing easing` field
6. JSON round-trip: `savePresets()` writes `morphDuration` + `morphEasing` string; `loadPresets()` reads them (missing → defaults)
7. PresetPanel: `ImGui::SliderFloat` for duration + `ImGui::Combo` for easing in the preset editor section

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/ui/KinectPanel.h/.cpp` | Modified | Quick Map tab + drawQuickMapTab() |
| `src/presets/PresetManager.h/.cpp` | Modified | PresetDef + kinectMappings + morphDuration/morphEasing; load/save/apply |
| `src/ui/PresetPanel.h/.cpp` | Modified | Morph duration slider + easing dropdown in editor |
| `src/mapping/KinectMappingEngine.h/.cpp` | Modified | removeMappingsByScene, addMappings, toJson/fromJson |
| `src/mapping/KinectMappingTypes.h` | Modified | Serialization helper declarations |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| No tracking data — skeleton invisible | Medium | Ghost skeleton + "Start tracking" message |
| No active preset | Low | "Activate a preset first", disable clicks |
| Orphaned mappings on preset switch | Medium | removeMappingsByScene clears old before load |
| Old preset JSON lacks kinectMappings | Low | Treat missing field as empty array |
| Old preset JSON lacks morphDuration/morphEasing | Low | Default to 0.0f / LINEAR on load (backward-compatible) |
| Double-write (engine + preset vector) | Medium | Engine for dispatch; PresetDef for save |

## Rollback Plan

Revert 4 commits: Quick Map tab removal, PresetDef fields removal (kinectMappings + morphDuration/morphEasing), PresetPanel easing UI removal, engine helper removal. Stored JSON remains backward-compatible (new fields ignored by old code).

## Dependencies

None. Existing APIs: PresetManager, KinectMappingEngine, SceneEngine, IKinectAdapter.

## Success Criteria

- [ ] Interactive skeleton with click-to-assign VEC2_XY, gold-ring mapped-joint indicator
- [ ] Mappings scoped to active preset (PresetDef::kinectMappings)
- [ ] PresetDef round-trips kinectMappings through JSON
- [ ] applyPreset replaces scene mappings with preset's mappings
- [ ] Old preset files without kinectMappings load cleanly (empty array)
- [ ] Old preset files without morphDuration/morphEasing load with defaults (0.0f/LINEAR)
- [ ] Morph easing curves (EASE_IN, EASE_OUT, EASE_IN_OUT) produce visually distinct interpolation
- [ ] PresetPanel shows morph duration slider + easing dropdown; changes persist via savePresets
- [ ] Mappings tab continues normal operation
