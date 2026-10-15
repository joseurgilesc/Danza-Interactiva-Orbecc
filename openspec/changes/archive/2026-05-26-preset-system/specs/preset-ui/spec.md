# preset-ui Specification

## Purpose
ImGui panel displaying loaded presets per scene with apply buttons and active morph progress visualization.

## Requirements

### Requirement: Panel Lifecycle
PresetPanel MUST be toggleable via UIManager and dockable with other panels.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Toggle on | Panel hidden | User toggles visibility via UIManager | Panel renders |
| Toggle off | Panel visible | User toggles visibility | Panel hidden |

### Requirement: Preset List Display
Panel MUST display presets grouped by scene ID. Each scene group SHALL show presets in file order. Scenes with no presets MAY be omitted or show "(none)".

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Two scenes | pinturaFluorescente has 3 presets, furry has 2 | Panel renders | Two groups: pinturaFluorescente (3), furry (2) |
| No presets loaded | ribbonDance has no file | Panel renders | ribbonDance shows "(no presets)" or group omitted |

### Requirement: Apply Button
Each preset row SHALL have an "Apply" button. A configurable duration input SHALL control morphDuration passed to `PresetManager::applyPreset()`.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Apply with duration | Duration input=2.0, "full" preset selected | Click "Apply" | `applyPreset(scene, "full", 2.0f)` called for active scene |
| Apply instant | Duration input=0.0 | Click "Apply" | `applyPreset(scene, presetName, 0.0f)` called |

### Requirement: Morph Progress Bar
Panel MUST show a progress bar (0%–100%) for each active PresetMorph with the target preset name and estimated remaining time.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Active morph | 2s morph at 1.0s elapsed for "lite" | Panel renders | Progress bar at 50%; label "lite (1.0s)" |
| No morphs | All morphs complete | Panel renders | No progress bars shown |
