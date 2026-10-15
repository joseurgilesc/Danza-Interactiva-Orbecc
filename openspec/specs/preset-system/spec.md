# preset-system Specification

## Purpose
Named parameter presets per scene loaded from JSON. PresetManager applies presets instantly or with morphing interpolation over time.

## Requirements

### Requirement: Preset Loading from JSON
The system MUST load `config/presets/<sceneId>.json` per registered scene. Each file SHALL contain an array of presets with `name` and `params` (key-value overrides). Missing or malformed files SHALL degrade gracefully.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Load valid file | `pinturaFluorescente.json` with 2 presets | Scene registers with PresetManager | Both presets loaded and queryable |
| Missing file | No `ribbonDance.json` | Scene registers | Warning logged; empty preset list for that scene |
| Malformed JSON | File has syntax error | PresetManager loads file | Error logged; empty preset list |
| Empty array | File contains `[]` | PresetManager loads file | Info logged; empty preset list |

### Requirement: Instant Preset Apply
`applyPreset(scene, presetName, morphDuration=0.0f)` MUST call `scene->setParameter(key, value)` for every override in the preset. Unknown preset names SHALL log an error and leave scene unchanged.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Apply known preset | "lite" overrides `maxParticles=1000, trailEnabled=false` | `applyPreset(scene, "lite", 0)` | Both params set instantly on scene |
| Unknown preset | No preset named "heavy" | `applyPreset(scene, "heavy", 0)` | Error logged; params unchanged |
| Scene has no presets | Scene registered but no file loaded | `applyPreset(scene, "any", 0)` | Warning logged; scene unchanged |

### Requirement: Preset Morphing
When `morphDuration > 0`, a PresetMorph MUST lerp each parameter from current to target over the duration. `update(deltaTime)` SHALL advance all active morphs each frame.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Morph at midpoint | `maxParticles=5000`, target=1000, duration=2.0 | 1.0s elapsed | `maxParticles` ≈ 3000 (± tolerance) |
| Morph completion | Duration=1.0 | Final frame (≥1.0s elapsed) | All params at target; morph removed from active list |
| Zero duration | Duration=0 | Apply called | Instant set; no morph created |

### Requirement: Concurrent Morphs
Multiple morphs MAY run concurrently. If two active morphs target the same parameter, the most recently started morph SHALL take priority.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Same-param conflict | Morph A (50% through) targets `speed=10`; Morph B starts targeting `speed=20` | Next frame | Param follows B's trajectory from current value |
| Different params | Morph A targets `speed`, Morph B targets `color` | Both active concurrently | Both advance independently; no interference |

### Requirement: Fallback to Scene Defaults
When no preset is applied and no morph is active, scene SHALL use registered defaults. PresetManager SHALL NOT modify scene parameters unless explicitly invoked.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| No preset loaded | Scene has no preset file | Scene runs | All params at registered defaults |
| Last preset sticks | "lite" applied once with duration=0 | Scene continues | Params remain at "lite" values until next explicit apply |
