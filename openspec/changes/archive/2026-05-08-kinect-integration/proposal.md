# Proposal: kinect-integration

## Intent

Integrar Kinect v1 (Xbox 360) como sistema de entrada corporal usando ofxOpenNI + OpenNI2/NiTE2 para skeleton tracking. Movimientos y posiciones articulares controlan o disparan escenas a través del ParameterRegistry, sin que ninguna escena conozca la fuente de entrada.

## Scope

### In Scope

| # | Deliverable | Description |
|---|-------------|-------------|
| 1 | **KinectDataTypes** | `JointType` (18 joints), `TrackedBody`, `Joint`, `Vector3D`, `HandState`, `ConnectionState` |
| 2 | **IKinectAdapter** | Interfaz abstracta pura con callbacks para joints, hands, gestures, zones. Sin dependencia de SDK Kinect |
| 3 | **OpenNIKinectAdapter** | Implementación concreta con ofxOpenNI. Inicializa dispositivo, lee skeleton de NiTE2, normaliza coordenadas, invoca callbacks |
| 4 | **MockKinectAdapter** | Mock para testing sin hardware. Produce datos corporales determinísticos |
| 5 | **KinectMappingEngine** | Mapea posiciones articulares/gestos a parámetros de escena. Mismo patrón que OscMappingEngine (CRUD, condiciones, transforms, JSON). Archivos en `mapping/` |
| 6 | **KinectPanel** | Panel ImGui implementando IUIModule. Muestra estado Kinect, coordenadas articulares, UI de configuración de mappings, preview de skeleton. Panel ID: `"Kinect"` |
| 7 | **SkeletonView** | Renderiza skeleton tracked con ofPolyline/ofCircle. Color-coded por confianza. Dibuja en el viewport principal |
| 8 | **ofApp wiring** | Cablear nuevos componentes en ofApp (mismo patrón OSC/MIDI). Save/load configuraciones en exit() |
| 9 | **Installation docs** | Documentar pasos de instalación de ofxOpenNI + OpenNI2 + NiTE2 en macOS |

### Out of Scope

- Azure Kinect o Kinect v2 (arquitectura permite swap futuro)
- Nuitrack SDK
- Datos de cámara de profundidad o RGB (solo skeleton tracking)
- Reconocimiento de voz

## Approach

**Seguir patrones existentes estrictamente:**
- Adapter: `OpenNIKinectAdapter` paralelo a `OSCAdapter`
- Mapping engine: `KinectMappingEngine` paralelo a `OscMappingEngine`
- UI panel: `KinectPanel` paralelo a `OscMappingPanel` (IUIModule)
- Persistencia: JSON via `ofSaveJson`/`ofLoadJson`, archivos en `config/`

**Principio arquitectónico**: Las escenas NUNCA conocen Kinect. Solo reciben parámetros a través del ParameterRegistry. El KinectMappingEngine traduce "mano derecha arriba de la cabeza" → `"pinturaFluorescente.flockSpeed = 1.5"`.

**File structure:**
```
src/adapters/kinect/
  KinectTypes.h              — JointType, TrackedBody, Joint, Vector3D, etc.
  IKinectAdapter.h           — Abstract interface
  OpenNIKinectAdapter.h/.cpp — ofxOpenNI implementation
  MockKinectAdapter.h/.cpp   — Mock for testing
src/mapping/
  KinectMappingEngine.h/.cpp — Mapping engine
  KinectMappingTypes.h       — KinectMapping struct
src/input/sensors/
  SkeletonView.h/.cpp        — Skeleton renderer
src/ui/
  KinectPanel.h/.cpp         — ImGui configuration panel
```

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/adapters/kinect/` | **New** | Tipos de datos Kinect, interfaz abstracta, implementación OpenNI, mock |
| `src/mapping/` | **New files** | KinectMappingEngine + KinectMappingTypes (junto a OscMappingEngine existente) |
| `src/input/sensors/` | **New** | SkeletonView — renderer de skeleton en viewport |
| `src/ui/` | **New files** | KinectPanel — panel ImGui (IUIModule) |
| `src/ui/PanelIds.h` | Modified | Agregar `kKinect = "Kinect"` |
| `src/ui/UIManager.h` | Modified | Nuevo dock ID `kinectDockId_` |
| `src/ui/UIManager.cpp` | Modified | Registrar dock Kinect en default layout |
| `src/ofApp.h` | Modified | Nuevos miembros: `kinectAdapter_`, `kinectMappingEngine_`, `skeletonView_` |
| `src/ofApp.cpp` | Modified | Wiring en setup(), update(), draw(), exit() |
| `addons.make` | Modified | Agregar `ofxOpenNI` |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| OpenNI2/NiTE2 no compila en macOS moderno | High | Verificar compatibilidad primero; usar Nuitrack como fallback |
| Kinect v1 requiere adaptador USB/power especial | Med | Documentar claramente en installation docs |
| ofxOpenNI puede necesitar updates para OF 0.12.1 | Med | Forkear y adaptar si es necesario |
| CPU usage de skeleton tracking impacta performance de escenas | Med | Capear procesamiento de skeleton a 30fps |
| Sin hardware para testing real | High | MockKinectAdapter produce datos determinísticos; testear mappings con mock |

## Rollback Plan

1. Remover `ofxOpenNI` de `addons.make`
2. Eliminar todos los archivos nuevos (`src/adapters/kinect/`, `src/mapping/Kinect*`, `src/ui/KinectPanel.*`, `src/input/sensors/`)
3. Revertir cambios en `ofApp.h`, `ofApp.cpp`, `PanelIds.h`, `UIManager.h`, `UIManager.cpp`
4. Sin migraciones de base de datos, sin corrupción de estado compartido — cambio puramente aditivo

## Dependencies

- **Phase chain**: proposal → specs → design → tasks → apply (6 fases) → verify → archive
- **Sin dependencias de otros cambios activos**
- **Reusa**: ParameterRegistry, SceneEngine, IUIModule, UIManager, MappingLayer
- **Externas**: ofxOpenNI addon, OpenNI2 SDK, NiTE2 middleware
- **KinectContract diferido**: Ya definido en `modular-scene-system/specs/kinect-contract/` — IKinectAdapter extiende ese contrato

## Success Criteria

- [ ] Compilación exitosa con `make` en macOS (con y sin hardware Kinect — mock compila siempre)
- [ ] MockKinectAdapter produce datos de skeleton determinísticos verificables
- [ ] KinectMappingEngine mapea joints/gestures a parámetros via ParameterRegistry (sin que escenas conozcan Kinect)
- [ ] KinectPanel muestra estado de conexión, joints, y UI de configuración de mappings
- [ ] SkeletonView renderiza skeleton con ofPolyline en viewport principal
- [ ] Configuración de mappings se guarda/carga via JSON en `config/kinect-mappings.json`
- [ ] ofApp compila y ejecuta sin regresiones en escenas existentes
- [ ] Installation docs cubren OpenNI2 + NiTE2 en macOS paso a paso
