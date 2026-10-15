# Delta for scene-engine-preset-access

## ADDED Requirements

### Requirement: Scene Access for PresetManager
SceneEngine MUST expose the existing `getScene(const std::string& sceneId)` method for PresetManager to access any loaded scene by ID. This access is needed for `applyPreset()` to call `setParameter()` on the target scene.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Get loaded scene | pinturaFluorescenteScene loaded | `getScene("pinturaFluorescente")` | Returns valid pointer |
| Get unloaded scene | ribbonDance not loaded | `getScene("ribbonDance")` | Returns nullptr |
| Apply preset via access | PresetManager holds SceneEngine reference | `getScene("anemone")`→apply "lite" | Preset applied to anemone |

### Requirement: No New API Added
This change SHALL NOT introduce new methods to SceneEngine. The existing `getScene()` method is sufficient.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Existing API used | PresetManager needs scene access | Implementation | Uses `getScene()` only; no new SceneEngine methods |
