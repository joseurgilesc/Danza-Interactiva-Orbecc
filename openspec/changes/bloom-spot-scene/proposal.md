# Proposal: BloomSpotScene - Limited Area Bloom Variation

## Intent

Crear una nueva escena basada en FluorescenteBloomScene pero con bloom concentrado en un area circular limitada en lugar de pantalla completa. El fondo fuera del circulo sera negro, creando un efecto de "spot" u "orb" de bloom.

## Scope

### In Scope
- Nueva clase `BloomSpotScene` heredando de `FluorescenteBloomScene`
- Parametros configurables: `spotCenterX`, `spotCenterY`, `spotRadius`
- Spawning limitado al circulo definido por spot center + radius
- Registro en ofApp.cpp como "5-BloomSpot"

### Out of Scope
- Nuevos efectos visuales adicionales (mantener misma apariencia que parent)
- Modificaciones al sistema de fisica (reutilizar completamente)

## Approach

- Copiar FluorescenteBloomScene como base
- Agregar parametros de spot center/radius (rango 0.0-1.0 para center, 0.05-0.5 para radius)
- En spawn logic: generar posiciones solo dentro del circulo (usar coordenadas polares con radio aleatorio < radius)
- En campo de fuerza: usar spotCenter como centro en lugar de screenCenter
- En draw: mantener fondo negro (ya comportamiento actual)

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/scenes/BloomSpotScene.h` | New | Header con clase y parametros |
| `src/scenes/BloomSpotScene.cpp` | New | Implementacion |
| `src/ofApp.cpp` | Modified | Registro de escena "5-BloomSpot" |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Duplicacion de codigo con parent | Medium | Comentar clara herencia, reutilizar BloomUnit/BloomUnitPool tal cual |

## Rollback Plan

- Eliminar archivos `src/scenes/BloomSpotScene.*`
- Remover registro en ofApp.cpp
- No requiere migration de datos

## Dependencies

- Ninguna dependencia externa
- Requiere FluorescenteBloomScene existente

## Success Criteria

- [ ] Escena compila sin errores
- [ ] Registro en ofApp como "5-BloomSpot" funciona
- [ ] Spawning limitado al area circular
- [ ] Parametros spotCenterX/Y/R modificables en runtime