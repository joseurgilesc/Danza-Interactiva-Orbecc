# Delta for preset-system

## ADDED Requirements

### Requirement: MorphEasing Enum
The system MUST define `enum class MorphEasing { LINEAR, EASE_IN, EASE_OUT, EASE_IN_OUT }`. PresetDef SHALL include `MorphEasing morphEasing = LINEAR` and `float morphDuration = 0.0f`. Old preset JSON lacking these keys SHALL default to 0.0f / LINEAR without error.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Default preset | New PresetDef constructed | Access morphDuration/morphEasing | Returns 0.0f / LINEAR |
| Old file backward-compat | JSON lacks morphDuration, morphEasing | loadPresets() | Defaults applied; preset loads cleanly |

### Requirement: Easing-Weighted Morph Update
update(deltaTime) MUST apply easing function to progress before lerp. EASE_IN SHALL use quadratic-in (t²), EASE_OUT quadratic-out (1−(1−t)²), EASE_IN_OUT smoothstep (3t²−2t³). LINEAR SHALL pass through unchanged. MorphState SHALL store MorphEasing per instance.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| EASE_IN at t=0.5 | duration=2.0, target=1000, start=0 | 1.0s elapsed | Lerp input = 0.25 (t²); param ≈ 250 |
| EASE_OUT at t=0.5 | duration=2.0, target=1000, start=0 | 1.0s elapsed | Lerp input = 0.75 (1−(1−0.5)²); param ≈ 750 |
| LINEAR unchanged | duration=2.0, any target | Any t | Lerp input = raw progress (t) |

## MODIFIED Requirements

### Requirement: Instant Preset Apply
applyPreset(scene, presetName, morphDuration=0.0f) MUST: (1) call engine->removeMappingsByScene(sceneId) then engine->addMappings(preset.kinectMappings); (2) call scene->setParameter(key, value) for every param override. When caller passes morphDuration=0.0f, SHALL fall back to preset->morphDuration. Unknown preset names SHALL log an error and leave scene unchanged.
(Previously: applyPreset only set parameters; no mapping lifecycle or duration fallback.)

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Apply with mappings | Preset "lite" has 2 kinectMappings + 3 params | applyPreset(scene, "lite", 0) | 2 mappings set after remove-by-scene; 3 params applied instantly |
| Duration fallback | Preset has morphDuration=0.5f | applyPreset(scene, name, 0.0f) | Morph uses 0.5s from preset |
| Explicit duration wins | Preset has morphDuration=0.5f | applyPreset(scene, name, 1.0f) | Morph uses 1.0s (caller overrides) |
| Unknown preset | No preset named "heavy" | applyPreset(scene, "heavy", 0) | Error logged; scene unchanged |

### Requirement: Preset Morphing
When morphDuration > 0, a PresetMorph MUST lerp each parameter from current to target over the duration. update(deltaTime) SHALL compute eased progress via MorphState.easing before lerp. MorphState SHALL capture PresetDef.morphEasing at morph creation time.
(Previously: MorphState had no easing field; lerp used linear progress only.)

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Eased morph | morphEasing=EASE_IN_OUT, duration=2.0, target=1000, start=0 | 1.0s elapsed | Lerp input = smoothstep(0.5); param ≈ final value × eased_progress |
| Morph completion | Duration=1.0, easing=LINEAR | Final frame (≥1.0s) | All params at target; morph removed from active list |
| Zero duration | morphDuration=0 | Apply called | Instant set; no morph created |
