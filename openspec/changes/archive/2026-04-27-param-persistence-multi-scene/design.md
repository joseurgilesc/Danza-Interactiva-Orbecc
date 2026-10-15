# Design: Efficient Multi-Scene Parameter Persistence

## Technical Approach

Reemplazar la lógica frágil de substring matching con namespacing determinista. El flujo será:

1. **Save**: Iterar por cada sceneID en `loadedScenes`, para cada parámetro crear ID como `"{scene->getId()}.{localParamId}"`, guardar al registry y luego a JSON
2. **Load**: El JSON contiene IDs con formato `"sceneId.paramId"`. En lugar de hacer matching, hacemos lookup simple: parsear el ID, buscar la scene por el prefijo (sceneId), actualizar parámetro con el sufijo (localParamId)

No se necesita lógica de matching — los IDs ya tienen toda la información necesaria.

## Architecture Decisions

### Decision: Format de IDs en archivo JSON

**Choice**: Usar formato `"sceneId.paramValue"` (ej: `"pinturaFluorescente.k"`)
**Alternatives considered**: 
- Usar estructura anidada `{"scenes": [{"id": "...", "params": {...}}]` — más complejo de parsear
- Usar IDs planos con prefijos `id_1`, `id_2` — pierde traceability
**Rationale**: El formato actual ya usa este patrón (línea 91 en ParamPanel.cpp: `fullParamId = sceneId + "." + localId`). Mantiene compatibilidad hacia atrás y es eficiente (O(1) lookup).

### Decision: Cómo sincronizar parámetros en load

**Choice**: Parsear cada ID en el JSON, extraer prefijo como sceneId, luego lookup directo con `sceneEngine.getScene(sceneId)`
**Alternatives considered**:
- Mantener la lógica de substring matching — frágil, propensa a errores (ej: "3-Anemone" matches wrong scene)
- Crear un índice inverso map<sceneId, vector<paramId>> — overhead innecesario
**Rationale**: Con namespacing determinista, extracción de prefijo es simple y certera. Si `sceneId = "pinturaFluorescente"` y el ID guardado es `"pinturaFluorescente.k"`, el prefijo es exactamente el sceneId.

### Decision: Dónde executar auto-load

**Choice**: En `ofApp::setup()` después de cargar las escenas y antes de crear los UI panels
**Alternatives considered**:
- En ParamPanel constructor — no tiene acceso a sceneEngine todavía
- En SceneEngine después de loadScenes — acoplamiento excesivo
**Rationale**: El setup actual ya hace auto-load (líneas 44-45) después de carga de scenes. Mantener el mismo flujo.

## Data Flow

### Save Flow

```
Usuario click "Save" → ParamPanel::draw() 
    → sceneEngine_->getLoadedScenes() 
    → Para cada scene:
        → scene->getParameterIds()
        → Para cada paramId:
            → registry_->setValue(sceneId + "." + paramId, value)
    → registry_->saveToJson("config/parameters.json")
```

### Load Flow

```
ofApp::setup() / Click "Load"
    → registry_->loadFromJson("config/parameters.json")
    → registry_->getAllParams()
    → Para cada param con scope SCENE:
        → Extraer prefijo antes del '.': sceneId = "pinturaFluorescente"
        → sceneEngine_->getScene(sceneId)
        → Si existe: scene->setParameter(localParamId, value)
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/ui/ParamPanel.cpp` | Modify | Líneas 63-153: Save button (eliminar log verbose), Load button (eliminar matching frágil líneas 119-147) |
| `src/ofApp.cpp` | Modify | Líneas 44-99: Auto-load (eliminar matching frágil líneas 64-95) |

## Interfaces / Contracts

No se agregan nuevas interfaces. Se modifica el flujo existente:

### Save: Sin cambios en API
- `registry_->setValue(fullParamId, value)` — ya existe
- `registry_->saveToJson(filepath)` — ya existe

### Load: Sin cambios en API  
- `registry_->loadFromJson(filepath)` — ya existe
- `sceneEngine_->getScene(sceneId)` — ya existe
- `scene->setParameter(paramId, value)` — ya existe

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Manual | Save graba todas las scenes | Verificar parameters.json tiene entradas de todas las scenes |
| Manual | Load restaura valores correctos | Cambiar valor de un param, guardar, cerrar programa, abrir - valor debe restaurarse |
| Manual | Sin conflictos | Dos escenas con mismo paramId (ej: "speed") se guardan y cargan correctamente |

## Migration / Rollout

No migration requerida. El cambio es backward-compatible:
- El archivo JSON usa el mismo formato (IDs con ".")
- Si el usuario tiene un parameters.json viejo, seguirá funcionando
- La lógica nueva es más estricta, no más permisiva

## Open Questions

- [ ] ¿Hay escenarios donde `scene->getId()` cambia entre sesiones? (Si sí, habría que guardar un mapping de scene IDs). Por ahora asumimos que los scene IDs son estables.