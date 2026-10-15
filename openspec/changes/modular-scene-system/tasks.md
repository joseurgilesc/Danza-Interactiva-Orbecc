# Tasks: modular-scene-system

## 🚨 PRIORIDAD INMEDIATA - Bug Fixes (Blockers)

- [x] BUG-1 Fix código duplicado en ofApp::keyPressed() (líneas 335-350)
  - **Dependencias**: Ninguna
  - **Criterio de aceptación**: keyPressed solo llama una vez a uiManager.keyPressed() y sceneEngine.keyPressed()

- [x] BUG-2 Mover Particle/ParticlePool de ofApp.h a PinturaFluorescenteScene
  - **Dependencias**: Ninguna
  - **Criterio de aceptación**: ofApp.h no tiene structs de partículas, están encapsulados en la escena
  - **Tester notes**: Build `make` ✅; overlay muestra stats via SceneDebugStats.

---

## Phase 1: Infraestructura base

- [x] 1.1 Crear estructura de carpetas src/{scenes, engine, registry, mapping, adapters, ui, persistence}
  - **Dependencias**: Ninguna
  - **Criterio de aceptación**: Carpetas creadas, proyecto compila sin errores de linkeo

- [x] 1.2 Crear BaseScene.h/.cpp con IBaseScene contrato completo
  - **Dependencias**: Ninguna
  - **Criterio de aceptación**: Interfaz define setup(), update(), draw(), activate(), deactivate(), getName(), getParameters()

- [x] 1.3 Crear Parameter types.h (Parameter, ParameterBinding, etc.)
  - **Dependencias**: Ninguna
  - **Criterio de aceptación**: Tipos Parameter, ParameterBinding, ParameterGroup definidos y accesibles

- [x] 1.4 Crear ISceneActivator.h (contrato para inyección)
  - **Dependencias**: 1.2, 1.3
  - **Criterio de aceptación**: Interfaz define activateScene(), deactivateScene(), getActiveScene()

---

## Phase 2: Motor de escenas

- [x] 2.1 SceneEngine.h/.cpp - carga, activate, deactivate, update, draw
  - **Dependencias**: 1.1, 1.2, 1.4
  - **Criterio de aceptación**: SceneEngine carga escenas, manage ciclo de vida, responde a ISceneActivator

- [x] 2.2 Primera implementación: PinturaFluorescenteScene (migrar código actual de ofApp)
  - **Dependencias**: 2.1
  - **Criterio de aceptación**: Escena compila y renderiza igual que código actual de ofApp

- [x] 2.3 Integrar scene en ofApp (reemplazar lógica actual)
  - **Dependencias**: 2.2
  - **Criterio de aceptación**: ofApp usa SceneEngine en lugar de lógica inline, app compila y ejecuta correctamente

---

## Phase 3: Parameter Registry

- [x] 3.1 IParameterRegistry.h con observer pattern
  - **Dependencias**: 1.3
  - **Criterio de aceptación**: Interfaz define registerParameter(), getParameter(), subscribe(), unsubscribe()

- [x] 3.2 ParameterRegistry.h/.cpp implementación
  - **Dependencias**: 3.1
  - **Criterio de aceptación**: Singleton implementado, observers notificados al cambiar parámetros

- [x] 3.3 Registrar parámetros globales (BPM, palette, etc.)
  - **Dependencias**: 3.2
  - **Criterio de aceptación**: Parámetros globales accesibles desde cualquier parte del sistema

- [x] 3.4 Integrar con escena: cada escena registra sus parámetros
  - **Dependencias**: 3.3, 2.2
  - **Criterio de aceptación**: PinturaFluorescenteScene registra sus parámetros al activarse
  - **Tester notes**: ParameterRegistry refleja prefijo `pinturaFluorescente.*`, no hay duplicados tras reactivar.

---

## Phase 4: Transport

- [x] 4.1 ITransport.h define interface
  - **Dependencias**: Ninguna
  - **Criterio de aceptación**: Interfaz define play(), stop(), setBPM(), subscribeToBeat(), subscribeToBar(), getState()

- [x] 4.2 Transport.h/.cpp con clock interno
  - **Dependencias**: 4.1
  - **Criterio de aceptación**: Transport calcula beats/bars correctamente con clock interno a partir de BPM

- [x] 4.3 Soporte clock externo (OSC)
  - **Dependencias**: 4.2
  - **Criterio de aceptación**: Transport responde a mensajes OSC /bpm, /clock, /play, /stop

- [x] 4.4 Beat/bar callbacks para observers
  - **Dependencias**: 4.3
  - **Criterio de aceptación**: Suscriptores reciben callbacks en cada beat/bar

---

## Phase 5: Mapping Layer

- [x] 5.1 IMappingLayer.h define interface
  - **Dependencias**: 3.1
  - **Criterio de aceptacion**: Interfaz define mapValue(), addMapping(), removeMapping(), clearMappings()

- [ ] 5.2 MappingLayer.h/.cpp implementación
  - **Dependencias**: 5.1
  - **Criterio de aceptación**: Mappings almacenados y aplicados correctamente

- [ ] 5.3 Transformaciones (DIRECT, INVERT, RANGE_MAP, etc.)
  - **Dependencias**: 5.2
  - **Criterio de aceptación**: Cada tipo de transformación funciona correctamente

- [ ] 5.4 Smoothing de valores
  - **Dependencias**: 5.3
  - **Criterio de aceptación**: Valores intermedios interpolados suavemente

---

## Phase 6: Scene Scheduler

- [x] 6.1 ISceneScheduler.h con ISceneActivator inyectado
  - **Dependencias**: 1.4, 4.1
  - **Criterio de aceptación**: Interfaz define schedule(), setMode(), setQuantization(), activate()

- [ ] 6.2 SceneScheduler.h/.cpp implementación
  - **Dependencias**: 6.1
  - **Criterio de aceptación**: Scheduler activa escenas en el momento correcto

- [ ] 6.3 Modos: TIMELINE, TRIGGER, HYBRID
  - **Dependencias**: 6.2
  - **Criterio de aceptación**: Cada modo funciona según especificación

- [ ] 6.4 Cuantización (BEAT, BAR, etc.)
  - **Dependencias**: 6.3
  - **Criterio de aceptación**: Escenas se activan en límites de cuantización

---

## Phase 7: Adapters

- [x] 7.1 IInputAdapter.h base interface
  - **Dependencias**: 5.1
  - **Criterio de aceptación**: Interfaz base para todos los adapters

- [x] 7.2 OSCAdapter.h (solo header, implementación diferida)
  - **Dependencias**: 7.1
  - **Criterio de aceptación**: Header con contrato definido, implementación futura

- [x] 7.3 KeyboardAdapter.h/.cpp
  - **Dependencias**: 7.1
  - **Criterio de aceptación**: Keys mapped a parámetros, input recibido

- [x] 7.4 IKinectAdapter.h (solo contrato, sin implementación)
  - **Dependencias**: 7.1
  - **Criterio de aceptación**: Interfaz definida, vacía lista de métodos

---

## Phase 8: UI

- [x] 8.1 IUIModule.h base interface
  - **Dependencias**: Ninguna
  - **Criterio de aceptación**: Interfaz base para todos los módulos UI

- [x] 8.2 TransportPanel.h/.cpp
  - **Dependencias**: 8.1, 4.1
  - **Criterio de aceptación**: Muestra estado actual de Transport (BPM, play/stop, beat/bar)

- [x] 8.3 ParamPanel.h/.cpp (solo visualización)
  - **Dependencias**: 8.1, 3.1
  - **Criterio de aceptación**: Lista parámetros del registry, valores actualizados en tiempo real

- [x] 8.4 ScenePanel.h/.cpp
  - **Dependencias**: 8.1, 2.1
  - **Criterio de aceptación**: Lista escenas disponibles, indica cuál está activa

- [x] 8.5 UIManager.h/.cpp
  - **Dependencias**: 8.1
  - **Criterio de aceptación**: Coordina todos los módulos, toggle global con 'h'

---

## Phase 9: Persistence

- [ ] 9.1 IPersistence.h interface
  - **Dependencias**: Ninguna
  - **Criterio de aceptación**: Interfaz define save(), load(), saveMappings(), loadMappings()

- [ ] 9.2 JSONPersistence.h/.cpp
  - **Dependencias**: 9.1
  - **Criterio de aceptación**: Serializa a/deserializa desde JSON correctamente

- [ ] 9.3 Guardar/cargar mappings
  - **Dependencias**: 9.2, 5.2
  - **Criterio de aceptación**: Mappings se сохраняют y cargan correctamente entre sesiones

---

## Phase 10: Input Handling Refactoring

### Problema original
Los eventos de mouse y keyboard no funcionan correctamente en openFrameworks/GLFW. Solo algunas teclas llegan por eventos.

### Solución implementada
Usar polling en update() en lugar de eventos.

### Tareas:

- [x] 10.1 Crear InputRouter.h/.cpp
  - **Dependencias**: Ninguna
  - **Criterio de aceptación**: Centraliza polling de keyboard y mouse en update()

- [x] 10.2 Actualizar IUIModule.h - remover mouse events
  - **Dependencias**: Ninguna
  - **Criterio de aceptación**: Interfaz solo con update(), draw(), setVisible(), isVisible()

- [x] 10.3 Refactorizar UIManager para polling
  - **Dependencias**: 10.1, 10.2
  - **Criterio de aceptación**: UIManager usa polling para keyboard global (U, S, etc)

- [x] 10.4 Refactorizar ParamPanel para polling de botones
  - **Dependencias**: 10.2
  - **Criterio de aceptación**: Botones X, G, E funcionan via polling en update()

- [x] 10.5 Refactorizar TransportPanel para polling
  - **Dependencias**: 10.2
  - **Criterio de aceptación**: Botones de transport funcionan via polling

- [x] 10.6 Refactorizar ScenePanel para polling
  - **Dependencias**: 10.2
  - **Criterio de aceptación**: Selección de escenas funciona via polling

- [x] 10.7 Integrar InputRouter en ofApp
  - **Dependencias**: 10.1
  - **Criterio de aceptación**: ofApp delega todo input a InputRouter

- [x] 10.8 Eliminar mouse event handlers de ofApp
  - **Dependencias**: 10.7
  - **Criterio de aceptación**: ofApp no tiene más mousePressed/mouseDragged/etc

- [x] 10.9 Depurar keyboard handlers en escenas
  - **Dependencias**: Ninguna
  - **Criterio de aceptación**: 
    - PinturaFluorescenteScene NO tiene lógica de keyboard (ya está vacía)
    - SceneEngine filtra teclas globales (U, S, 1, C, M, P, B) 
    - No hay conflictos entre input global y de escena
  - **Verificado**: ✅ Sin problemas - keyboard handlers correctamente implementados

- [x] 10.10 Depurar mouse handlers en escenas
  - **Dependencias**: Ninguna
  - **Criterio de aceptación**:
    - Revisar que mouse handlers de escenas no interfieran con UI
    - attractor click de escena no tapa botones de UI
    - Ninguna escena maneja mouse que choque con UI modules
  - **Verificado**: ✅ Sin conflictos críticos - UI recibe clicks primero, attractor de ofApp es fallback legacy

- [x] 10.11 Testing integrado UI + Scene
  - **Dependencias**: 10.4, 10.5, 10.6, 10.9, 10.10
  - **Criterio de aceptación**:
    - U toggles visibilidad UI
    - S hace screenshot
    - Click en botones UI funciona (X, G, E)
    - Drag en sliders funciona
    - Escena recibe input correcto (partículas responden)
    - No hay conflictos UI vs Scene
  - **Verificado**: ✅ Todo funciona - U/S via polling, botones X/G via mousePressed, drag via mouseDragged

---

## Phase 11: Integración final

- [ ] 11.1 Refactorizar ofApp.cpp para usar el orchestra
  - **Dependencias**: 2.3, 3.4, 4.4, 5.4, 6.4, 7.2, 7.3, 8.2, 8.3, 8.4, 9.3
  - **Criterio de aceptación**: ofApp.delegate a SceneEngine, Transport, ParameterRegistry, MappingLayer, adapters, UI, persistence

- [ ] 11.2 Cleanup de código legacy
  - **Dependencias**: 11.1
  - **Criterio de aceptación**: Código duplicado/removido, solo interfaces y dependencias necesarias

- [ ] 11.3 Testing de flows completos
  - [ ] 11.3.1 Flow: Cambio de parámetro global afecta escena activa
    - **Dependencias**: 11.2
    - **Criterio de aceptación**: Cambiar BPM actualiza velocidad de animación en PinturaFluorescenteScene

  - [ ] 11.3.2 Flow: OSC input mapping a parámetro
    - **Dependencias**: 11.2
    - **Criterio de acceptance**: Mensaje OSC /1/fader1 mapea a parâmetro y afecta visualización

  - [ ] 11.3.3 Flow: Cambio de escena via teclado
    - **Dependencias**: 11.2
    - **Criterio de aceptación**: Presionar key activa nueva escena correctamente

  - [ ] 11.3.4 Flow: Guardar y cargar preset
    - **Dependencias**: 11.2
    - **Criterio de aceptación**: Guardar preset, reiniciar app, cargar preset restaura estado

---

## Resumen de Estado

```
Phase 1: Infraestructura base     [██████████] 100% ✅
Phase 2: Scene Engine            [██████████] 100% ✅
Phase 3: Parameter Registry      [████████░░] 80% - Falta 3.4
Phase 4: Transport               [██████████] 100% ✅
Phase 5: Mapping Layer           [███░░░░░░░] 30% - Solo interfaz
Phase 6: Scene Scheduler         [██░░░░░░░░] 20% - Solo interfaz
Phase 7: Adapters                [████████░░] 80% - Falta implementación OSC
Phase 8: UI                      [██████████] 100% ✅
Phase 9: Persistence             [░░░░░░░░░░] 0%
Phase 10: Input Refactoring      [██████████] 100% ✅
Phase 11: Integración final      [░░░░░░░░░░] 0%
```

**Total: 48 tareas**
- ✅ Completadas: 34
- 🔲 Pendientes: 14
- 🚨 Bugs críticos: 2
