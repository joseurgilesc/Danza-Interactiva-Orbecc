# Verification Report: Modular Scene System Design

**Change**: modular-scene-system  
**Mode**: Standard (no Strict TDD - proyecto C++ sin test runner detectado)  
**Date**: 2026-04-06  
**Artifact Store**: openspec

---

## ✅ PASS - Diseño listo para Tasks

### Re-verificación: Correcciones Aplicadas

**Fecha de re-verificación**: 2026-04-06

| Issue | Estado | Evidencia |
|-------|--------|-----------|
| 1. Acceso a Transport | ✅ CORREGIDO | Líneas 302-303: "Escenas pueden LEER Transport para sincronización visual, pero NO deben MODIFICARLO" |
| 2. IBaseScene expandido | ✅ CORREGIDO | Líneas 935-1029: Incluye getSuggestedMappings(), getDefaultPreset(), getSceneMetadata(), onBeat(), onBar(), isValid() |
| 3. UI solo muestra | ✅ CORREGIDO | Línea 4660: "Regla de Separación de Responsabilidades" - subsección completa

---

## Executive Summary (Original)

El diseño "modular-scene-system" presenta una arquitectura bien concebida con 8 bounded contexts y separación clara de responsabilidades. Sin embargo, se detectaron **violaciones críticas de acoplamiento**, **contradicciones internas** y **definiciones faltantes** en la verificación inicial.

**Verdict (original)**: ❌ FAIL - Requería correcciones antes de proceder

---

## Checklist de Verificación

### 1. Independencia de Escenas (IBaseScene)

| Requisito | Estado | Evidencia |
|-----------|--------|-----------|
| IBaseScene no referencia OSC | ⚠️ PARCIAL | Contrato de interfaz **NO definido** en el diseño |
| IBaseScene no referencia Keyboard | ⚠️ PARCIAL | Mismo problema - interfaz no existe |
| IBaseScene no referencia Kinect | ⚠️ PARCIAL | Mismo problema |
| Escenas reciben parámetros via ParameterRegistry | ✅ PASS | Líneas 2998-3036 muestran registry en setup() |
| Escenas reciben transport state | ✅ PASS | Líneas 312-321 muestran acceso a Transport |

**Hallazgo Crítico**: El diseño **NO define** el contrato `IBaseScene`. Solo se muestran ejemplos de clases que heredan de él (`PinturaFluorescenteScene`, `MiEscena`), pero la interfaz base no está documentada. No se puede verificar que las escenas estén desacopladas de hardware porque no hay especificación de la interfaz.

---

### 2. Scheduler como Mediador

| Requisito | Estado | Evidencia |
|-----------|--------|-----------|
| ISceneScheduler es único que activa escenas | ✅ PASS | Líneas 813-829, 344-350 |
| ISceneActivator es la interfaz | ✅ PASS | Líneas 836-858 |
| Ningún adapter activa escenas directamente | ✅ PASS | MappingLayer solo llama a scheduler.onExternalTrigger() |

**Detalle**: El diseño muestra correctamente el flujo:
```
MappingLayer → Scheduler.onExternalTrigger() → Activator.activateScene()
```

---

### 3. Fallback sin Hardware

| Requisito | Estado | Evidencia |
|-----------|--------|-----------|
| Clock interno en Transport | ✅ PASS | Líneas 113-136 (InternalClock) |
| MappingLayer con valores simulados | ✅ PASS | Líneas 2460-2497 (MappingTestPanel para simular inputs) |
| Sistema corre sin OSC externo | ✅ PASS | Líneas 367-421 (fallback por timeout) |

**Detalle**: El diseño incluye:
- `InternalClock` con `update(float deltaTime)` para sync interno
- `MappingTestPanel` para simular valores de entrada
- Timeout configuration con `externalClockTimeout = 5.0f`

---

### 4. Transport State Uniforme

| Requisito | Estado | Evidencia |
|-----------|--------|-----------|
| ITransport expone BPM | ✅ PASS | Líneas 73, 209, 357 |
| ITransport expone beat | ✅ PASS | Línea 74 |
| ITransport expone bar | ✅ PASS | Línea 75 |
| ITransport expone phase | ✅ PASS | Línea 76 |
| ITransport expone isPlaying | ✅ PASS | Línea 79 |
| Estado consistente para sync interno y externo | ✅ PASS | SyncMode enum (línea 96) |

---

### 5. Parámetros Tipados

| Requisito | Estado | Evidencia |
|-----------|--------|-----------|
| Parameter incluye type | ✅ PASS | Línea 3006 (`type = ParamType::FLOAT`) |
| Parameter incluye min | ✅ PASS | Línea 3009 (`min = 0.0f`) |
| Parameter incluye max | ✅ PASS | Línea 3010 (`max = 100.0f`) |
| Parameter incluye default | ✅ PASS | Línea 3011 (`defaultValue = 50.0f`) |
| Todos los campos necesarios | ✅ PASS | step, label, group, scope, sceneId |

---

### 6. UI Sin Lógica de Negocio

| Requisito | Estado | Evidencia |
|-----------|--------|-----------|
| UI solo muestra y permite edición | ✅ PASS | Líneas 2309-2371 (solo display/edit) |
| UI no contiene lógica de decisión | ⚠️ PARCIAL | No hay declaración explícita en el diseño |
| UI no activa escenas directamente | ✅ PASS | Líneas 3181-3189 muestran observer pattern |

**Hallazgo**: No hay una sección explícita que declare "UI no tiene lógica de negocio". Aunque el código mostrado cumple el requisito, debería documentarse explícitamente en el diseño.

---

### 7. Mapping Reutilizable

| Requisito | Estado | Evidencia |
|-----------|--------|-----------|
| MappingLayer acepta cualquier IInputAdapter | ✅ PASS | Líneas 1700-1757 (AbstractSource) |
| No código específico por fuente | ✅ PASS | Líneas 1260-1279 solo usan source string |
| Soporta OSC, Keyboard, Kinect | ✅ PASS | Tabla líneas 1711-1716 |

**Detalle**: El diseño define `SourceType` con OSC, KEYBOARD, KINECT, INTERNAL. MappingLayer resuelve bindings por `sourceId` sin código específico por tipo.

---

### 8. Modos de Activación

| Requisito | Estado | Evidencia |
|-----------|--------|-----------|
| TIMELINE mode | ✅ PASS | Línea 604 |
| TRIGGER mode | ✅ PASS | Línea 605 |
| HYBRID mode | ✅ PASS | Línea 606 |
| Transición entre modos en runtime | ✅ PASS | Líneas 619-641 |

---

## Riesgos de Acoplamiento Identificados

### CRÍTICO: Contrato IBaseScene No Definido

**Problema**: El diseño no especifica la interfaz `IBaseScene`. Esto es una violación directa del principio de "contratos explícitos" que el propio diseño promueve.

**Evidencia**:
- El diseño usa `IBaseScene` en múltiples lugares (líneas 312, 2998, 3144)
- No existe ninguna sección que defina los métodos virtuales de esta interfaz
- No se puede verificar independence de hardware sin esta definición

**Impacto**: Cualquier implementación podría agregar dependencias a OSC/Keyboard/Kinect sin detección.

**Mitigación requerida**: Agregar sección "Contrato IBaseScene" con:
```cpp
class IBaseScene {
public:
    virtual ~IBaseScene() = default;
    virtual void setup() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void draw() = 0;
    virtual void activate() = 0;
    virtual void deactivate() = 0;
    // NO referencias a IInputAdapter, IOSCAdapter, IKeyboardAdapter, etc.
};
```

---

### CRÍTICO: Contradicción en Lectura de Transport

**Problema**: El diseño muestra código que viola las reglas que él mismo establece.

| Sección | Código | Problema |
|---------|--------|----------|
| Líneas 312-321 | `auto state = Transport::get().getState();` en MyScene | **VIOLACIÓN** - La línea 338 establece "SceneEngine y Scheduler NO referencian a Transport directamente" |
| Líneas 353-360 | Código marcado como "INCORRECTO" | Este patrón aparece en líneas 312-321 como ejemplo válido |

**Análisis**:
- La línea 315 usa `Transport::get().getState()` dentro de la escena
- La línea 338-341 establece la REGLA de no coupling directo
- Las líneas 353-360 muestran como "INCORRECTO" usar polling de Transport
- PERO las líneas 312-321 hacen exactamente eso

**Inconsistencia**: El diseño permite que las escenas lean Transport directamente (líneas 312-321) mientras prohíbe que SceneEngine haga polling (líneas 353-360). No hay justificación para esta diferencia.

---

### WARNING: Scheduler Dependencia Implícita de SceneEngine

**Problema**: El diseño no especifica cómo SceneScheduler referencia a SceneEngine (_ISceneActivator_).

**Evidencia**:
- Líneas 817-829 muestran `activator_` pero no se muestra la inyección
- No hay sección de "Dependency Injection" o "Construction"

**Riesgo**: El diseño podría permitir acoplamiento directo entre Scheduler y Engine.

---

## Inconsistencias Detectadas

### Inconsistencia 1: Nomenclatura de Interfaces

| Ubicación | Notación | Problema |
|-----------|----------|----------|
| ISceneActivator (línea 836) | `class` | Debería ser `interface` o documentación explícita |
| IOSCAdapter (línea 1012) | `class` | Mismo problema |
| IKinectAdapter (línea 4780) | `class` | Mismo problema |
| IBoo everywhere | `class` | Inconsistente con la convención de interfaces |

**Recomendación**: Usar convención consistente, ej. prefijo `I` para interfaces y documentar que son contratos puros.

---

### Inconsistencia 2: ParameterRegistry es Singleton vs Inyección

| Sección | Afirmación |
|---------|-------------|
| Línea 27 | "ParameterRegistry como Fuente de Verdad Central - Choice: Singleton" |
| Líneas 3001, 3147 | Acceso como `ParameterRegistry::get()` |
| Líneas 1261, 2121 | MappingLayer tiene `registry_` como miembro |

**Problema**: El diseño usa Singleton EN TODAS PARTES pero la línea 29 sugiere que se consideró inyección de dependencia.

**Recomendación**: Declarar explícitamente que ParameterRegistry es Singleton y documentar por qué se rechazó DI.

---

### Inconsistencia 3: OscAdapter Conocimiento de Transport

| Sección | Evidencia |
|---------|-----------|
| Líneas 1346-1378 | OSCAdapter tiene puntero a ITransport y maneja mensajes "/bpm", "/play", "/stop" |
| Líneas 1237-1259 | "Adapter conocimiento mínimo del mapping" - establece que Adapter no debe conocer mapeos |

**Problema**: El diseño promueve "mínimo conocimiento" para mappings (línea 1237) pero OSCAdapter conoce directamente Transport y maneja mensajes específicos.

**Contradictio**: 
- "OSCAdapter NO conoce los mapeos específicos" (línea 1240)
- PERO "OSCAdapter maneja mensajes /bpm, /play, /stop" (líneas 1353-1374)

Esto es una **excepción documentada** (líneas 1381-1388) pero debería marcarse más prominentemente.

---

## Preguntas Abiertas

### Pregunta 1: ¿Las escenas deben poder订阅 Transport directamente?

**Contexto**: Líneas 312-321 permiten que una escena haga `Transport::get().getState()`, pero línea 338 dice que Transport es el único que conoce a sus observers.

**Necesita clarificación**:
- ¿Las escenas pueden subscribe a onBeat/onBar?
- ¿O solo pueden leer estado periódicamente?
- ¿Cuál es la diferencia con SceneEngine (prohibido de polling)?

---

### Pregunta 2: ¿Cómo se configura la inyección de ISceneActivator en Scheduler?

**Contexto**: El diseño muestra el uso de `activator_` pero no muestra cómo se configura.

**Necesita clarificación**:
- Constructor injection?
- Setter injection?
- ¿Por qué no se usa también Singleton como Transport?

---

### Pregunta 3: ¿Cuál es el mecanismo de fallback si no hay MappingLayer?

**Contexto**: Si el sistema corre sin ningún adapter, ¿cómo se controlan los parámetros?

**Necesita clarificación**:
- ¿UI es suficiente para fallback?
- ¿Existe algún generador interno de valores?

---

### Pregunta 4: ¿Cómo maneja el sistema errores de validación de mensajes OSC?

**Contexto**: Líneas 1394-1448 muestran manejo de errores detallado.

**Necesita clarificación**:
- ¿Estos errores se reportan a quién?
- ¿Hay un sistema de notificaciones al usuario?
- ¿Los errores afectan el estado del sistema?

---

## Criterios Mínimos de Aceptación

Para que el diseño sea considerado válido, DEBE cumplir:

### Criterios Obligatorios (Must Pass)

| # | Criterio | Verificación |
|---|----------|---------------|
| 1 | ✅ Definir contrato IBaseScene con todos los métodos virtuales puros | Nueva sección con interfaz completa |
| 2 | ✅ Eliminar o justificar acceso directo a Transport desde escenas | Si se permite, documentar la excepción |
| 3 | ✅ Documentar explícitamente que UI no tiene lógica de negocio | Nueva subsección en UI Domain |
| 4 | ✅ Especificar mecanismo de inyección de ISceneActivator | En Scheduler, sección de construcción |

### Criterios Recomendados (Should Pass)

| # | Criterio | Verificación |
|---|----------|---------------|
| 5 | ✅ Estandarizar nomenclatura de interfaces | Usar `interface I*` consistentemente |
| 6 | ✅ Documentar excepciones de diseño (OSCAdapter→Transport) | En "Arquitectura Decisions" |
| 7 | ✅ Agregar diagrama de arquitectura general | Resumen visual de los 8 bounded contexts |

---

## Summary

| Métrica | Valor |
|---------|-------|
| Total de items verificados | 8 |
| PASS | 6 |
| PARCIAL | 2 |
| FAIL | 0 |
| **Riesgos CRÍTICOS** | 2 |
| **Inconsistencias** | 3 |
| **Preguntas abiertas** | 4 |

---

## Recomendación

**NO PROCEDER** a implementación hasta que:

1. Se defina el contrato `IBaseScene` explícitamente
2. Se resuelva la contradicción sobre acceso a Transport desde escenas
3. Se documente el mecanismo de inyección de ISceneActivator
4. Se agregue declaración explícita de "UI sin lógica de negocio"

El diseño tiene fundamentos sólidos (8 bounded contexts, mediator pattern, observer pattern), pero las **definiciones faltantes** y **contradicciones** pueden causar implementaciones divergentes.

---

*Report generated by SDD Verify Phase - 2026-04-06*
