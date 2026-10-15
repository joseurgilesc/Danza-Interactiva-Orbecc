# Proposal: modular-scene-system

## Intent

Diseñar e implementar una arquitectura modular desacoplada para el sistema de ejecución de escenas de danza. El objetivo es transformar la aplicación monolítica en un sistema donde las escenas son componentes plug-and-play, controlados por un motor central que gestiona sincronización, parámetros, mappings y persistencia, sin conocimiento directo de las entradas externas (OSC, Kinect, teclado).

## Scope

### In Scope (10 Dominios)

| # | Dominio | Descripción |
|---|---------|-------------|
| 1 | **Scene Engine** | Carga, activación, pausa, finalización de escenas. Ciclo de vida completo. |
| 2 | **Scene Scheduler / Cue Engine** | Activación por tiempo, por trigger externo, o híbrido. |
| 3 | **Transport / Sync Engine** | Gestión de BPM, beat, compás, clock interno y externo. |
| 4 | **Parameter Registry** | Registro central de parámetros globales y de escena. Punto único de acceso. |
| 5 | **Mapping Layer** | Mapeo de controles externos a parámetros del registry. |
| 6 | **OSC Adapter** | Recepción de mensajes OSC (faders, botones, BPM, transport). |
| 7 | **Keyboard Trigger Adapter** | Conversión de teclas del teclado a eventos de activación de escenas/parámetros. |
| 8 | **Kinect Adapter Contract** | Interfaz diferida para futura integración con Kinect. |
| 9 | **Control UI** | Visualización de estado del sistema, mappings activos, valores de parámetros. |
| 10 | **State / Preset Persistence** | Guardado y carga de mappings, presets y configuración completa. |

### Out of Scope

- Implementación completa de Kinect Adapter (solo contrato/contrato diferido)
- Sistema de transiciones visuales entre escenas (solo activación lógica)
- Persistencia de estado runtime de cada escena (solo configuración/mappings)

## Reglas Arquitectónicas

1. **Ninguna escena depende de OSC, Ableton, Kinect o teclado directamente** — Las escenas solo interactúan con el Parameter Registry.
2. **Toda entrada externa mediante adaptadores desacoplados** — Cada fuente de entrada es un módulo independiente.
3. **Registro central de parámetros** — Punto único de acceso para leer/modificar cualquier parámetro.
4. **Funcionamiento sin hardware externo** — Debe operar con clock interno y eventos simulados.
5. **Cada escena autocontenida con interfaz común** — Contrato BaseScene define el ciclo de vida.
6. **Mapeo no hardcodeado dentro de escenas** — Todo mapeo vive en el Mapping Layer.
7. **Soporte multicanal de activación** — Timeline, trigger, híbrido.
8. **Kinect reusable el mismo Mapping Layer** — Usa la misma abstracción que OSC/teclado.
9. **Favorecer extensibilidad, claridad, testabilidad, trazabilidad** — Principios rectores.
10. **Evitar acoplamientos innecesarios** — Modularidad real, no falsa.

## Approach

### Fase 1: Fundamentos (Semanas 1-2)
- Implementar `BaseScene` con interfaz de ciclo de vida (setup, update, draw, activate, deactivate, pause, resume)
- Crear `ParameterRegistry` como singleton/central registry
- Definir tipos de datos base (`Parameter`, `ParameterType`, `SceneHandle`)

### Fase 2: Motor de Escenas (Semanas 2-3)
- Implementar `SceneEngine` con carga/descarga de escenas
- Definir `SceneScheduler` para activación por tiempo
- Implementar `Transport` con clock interno

### Fase 3: Adaptadores (Semanas 3-4)
- Implementar `OSCAdapter` para recepción de mensajes
- Implementar `KeyboardTriggerAdapter`
- Definir `KinectAdapterContract` (interfaz vacía para futura implementación)

### Fase 4: Mapping y UI (Semanas 4-5)
- Implementar `MappingLayer` vinculando adaptadores al registry
- Crear `ControlUI` mínima para debug/visualización
- Implementar `PresetPersistence` para guardar/cargar configuraciones

### Fase 5: Integración y Testing (Semanas 5-6)
- Integrar todos los módulos en ofApp
- Tests de unidad para ParameterRegistry, MappingLayer
- Tests de integración para flujo completo

## Entregables Esperados

### 1. Contexto del Sistema
- Diagrama de componentes mostrando los 10 dominios y sus relaciones
- Flujo de datos desde entrada externa hasta render de escena

### 2. Requerimientos Funcionales
- RF1: El sistema debe poder cargar múltiples escenas y mantenerlas en memoria
- RF2: El sistema debe permitir activar una escena por nombre/index en cualquier momento
- RF3: El sistema debe poder pausar y reanudar una escena activa
- RF4: El sistema debe exponer parámetros de escena a través del Registry
- RF5: El sistema debe responder a mensajes OSC en formato `/fader N 0-1`, `/button N 0-1`, `/bpm N`, `/play`, `/stop`
- RF6: El sistema debe permitir mapeo arbitrario de OSC → parámetro
- RF7: El sistema debe funcionar con clock interno configurable (BPM 40-200)
- RF8: El sistema debe poder responder a teclas para activación de escenas
- RF9: El sistema debe poder guardar y restaurar el estado de mappings
- RF10: El sistema debe poder recibir activación basada en timeline (beat/bar)

### 3. Requerimientos No Funcionales
- RNF1: Tiempo de respuesta < 10ms para cambio de parámetro
- RNF2: Memoria por escena promedio < 50MB
- RNF3: El sistema debe compilar en macOS con of_v0.12.1
- RNF4: Tests de unidad cubriendo ParameterRegistry y MappingLayer
- RNF5: Documentación de API para cada módulo público

### 4. Arquitectura Modular

```
┌─────────────────────────────────────────────────────────────────────┐
│                         ofApp (Orchestrator)                        │
├─────────────────────────────────────────────────────────────────────┤
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────────┐  │
│  │   SceneEngine  │  │    Transport    │  │ ParameterRegistry  │  │
│  │  ────────────  │  │   ───────────   │  │    ────────────    │  │
│  │ + loadScene()  │  │ + setBPM()       │  │ + registerParam()  │  │
│  │ + activate()   │  │ + getBeat()      │  │ + getParam()       │  │
│  │ + pause()      │  │ + getBar()       │  │ + setParam()       │  │
│  │ + update()     │  │ + sync()         │  │ + subscribe()      │  │
│  └───────┬────────┘  └────────┬─────────┘  └────────┬───────────┘  │
│          │                    │                      │              │
│  ┌───────┴────────────────────┴──────────────────────┴───────────┐ │
│  │                         MappingLayer                           │ │
│  │  ┌──────────────────────────────────────────────────────────┐ │ │
│  │  │  OSCAdapter  │  KeyboardAdapter  │  KinectContract       │ │ │
│  │  │  + listen()  │  + onKeyPress()   │  + connect() (future) │ │ │
│  │  └──────────────────────────────────────────────────────────┘ │ │
│  └───────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────────┤
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────────┐  │
│  │ SceneScheduler │  │  ControlUI      │  │ PresetPersistence  │  │
│  │  ────────────  │  │   ──────────    │  │    ──────────────   │  │
│  │ + schedule()   │  │ + render()      │  │ + save()           │  │
│  │ + trigger()    │  │ + drawParams()  │  │ + load()           │  │
│  │ + hybrid()     │  │ + drawMappings()│  │ + export()        │  │
│  └────────────────┘  └─────────────────┘  └────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

### 5. Contratos entre Módulos

| Módulo A | → | Módulo B | Contrato |
|----------|---|----------|----------|
| Adaptadores | → | MappingLayer | `onInput(source, channel, value)` |
| MappingLayer | → | ParameterRegistry | `map(source, channel, paramId)` / `getMappings()` |
| SceneEngine | → | ParameterRegistry | `getParam(id)` / `setParam(id, value)` |
| Transport | → | SceneScheduler | `onBeat()`, `onBar()` callbacks |
| SceneScheduler | → | SceneEngine | `activate(sceneId)` |
| ParameterRegistry | → | ControlUI | Observer pattern para cambios |
| PresetPersistence | → | MappingLayer | `saveMappings(path)`, `loadMappings(path)` |

### 6. Tipos de Datos Base

```cpp
// parameter.h
enum class ParameterType { FLOAT, INT, BOOL, STRING, VEC2, VEC3 };
struct Parameter { string id; ParameterType type; float value; float min; float max; };
struct ParameterBinding { string source; int channel; string paramId; float minIn; float maxIn; };

// scene.h
using SceneHandle = unique_ptr<BaseScene>;
struct SceneInfo { string id; string name; vector<string> params; };

// transport.h
struct BeatInfo { int beat; int bar; float bpm; bool isPlaying; };

// osc.h
struct OSCMessage { string address; vector<float> args; };
```

### 7. Flujo de Eventos

```
[Entrada Externa] → [Adaptador] → [MappingLayer] → [ParameterRegistry] → [Escena]
                                        ↓
                                  [ControlUI] ← Suscriptor
```

**Flujo de Activación por Tiempo:**
```
[Transport] (beat/bar) → [SceneScheduler] → [SceneEngine.activate(sceneId)]
```

**Flujo de Trigger Externo:**
```
[Adaptador.onInput] → [MappingLayer.resolve] → [ParameterRegistry.set]
                      → [Scene.update] (si escena activa)
```

### 8. Modelo de Activación de Escenas

| Modo | Descripción | Cuándo usar |
|------|-------------|-------------|
| **Timeline** | Activación programada en beats/bars | Performance coreográfica estructurada |
| **Trigger** | Activación por evento externo (tecla, botón OSC) | Improvisación, interacción en vivo |
| **Híbrido** | Combinación: timeline con override de trigger | Sesiones con estructura + flexibilidad |

### 9. Estrategia de Sync y BPM

- **Clock Interno**: `Transport` genera beats internos basados en BPM configurable (40-200)
- **Clock Externo**: Receptor OSC para `/bpm` y `/clock` (futuro Ableton Link)
- **Beat Callback**: Observadores se suscriben a `onBeat`, `onBar`
- **Cuantización**: Posibilidad de cuantizar activación a grid (beat, 2 beats, bar, 4 bars)

### 10. Estrategia de Mapping

```
[Fuente] → [Canal] → [Parámetro Destino] → [Rango Input] → [Rango Output]
   ↓
 OSC /fader/1 → 1 → "scene.physics.k" → 0.0-1.0 → 0.1-2.0
```

- **Mappings almacenados en JSON**: `{ "mappings": [{ "source": "osc", "channel": 1, "param": "scene.physics.k", "in": [0,1], "out": [0.1,2] }] }`
- **Runtime hot-reload**: Permite agregar mappings sin recompilar
- **Multiple fuentes**: Mismo sistema sirve OSC, teclado, Kinect (futuro)

### 11. Contrato Inicial de Kinect

```cpp
// kinectAdapterContract.h - Interfaz para implementación futura
class IKinectAdapter {
public:
    virtual ~IKinectAdapter() = default;
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() = 0;
    virtual vector<Body> getBodies() = 0;
    virtual HandState getLeftHandState() = 0;
    virtual HandState getRightHandState() = 0;
    virtual Joint getJoint(JointType) = 0;
    
    // Eventos que el MappingLayer puede escuchar
    signal<void(HandGesture)> onHandGesture;  // leftRaise, rightRaise, etc.
    signal<void(BodyPose)> onBodyPose;         // armsUp, leanLeft, etc.
};
```

**Nota**: La implementación real de Kinect Adapter queda fuera del scope inicial. El MappingLayer usará la misma interfaz que OSC Adapter para que la migración sea transparente.

### 12. Estructura de Carpetas

```
src/
├── scenes/
│   ├── BaseScene.h / .cpp
│   ├── PinturaFluorescenteScene.h / .cpp
│   └── [futuras escenas]
├── engine/
│   ├── SceneEngine.h / .cpp
│   ├── SceneScheduler.h / .cpp
│   └── Transport.h / .cpp
├── registry/
│   └── ParameterRegistry.h / .cpp
├── mapping/
│   ├── MappingLayer.h / .cpp
│   └── ParameterBinding.h / .cpp
├── adapters/
│   ├── OSCAdapter.h / .cpp
│   ├── KeyboardTriggerAdapter.h / .cpp
│   └── kinect/
│       └── KinectAdapterContract.h
├── ui/
│   └── ControlUI.h / .cpp
├── persistence/
│   └── PresetPersistence.h / .cpp
└── ofApp.h / .cpp
```

### 13. Roadmap por Tareas

| Task | Descripción | Depende de | Estimación |
|------|-------------|------------|------------|
| T1 | Definir tipos base (Parameter, ParameterBinding, SceneInfo) | - | 1 día |
| T2 | Implementar ParameterRegistry | T1 | 2 días |
| T3 | Implementar BaseScene con ciclo de vida | - | 1 día |
| T4 | Migrar PinturaFluorescenteScene a BaseScene | T3 | 2 días |
| T5 | Implementar SceneEngine (load/activate/pause) | T2, T4 | 2 días |
| T6 | Implementar Transport (BPM, beat, clock interno) | - | 2 días |
| T7 | Implementar SceneScheduler (timeline, trigger, híbrido) | T5, T6 | 2 días |
| T8 | Implementar OSCAdapter | - | 2 días |
| T9 | Implementar KeyboardTriggerAdapter | - | 1 día |
| T10 | Definir KinectAdapterContract | - | 1 día |
| T11 | Implementar MappingLayer | T2, T8, T9, T10 | 3 días |
| T12 | Implementar ControlUI | T2, T11 | 2 días |
| T13 | Implementar PresetPersistence | T11 | 1 día |
| T14 | Integrar todo en ofApp | T5-T13 | 2 días |
| T15 | Tests de unidad (ParameterRegistry, MappingLayer) | T2, T11 | 2 días |

**Total estimado**: ~24 días (6 semanas)

### 14. Criterios de Aceptación

- [ ] **CA1**: ofApp compila sin errores con `make` en macOS
- [ ] **CA2**: Al menos una escena (PinturaFluorescente) funciona correctamente
- [ ] **CA3**: Parámetros de escena accesibles y modificables via ParameterRegistry
- [ ] **CA4**: OscAdapter recibe mensajes y los mapea correctamente a parámetros
- [ ] **CA5**: KeyboardTriggerAdapter activa escenas por teclas
- [ ] **CA6**: Transport genera beats internos y responde a cambios de BPM
- [ ] **CA7**: SceneScheduler activa escenas por tiempo y por trigger
- [ ] **CA8**: Mappings se pueden guardar y cargar desde archivo JSON
- [ ] **CA9**: ControlUI muestra estado de parámetros y mappings activos
- [ ] **CA10**: Sistema funciona sin hardware externo (clock interno)
- [ ] **CA11**: Tests de unidad pasan para ParameterRegistry y MappingLayer

### 15. Riesgos Técnicos y Tradeoffs

| Riesgo | Likelihood | Impacto | Mitigation | Tradeoff |
|--------|------------|---------|------------|----------|
| Complejidad excesiva en ParameterRegistry | Med | Alto | Limitar tipos de parámetros a 6 esenciales | Flexibilidad vs Simocidad |
| Acoplamiento accidental adaptadores↔escenas | Med | Alto | Tests de integración, contratos claros | Reusabilidad vs Acoplamiento |
| Latencia en actualización de parámetros | Low | Med | Benchmarking, optimizar hot path | Features vs Performance |
| Kinect no disponible para testing | High | Med | Mock adapter para desarrollo | Testing real vs Mock |
| Clock drift con transporte externo | Med | Med | Implementar clock smoothing | Sync precisión vs Estabilidad |
| Serialización de presets quebrada | Low | Med | Tests de round-trip JSON | Persistence vs Versioning |

**Tradeoffs Architeturarios Definidos:**
1. **ParameterRegistry como Singleton** — Simplifica acceso pero dificulta testing con mocks → Aceptado por simplicidad, poder injectar dependencia en tests.
2. **Mappings en JSON runtime** — Flexibilidad maxima pero parsing overhead → Aceptado, JSON es pequeño (~1KB typical).
3. **Kinect diferido** — Demora validación real pero permite progreso → Aceptado, contrato define interfaz testeable.

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/scenes/` | New | BaseScene, escenas daughters |
| `src/engine/` | New | SceneEngine, SceneScheduler, Transport |
| `src/registry/` | New | ParameterRegistry |
| `src/mapping/` | New | MappingLayer, ParameterBinding |
| `src/adapters/` | New | OSCAdapter, KeyboardTriggerAdapter, KinectAdapterContract |
| `src/ui/` | New | ControlUI |
| `src/persistence/` | New | PresetPersistence |
| `src/ofApp.cpp/h` | Modified | Replaced porOrchestrator delegating a módulos |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Alcance demasiado amplio para una iteración | High | Dividir en multiple releases (v1: SceneEngine+Registry, v2: Adaptadores, v3: Kinect) |
| Dependencias circulares entre módulos | Med | Dependecy injection, contratos claros |
| Romper visualización existente durante refactor | Med | Tests visuales manuales post-cambio |
| Dificultad para testing sin hardware | High | Mock adapters, modo simulación |

## Rollback Plan

1. Mantener backup de ofApp.cpp/h original
2. Si el build falla: restaurar archivos originales
3. Si la visualización se rompe: verificar que todos los métodos de BaseScene están correctamente implementados
4. Mantener archivo de migración gradual: cada módulo puede removerse si falla

## Dependencies

- of_v0.12.1 (framework)
- liblo (para OSC) — verificar si ya está instalado
- JSON for modern C++ (nlohmann/json) — verificar si ya está disponible o incluir header-only

## Success Criteria

- [ ] Compilación exitosa con `make` en macOS
- [ ] PinturaFluorescenteScene renderiza correctamente
- [ ] Parámetros accesibles via ParameterRegistry
- [ ] OSC mapping funciona (test con TouchOSC o similar)
- [ ] Keyboard trigger activa escenas
- [ ] Clock interno genera beats
- [ ] Mappings se guardan y cargan
- [ ] ControlUI muestra estado
- [ ] Tests unitarios pasando para Registry y Mapping
- [ ] Documentación de API para módulos principales
