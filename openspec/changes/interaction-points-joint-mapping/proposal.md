# Proposal: Multi-Point Joint-to-Scene Interaction Mapping

## Intent

Scenes currently read `ofGetMouseX()`/`ofGetMouseY()` directly — mouse-only, hardcoded. The `interactionPoint_` field in Furry/Añemona exists but is **dead code** (set by mouse handlers, never read in `update()`). Kinect joints deliver real-time body positions, but KinectMappingEngine only dispatches scalar floats. Scenes have no way to receive joint-driven interaction points. This change bridges that gap: multiple named interaction points per scene, driven by Kinect joints with graceful fade-out on tracking loss, plus a global toggle to disable mouse entirely when joints are active.

## Scope

### In Scope
- `InteractionPoint` struct (position, velocity, confidence, decayRate, active, sourceName)
- `map<string, InteractionPoint>` per scene replacing single `interactionPoint_`
- VEC2 dispatch in KinectMappingEngine (`KinectTransformType::VEC2_XY`)
- Implement `setParameter(glm::vec2)` in Furry, Añemona, PinturaFluorescente
- Scenes use interaction points in their `update()` loops (not raw mouse API)
- Linear-decay fade-out when joint disappears (configurable `decayRate` per point)
- Global `useMouseInput` toggle in KinectPanel, routed through SceneEngine
- Fix missing `mouseMoved` override in ofApp.h (events silently dropped today)

### Out of Scope
- Face mesh landmarks (MediaPipe Face Mesh)
- OSC/MIDI-mapped interaction points
- Per-point visual customization (colors, shapes)
- Inter-point interaction (repulsion/attraction between points)

## Capabilities

### Modified Capabilities
- `kinect-mapping`: Add VEC2_XY transform type; dispatch `glm::vec2` to scenes alongside existing float dispatch
- `ofapp-integration`: Add `mouseMoved` override; wire `useMouseInput` toggle into scene input routing
- `kinect-panel`: Add `useMouseInput` checkbox in status section

### New Capabilities
- `interaction-points`: Scene-side multi-point map with decay behavior — each point has position, velocity, confidence, decay rate, and an active flag

## Approach

**Option B (Multi-Point)**. Each scene stores `map<string, InteractionPoint>` keyed by logical names (e.g., `"hand_left"`, `"hand_right"`). KinectMappingEngine gains `VEC2_XY` that dispatches raw joint `Vector3D` as `glm::vec2(x, y)` to the scene's `setParameter(vec2)`. On receipt, scenes populate velocity via `(newPos - lastPos) / deltaTime` and set confidence from joint data. When confidence drops to zero (tracking lost), the point enters linear decay: alpha reduces each frame until `active=false`. The `update()` loop blends currently active points into effect calculations. The `useMouseInput` toggle controls whether `mouseMoved`/`mouseDragged` handlers populate a synthetic `"mouse"` point — when OFF, only joint-mapped points drive interaction.

| File | Impact | Description |
|------|--------|-------------|
| `src/scenes/BaseScene.h` | New struct | `InteractionPoint` struct |
| `src/scenes/FurryScene.h/.cpp` | Modified | `map<string, InteractionPoint>`, vec2 setParameter, update() uses it |
| `src/scenes/AnemoneFieldScene.h/.cpp` | Modified | Same as Furry |
| `src/scenes/PinturaFluorescenteScene.h/.cpp` | Modified | vec2 setParameter for multi-point attractor |
| `src/mapping/KinectMappingTypes.h` | Modified | `VEC2_XY` enum value |
| `src/mapping/KinectMappingEngine.h/.cpp` | Modified | VEC2 dispatch path |
| `src/ofApp.h/.cpp` | Modified | `mouseMoved` override, `useMouseInput` wiring |
| `src/ui/KinectPanel.h/.cpp` | Modified | `useMouseInput` checkbox |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Perf degradation with many points | Low | Cap at 8 points; pre-allocated map |
| Decay behavior feels unnatural | Med | Start linear; make rate configurable per-point |
| mouseMoved fix changes UI interaction | Low | InputRouter already checks `wantsMouseCapture()` |
| VEC2_XY dispatch conflicts with float mappings on same joint | Low | Allow both; different target params |
| KinectPanel already complex | Med | Add toggle as minimal checkbox in status row |

## Rollback Plan

1. Revert `ofApp.h` mouseMoved addition (if only that changed)
2. If VEC2_XY is live: delete mappings using it; scenes fall back to float-only
3. Scene-side: remove `map<InteractionPoint>`, restore single `interactionPoint_` and raw mouse API calls
4. Each change is independent: mouseMoved fix, VEC2 dispatch, and scene multi-point can be reverted separately

## Dependencies

- `kinect-types` spec (Joint, Vector3D, confidence) — already implemented
- `kinect-mapping` spec (mapping engine dispatch) — already implemented
- `ofapp-integration` spec (app wiring) — already implemented

## Success Criteria

- [ ] Furry/Añemona scenes respond visually to joint positions received via VEC2_XY mappings
- [ ] When `useMouseInput` is OFF, `ofGetMouseX()`/`ofGetMouseY()` are never called in scene update()
- [ ] When tracking is lost, interaction points fade out over time (not vanish instantly)
- [ ] `mouseMoved` events reach scenes through InputRouter → SceneEngine
- [ ] Existing float-based Kinect mappings continue working unchanged
- [ ] KinectPanel shows useMouseInput toggle that persists across scene changes
