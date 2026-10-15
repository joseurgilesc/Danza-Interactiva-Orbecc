## Exploration: Kinect Quick Map — Visual Skeleton Click-to-Assign

### Current State

#### The 8-Step Mapping Creation Flow (Current)

To create a VEC2_XY mapping today, the user must:

1. **Navigate to the Mappings tab** in KinectPanel
2. **Click "+ Add"** — initializes editor in "new" mode with defaults (head.x, LINEAR transform, CONTINUOUS condition)
3. **Open Joint Type dropdown** — select from 18 joint types (e.g., "Elbow R" → ELBOW_RIGHT). This auto-fills `editJointSourceBuf_` as `"jointName.axis"`
4. **See irrelevant Axis dropdown** — X/Y/Z/Conf selector is always visible, but for VEC2_XY it's meaningless (the dispatch uses both X and Y raw). This creates confusion: "which axis do I pick for position?"
5. **Discover and change Transform dropdown** — buried as the 5th/last option ("Vec2 XY", index 4), users must know VEC2_XY exists before they can find it
6. **Open Target Parameter combo** — a single long dropdown mixing: FLOAT parameters, existing Interaction Points, and Quick Names (the last is only visible AFTER selecting VEC2_XY transform)
7. **Select target name** from Quick Names or Interaction Points section — e.g., `"pinturaFluorescente.wrist_right"`
8. **Click "Add Mapping"** — final commit

**Total: 8 mouse clicks across 6 distinct UI widgets, with conceptual overload.**

#### What Already Exists (And Is Good)

- **Visual skeleton** in the Joints tab (lines 331–411 of KinectPanel.cpp) renders 18 joints connected by 17 bone pairs, color-coded by confidence. Joint circles are drawn but NOT interactive.
- **JoinType <-> string helpers**: `jointTypeToName()` and `jointNameToType()` in `KinectMappingTypes.h` / `KinectMappingEngine.cpp` provide clean bidirectional conversion.
- **Quick Names** feature (lines 739–762 in the target dropdown) pre-populates 18 common joint names — exactly the right idea, but buried inside a sub-combo of the editor.
- **Duplicate detection**: `KinectMappingEngine::addMapping()` rejects duplicate `(jointSource, targetParameterId)` pairs automatically.
- **Auto-creation pattern**: ALL scenes (PinturaFluorescente, TrappedBodyNet, Furry, RibbonDance, OrganicNet, ParticleSwarm) follow the SAME `setParameter(vec2)` pattern: `interactionPoints_[paramId]` is auto-created on first access, `setTarget()` converts normalized→pixel coords. No scene needs modification.

#### VEC2_XY Dispatch Flow (processJointData lines 241–271)

```
processJointData(bodyId, jointType, joint)
  → jointTypeToName(jointType) → "wrist_right"
  → for each mapping where parseJointName(mapping.jointSource) == "wrist_right"
      → if transformType == VEC2_XY:
          → parse targetParameterId: "pinturaFluorescente.wrist_right" → sceneId + localParamId
          → scene->setParameter("wrist_right", vec2(x, y))
              → interactionPoints_["wrist_right"].setTarget(normalizedPos, 1.0, screenW, screenH)
                  → position = normalizedPos * screenSize (pixel coords)
```

Key insight: the `jointSource` field (e.g., `"elbow_right.x"`) is only used for matching via `parseJointName()` — the axis suffix (`.x`/`.y`/`.z`) is irrelevant for VEC2_XY because both raw `x` and `y` from `joint.position` are sent directly.

### Affected Areas

- `src/ui/KinectPanel.h` — Add `drawQuickMapTab()` declaration, skeleton interaction state variables, scene selector state
- `src/ui/KinectPanel.cpp` — Add 4th tab item + `drawQuickMapTab()` implementation (~200 lines of new UI code)
- `src/ui/KinectPanel.cpp` — `draw()` method: add 4th `BeginTabItem("Quick Map")` block after the existing 3 tabs
- No changes needed to `KinectMappingEngine`, `KinectMappingTypes`, `InteractionPoint`, `BaseScene`, or any scene files — the existing API is fully sufficient

### Approaches

1. **Quick Map Tab with Visual Skeleton (RECOMMENDED)** — 4th tab in KinectPanel with a clickable skeleton preview. Clicking any joint auto-creates a VEC2_XY mapping targeting `{activeSceneId}.{jointName}`. Color-coded dots show already-mapped joints.
   - Pros: 2-click flow (tab → click joint), visual/spatial feedback, reuses existing skeleton rendering code, no concept overload (no axis/transform/condition choices), self-documenting UX
   - Cons: Requires tracking data to render skeleton (needs guard for no-tracking state), only works for the active scene
   - Effort: **Low** (~200 lines of additive UI code, no architecture changes)

2. **Inline "Quick Assign" Button in Joints Tab** — Add a small "+" or "Map" button next to each row in the Joints tab table. Clicking it creates the VEC2_XY mapping.
   - Pros: Exposes the feature in an existing tab users already visit, simpler than a full new tab
   - Cons: No visual/spatial feedback, the joint table shows raw numbers (not intuitive for mapping), doesn't show already-mapped status, 3 clicks (Joints tab → find row → click button)
   - Effort: **Low** (~60 lines)

3. **Context Menu on Joints Tab Skeleton** — Make the existing Joints tab skeleton interactive with right-click → "Map this joint".
   - Pros: Uses existing skeleton in Joints tab, no new tab needed
   - Cons: Hidden UX (right-click is not discoverable), mixes inspection (Joints tab) with configuration (mapping), no mapped-status indicator
   - Effort: **Low-Medium** (~100 lines)

### Recommendation

**Approach 1: Quick Map Tab with Visual Skeleton.** It delivers the biggest UX improvement for the smallest code change. The visual skeleton already exists in the Joints tab — we just copy/adapt the rendering loop, add hit-testing on joint circles using ImGui's `InvisibleButton` or manual distance check, and wire a single `addMapping()` call per click. No architecture changes needed. No scene needs modification. The entire feature is ~200 lines of additive UI code in 2 files.

The Joints tab skeleton code (lines 331–411) provides 95% of the rendering we need — bone connections, joint circles, confidence-based coloring. The main additions are:
- Hit-testing on joint circles (~20 lines)
- Mapping creation on click (~15 lines)
- Scene selector dropdown (~10 lines)
- Mapped-joint status highlight (gold ring or checkmark on already-mapped joints) (~30 lines)
- No-tracking / no-scene guard messages (~15 lines)

### Risks

- **No tracking data**: If `adapter_->getAllTrackedBodies()` is empty, the skeleton has no positions to render. Mitigation: show a clear message "Start tracking to see the skeleton" and disable/ghost the skeleton.
- **No active scene**: If `sceneEngine_->getActiveScene()` returns nullptr, there's no sceneId for the target string. Mitigation: show "No active scene — activate a scene first" and disable clicks.
- **Duplicate clicks**: `addMapping()` already rejects duplicates — no risk. Should show a brief toast/flash on the joint to confirm "already mapped."
- **Multiple bodies**: VEC2_XY dispatch currently does NOT filter by bodyId — all tracked bodies dispatch to the same mapping. This is existing behavior and not changed by this feature. Future enhancement could add body filtering.
- **Persistence**: Created mappings are only in memory. The user must still click "Save" in the Mappings toolbar to persist to `config/kinect-mappings.json`. Mitigation: auto-save on each Quick Map creation, or show a noticeable "⚡ Unsaved" indicator.

### Ready for Proposal

**Yes.** The exploration confirms technical feasibility with minimal risk. The design is clear: 4th tab, visual skeleton, click-to-assign, no architecture changes. Ready for `sdd-propose` to formalize scope and approach.
