# Proposal: Efficient Multi-Scene Parameter Persistence

## Intent

Reemplazar la lógica frágil de matching por namespacing determinista para guardar/cargar parámetros de múltiples escenas sin conflictos. El usuario necesita guardar TODOS los parámetros de TODAS las escenas con un solo botón, cargarlos manualmente, y autoreload al iniciar.

## Scope

### In Scope
- **Save**: Guardar TODOS los parámetros de TODAS las escenas a `config/parameters.json` con IDs scoped (`"sceneName.paramId"`) sin lógica de matching
- **Load button**: Botón manual en ParamPanel que carga parámetros sin matching frágil
- **Auto-load**: Cargar automáticamente en startup (setup() en ofApp.cpp)
- **Namespacing determinista**: Usar `getId()` del scene como prefijo exacto

### Out of Scope
- Cambiar la estructura de ParameterRegistry (ya existe)
- Agregar features de undo/redo
- Persistencia de settings de UI (solo parámetros)

## Approach

1. **Save**: Iterar loadedScenes, para cada scene obtener sus parámetros con `getParameterIds()`, construir IDs como `{scene->getId()}.{localParamId}`, guardar al JSON
2. **Load**: En lugar de matching frágil, el JSON contiene un mapping explícito `{"scene": "sceneId", "params": {...}}` por scene
3. **Auto-load**: Mismo flujo de load en setup() de ofApp.cpp

El cambio clave es eliminar la lógica de "prefix contains sceneName" (líneas 126-146 en ParamPanel.cpp, 64-95 en ofApp.cpp) y usar lookup directo por nombre de scene.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/ui/ParamPanel.cpp` | Modified | Save/Load buttons (lines 63-153), eliminar matching frágil |
| `src/ofApp.cpp` | Modified | Auto-load en setup() (lines 44-99), eliminar matching frágil |
| `src/registry/ParameterRegistry.h` | Verified | No cambios necesarios |
| `src/engine/SceneEngine.h` | Verified | getLoadedScenes(), getScene() ya existen |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Scene IDs cambian entre sesiones | Low | Antes de save, loguear los sceneIds; si cambio, el usuario debe re-saved |
| Archivo corrupto | Low | Verificar existencia de archivo antes de load, loguear error claro |
| Performance con many scenes | Low | Save es O(n_params), load es O(n_params); ambos son lineales |

## Rollback Plan

1. Git: `git checkout src/ui/ParamPanel.cpp src/ofApp.cpp` revierte los cambios
2. El archivo parameters.json mantiene formato compatible (IDs con ".")
3. No hay cambios a nivel de binario o config.make

## Dependencies

- ParameterRegistry::saveToJson() y loadFromJson() ya existen y funcionan
- SceneEngine::getLoadedScenes() y getScene() ya existen

## Success Criteria

- [ ] Botón Save guarda parámetros de todas las scenes sin conflictos de nombres
- [ ] Botón Load carga parámetros correctamente sin lógica de matching
- [ ] Auto-load en startup funciona sin matching frágil
- [ ] Build exitoso con `make`