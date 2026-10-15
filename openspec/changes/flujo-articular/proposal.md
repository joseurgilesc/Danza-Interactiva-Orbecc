# Proposal: Flujo Articular Scene

## Intent

Nueva escena donde partículas **fluyen secuencialmente** a través de múltiples articulaciones activas (Kinect joints), en lugar de orbitar un único centro fijo. A medida que los joints aparecen y desaparecen, las partículas viajan de joint en joint creando un camino fluido orgánico por el cuerpo.

Problema resuelto: PinturaFluorescenteScene orbita un centro único — no aprovecha el tracking multi-joint para crear flujo anatómico.

## Scope

### In Scope
- `FlujoArticularScene` (`.h` + `.cpp`): nueva escena independiente, hereda `IBaseScene`
- Física de flujo articular: partículas viajan joint→joint con interpolación y decay
- Reuso de `InteractionPoint` (ya en `BaseScene.h`) como fuente multi-joint
- Registro en `SceneEngine` / factory `CreateFlujoArticularScene()`
- Carga en `ofApp::setup()` junto a escenas existentes

### Out of Scope
- Modificar `PinturaFluorescenteScene` o su física
- Refactorizar casts hardcodeados de `ofApp` (`dynamic_pointer_cast<PinturaFluorescenteScene>`)
- Nuevo sistema de renderizado de partículas
- Persistencia o serialización de estado de escena

## Capabilities

### New Capabilities
- `flujo-articular-scene`: escena de partículas con flujo secuencial multi-joint

### Modified Capabilities
None — no existing spec-level behavior changes.

## Approach

**Clase independiente** basada en el patrón de PinturaFluorescenteScene:
- `ParticlePool` pre-allocado (5000 partículas máxima)
- `std::map<std::string, InteractionPoint>` compartido vía `IBaseScene::getInteractionPointIds()`
- **Diferencia clave**: en `update()`, las partículas migran hacia el joint activo más cercano o siguiente en la cadena; cuando un joint desaparece, las partículas derivan hacia el próximo joint activo con interpolación suave
- Parámetros expuestos: velocidad de flujo, radio de atracción por joint, decay de inercia entre joints
- Renderizado hereda estilo visual de PinturaFluorescente (colores fluorescentes, trails)

## Dependencies

- `InteractionPoint` (existente, `src/scenes/InteractionPoint.h`)
- `IBaseScene` / `BaseScene.h` (existente)
- `ParameterRegistry` (existente)
- Ninguna dependencia en PinturaFluorescenteScene

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/scenes/FlujoArticularScene.h` | New | Declaración de clase + structs |
| `src/scenes/FlujoArticularScene.cpp` | New | Implementación (~1000 líneas estimadas) |
| `src/ofApp.cpp` | Modified | Registrar y cargar nueva escena (~5 líneas) |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Boilerplate IBaseScene duplica ~300 líneas de stubs | High | Aceptado — patrón del proyecto, diferir refactor |
| Partículas sin joint activo quedan huérfanas | Medium | Fallback a centro de pantalla con decay suave |
| Saturación del pool con muchos joints activos | Low | Limitar partículas por joint; `MAX_PARTICLES=5000` es suficiente |
| ofApp hardcoded casts no afectan nueva escena | None | Son específicos de PinturaFluorescenteScene |

## Rollback Plan

Eliminar `FlujoArticularScene.h/.cpp`, remover registro en `ofApp.cpp` y `SceneEngine`. Sin impacto en escenas existentes.

## Success Criteria

- [ ] Escena compila y se activa vía `SceneEngine`
- [ ] Partículas fluyen visualmente entre múltiples InteractionPoints activos
- [ ] Al desaparecer un joint, partículas migran al siguiente sin glitches
- [ ] Build exitoso (`make`) sin warnings nuevos
- [ ] Escena responde a `activate()`/`deactivate()` sin leaks
