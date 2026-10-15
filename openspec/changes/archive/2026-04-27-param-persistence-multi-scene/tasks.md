# Tasks: Efficient Multi-Scene Parameter Persistence

## Phase 1: Cleanup - Eliminar Fragile Matching (Core Logic)

- [x] 1.1 En `src/ui/ParamPanel.cpp`, líneas 119-147: Eliminar código de matching frágil (el bloque `if (param.scope == ParameterScope::SCENE)` con lógica de `prefix.find(sceneName)`)
- [x] 1.2 En `src/ui/ParamPanel.cpp`, líneas 126-146: Reemplazar con lógica simple: parsear ID, extraer prefijo ( sceneId) antes de '.', lookup directo con `sceneEngine_->getScene(prefijo)`
- [x] 1.3 En `src/ofApp.cpp`, líneas 64-95: Eliminar lógica de matching frágil del bloque de auto-load
- [x] 1.4 En `src/ofApp.cpp`, líneas 64-95: Reemplazar con lógica simple de lookup directo (mismo patrón que 1.2)

## Phase 2: Simplificación - Save Button

- [x] 2.1 En `src/ui/ParamPanel.cpp`, líneas 63-101: Revisar que el Save button use `scene->getId()` como prefijo (ya existe en línea 91)
- [x] 2.2 Opcional: Reducir logging verboso (líneas 68, 71, 79, 82, 87, 92) - mantener solo lo esencial

## Phase 3: Testing Manual

- [ ] 3.1 Compilar con `make` y verificar no hay errores
- [ ] 3.2 ejecutar la app, presionar Save, verificar `config/parameters.json` tiene parámetros de TODAS las escenas
- [ ] 3.3 Cambiar un valor de parámetro manualmente, presionar Load, verificar se restauró el valor
- [ ] 3.4 Cerrar y reopen la app, verificar auto-load restauró los valores

## Phase 4: Edge Cases

- [ ] 4.1 Verificar que dos escenas con mismo paramId (ej: "speed") se guardan sin conflictos: "anemone.speed", "furry.speed"
- [ ] 4.2 Verificar que al cargar, "anemone.speed" NO se aplica a "furry" y viceversa
- [ ] 4.3 Verificar que si no existe parameters.json, Load button muestra warning pero no crashea