# Proposal: FluorescenteBloomScene

## Intent

Crear una nueva escena visual "FluorescenteBloomScene" basada en PinturaFluorescenteScene pero con unidades orgánicas de mayor tamaño (blooms/blobs) en lugar de partículas pequeñas. La nueva escena heredará toda la física de ondas y comportamiento de PinturaFluorescenteScene (F_center, F_wave, F_swirl, damping) mientras introduce renderizado orgánico con glow/halo, interacción de mouse más sofisticada, y movimiento elástico más lento.

## Scope

### In Scope
- Nueva clase FluorescenteBloomScene (.h + .cpp)
- Struct BloomUnit para representar cada entidad (posición, tamaño, color, glow, animación de respiración)
- Pool pre-allocado de 300-900 BloomUnits (vs 5000 partículas)
- Renderizado orgánico con formas suaves y halo de glow
- Movimiento elástico, "breathing" más lento
- Herencia completa de física de ondas: F_center, F_wave, F_swirl, F_damping
- Nuevos parámetros: numBloomUnits, minSize, maxSize, noiseAmount, mouseRadius, mouseForce
- Integración con IBaseScene, ParameterRegistry, scene factory

### Out of Scope
- Implementación de flock mode (heredado pero no priorizado)
- Trail particles (el bloom tiene su propio glow interno)
- Sistema de presets externo
- Documentación de usuario

## Approach

1. **Arquitectura basada en PinturaFluorescenteScene**: Reutilizar la estructura existente (ParticlePool → BloomUnitPool, Particle → BloomUnit)
2. **Nuevo BloomUnit**: Añadir campos para glow, breathingPhase, baseSize, currentSize, noiseInfluence
3. **Renderizado orgánico**: Usar ofShader o múltiples círculos superpuestos para crear efecto de halo/glow
4. **Parámetros específicos de bloom**: numBloomUnits controlando el pool size, minSize/maxSize para el rango de tamaños, noiseAmount para variación orgánica
5. **Física hereda pero ajusta**: Usar los mismos parámetros (k, A, w, q, beta, S, ws, qs) con ajustes de damping más suave para movimiento elástico

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/scenes/FluorescenteBloomScene.h` | New | Header con BloomUnit struct y clase |
| `src/scenes/FluorescenteBloomScene.cpp` | New | Implementación completa |
| `src/scenes/SceneFactory.cpp` | Modified | Registrar nueva escena |
| `src/scenes/CMakeLists.txt` o Makefile | Modified | Agregar nuevos archivos |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Performance con glow por shader | Medium | Usar fallback de múltiples círculos si shader no carga |
| Tamanos demasiado grandes | Low | minSize/maxSize con valores por defecto conservadores (20-80) |

## Rollback Plan

1. Eliminar los archivos FluorescenteBloomScene.h y .cpp
2. Revertir cambios en SceneFactory.cpp
3. Remover entradas del build system

## Dependencies

- PinturaFluorescenteScene.h/.cpp (código base, copiar estructura)
- IBaseScene interface
- ParameterRegistry
- ofGraphics, ofShader (para glow)

## Success Criteria

- [ ] Compila sin errores (`make` successful)
- [ ] Pool de 300-900 BloomUnits funcionando
- [ ] Renderizado orgánico visible (formas suaves con glow)
- [ ] Movimiento de breathing visible (oscilación de tamaño)
- [ ] Herencia de física de ondas verificada
- [ ] Parámetros registrados y modificables