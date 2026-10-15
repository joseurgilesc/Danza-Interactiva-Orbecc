# Design: Modular Scene System

## Technical Approach

Arquitectura basada en **Domain-Driven Design** con 8 bounded contexts claramente definidos. Cada dominio es autocontenido y se comunica mediante contratos bien definidos. La separación estricta entre adaptadores (OSC/Keyboard/Kinect) y escenas se implementa a través del MappingLayer como mediator.

## Architecture Decisions

### Decision 1: Communication Flow Between Bounded Contexts

**Choice**: Mediator pattern con MappingLayer como único puente entre adaptadores y dominios internos
**Alternatives considered**: 
- Direct coupling entre adaptadores y ParameterRegistry
- Event bus global con pub/sub
**Rationale**: MappingLayer centraliza la transformación de inputs, permitiendo agregar nuevos adaptadores (Kinect futuro) sin modificar escenas ni ParameterRegistry. Event bus introduce complejidad innecesaria para el volumen de mensajes esperado.

### Decision 2: Transport como Servicio Transversal (Cross-Cutting)

**Choice**: Singleton accesible por todos los dominios que necesiten sincronización temporal
**Alternatives considered**:
- Inyección de dependencia en cada dominio
- Event-based donde Transport publica beats
**Rationale**: El volumen de beat callbacks es predecible y fijo (~120/min máx). Singleton simplifica la arquitectura para un sistema de este tamaño. event-based añade overhead de subscribe/unsubscribe que no aporta valor aquí.

### Decision 3: ParameterRegistry como Fuente de Verdad Central

**Choice**: Singleton con observer pattern para notificaciones de cambio
**Alternatives considered**:
- Cada escena mantiene su propio estado
- CQRS con comando/query separation
**Rationale**: Especificación requiere punto único de acceso. Observer pattern permite que ControlUI y MappingLayer reactiven a cambios sin polling. CQRS es overkill para un sistema con ~50-100 parámetros máx.

### Decision 4: Scheduler Desacoplado del Render Loop

**Choice**: SceneScheduler subscribe a Transport callbacks, no tiene referencias a SceneEngine directamente
**Alternatives considered**:
- SceneEngine directo llama a SceneScheduler.update()
- Scheduler contenido dentro de SceneEngine
**Rationale**: Separación clara de responsabilidades. Scheduler solo decide QUÉ activar, SceneEngine decide CÓMO. Esto permite cambiar lógicas de scheduling sin tocar el motor de escenas.

### Decision 5: Kinect como Contrato Diferido (Deferred Contract)

**Choice**: IKinectAdapter interface vacía, implementación futura
**Alternatives considered**:
- Mock adapter funcional desde el inicio
- Postergar definición de contrato hasta implementación
**Rationale**: La especificación establece claramente que Kinect está out-of-scope. Definir la interfaz ahora permite que MappingLayer esté listo para cuando Kinect esté disponible.

---

## Bounded Contexts

| Context | Responsibility | Dependencies |
|---------|-----------------|--------------|
| **Scene Domain** | Ciclo de vida visual de escenas (setup/update/draw/activate/deactivate) | ParameterRegistry (read-only parameters) |
| **Scheduling Domain** | Activación por timeline/trigger/híbrido | Transport (beat/bar callbacks), SceneEngine |
| **Transport Domain** | BPM, beat tracking, clock interno/externo | Ninguno (servicio transversal) |
| **Parameter Domain** | Registro central de parámetros, observer pattern | Ninguno (base del sistema) |
| **Mapping Domain** | Transformación de inputs externos a parámetros | ParameterRegistry, Input Adapters |
| **Adapter Domain** | Recepción de OSC, keyboard, Kinect (contrato) | MappingLayer |
| **UI Domain** | Visualización de estado y parámetros | ParameterRegistry |
| **Persistence Domain** | Serialización de mappings, presets | MappingLayer, Scheduling Domain |

---

## Transport / Sync Engine

### 1. Modelo de Transport State

```cpp
struct TransportState {
    // Tempo y posición
    float bpm = 120.0f;
    int beat = 1;           // 1-4 (o subdivisión)
    int bar = 1;             // Compás desde inicio
    float phase = 0.0f;      // 0.0 - 1.0 dentro del beat actual
    
    // Estado de playback
    bool isPlaying = false;
    bool isPaused = false;
    float timeElapsed = 0.0f;  // Segundos desde inicio
    float lastBeatTime = 0.0f; // Timestamp del último beat
    
    // Subdivisiones
    enum class Subdivision { QUARTER = 4, EIGHTH = 8, SIXTEENTH = 16 };
    Subdivision subdivision = Subdivision::QUARTER;
    
    // Time signature
    struct TimeSignature {
        int numerator = 4;   // 4/4, 3/4, 6/8, etc.
        int denominator = 4;
    };
    TimeSignature timeSignature;
    
    // Modo de sync
    enum class SyncMode { INTERNAL_SYNC, EXTERNAL_SYNC, FREE_RUN };
    SyncMode syncMode = SyncMode::INTERNAL_SYNC;
};
```

**Beat subdivisions:**
- `QUARTER` (1/4): 1 beat = 1tick
- `EIGHTH` (1/8): 2 ticks por beat
- `SIXTEENTH` (1/16): 4 ticks por beat

**Time signature:** Configurable (4/4, 3/4, 6/8, etc.) afecta el cálculo de compases.

---

### 2. Estrategia de Sync Interno vs Externo

#### Clock Interno (INTERNAL_SYNC)
```cpp
class InternalClock {
    void update(float deltaTime) {
        if (!isPlaying_ || isPaused_) return;
        
        elapsed_ += deltaTime;
        float beatDuration = 60.0f / bpm_;
        float currentBeatFloat = elapsed_ / beatDuration;
        
        // Detectar cruce de beat
        int newBeat = static_cast<int>(currentBeatFloat) % beatsPerBar_ + 1;
        if (newBeat != currentBeat_) {
            currentBeat_ = newBeat;
            notifyBeatObservers(newBeat);
            
            if (newBeat == 1) {
                currentBar_++;
                notifyBarObservers(currentBar_);
            }
        }
        
        phase_ = fmod(currentBeatFloat, 1.0f);  // 0.0 - 1.0
    }
};
```

#### Clock Externo (EXTERNAL_SYNC)
```cpp
class ExternalClock {
    // reception de mensajes OSC
    void onClockTick(int tickCount) {
        lastTickTime_ = ofGetElapsedTimef();
        externalTickCount_ = tickCount;
        updateFromExternal();
    }
    
    void onBPMChange(float bpm) {
        // Smoothing con promedio móvil
        bpmHistory_.push_back(bpm);
        if (bpmHistory_.size() > SMOOTHING_WINDOW) {
            bpmHistory_.pop_front();
        }
        currentBPM_ = calculateMovingAverage();
    }
    
private:
    std::deque<float> bpmHistory_;
    static constexpr size_t SMOOTHING_WINDOW = 4;
    
    float calculateMovingAverage() {
        float sum = 0.0f;
        for (float bpm : bpmHistory_) sum += bpm;
        return sum / bpmHistory_.size();
    }
};
```

#### Auto-detección de Fuente
```cpp
enum class ClockSource { INTERNAL, EXTERNAL };

class Transport {
    ClockSource detectSource() {
        if (lastExternalClockTime_ > 0) {
            float timeSinceLastExternal = ofGetElapsedTimef() - lastExternalClockTime_;
            if (timeSinceLastExternal < TIMEOUT_SECONDS) {
                return ClockSource::EXTERNAL;
            }
        }
        return ClockSource::INTERNAL;
    }
};
```

---

### 3. Puntos de Integración con OSC

#### Mensajes OSC Recibidos

| OSC Address | Tipo | Descripción |
|-------------|------|-------------|
| `/bpm` | float | Nuevo tempo (e.g., 120.0) |
| `/clock` | int | Tick de clock (beat counter) |
| `/play` | - | Iniciar playback |
| `/stop` | - | Detener playback |
| `/position` | int, int | Posición (beat, bar) |

```cpp
class OSCTransportHandler {
    void processMessage(const ofxOscMessage& msg) {
        std::string address = msg.getAddress();
        
        if (address == "/bpm" && msg.getNumArgs() >= 1) {
            float bpm = msg.getArgAsFloat(0);
            if (validateBPM(bpm)) {
                transport_.setBPM(bpm);
            }
        }
        else if (address == "/clock" && msg.getNumArgs() >= 1) {
            int tick = msg.getArgAsInt(0);
            externalClock_.onClockTick(tick);
        }
        else if (address == "/play") {
            transport_.play();
        }
        else if (address == "/stop") {
            transport_.stop();
        }
        else if (address == "/position" && msg.getNumArgs() >= 2) {
            int beat = msg.getArgAsInt(0);
            int bar = msg.getArgAsInt(1);
            transport_.seek(beat, bar);
        }
    }
    
    bool validateBPM(float bpm) {
        return bpm >= MIN_BPM && bpm <= MAX_BPM;  // 20 - 300
    }
};
```

#### Mensajes OSC Enviados (Opcional - Feedback)

| OSC Address | Tipo | Descripción |
|-------------|------|-------------|
| `/transport/beat` | int | Beat actual (1-4) |
| `/transport/bar` | int | Compás actual |
| `/transport/bpm` | float | BPM actual |

```cpp
class OSCFeedbackSender {
    void onBeat(int beat) {
        ofxOscMessage msg;
        msg.setAddress("/transport/beat");
        msg.addIntArg(beat);
        sender_.sendMessage(msg);
    }
    
    void onBar(int bar) {
        ofxOscMessage msg;
        msg.setAddress("/transport/bar");
        msg.addIntArg(bar);
        sender_.sendMessage(msg);
    }
};
```

#### Puerto Configurable
```cpp
class OSCAdapter {
    void setPort(int port) {
        if (receiver_.setup(port)) {
            port_ = port;
            log_.info("OSC transport listening on port " + std::to_string(port));
        }
    }
};
```

---

### 4. Relación con Scene Engine y Scheduler

#### Diagrama de Dependencias

```
┌─────────────────────────────────────────────────────────────────────┐
│                            Transport                                │
│  • Singleton transversal                                           │
│  • No conoce a SceneEngine ni Scheduler                            │
│  • Publish: onBeat(), onBar() via callbacks                        │
└─────────────────────────────────┬───────────────────────────────────┘
                                  │
                    ┌─────────────┴─────────────┐
                    ▼                           ▼
        ┌───────────────────┐         ┌───────────────────┐
        │   SceneEngine     │         │  SceneScheduler   │
        │ (update deltaTime)│         │ (subscribe beats) │
        │                   │         │                   │
        │ • Lee transport   │         │ • onBeat()        │
        │   para animaciones│         │ • onBar()         │
        └───────────────────┘         │ • checkTimeline()│
                                       └───────────────────┘
```

#### Scene Engine y Acceso a Transport por Escenas

```
REGLA ACTUALIZADA: Escenas pueden LEER Transport para sincronización visual,
pero NO deben MODIFICARLO ni depender de él para transiciones.
```

Las escenas pueden acceder al estado actual de Transport para sincronizar animaciones visuales con el beat. Esto es **LECTURA SOLAMENTE**, no suscripción directa:

```cpp
class SceneEngine {
    void update(float deltaTime) {
        if (activeScene_) {
            // ✅ CORRECTO: SceneEngine pasa Transport state a la escena
            // o la escena lo lee directamente si lo necesita
            activeScene_->update(deltaTime);
        }
    }
};

// ✅ CORRECTO: Escena LEE Transport para sincronización visual
// Ejemplo 1: Ondas sincronizadas con beat
class OndasSincronizadasScene : public IBaseScene {
    void update(float deltaTime) override {
        // Leer estado actual del transport (READ-ONLY)
        auto state = Transport::get().getState();
        
        // Sincronizar fase de onda con beat actual
        float beatPhase = state.phase;  // 0.0 - 1.0 dentro del beat
        waveFrequency_ = baseFreq_ * (1.0f + beatPhase * 0.5f);
    }
};

// Ejemplo 2: Flock con movimiento relacionado a BPM
class FlockScene : public IBaseScene {
    void update(float deltaTime) override {
        auto state = Transport::get().getState();
        
        // Velocidad del flock relacionada al BPM
        float speedMultiplier = state.bpm / 120.0f;
        
        for (auto& bird : flock_) {
            bird.maxSpeed = baseSpeed * speedMultiplier;
        }
    }
};
```

**Justificación del允许 acceso LECTURA a Transport:**
- **Ondas sincronizadas**: Una escena de ondas puede modular su frecuencia o fase según el beat actual para crear efectos visuales sincronizados con la música
- **Flock relacionado a BPM**: Un sistema de partículas puede ajustar su velocidad según el tempo, creando movimiento más vivo a tempos más altos
- **Visualizaciones de audio**: Cualquier escena que quiera sincronizar elementos visuales con el beat puede hacerlo leyendo el estado

**Lo que NO se permite:**
- Las escenas NO pueden llamar `Transport::get().setBPM()` - eso es responsabilidad del usuario via UI o OSC
- Las escenas NO deben usar Transport para decidir transiciones - eso es trabajo del Scheduler
- Las escenas NO deben subscribe directamente a onBeat/onBar - deben usar el update() con el estado

#### Scheduler Subscribe a Callbacks
```cpp
class SceneScheduler : public IBeatObserver, public IBarObserver {
    void onBeat(int beatNumber) override {
        checkTimeline(beatNumber, currentBar_);
    }
    
    void onBar(int barNumber) override {
        currentBar_ = barNumber;
        checkTimeline(currentBeat_, barNumber);
    }
};
```

#### No Coupling Directo
```
REGLA: Transport es el único que conoce a sus observers.
       SceneEngine y Scheduler NO referencian a Transport directamente.
```

```cpp
// ✅ CORRECTO: Scheduler subscribe a Transport
class SceneScheduler {
    SceneScheduler() {
        Transport::get().subscribeBeat(this);
        Transport::get().subscribeBar(this);
    }
};

// ❌ INCORRECTO: SceneEngine consulta Transport en update()
class SceneEngine {
    void update(float dt) {
        if (Transport::get().getBeat() == 1) {  // POLLING - EVITAR
            // ...
        }
    }
};
```

---

### 5. Criterios para Fallback a Clock Interno

#### Timeout Configuration
```cpp
struct TransportConfig {
    float externalClockTimeout = 5.0f;  // segundos sin clock externo
    float bpmSmoothingFactor = 0.25f;    // para promedio móvil
    bool autoFallback = true;            // activar fallback automático
    float maxBPMDrift = 2.0f;             // diferencia máxima antes de alertar
};
```

#### Fallback Logic
```cpp
class Transport {
    void update(float deltaTime) {
        if (syncMode_ == SyncMode::EXTERNAL_SYNC) {
            float timeSinceLastExternal = ofGetElapsedTimef() - lastExternalClockTime_;
            
            if (timeSinceLastExternal > config_.externalClockTimeout) {
                // Fallback automático
                log_.warn("External clock timeout, switching to internal");
                switchToInternalSync();
            }
        }
        
        if (syncMode_ == SyncMode::INTERNAL_SYNC) {
            internalClock_.update(deltaTime);
        }
    }
    
    void switchToInternalSync() {
        syncMode_ = SyncMode::INTERNAL_SYNC;
        // Mantener último BPM conocido
        log_.info("Using internal clock at " + std::to_string(currentBPM_) + " BPM");
    }
};
```

#### Manual Override
```cpp
class Transport {
    void setSyncMode(SyncMode mode) {
        previousSyncMode_ = syncMode_;
        syncMode_ = mode;
        
        if (mode == SyncMode::INTERNAL_SYNC) {
            // Resetear contador externo
            externalClock_.reset();
        }
    }
    
    void restorePreviousSyncMode() {
        syncMode_ = previousSyncMode_;
    }
};
```

#### Transición Suave entre Fuentes
```cpp
void transitionToExternalClock(float externalBPM) {
    // 1. Registrar BPM externo
    externalClock_.onBPMChange(externalBPM);
    
    // 2. Gradualmente ajustar BPM interno hacia externo
    // (evitar salto brusco)
    float targetBPM = externalBPM;
    float currentBPM = getBPM();
    
    // Interpolación suave (1 segundo)
    float step = (targetBPM - currentBPM) * 0.25f * ofGetLastFrameTime();
    setBPM(currentBPM + step);
}
```

---

### 6. Riesgos de Sincronización

| Riesgo | Descripción | Mitigation |
|--------|-------------|-------------|
| **Clock Drift** | Acumulación de error temporal en clock interno | Comparar con timestamps NTP, resincronizar periódicamente |
| **Jitter** | Variación no predecible en intervalos de beat | Smoothing con moving average, ignorar outliers |
| **Latencia de Red** | Retardo en mensajes OSC entrantes | Compensar con timestamp en mensaje, usar timestamps relativos |
| **Race Conditions** | Múltiples threads actualizando estado | Thread-safe con mutex o atomic operations |
| **Beat Slip** | Pérdida de sincronía tras cambio de BPM | Recalcular phase en cambio de BPM, beat anchor |

```cpp
// Mitigación de Clock Drift
class Transport {
    void syncWithNTP() {
        // O cada 60 segundos, comparar con clock externo
        // y ajustar si drift > THRESHOLD
        if (std::abs(internalElapsed_ - externalTimestamp_) > DRIFT_THRESHOLD) {
            log_.warn("Clock drift detected, adjusting");
            internalElapsed_ = externalTimestamp_;
        }
    }
};

// Mitigación de Jitter
class ExternalClock {
    void onClockTick(int tick) {
        float now = ofGetElapsedTimef();
        float interval = now - lastTickTime_;
        
        // Ignorar intervalos anómalos (> 2x expected o < 0.5x expected)
        float expectedInterval = 60.0f / currentBPM_;
        if (interval > expectedInterval * 2.0f || interval < expectedInterval * 0.5f) {
            log_.debug("Jitter detected, ignoring tick");
            return;
        }
        
        lastTickTime_ = now;
        processTick(tick);
    }
};

// Mitigación de Beat Slip
void onBPMChange(float newBPM) {
    float oldBeatDuration = 60.0f / currentBPM_;
    float newBeatDuration = 60.0f / newBPM;
    
    // Recalcular posición exacta en el beat
    float beatPosition = elapsed_ / oldBeatDuration;
    float newElapsed = beatPosition * newBeatDuration;
    
    elapsed_ = newElapsed;
    currentBPM_ = newBPM;
    
    // Notificar para realinear visualizaciones
    notifyBeatSlipCorrected();
}
```

---

### 7. Recomendaciones para Pruebas

#### Test Suite para Transport

```cpp
class TransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        transport_.reset();
    }
    
    Transport transport_;
};

TEST_F(TransportTest, FixedBPM_BeatCounting) {
    transport_.setBPM(120.0f);
    transport_.play();
    
    // Simular 2 segundos a 120 BPM = 4 beats
    simulateTime(2.0f);
    
    EXPECT_EQ(transport_.getBeat(), 4);
    EXPECT_EQ(transport_.getBar(), 2);  // 4/4 time
}

TEST_F(TransportTest, RuntimeBPMChange) {
    transport_.setBPM(120.0f);
    transport_.play();
    
    // Cambiar BPM en runtime
    transport_.setBPM(180.0f);
    simulateTime(1.0f);
    
    // Verificar que el cambio se refleja correctamente
    EXPECT_NEAR(transport_.getBPM(), 180.0f, 0.1f);
}

TEST_F(TransportTest, InternalToExternalTransition) {
    transport_.setBPM(120.0f);
    transport_.play();
    
    // Simular recepción de clock externo
    transport_.setSyncMode(SyncMode::EXTERNAL_SYNC);
    transport_.onExternalClockTick(1);
    
    EXPECT_EQ(transport_.getSyncMode(), SyncMode::EXTERNAL_SYNC);
}

TEST_F(TransportTest, ExternalTimeoutFallback) {
    transport_.setBPM(120.0f);
    transport_.setSyncMode(SyncMode::EXTERNAL_SYNC);
    
    // Simular timeout
    lastExternalClockTime_ = ofGetElapsedTimef() - 10.0f;  // 10 segundos atrás
    
    transport_.update(0.016f);
    
    EXPECT_EQ(transport_.getSyncMode(), SyncMode::INTERNAL_SYNC);
}

TEST_F(TransportTest, MockTransport_UnitTests) {
    // Para testing de componentes que dependen de Transport
    auto mockTransport = std::make_unique<MockTransport>();
    
    EXPECT_CALL(*mockTransport, getBeat()).WillRepeatedly(testing::Return(1));
    EXPECT_CALL(*mockTransport, subscribeBeat(testing::_));
    
    SceneScheduler scheduler(std::move(mockTransport));
    scheduler.update();
    
    // Verificar que scheduler responde a beat
}
```

#### Test Scenarios Recomendados

| Escenario | Configuración | Validación |
|-----------|---------------|------------|
| BPM Fijo | 120 BPM, INTERNAL_SYNC | Beat/bar counter exacto |
| Cambio BPM Runtime | 120 → 180 BPM | Transición suave, sin beat slip |
| Transición Interno→Externo | Clock OSC externo | Cambio de fuente sin glitches |
| Transición Externo→Interno | Timeout 5 segundos | Fallback automático |
| Timeout Fallback | Sin clock externo > 5s | Respaldo a clock interno |
| Stress Test | 300 BPM sostenidos | Sin pérdida de beats |

---

## Tipos de Activation Mode

### Comparación de Modos

| Aspecto | TIMELINE | TRIGGER | HYBRID |
|---------|----------|---------|--------|
| **Activación** | Automática por beat/bar | Manual por trigger externo | Prepare por timeline, launch manual |
| **Precisión** | Depende de quantization | Inmediata | Exactamente en beat配置的 |
| **Uso típico** | Coreografía fija | Improvisación | Transiciones controladas |
| **Rollback** | Difícil | Fácil | Medio (prepare ≠ launch) |

### Definición de Modos

```cpp
enum class ActivationMode {
    TIMELINE,   // Activación puramente temporal
    TRIGGER,    // Activación puramente por eventos
    HYBRID      // Preparación temporal, lanzamiento manual
};

enum class Quantization {
    NONE,       // Sin cuantización
    BEAT,       // En cada beat
    HALF_BAR,   // Cada 1/2 compás (beat 1, 3)
    BAR,        // Inicio de compás
    TWO_BARS,   // Cada 2 compases
    FOUR_BARS   // Cada 4 compases
};
```

### Transición entre Modos en Runtime

```cpp
class SceneScheduler {
    void setMode(ActivationMode mode) {
        // En modo TIMELINE → TRIGGER: cues timeline se mantienen pero no se ejecutan automáticamente
        // En modo TRIGGER → HYBRID: preparar último cue triggered para launch manual
        // En modo HYBRID → TIMELINE: discard prepared scenes, return to timeline execution
        
        switch (mode) {
            case ActivationMode::TIMELINE:
                cancelAllPrepared();
                break;
            case ActivationMode::TRIGGER:
                clearTimelineSchedule();
                break;
            case ActivationMode::HYBRID:
                // Mantener timeline, solo cambiar dispatch
                break;
        }
        currentMode_ = mode;
    }
};
```

---

## Reglas de Cuantización

### Matriz de Cuantización

| Quantization | Dispara en | Ejemplo (120 BPM) |
|--------------|------------|-------------------|
| **NONE** | Tiempo exacto | Inmediato |
| **BEAT** | Beat 1, 2, 3, 4 | Cada 500ms |
| **HALF_BAR** | Beat 1, 3 | Cada 1000ms |
| **BAR** | Beat 1 | Cada 2000ms |
| **TWO_BARS** | Beat 1 de bar 1, 3, 5... | Cada 4000ms |
| **FOUR_BARS** | Beat 1 de bar 1, 5, 9... | Cada 8000ms |

### Cuantización para Launch vs Prepare

```cpp
struct QuantizationConfig {
    Quantization launchQuantization = Quantization::BAR;  // Para launch (activación)
    Quantization prepareQuantization = Quantization::BEAT; // Para prepare (precarga)
    int beatsAheadToPrepare = 4;  // Preparar 4 beats antes
};

class SceneScheduler {
    void checkTimeline(float currentBeat, int currentBar) {
        auto pendingCues = getPendingCues();
        
        for (auto& cue : pendingCues) {
            // CHECK PREPARE: ¿Debe precargarse?
            if (shouldPrepare(cue, currentBeat, currentBar)) {
                if (cue.mode == ActivationMode::HYBRID) {
                    activator_.prepareScene(cue.sceneId);
                    cue.state = CueState::READY;
                }
            }
            
            // CHECK LAUNCH: ¿Debe activarse?
            if (shouldLaunch(cue, currentBeat, currentBar)) {
                if (cue.mode == ActivationMode::TIMELINE) {
                    activator_.activateScene(cue.sceneId);
                    cue.state = CueState::LAUNCHED;
                }
                // HYBRID: solo launch si hay trigger manual
            }
        }
    }
};
```

---

## Política de Prioridad

### Niveles de Prioridad

```cpp
enum class Priority {
    CRITICAL = 0,  // Siempre se ejecuta, no se puede cancelar
    HIGH = 1,      // Override manual de usuario
    NORMAL = 2,    // Scheduling padrão
    LOW = 3         // Puede ser preempted
};
```

### Reglas de Prioridad

```cpp
class SceneScheduler {
    // Manual override siempre gana a auto
    void onExternalTrigger(const std::string& sceneId) {
        auto manualCue = createManualCue(sceneId);
        manualCue.priority = Priority::HIGH;  // Override manual
        
        // Buscar cues conflictivos
        auto conflicts = findCuesAtSameTime(manualCue.beat, manualCue.bar);
        for (auto& cue : conflicts) {
            if (cue.priority < manualCue.priority) {
                cancelCue(cue.id);  // Cancelar cues de menor prioridad
            }
        }
        
        activator_.activateScene(sceneId);
    }
    
    // Priority inheritance para HYBRID
    void onCuePrepared(const std::string& cueId) {
        auto cue = getCue(cueId);
        if (cue.mode == ActivationMode::HYBRID) {
            // Heredar prioridad del trigger que lo preparó
            if (pendingTrigger_) {
                cue.priority = max(cue.priority, pendingTrigger_->priority);
            }
        }
    }
};
```

### Resolución de Conflictos

| Cue A | Cue B | Resultado |
|-------|-------|-----------|
| CRITICAL | Cualquiera | A ejecuta, B cancelado |
| HIGH | NORMAL | HIGH ejecuta, NORMAL cancelado |
| HIGH | HIGH | Ambos se evalúan (último agregado gana) |
| NORMAL | NORMAL | Ejecución secuencial por orden de adición |
| LOW | Cualquiera | LOW puede ser preempted |

---

## Flujo de Activación Detallado

### Diagrama de Secuencia

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│MappingLayer  │    │  Transport   │    │   Scheduler  │    │  Activator   │
└──────┬───────┘    └──────┬───────┘    └──────┬───────┘    └──────┬───────┘
       │                  │                  │                  │
       │ onInput()        │                  │                  │
       │───────────────▶  │                  │                  │
       │                  │                  │                  │
       │ onTrigger(sceneId)                  │                  │
       │─────────────────────────────────▶  │                  │
       │                  │                  │                  │
       │                  │ onBeat(beat)     │                  │
       │                  │─────────────────▶│                  │
       │                  │                  │                  │
       │                  │ onBar(bar)       │                  │
       │                  │─────────────────▶│                  │
       │                  │                  │                  │
       │                  │                  │ checkTimeline()  │
       │                  │                  │───────────────▶  │
       │                  │                  │                  │ activateScene()
       │                  │                  │◀─────────────────│─────────────▶
       │                  │                  │                  │ deactivate(old)
       │                  │                  │◀─────────────────│─────────────▶
       │                  │                  │                  │ activate(new)
       │                  │                  │◀─────────────────│─────────────▶
       │                  │                  │                  │    (complete)
```

### Flujo Completo

```cpp
// MappingLayer → Scheduler (NO directo a escena)
class MappingLayer {
    void onTrigger(const std::string& sceneId, TriggerSource source) {
        // En TRIGGER y HYBRID mode, delegar al scheduler
        auto scheduler = getScheduler();
        scheduler->onExternalTrigger(sceneId, source);
    }
};

// Transport beat callback → Scheduler
class SceneScheduler : public IBeatObserver, public IBarObserver {
    void onBeat(int beatNumber) override {
        checkTimeline(beatNumber, currentBar_);
    }
    
    void onBar(int barNumber) override {
        currentBar_ = barNumber;
        checkTimeline(currentBeat_, barNumber);
    }
    
    void checkTimeline(int beat, int bar) {
        // 1. Buscar cues que coincidan con beat/bar actual
        // 2. Evaluar quantization
        // 3. Verificar prioridad
        // 4. Dispatch a ISceneActivator
    }
};

// Scheduler → ISceneActivator → SceneEngine
class SceneScheduler {
    void dispatchActivation(const SceneCue& cue) {
        if (cue.mode == ActivationMode::HYBRID) {
            // Solo preparar, no lanzar
            activator_.prepareScene(cue.sceneId);
        } else {
            // TIMELINE: activación directa
            activator_.activateScene(cue.sceneId);
        }
    }
};
```

---

## Contrato ISceneActivator (Expandido)

```cpp
class ISceneActivator {
public:
    virtual ~ISceneActivator() = default;
    
    // Activación completa: deactivate old → activate new
    // Retorna true si éxito, false si escena no existe
    virtual bool activateScene(const std::string& sceneId) = 0;
    
    // Desactivación explícita
    virtual void deactivateScene(const std::string& sceneId) = 0;
    
    // Preparación para modo HYBRID: pre-load de recursos
    // No activa renderizado, solo precarga en memoria
    virtual void prepareScene(const std::string& sceneId) = 0;
    
    // Cancelar preparación híbrida (si no se lanzó)
    virtual void cancelPreparation(const std::string& sceneId) = 0;
    
    // Estado
    virtual std::string getActiveSceneId() const = 0;
    virtual bool hasPreparedScene(const std::string& sceneId) const = 0;
    virtual std::optional<std::string> getPreparedSceneId() const = 0;
};

// Implementación en SceneEngine
class SceneEngine : public ISceneActivator {
public:
    bool activateScene(const std::string& sceneId) override {
        // 1. Deactivate escena actual si existe
        if (auto active = getActiveScene()) {
            active->deactivate();
        }
        
        // 2. Verificar si está preparada (HYBRID) o cargar
        auto scene = loadOrGetPrepared(sceneId);
        if (!scene) return false;
        
        // 3. Activate nueva escena
        scene->activate();
        activeSceneId_ = sceneId;
        return true;
    }
    
    void prepareScene(const std::string& sceneId) override {
        // Pre-cargar recursos sin activar
        cancelPreparation();  // Limpiar preparación anterior
        
        auto scene = loadScene(sceneId);
        if (scene) {
            scene->setup();  // Setup sin activate
            preparedSceneId_ = sceneId;
        }
    }
    
    void cancelPreparation(const std::string& sceneId) override {
        if (preparedSceneId_ == sceneId) {
            // Limpiar recursos precargados
            releaseScene(sceneId);
            preparedSceneId_.reset();
        }
    }
};
```

---

## Contrato IBaseScene - Interfaz Completa

La interfaz `IBaseScene` define el contrato que todas las escenas deben cumplir. Esto garantiza el desacoplamiento de escenas respecto a hardware (OSC, Keyboard, Kinect) y asegura que solo se comuniquen mediante ParameterRegistry.

```cpp
class IBaseScene {
public:
    virtual ~IBaseScene() = default;
    
    // ==================== Ciclo de Vida ====================
    
    // Setup: se llama una vez al cargar la escena
    // Aquí se registran los parámetros específicos de la escena
    virtual void setup() = 0;
    
    // Update: se llama cada frame con deltaTime en segundos
    // Aquí se actualiza la lógica de la escena (animaciones, física, etc.)
    virtual void update(float deltaTime) = 0;
    
    // Draw: se llama cada frame para renderizar
    // Aquí se dibuja el contenido visual de la escena
    virtual void draw() = 0;
    
    // Activate: se llama cuando la escena se vuelve activa
    // Preparar recursos, inicializar estado visual
    virtual void activate() = 0;
    
    // Deactivate: se llama cuando la escena deja de ser activa
    // Liberar recursos que no sean necesarios cuando inactiva
    virtual void deactivate() = 0;
    
    // ==================== Identificación ====================
    
    // ID único de la escena (debe coincidir con el registro en SceneRegistry)
    virtual std::string getSceneId() const = 0;
    
    // Nombre legible para UI
    virtual std::string getDisplayName() const = 0;
    
    // ==================== Metadatos ====================
    
    // Sugiere qué mappings podrían ser útiles para esta escena
    // El MappingLayer usa esto para sugerir al usuario
    virtual std::vector<std::string> getSuggestedMappings() const = 0;
    
    // Retorna el preset por defecto para esta escena
    // Puede ser vacío si no hay defaults
    virtual std::optional<ScenePreset> getDefaultPreset() const = 0;
    
    // Metadatos adicionales (categoría, tags, descripción)
    virtual SceneMetadata getSceneMetadata() const = 0;
    
    // ==================== Callbacks Opcionales de Sincronización ====================
    
    // Se llama en cada beat (opcional - implementar solo si necesita sync)
    // La implementación debe ser ligera - no blocking operations
    virtual void onBeat(int beatNumber) {
        // Default: no-op
    }
    
    // Se llama en cada compás (opcional)
    virtual void onBar(int barNumber) {
        // Default: no-op
    }
    
    // ==================== Validación de Estado ====================
    
    // Verifica que el estado interno de la escena sea válido
    // Retorna true si la escena está lista para renderizar
    virtual bool isValid() const = 0;
    
    // Si isValid() retorna false, esto describe el problema
    virtual std::string getInvalidReason() const = 0;
};

// ==================== Estructuras de Soporte ====================

struct SceneMetadata {
    std::string category;        // e.g., "Generative", "Particle", "Video"
    std::vector<std::string> tags;  // e.g., {"noise", "color", "fluid"}
    std::string description;    // Descripción corta para UI
    std::string author;          // Opcional
    std::string version;         // e.g., "1.0.0"
    
    // Sugiere complejidad (para UI - mostrar warning en escenas pesadas)
    enum class Complexity { LOW, MEDIUM, HIGH };
    Complexity complexity = Complexity::MEDIUM;
};

struct ScenePreset {
    std::string presetId;
    std::string name;
    std::unordered_map<std::string, float> paramValues;  // paramId → value
};
```

### Ejemplo de Implementación Completa de una Escena

```cpp
class PinturaFluorescenteScene : public IBaseScene {
public:
    // ==================== Ciclo de Vida ====================
    
    void setup() override {
        auto& registry = ParameterRegistry::get();
        
        // Registrar parámetros específicos de esta escena
        registry.registerParam({
            .id = "pinturaFluorescente.k",
            .label = "Noise Amount",
            .type = ParamType::FLOAT,
            .scope = ParamScope::SCENE,
            .sceneId = "pinturaFluorescente",
            .min = 0.0f,
            .max = 100.0f,
            .defaultValue = 50.0f,
            .step = 1.0f,
            .group = "Animation"
        });
        
        registry.registerParam({
            .id = "pinturaFluorescente.speed",
            .label = "Speed",
            .type = ParamType::FLOAT,
            .scope = ParamScope::SCENE,
            .sceneId = "pinturaFluorescente",
            .min = 0.0f,
            .max = 5.0f,
            .defaultValue = 1.0f,
            .group = "Animation"
        });
        
        registry.registerParam({
            .id = "pinturaFluorescente.glowEnabled",
            .label = "Glow",
            .type = ParamType::BOOL,
            .scope = ParamScope::SCENE,
            .sceneId = "pinturaFluorescente",
            .defaultValue = true,
            .group = "Rendering"
        });
        
        // Estado inicial válido
        isValid_ = true;
    }
    
    void update(float deltaTime) override {
        // Leer parámetros de Registry
        auto k = ParameterRegistry::get().getParam("pinturaFluorescente.k");
        auto speed = ParameterRegistry::get().getParam("pinturaFluorescente.speed");
        auto glow = ParameterRegistry::get().getParam("pinturaFluorescente.glowEnabled");
        
        // Actualizar lógica de animación
        time_ += deltaTime * speed.value_or(1.0f);
        
        // Validar estado
        if (k.value_or(0) < 0 || k.value_or(0) > 100) {
            isValid_ = false;
            invalidReason_ = "k parameter out of range";
        } else {
            isValid_ = true;
            invalidReason_ = "";
        }
    }
    
    void draw() override {
        if (!isValid_) return;
        
        auto k = ParameterRegistry::get().getParam("pinturaFluorescente.k").value_or(50.0f);
        auto glow = ParameterRegistry::get().getParam("pinturaFluorescente.glowEnabled").value_or(true);
        
        // Renderizar con shaders
        shader_.begin();
        shader_.setUniform1f("k", k / 100.0f);
        shader_.setUniform1i("glowEnabled", glow ? 1 : 0);
        
        ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
        
        shader_.end();
    }
    
    void activate() override {
        // Inicializar recursos cuando se activa
        shader_.load("shaders/pinturaFluorescente");
    }
    
    void deactivate() override {
        // Limpiar cuando se desactiva
    }
    
    // ==================== Identificación ====================
    
    std::string getSceneId() const override {
        return "pinturaFluorescente";
    }
    
    std::string getDisplayName() const override {
        return "Pintura Fluorescente";
    }
    
    // ==================== Metadatos ====================
    
    std::vector<std::string> getSuggestedMappings() const override {
        return {
            "osc.fader.1 → pinturaFluorescente.k",
            "osc.fader.2 → pinturaFluorescente.speed",
            "osc.toggle.1 → pinturaFluorescente.glowEnabled"
        };
    }
    
    std::optional<ScenePreset> getDefaultPreset() const override {
        return ScenePreset{
            .presetId = "default",
            .name = "Default",
            .paramValues = {
                {"pinturaFluorescente.k", 50.0f},
                {"pinturaFluorescente.speed", 1.0f},
                {"pinturaFluorescente.glowEnabled", 1.0f}
            }
        };
    }
    
    SceneMetadata getSceneMetadata() const override {
        return {
            .category = "Generative",
            .tags = {"noise", "shader", "color"},
            .description = "Generative painting with fluorescent colors and noise-based flow",
            .author = "Core Team",
            .version = "1.0.0",
            .complexity = SceneMetadata::Complexity::MEDIUM
        };
    }
    
    // ==================== Callbacks de Sincronización ====================
    
    void onBeat(int beatNumber) override {
        // Ejemplo: flash en beat 1
        if (beatNumber == 1) {
            flashIntensity_ = 1.0f;
        }
    }
    
    void onBar(int barNumber) override {
        // Ejemplo: cambiar color cada 4 compases
        if (barNumber % 4 == 0) {
            currentPaletteIndex_ = (currentPaletteIndex_ + 1) % palettes_.size();
        }
    }
    
    // ==================== Validación ====================
    
    bool isValid() const override {
        return isValid_;
    }
    
    std::string getInvalidReason() const override {
        return invalidReason_;
    }

private:
    ofShader shader_;
    float time_ = 0.0f;
    float flashIntensity_ = 0.0f;
    int currentPaletteIndex_ = 0;
    std::vector<std::vector<ofColor>> palettes_;
    bool isValid_ = false;
    std::string invalidReason_;
};
```

### Notas sobre Implementación

1. **Parámetros se leen del Registry**: La escena NUNCA recibe parámetros directamente de adaptadores. Siempre usa `ParameterRegistry::get().getParam()`.

2. **Suscripción a onBeat/onBar**: Si la escena implementa estos métodos, el SceneScheduler la añadirá a la lista de observers. IMPORTANTE: la implementación debe ser ligera (sin operaciones blocking).

3. **isValid()**: Se verifica en cada update. Si retorna false, la escena no se renderiza y se muestra el motivo en UI.

4. **getSuggestedMappings()**: El MappingLayer usa esto para sugerir al usuario qué crear cuando registra esta escena.

---

## Riesgos y Edge Cases

### Matriz de Riesgos

| Riesgo | Likelihood | Impacto | Mitigation | Estado |
|--------|-----------|---------|------------|--------|
| **Cue colisiones en mismo beat** | Med | Alto | Priority levels, orden de ejecución | 🔲 Pendiente |
| **Escena activa no existe** | Low | Med | Validación en addCue(), fallback UI | 🔲 Pendiente |
| **Scheduler sin Transport** | Low | Alto | Fail-fast en init, status check | 🔲 Pendiente |
| **Modo híbrido: prepare sin launch** | Med | Low | Timeout cleanup, memoria de prepared | 🔲 Pendiente |
| **Memoria de cues completados** | Med | Low | Circular buffer, cleanup automático | 🔲 Pendiente |

### Detalle de Edge Cases

```cpp
// Edge Case 1: Cue colisiones en mismo beat
void resolveCueCollision(const std::vector<SceneCue>& cuesAtBeat) {
    if (cuesAtBeat.empty()) return;
    
    // Ordenar por prioridad descendente
    std::vector<SceneCue> sorted = cuesAtBeat;
    std::sort(sorted.begin(), sorted.end(), 
        [](auto& a, auto& b) { return a.priority < b.priority; });
    
    // CRITICAL siempre gana
    auto critical = std::find_if(sorted.begin(), sorted.end(),
        [](auto& c) { return c.priority == Priority::CRITICAL; });
    if (critical != sorted.end()) {
        executeOnly(*critical);
        return;
    }
    
    // HIGH: ejecutar todos con crossfade
    executeAll(sorted);
}

// Edge Case 2: Escena no existe
bool validateCueBeforeAdd(const SceneCue& cue) {
    auto available = sceneRegistry_.getAvailableScenes();
    if (std::find(available.begin(), available.end(), cue.sceneId) == available.end()) {
        log_.warn("Cue references non-existent scene: " + cue.sceneId);
        return false;  // O throw exception
    }
    return true;
}

// Edge Case 3: Scheduler sin Transport
void SceneScheduler::ensureTransportConnected() {
    if (!transport_) {
        throw std::runtime_error("Scheduler requires Transport connection");
    }
}

// Edge Case 4: Hybrid prepare sin launch
class HybridTimeoutManager {
    static constexpr float PREPARE_TIMEOUT = 30.0f;  // segundos
    
    void update(float deltaTime) {
        for (auto& [cueId, preparedTime] : preparedCues_) {
            preparedTime += deltaTime;
            if (preparedTime > PREPARE_TIMEOUT) {
                // Timeout: cleanup automático
                cancelPreparation(cueId);
                log_.warn("Hybrid prepare timeout: " + cueId);
            }
        }
    }
};

// Edge Case 5: Memoria de cues completados
class CompletedCueMemory {
    static constexpr size_t MAX_COMPLETED = 100;
    
    void add(const SceneCue& cue) {
        completedCues_.push_back(cue);
        if (completedCues_.size() > MAX_COMPLETED) {
            completedCues_.pop_front();  // Circular buffer
        }
    }
    
    // Útil para debugging y undo
    std::vector<SceneCue> getLastN(size_t n) const;
    void undoLast();  // Rollback al estado anterior
};
```

---

## OSC Adapter - Detalle de Implementación

### 1. Esquema de Direcciones OSC

El adapter soporta el estándar TouchOSC con prefijo configurable:

| Categoría | Dirección Base | Rango | Tipo de Valor |
|-----------|----------------|-------|---------------|
| **Faders** | `/1/fader/` | 1-10 | float 0.0-1.0 |
| **Toggles** | `/1/toggle/` | 1-10 | int 0/1 |
| **Push Buttons** | `/1/push/` | 1-10 | int 0/1 |
| **Transport BPM** | `/bpm` | - | float 20-300 |
| **Transport Play** | `/play` | - | int 0/1 |
| **Transport Stop** | `/stop` | - | int 0/1 |
| **Transport Record** | `/record` | - | int 0/1 |
| **XY Pad X** | `/1/x` | - | float 0.0-1.0 |
| **XY Pad Y** | `/1/y` | - | float 0.0-1.0 |
| **Multiplexores** | `/1/multi/` | 1-4 | int 0-3 |

**Configuración del prefijo:**

```cpp
class OSCAdapter : public IOSCAdapter {
public:
    void setPrefix(const std::string& prefix) {
        prefix_ = prefix;  // Por defecto: "1" para TouchOSC
        rebuildAddressMap();  // Recachear regex patterns
    }
    
    std::string prefix_ = "1";  // No hardcodeado
    
private:
    // Compila regex patterns para validación rápida
    void rebuildAddressMap();
};
```

**Mapeo de direcciones a channels:**

```
/1/fader/1   → channel 1
/1/fader/2   → channel 2
...
/1/fader/10  → channel 10

/1/toggle/1  → channel 101  (100 + index para distinguir de faders)
/1/push/1    → channel 201  (200 + index)

/1/x         → channel 301  (vec2 X component)
/1/y         → channel 302  (vec2 Y component)

/1/multi/1   → channel 401  (multiplexor 1)
```

---

### 2. Modelo de Datos para Controles OSC

**Estructura OSCMessage:**

```cpp
struct OSCMessage {
    std::string address;      // e.g., "/1/fader/1"
    std::string typeTags;    // e.g., "f" para float, "i" para int
    std::vector<std::variant<float, int, std::string>> values;
    
    // Helpers
    float getFloat(size_t index = 0) const;
    int getInt(size_t index = 0) const;
    bool isValid() const;
};
```

**Channel Mapping:**

```cpp
class OSCChannelMapper {
public:
    // Convierte dirección OSC a channel lógico
    int addressToChannel(const std::string& address) const;
    
    // Detecta si es componente de vec2
    bool isVec2Component(const std::string& address) const;
    int getVec2Id(const std::string& address) const;  // /1/x → id 1
    
private:
    // Regex patterns precompilados
    std::regex faderPattern_ = R"(^/(\d+)/fader/(\d+)$)";
    std::regex togglePattern_ = R"(^/(\d+)/toggle/(\d+)$)";
    std::regex pushPattern_   = R"(^/(\d+)/push/(\d+)$)";
    std::regex xyPattern_    = R"(^/(\d+)/([xy])$)";
    std::regex multiPattern_ = R"(^/(\d+)/multi/(\d+)$)";
    std::regex transportPattern_ = R"(^/(bpm|play|stop|record)$)";
};
```

**Value Normalization:**

```cpp
struct NormalizedValue {
    float rawValue;           // Valor original del mensaje OSC
    float normalizedValue;   // 0.0-1.0 para faders, 0/1 para botones
    bool isValid;
    std::string error;       // Si isValid == false
    
    // Factory method con validación
    static NormalizedValue fromOSC(const OSCMessage& msg, ParamType expectedType);
};

class ValueNormalizer {
public:
    // Faders: 0.0-1.0 tal cual
    static float normalizeFader(float value) {
        return std::clamp(value, 0.0f, 1.0f);
    }
    
    // Botones: cualquier valor no-cero → 1
    static int normalizeButton(float value) {
        return value > 0.5f ? 1 : 0;
    }
    
    // XY: clamping explícito
    static float normalizeXY(float value) {
        return std::clamp(value, 0.0f, 1.0f);
    }
    
    // Multiplexor: 0-3 con clamp
    static int normalizeMultiplexor(float value) {
        return static_cast<int>(std::clamp(value, 0.0f, 3.0f));
    }
};
```

**Raw vs Normalized:**

```cpp
// El adapter mantiene ambos valores para debugging
struct OSCInputEvent {
    std::string address;
    float rawValue;        // Lo que llegó del wire
    float normalizedValue; // Transformado según tipo de control
    uint64_t timestamp;    // Timestamp del sistema
    
    std::string toDebugString() const;
};

// Ejemplo de logging:
// /1/fader/5: raw=0.7531 → normalized=0.7531 (fader)
// /1/toggle/2: raw=1.0000 → normalized=1 (button)
// /1/multi/1: raw=2.7000 → normalized=2 (multiplexor, clamped)
```

---

### 3. Flujo de Mensajes de Entrada

**Diagrama de flujo:**

```
┌─────────────────┐      ┌─────────────────┐      ┌─────────────────┐
│  ofxOscReceiver │      │   OSCAdapter    │      │   MappingLayer  │
│    (OF thread)  │      │                 │      │                 │
└────────┬────────┘      └────────┬────────┘      └────────┬────────┘
         │                        │                        │
         │ hasWaitingMessage()   │                        │
         │◀─────────────────────│                        │
         │                        │                        │
         │ getNextMessage(msg)   │                        │
         │──────────────────────▶│                        │
         │                        │                        │
         │                ┌──────┴──────┐               │
         │                │ parseMessage │               │
         │                │    (msg)     │               │
         │                └──────┬───────┘               │
         │                        │                        │
         │                ┌──────┴──────┐               │
         │                │ validate()   │               │
         │                │ - address    │               │
         │                │ - type tags  │               │
         │                │ - value      │               │
         │                └──────┬───────┘               │
         │                        │                        │
         │           ┌────────────┼────────────┐         │
         │           ▼            ▼            ▼         │
         │    ┌──────────┐ ┌──────────┐ ┌──────────┐   │
         │    │ Transport │ │ Mapping   │ │  Unknown  │   │
         │    │  (BPM,    │ │  Layer    │ │  address  │   │
         │    │ play,     │ │ (faders,  │ │ (log+ign) │   │
         │    │  stop)    │ │  toggles) │ │           │   │
         │    └────┬─────┘ └────┬─────┘ └─────┬─────┘   │
         │         │            │             │          │
         │         ▼            ▼             │          │
         │    ┌──────────┐ ┌──────────┐       │          │
         │    │Transport │ │ onInput()│◀──────┘          │
         │    │ .setBPM()│ │ (source,  │                   │
         │    │ .play()  │ │  channel, │                   │
         │    │ .stop()  │ │  value)   │                   │
         │    └──────────┘ └──────────┘                   │
```

**Thread Safety:**

```cpp
class OSCAdapter : public IOSCAdapter {
public:
    // Called from OF main thread (update loop)
    void update() {
        while (receiver_.hasWaitingMessage()) {
            ofxOscMessage msg;
            receiver_.getNextMessage(msg);
            
            // Procesamiento síncrono en el mismo thread
            // NO hay locking dentro del parse porque es single-threaded
            processMessage(msg);
        }
    }
    
private:
    // El ofxOscReceiver NO es thread-safe por defecto
    // Solo se llama desde el thread principal de OF
    // Esto elimina la necesidad de mutexes
    
    void processMessage(const ofxOscMessage& msg) {
        auto parsed = parseMessage(msg);
        if (!parsed.isValid) {
            stats_.errorCount++;
            return;
        }
        
        // Routing basado en dirección
        if (isTransportMessage(parsed.address)) {
            handleTransportMessage(parsed);
        } else if (isParameterMessage(parsed.address)) {
            handleParameterMessage(parsed);  // → MappingLayer.onInput()
        } else {
            log_.debug("Unknown address: " + parsed.address);
        }
    }
};
```

**Nota importante:** El flujo corre en el thread principal de OpenFrameworks (dentro de `update()`), por lo que no se necesita sincronización. El `ofxOscReceiver` debe configurarse en el thread donde se llama `update()`.

---

### 4. Conexión con Mapping Layer

**Principio: Adapter conocimiento mínimo del mapping:**

```cpp
// OSCAdapter NO conoce los mapeos específicos
// Solo sabe que viene de "osc" y un channel number

class OSCAdapter : public IOSCAdapter {
public:
    void setMappingLayer(IMappingLayer* mappingLayer) {
        mappingLayer_ = mappingLayer;
    }
    
private:
    void handleParameterMessage(const ParsedOSCMessage& msg) {
        // Solo transfiere datos, sin conocimiento de qué parámetro es
        if (mappingLayer_) {
            mappingLayer_->onInput("osc", msg.channel, msg.normalizedValue);
        }
    }
    
    IMappingLayer* mappingLayer_ = nullptr;
};

// MappingLayer resuelve el mapeo completo
class MappingLayer : public IMappingLayer {
public:
    void onInput(const std::string& source, int channel, float value) override {
        // source = "osc", "keyboard", o "kinect"
        // channel = número del control OSC
        
        auto binding = resolve(source, channel);
        if (!binding) {
            return;  // No hay mapeo configurado
        }
        
        auto transformed = transform(value, binding->transform);
        registry_.setParam(binding->paramId, transformed);
    }
    
    // Ejemplo de resolución:
    // source="osc", channel=1 → busca binding con source="osc", channel=1
    // Binding: { source: "osc", channel: 1, paramId: "scene.color.r", transform: "direct" }
};
```

**Ejemplo de flujo completo:**

```
Mensaje OSC: /1/fader/1 → value 0.75
        │
        ▼
OSCAdapter.parseMessage() → channel=1, normalized=0.75
        │
        ▼
OSCAdapter.handleParameterMessage("osc", 1, 0.75)
        │
        ▼
MappingLayer.onInput("osc", 1, 0.75)
        │
        ▼
MappingLayer.resolve("osc", 1) → encuentra binding: osc.channel.1 → scene.color.r
        │
        ▼
MappingLayer.transform(0.75, "direct") → 0.75
        │
        ▼
ParameterRegistry.setParam("scene.color.r", 0.75)
        │
        ▼
Observers notificados → Scene actualiza color
```

**JSON de ejemplo para bindings:**

```json
{
  "mappings": [
    {
      "id": "osc-fader-1-to-color-r",
      "source": "osc",
      "channel": 1,
      "paramId": "scene.color.r",
      "transform": "direct"
    },
    {
      "id": "osc-fader-2-to-color-g", 
      "source": "osc",
      "channel": 2,
      "paramId": "scene.color.g",
      "transform": "direct"
    },
    {
      "id": "osc-toggle-1-to-glow",
      "source": "osc",
      "channel": 101,
      "paramId": "scene.glow.enabled",
      "transform": "direct"
    }
  ]
}
```

---

### 5. Conexión con Transport Engine

**Mensajes de transport tratados como privilegiado:**

```cpp
class OSCAdapter : public IOSCAdapter {
private:
    // Direcciones de transport que van directas a Transport (no a MappingLayer)
    static constexpr std::array<const char*, 4> TRANSPORT_ADDRESSES = {
        "/bpm", "/play", "/stop", "/record"
    };
    
    void handleTransportMessage(const ParsedOSCMessage& msg) {
        if (!transport_) return;
        
        if (msg.address == "/bpm") {
            auto bpm = std::clamp(msg.normalizedValue * 300.0f, 20.0f, 300.0f);
            transport_->setBPM(bpm);
            stats_.lastBPM = bpm;
        }
        else if (msg.address == "/play") {
            if (msg.intValue == 1) {
                transport_->play();
            }
        }
        else if (msg.address == "/stop") {
            if (msg.intValue == 1) {
                transport_->stop();
            }
        }
        else if (msg.address == "/record") {
            // Feature futuro: recording de automatización
            transport_->setRecording(msg.intValue == 1);
        }
    }
    
    ITransport* transport_ = nullptr;
};
```

**Por qué Transport es privilegiado:**

| Aspecto | Detalle |
|---------|---------|
| **Latencia** | BPM/transport debe ser inmediato, no pasar por resolución de mapping |
| **Prioridad** | Si el mapping falla, transport debe seguir funcionando |
| **Simplicidad** | No tiene sentido configurar "mapeo" para play/stop |
| **Seguridad** | Evitar que un mapping mal configurado bloquee el transport |

---

### 6. Validaciones y Manejo de Errores

**Tabla de errores y respuestas:**

| Error | Detección | Respuesta | Recovery |
|-------|------------|------------|----------|
| **Puerto inválido** | `receiver_.setup(port)` retorna false | Log error, intentar puerto +100 | Retry automático 3 veces |
| **Puerto en uso** | `EADDRINUSE` en setup | Notificar al usuario | UI muestra warning |
| **Mensaje malformado** | `typeTags` no coincide con values | Log warning, skip mensaje | Continuar con siguiente |
| **Valor fuera de rango** | value < 0.0 o > 1.0 | Clamp y log debug | Usar valor clampado |
| **Unknown address** | No match en regex patterns | Log debug (INFO level) | Ignorar silenciosamente |
| **Timeout keepalive** | Sin mensajes en 30 segundos | Log warning, mantener conexión | Monitor de conexión |

**Implementación de validaciones:**

```cpp
class OSCAdapter::Validator {
public:
    struct ValidationResult {
        bool isValid;
        std::string error;
        float clampedValue;
    };
    
    ValidationResult validateMessage(const ofxOscMessage& msg) const {
        // 1. Verificar que tiene address
        if (msg.getAddress().empty()) {
            return {false, "Empty address", 0.0f};
        }
        
        // 2. Verificar type tags匹配 values
        if (!validateTypeTagsMatchValues(msg)) {
            return {false, "TypeTags/Values mismatch", 0.0f};
        }
        
        // 3. Validar valor según tipo de control
        auto value = msg.getArgAsFloat(0);
        auto clamped = clampByAddress(msg.getAddress(), value);
        
        return {true, "", clamped};
    }
    
private:
    float clampByAddress(const std::string& address, float value) {
        if (address.find("/fader") != std::string::npos) {
            return std::clamp(value, 0.0f, 1.0f);
        }
        if (address.find("/bpm") != std::string::npos) {
            return std::clamp(value * 300.0f, 20.0f, 300.0f);
        }
        if (address.find("/toggle") != std::string::npos) {
            return value > 0.5f ? 1.0f : 0.0f;
        }
        return value;  // Sin clamping para otros
    }
};
```

**Reconnect con retry:**

```cpp
class ConnectionManager {
public:
    bool connect(int port) {
        for (int attempt = 0; attempt < maxRetries_; attempt++) {
            if (receiver_.setup(port)) {
                log_.info("Connected to OSC port " + std::to_string(port));
                return true;
            }
            log_.warn("Connection attempt " + std::to_string(attempt + 1) + " failed");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        log_.error("Failed to connect after " + std::to_string(maxRetries_) + " attempts");
        // Fallback: intentar puerto alternativo (port + 1000 para debug)
        return receiver_.setup(port + 1000);
    }
    
    void checkConnection() {
        if (!lastMessageTime_.has_value()) return;
        
        auto elapsed = now() - *lastMessageTime_;
        if (elapsed > keepaliveTimeout_) {
            log_.warn("OSC keepalive timeout: " + std::to_string(elapsed.count()) + "s");
            // No desconectar automáticamente, solo loguear
        }
    }
    
private:
    static constexpr int maxRetries_ = 3;
    static constexpr auto keepaliveTimeout_ = std::chrono::seconds(30);
};
```

---

### 7. Trazabilidad y Debugging

**Logging de todos los mensajes (debug level):**

```cpp
class OSCAdapter::Debugger {
public:
    void logMessage(const ParsedOSCMessage& msg) {
        log_.debug("[OSC] {} → raw: {:.4f}, normalized: {:.4f}, ch: {}",
            msg.address, msg.rawValue, msg.normalizedValue, msg.channel);
    }
    
    // Historial para UI
    void addToHistory(const ParsedOSCMessage& msg) {
        history_.push_back({msg.address, msg.normalizedValue, now()});
        if (history_.size() > maxHistory_) {
            history_.pop_front();
        }
    }
    
    std::deque<HistoryEntry> getLastMessages(size_t count) const;
    
private:
    static constexpr size_t maxHistory_ = 100;
    std::deque<HistoryEntry> history_;
};

struct HistoryEntry {
    std::string address;
    float value;
    std::chrono::system_clock::time_point timestamp;
};
```

**Stats para monitoreo:**

```cpp
struct OSCStats {
    uint64_t messageCount = 0;      // Total de mensajes recibidos
    uint64_t errorCount = 0;        // Mensajes con error
    float lastBPM = 0.0f;           // Último BPM recibido
    std::chrono::system_clock::time_point lastMessage;
    bool isConnected = false;
    int currentPort = 0;
    
    // Helpers
    float getErrorRate() const {
        if (messageCount == 0) return 0.0f;
        return static_cast<float>(errorCount) / messageCount;
    }
};
```

**UI para ver últimos mensajes:**

```cpp
class OSCAdapterUI {
public:
    void draw() {
        // Panel de stats
        ImGui::Begin("OSC Debug");
        
        ImGui::Text("Messages: %llu", stats_.messageCount);
        ImGui::Text("Errors: %llu (%.1f%%)", stats_.errorCount, stats_.getErrorRate() * 100);
        ImGui::Text("Last BPM: %.1f", stats_.lastBPM);
        ImGui::Text("Port: %d", stats_.currentPort);
        ImGui::Separator();
        
        // Lista de últimos mensajes
        if (ImGui::CollapsibleHeader("Recent Messages")) {
            auto recent = debugger_.getLastMessages(20);
            for (const auto& msg : recent) {
                auto timeStr = formatTime(msg.timestamp);
                ImGui::Text("[%s] %s: %.3f", timeStr.c_str(), 
                    msg.address.c_str(), msg.value);
            }
        }
        
        ImGui::End();
    }
};
```

---

### 8. Feedback Futuro (Opcional)

**Envío de feedback a TouchOSC:**

```cpp
class OSCAdapter::FeedbackManager {
public:
    void setEnabled(bool enabled) {
        enableFeedback_ = enabled;
    }
    
    // Throttling para evitar flooding
    void sendBeatFeedback(int beat) {
        if (!enableFeedback_) return;
        
        // Max 1 mensaje cada 50ms
        auto now = std::chrono::steady_clock::now();
        if (now - lastFeedback_ < std::chrono::milliseconds(50)) {
            return;
        }
        
        // Solo enviar si el cliente configuró接收
        if (sender_) {
            sender_->sendMsg("/feedback/beat", beat);
            lastFeedback_ = now;
        }
    }
    
    void sendBarFeedback(int bar) {
        if (!enableFeedback_) return;
        
        // Throttling: max 1 mensaje cada 100ms para bars
        auto now = std::chrono::steady_clock::now();
        if (now - lastBarFeedback_ < std::chrono::milliseconds(100)) {
            return;
        }
        
        if (sender_) {
            sender_->sendMsg("/feedback/bar", bar);
            lastBarFeedback_ = now;
        }
    }
    
    // Sync de parámetros (para displays en TouchOSC)
    void syncParameterFeedback(const std::string& paramId, float value) {
        // Solo para parámetros que el usuario decidió "mostrar en OSC"
        if (!feedbackParams_.count(paramId)) return;
        
        auto address = feedbackParams_[paramId];  // e.g., "/1/faderoutput/1"
        sender_->sendMsg(address, value);
    }
    
private:
    bool enableFeedback_ = false;
    ofxOscSender* sender_ = nullptr;
    std::unordered_map<std::string, std::string> feedbackParams_;  // paramId → OSC address
    std::chrono::steady_clock::time_point lastFeedback_;
    std::chrono::steady_clock::time_point lastBarFeedback_;
};
```

**Configuración de feedback:**

```cpp
// JSON config para feedback
{
  "osc": {
    "receivePort": 9000,
    "sendPort": 9001,  // Para feedback
    "prefix": "1",
    "feedback": {
      "enabled": false,
      "beat": true,
      "bar": true,
      "parameters": [
        {"paramId": "scene.color.r", "address": "/1/faderoutput/1"},
        {"paramId": "scene.color.g", "address": "/1/faderoutput/2"},
        {"paramId": "scene.color.b", "address": "/1/faderoutput/3"}
      ]
    }
  }
}
```

**Integración con Transport para feedback:**

```cpp
class TransportFeedbackBridge : public IBeatObserver, public IBarObserver {
public:
    void setOSCAdapter(OSCAdapter* adapter) {
        adapter_ = adapter;
    }
    
    void onBeat(int beat) override {
        adapter_->sendBeatFeedback(beat);
    }
    
    void onBar(int bar) override {
        adapter_->sendBarFeedback(bar);
    }
    
private:
    OSCAdapter* adapter_ = nullptr;
};
```

---

**Resumen de decisiones OSC Adapter:**

| Decisión | Rationale |
|----------|-----------|
| Prefijo no hardcodeado | Flexibilidad para diferentes layouts de OSC |
| Adapter conoce source="osc" | MappingLayer hace la resolución completa |
| Transport privilegiado | Latencia cero, simplicity, safety |
| Clamping en lugar de reject | El usuario puede enviar valores fuera de rango |
| Stats en memoria | Debugging sin overhead de I/O |
| Feedback throttleado | Evitar saturar la red OSC |

---

## Mapping Layer - Profundidad Completa

### 1. Modelo de Fuente Abstracta

**Estructura de AbstractSource:**

```cpp
struct AbstractSource {
    std::string sourceId;     // Unique identifier: "osc.fader.1", "keyboard.key.space"
    SourceType sourceType;   // OSC, KEYBOARD, KINECT, INTERNAL
    int channel;             // Channel number within source type
    ValueRange valueRange;   // RANGE_0_1 (0.0-1.0) o RANGE_NEG1_1 (-1.0 a 1.0)
};
```

**Tipos de fuente:**

| SourceType | Descripción | Value Range | Ejemplos |
|------------|-------------|-------------|----------|
| **OSC** | Cualesquiera mensajes OSC | 0.0-1.0 típicamente | faders, knobs, XY pads |
| **KEYBOARD** | Eventos de teclado | 0.0 (pressed) o 1.0 (released) | key.space, key.escape |
| **KINECT** | Datos de Kinect (futuro) | Depende del tracking | hand.left, hand.right |
| **INTERNAL** | Generadores internos | 0.0-1.0 | LFO, secuenciador |

**Ejemplos de sourceId:**

```
osc.fader.1           → OSC fader channel 1
osc.fader.2           → OSC fader channel 2
osc.toggle.1          → OSC toggle channel 1
osc.xy.1              → OSC XY pad component

keyboard.key.space    → Tecla ESPACIO
keyboard.key.escape   → Tecla ESCAPE
keyboard.mod.shift    → Modificador SHIFT

kinect.hand.left.x    → Mano izquierda, eje X
kinect.hand.right.y   → Mano derecha, eje Y

internal.lfo.1        → LFO interno 1
internal.lfo.2        → LFO interno 2
internal.sequencer.1  → Sequencer step 1
```

**Value Range - Normalización:**

```cpp
enum class ValueRange {
    RANGE_0_1,      // 0.0 a 1.0 (típico para faders)
    RANGE_NEG1_1    // -1.0 a 1.0 (típico para XY, audio)
};

class SourceNormalizer {
    // Toda fuente produce valores normalizados
    static float normalize(const AbstractSource& source, float rawValue) {
        switch (source.valueRange) {
            case ValueRange::RANGE_0_1:
                return std::clamp(rawValue, 0.0f, 1.0f);
            case ValueRange::RANGE_NEG1_1:
                return std::clamp(rawValue, -1.0f, 1.0f);
        }
    }
};
```

---

### 2. Modelo de Target Parameter

**Estructura de TargetParameter:**

```cpp
struct TargetParameter {
    std::string paramId;      // Reference completo: "pinturaFluorescente.k", "global.bpm"
    TargetType targetType;    // Tipo de parámetro
    std::string scope;        // "scene" o "global"
};

enum class TargetType {
    PARAM,      // Float continuo (0.0 - 100.0)
    TOGGLE,     // Boolean (0 o 1)
    TRIGGER,    // Pulso (one-shot, no mantiene estado)
    DISCRETE    // Entero (stepper, selector)
};
```

**Acceso por scope:**

```cpp
class ParameterRegistry {
    // Por sceneId: escena debe estar activa o registrada
    std::optional<float> getParam(const std::string& sceneId, const std::string& paramName);
    
    // Global: accesible siempre
    std::optional<float> getGlobalParam(const std::string& paramName);
    
    // Verificar existencia antes de mapear
    bool hasParam(const std::string& paramId) const;
};
```

**Ejemplos de paramId:**

```
pinturaFluorescente.k      → Parameter 'k' en escena 'pinturaFluorescente'
pinturaFluorescente.speed   → Parameter 'speed' en misma escena
global.bpm                  → Parameter global 'bpm'
global.masterVolume         → Parameter global 'masterVolume'
scene.color.r               → RGB component en escena activa
```

---

### 3. Modelo de Mapping (ParameterBinding)

**Estructura completa de ParameterBinding:**

```cpp
struct ParameterBinding {
    std::string bindingId;      // Unique: "bind-osc-1-to-color-r"
    AbstractSource source;      // Fuente del mapping
    TargetParameter target;    // Target del mapping
    TransformConfig transform; // Transformación a aplicar
    bool enabled = true;       // Enable/disable runtime
    SmoothingConfig smoothing; // Smoothing configuration
    QuantizationConfig quantization; // Cuantización si aplica
};

struct TransformConfig {
    TransformType type;
    
    // Para RANGE_MAP
    float inputMin = 0.0f;
    float inputMax = 1.0f;
    float outputMin = 0.0f;
    float outputMax = 1.0f;
    
    // Para CURVE
    CurveFunction curve = CurveFunction::LINEAR;
    
    // Para STEP
    float stepSize = 0.1f;
};

enum class TransformType {
    DIRECT,      // Sin transformación
    INVERT,      // Invierte el valor (1.0 - value)
    RANGE_MAP,   // Mapea rango input a output
    CURVE,       // Curva de respuesta (exponencial, log, sigmoid)
    STEP         // Cuantización (step size)
};

enum class CurveFunction {
    LINEAR,
    EXPONENTIAL,   // y = x^2
    LOGARITHMIC,   // y = sqrt(x)
    SIGMOID        // Curva S suave
};

struct SmoothingConfig {
    SmoothingType type = SmoothingType::NONE;
    float factor = 0.0f;        // Exponential: 0.0-1.0 (1 = sin smoothing)
    int windowSize = 0;         // Moving average: window size
};

enum class SmoothingType {
    NONE,
    SIMPLE_MOVING_AVERAGE,
    EXPONENTIAL
};
```

**Ejemplos de bindings completos:**

```json
{
  "bindingId": "osc-fader-1-to-k",
  "source": {
    "sourceId": "osc.fader.1",
    "sourceType": "OSC",
    "channel": 1,
    "valueRange": "RANGE_0_1"
  },
  "target": {
    "paramId": "pinturaFluorescente.k",
    "targetType": "PARAM",
    "scope": "scene"
  },
  "transform": {
    "type": "RANGE_MAP",
    "inputMin": 0.0,
    "inputMax": 1.0,
    "outputMin": 0.0,
    "outputMax": 100.0
  },
  "smoothing": {
    "type": "EXPONENTIAL",
    "factor": 0.1
  },
  "enabled": true
}
```

```json
{
  "bindingId": "osc-toggle-1-to-glow",
  "source": {
    "sourceId": "osc.toggle.1",
    "sourceType": "OSC",
    "channel": 101,
    "valueRange": "RANGE_0_1"
  },
  "target": {
    "paramId": "pinturaFluorescente.glowEnabled",
    "targetType": "TOGGLE",
    "scope": "scene"
  },
  "transform": {
    "type": "DIRECT"
  },
  "enabled": true
}
```

---

### 4. Reglas de Transformación de Valores

**Pipeline completo de transformación:**

```
rawValue → clamp → smooth → transform → quantize → setParam
```

**Implementación del pipeline:**

```cpp
class MappingPipeline {
    float process(float rawValue, const ParameterBinding& binding) {
        if (!binding.enabled) {
            return 0.0f;  // O mantener último valor
        }
        
        float value = rawValue;
        
        // 1. Clamp inicial según el tipo de fuente
        value = clampByRange(value, binding.source.valueRange);
        
        // 2. Apply smoothing
        value = applySmoothing(value, binding.smoothing);
        
        // 3. Apply transformation
        value = applyTransform(value, binding.transform);
        
        // 4. Apply quantization (si aplica discretización)
        value = applyQuantization(value, binding.quantization);
        
        // 5. Clamp final según target type
        value = clampByTarget(value, binding.target.targetType);
        
        return value;
    }
    
private:
    float clampByRange(float value, ValueRange range) {
        switch (range) {
            case ValueRange::RANGE_0_1:
                return std::clamp(value, 0.0f, 1.0f);
            case ValueRange::RANGE_NEG1_1:
                return std::clamp(value, -1.0f, 1.0f);
        }
    }
    
    float clampByTarget(float value, TargetType type) {
        switch (type) {
            case TargetType::PARAM:
                return value;  // Sin clamp para floats
            case TargetType::TOGGLE:
                return value > 0.5f ? 1.0f : 0.0f;
            case TargetType::DISCRETE:
                return std::floor(value + 0.5f);  // Round to nearest
            case TargetType::TRIGGER:
                return value;  // One-shot, se usa como bool
        }
    }
};
```

**Smoothing - Simple Moving Average:**

```cpp
class MovingAverageSmoother {
public:
    explicit MovingAverageSmoother(int windowSize) 
        : windowSize_(windowSize) {}
    
    float process(float value) {
        history_.push_back(value);
        if (history_.size() > windowSize_) {
            history_.pop_front();
        }
        
        float sum = 0.0f;
        for (float v : history_) {
            sum += v;
        }
        return sum / history_.size();
    }
    
private:
    int windowSize_;
    std::deque<float> history_;
};
```

**Smoothing - Exponential:**

```cpp
class ExponentialSmoother {
public:
    explicit ExponentialSmoother(float factor) 
        : factor_(factor) {
        // factor: 0.0 = sin cambio, 1.0 = sin smoothing
        // Factor 0.1 = 10% del nuevo valor, 90% del anterior
    }
    
    float process(float value) {
        if (!initialized_) {
            initialized_ = true;
            lastValue_ = value;
            return value;
        }
        
        float result = lastValue_ + factor_ * (value - lastValue_);
        lastValue_ = result;
        return result;
    }
    
private:
    float factor_;
    float lastValue_ = 0.0f;
    bool initialized_ = false;
};
```

**Transform functions:**

```cpp
class TransformFunctions {
    static float direct(float value) {
        return value;
    }
    
    static float invert(float value) {
        return 1.0f - value;
    }
    
    static float rangeMap(float value, float inMin, float inMax, 
                          float outMin, float outMax) {
        // Mapear de un rango a otro
        float normalized = (value - inMin) / (inMax - inMin);
        return outMin + normalized * (outMax - outMin);
    }
    
    static float exponential(float value) {
        return value * value;  // x^2
    }
    
    static float logarithmic(float value) {
        return std::sqrt(value);  // sqrt(x)
    }
    
    static float sigmoid(float value) {
        // Curva S suave
        return 1.0f / (1.0f + std::exp(-10.0f * (value - 0.5f)));
    }
    
    static float step(float value, float stepSize) {
        return std::round(value / stepSize) * stepSize;
    }
};
```

**Ejemplo de pipeline completo:**

```
Input: osc.fader.1 = 0.75

Pipeline:
1. clamp(0.75) → 0.75 (ya está en rango)
2. smooth(0.75, exponential factor=0.1) → 0.75 (primer valor)
3. transform(0.75, RANGE_MAP 0-1 → 0-100) → 75.0
4. quantize(75.0, stepSize=1) → 75.0
5. clampByTarget(75.0, PARAM) → 75.0

Output: setParam("pinturaFluorescente.k", 75.0)
```

```
Input: osc.fader.1 = 0.0 (invertido)

Pipeline:
1. clamp(0.0) → 0.0
2. smooth(0.0, exponential factor=0.1) → 0.0
3. transform(0.0, INVERT) → 1.0
4. transform(1.0, RANGE_MAP 0-1 → 0-100) → 100.0
5. quantize(100.0, stepSize=1) → 100.0

Output: setParam("pinturaFluorescente.k", 100.0)
```

---

### 5. Integración con Parameter Registry

**Principio fundamental:**

```
MappingLayer NO crea parámetros, solo los mapea.
ParameterRegistry es la fuente de verdad.
```

**Verificación de existencia:**

```cpp
class MappingLayer : public IMappingLayer {
public:
    void onInput(const std::string& sourceId, float value) override {
        auto binding = resolve(sourceId);
        if (!binding) {
            log_.debug("No binding for source: " + sourceId);
            return;
        }
        
        // VALIDACIÓN CRÍTICA: verificar que el parámetro existe
        if (!registry_.hasParam(binding->target.paramId)) {
            log_.warn("Mapping targets non-existent param: " + 
                      binding->target.paramId);
            return;  // Silent fail - o throw según config
        }
        
        // Procesar valor
        float processedValue = process(value, *binding);
        
        // Escribir al registry
        registry_.setParam(binding->target.paramId, processedValue);
    }
};
```

**Subscribe a cambios del registry:**

```cpp
class MappingLayer : public IParameterObserver {
public:
    void onParamRegistered(const std::string& paramId) override {
        // Nuevo parámetro disponible para mapping
        availableParams_.insert(paramId);
        log_.info("Param available for mapping: " + paramId);
    }
    
    void onParamUnregistered(const std::string& paramId) override {
        // Remover mappings que apunten a este parámetro
        removeMappingsToParam(paramId);
        availableParams_.erase(paramId);
        log_.info("Param removed, mappings cleaned: " + paramId);
    }
    
private:
    std::set<std::string> availableParams_;
};
```

**Obtención de parámetros válidos para UI:**

```cpp
class MappingLayer {
public:
    // Lista de parámetros disponibles para mapping
    std::vector<std::string> getAvailableParams() const {
        return registry_.getAllParamIds();
    }
    
    // Lista de sources disponibles
    std::vector<AbstractSource> getAvailableSources() const {
        std::vector<AbstractSource> sources;
        
        // OSC channels 1-10
        for (int i = 1; i <= 10; i++) {
            sources.push_back({"osc.fader." + std::to_string(i), 
                              SourceType::OSC, i, ValueRange::RANGE_0_1});
        }
        // ... más sources
        return sources;
    }
};
```

---

### 6. Persistencia Sugerida

**Formato JSON:**

```json
{
  "version": "1.0",
  "bindings": [
    {
      "bindingId": "osc-fader-1-to-k",
      "source": {
        "sourceId": "osc.fader.1",
        "sourceType": "OSC",
        "channel": 1,
        "valueRange": "RANGE_0_1"
      },
      "target": {
        "paramId": "pinturaFluorescente.k",
        "targetType": "PARAM",
        "scope": "scene"
      },
      "transform": {
        "type": "RANGE_MAP",
        "inputMin": 0.0,
        "inputMax": 1.0,
        "outputMin": 0.0,
        "outputMax": 100.0
      },
      "smoothing": {
        "type": "EXPONENTIAL",
        "factor": 0.1
      },
      "quantization": {
        "enabled": false,
        "stepSize": 0.1
      },
      "enabled": true
    }
  ]
}
```

**Interfaz de persistencia:**

```cpp
class IPersistence {
public:
    virtual ~IPersistence() = default;
    
    virtual bool loadMappings(const std::string& filename) = 0;
    virtual bool saveMappings(const std::string& filename) = 0;
    virtual std::string exportToJSON() const = 0;
    virtual bool importFromJSON(const std::string& json) = 0;
};

class JsonMappingPersistence : public IPersistence {
public:
    bool loadMappings(const std::string& filename) override {
        std::ifstream file(filename);
        if (!file.is_open()) {
            log_.error("Cannot open mappings file: " + filename);
            return false;
        }
        
        json data;
        file >> data;
        return loadFromJSON(data);
    }
    
    bool saveMappings(const std::string& filename) override {
        std::ofstream file(filename);
        if (!file.is_open()) {
            log_.error("Cannot write mappings file: " + filename);
            return false;
        }
        
        json data = exportToJSON();
        file << data.dump(2);
        return true;
    }
};
```

**Validación al cargar:**

```cpp
class MappingLayer {
    bool loadFromJSON(const json& data) {
        bindings_.clear();
        
        for (const auto& bindingJson : data["bindings"]) {
            auto binding = parseBinding(bindingJson);
            
            // VALIDACIÓN: verificar parámetro existe
            if (!registry_.hasParam(binding.target.paramId)) {
                log_.warn("Skipping binding to non-existent param: " + 
                          binding.target.paramId);
                continue;  // Skip, no throw
            }
            
            bindings_[binding.source.sourceId] = binding;
        }
        
        return true;
    }
};
```

---

### 7. UI para Editar Mappings

**Lista de bindings con enable/disable:**

```cpp
class MappingLayerUI {
public:
    void draw() {
        ImGui::Begin("Mapping Editor");
        
        // Toolbar
        if (ImGui::Button("Add Binding")) {
            showAddDialog_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            loadMappings();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            saveMappings();
        }
        
        ImGui::Separator();
        
        // Lista de bindings
        for (auto& [sourceId, binding] : bindings_) {
            bool enabled = binding.enabled;
            
            ImGui::PushID(sourceId.c_str());
            
            // Enable/disable checkbox
            if (ImGui::Checkbox("##enabled", &enabled)) {
                binding.enabled = enabled;
            }
            
            ImGui::SameLine();
            
            // Mostrar source → target
            ImGui::Text("%s → %s", 
                sourceId.c_str(), 
                binding.target.paramId.c_str());
            
            // Info de transform
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), 
                "[%s]", getTransformName(binding.transform.type).c_str());
            
            // Menú contextual
            if (ImGui::BeginPopupContextItem("context")) {
                if (ImGui::MenuItem("Edit")) {
                    selectedBinding_ = sourceId;
                }
                if (ImGui::MenuItem("Duplicate")) {
                    duplicateBinding(sourceId);
                }
                if (ImGui::MenuItem("Delete")) {
                    deleteBinding(sourceId);
                }
                ImGui::EndPopup();
            }
            
            ImGui::PopID();
        }
        
        ImGui::End();
    }
};
```

**Editor visual de binding:**

```cpp
class BindingEditorDialog {
public:
    void draw(const std::string& bindingId) {
        ImGui::Begin("Edit Mapping");
        
        auto& binding = getBinding(bindingId);
        
        // Source selector
        ImGui::Text("Source");
        ImGui::SameLine();
        ImGui::Combo("##source", &selectedSourceType_, 
            "OSC\0Keyboard\0Kinect\0Internal\0");
        ImGui::SameLine();
        ImGui::InputInt("Channel", &channel_);
        
        // Arrow visualization
        ImGui::Text("    ↓");
        
        // Transform section
        ImGui::Text("Transform");
        ImGui::SameLine();
        ImGui::Combo("##transform", &selectedTransform_, 
            "Direct\0Invert\0Range Map\0Curve\0Step\0");
        
        // Transform-specific options
        if (selectedTransform_ == TRANSFORM_RANGE_MAP) {
            ImGui::InputFloat("Input Min", &transform_.inputMin);
            ImGui::InputFloat("Input Max", &transform_.inputMax);
            ImGui::InputFloat("Output Min", &transform_.outputMin);
            ImGui::InputFloat("Output Max", &transform_.outputMax);
        }
        else if (selectedTransform_ == TRANSFORM_CURVE) {
            ImGui::Combo("Curve", &selectedCurve_, 
                "Linear\0Exponential\0Logarithmic\0Sigmoid\0");
        }
        else if (selectedTransform_ == TRANSFORM_STEP) {
            ImGui::InputFloat("Step Size", &transform_.stepSize);
        }
        
        // Arrow visualization
        ImGui::Text("    ↓");
        
        // Target selector
        ImGui::Text("Target");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##target", currentTarget_.c_str())) {
            for (const auto& param : availableParams_) {
                if (ImGui::Selectable(param.c_str())) {
                    currentTarget_ = param;
                }
            }
            ImGui::EndCombo();
        }
        
        // Smoothing
        ImGui::Separator();
        ImGui::Text("Smoothing");
        ImGui::Combo("##smoothing", &selectedSmoothing_, 
            "None\0Moving Average\0Exponential\0");
        
        if (selectedSmoothing_ == SMOOTHING_MOVING_AVG) {
            ImGui::InputInt("Window Size", &smoothing_.windowSize);
        }
        else if (selectedSmoothing_ == SMOOTHING_EXPONENTIAL) {
            ImGui::SliderFloat("Factor", &smoothing_.factor, 0.0f, 1.0f);
        }
        
        ImGui::Separator();
        
        if (ImGui::Button("Apply")) {
            applyChanges(bindingId);
            close();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            close();
        }
        
        ImGui::End();
    }
};
```

**Test input slider:**

```cpp
class MappingTestPanel {
public:
    void draw() {
        ImGui::Begin("Test Mapping");
        
        // Slider para simular valor de entrada
        static float testValue = 0.5f;
        ImGui::SliderFloat("Test Input", &testValue, 0.0f, 1.0f);
        
        // Seleccionar binding a probar
        ImGui::Combo("Binding", &selectedBindingIndex_, bindingsList_);
        
        if (selectedBindingIndex_ >= 0) {
            auto& binding = getBinding(selectedBindingIndex_);
            
            // Mostrar valor transformado en tiempo real
            float processed = processTestValue(testValue, binding);
            
            ImGui::Separator();
            ImGui::Text("Raw: %.3f", testValue);
            ImGui::Text("Processed: %.3f", processed);
            
            // Valor actual del parámetro objetivo
            float currentParamValue = registry_.getParam(binding.target.paramId);
            ImGui::Text("Current Param: %.3f", currentParamValue);
            
            // Apply button
            if (ImGui::Button("Apply to Param")) {
                registry_.setParam(binding.target.paramId, processed);
            }
        }
        
        ImGui::End();
    }
};
```

---

### 8. Riesgos y Tradeoffs

**Matriz de riesgos:**

| Riesgo | Likelihood | Impacto | Mitigation | Estado |
|--------|-----------|---------|------------|--------|
| **Mapping a parámetro inexistente** | Med | Low | Validación al cargar + runtime check | 🔲 Pendiente |
| **Circular mapping (A→B, B→A)** | Low | Alto | Detectar ciclos en tiempo de binding | 🔲 Pendiente |
| **Overlapping mappings al mismo target** | Med | Med | Política configurable (last wins/merge/error) | 🔲 Pendiente |
| **Performance: many-to-many** | Low | Med | Lookup optimization (hash map) | 🔲 Pendiente |
| **UI complejidad** | Alto | Low | Simplificar editor, presets | 🔲 Pendiente |

**Riesgo 1: Mapping a parámetro inexistente**

```cpp
// Política configurable
enum class MissingParamPolicy {
    SILENT_FAIL,    // Ignorar silently
    LOG_WARNING,    // Loguear warning
    THROW_ERROR,    // Lanzar excepción
    CREATE_PARAM    // Crear parámetro automáticamente
};

class MappingLayer {
    void setMissingParamPolicy(MissingParamPolicy policy) {
        missingParamPolicy_ = policy;
    }
    
    float processValue(float value, const ParameterBinding& binding) {
        // Runtime check
        if (!registry_.hasParam(binding.target.paramId)) {
            switch (missingParamPolicy_) {
                case MissingParamPolicy::SILENT_FAIL:
                    return value;  // Mantener sin aplicar
                case MissingParamPolicy::LOG_WARNING:
                    log_.warn("Mapping to non-existent param: " + 
                              binding.target.paramId);
                    return value;
                case MissingParamPolicy::THROW_ERROR:
                    throw std::runtime_error("Param not found: " + 
                                             binding.target.paramId);
                case MissingParamPolicy::CREATE_PARAM:
                    registry_.registerParam(binding.target.paramId);
                    break;
            }
        }
        // ... continue
    }
};
```

**Riesgo 2: Circular mapping**

```cpp
// Detectar ciclos antes de agregar binding
class CycleDetector {
public:
    bool wouldCreateCycle(const ParameterBinding& newBinding) {
        // BFS desde target hacia atrás
        std::set<std::string> visited;
        std::queue<std::string> queue;
        
        queue.push(newBinding.target.paramId);
        
        while (!queue.empty()) {
            auto current = queue.front();
            queue.pop();
            
            if (current == newBinding.source.sourceId) {
                return true;  // Ciclo detectado
            }
            
            if (visited.count(current)) continue;
            visited.insert(current);
            
            // Buscar bindings que apunten a este parámetro como source
            auto outgoing = getBindingsFromSource(current);
            for (auto& bind : outgoing) {
                queue.push(bind.target.paramId);
            }
        }
        
        return false;
    }
};
```

**Riesgo 3: Overlapping mappings**

```cpp
// Política cuando múltiples mappings apuntan al mismo target
enum class OverlapPolicy {
    LAST_WINS,      // Último binding agregado gana
    MERGE_AVERAGE,  // Promediar valores
    MERGE_SUM,      // Sumar valores
    ERROR           // Lanzar error al agregar
};

class MappingLayer {
    float resolveMultipleMappings(const std::vector<ParameterBinding*>& bindings,
                                  float inputValue) {
        switch (overlapPolicy_) {
            case OverlapPolicy::LAST_WINS:
                return processValue(inputValue, *bindings.back());
                
            case OverlapPolicy::MERGE_AVERAGE: {
                float sum = 0.0f;
                for (auto* bind : bindings) {
                    sum += processValue(inputValue, *bind);
                }
                return sum / bindings.size();
            }
            
            case OverlapPolicy::MERGE_SUM: {
                float sum = 0.0f;
                for (auto* bind : bindings) {
                    sum += processValue(inputValue, *bind);
                }
                return sum;
            }
            
            case OverlapPolicy::ERROR:
                throw std::runtime_error("Multiple mappings to same target");
        }
    }
};
```

**Riesgo 4: Performance**

```cpp
// Lookup optimization: hash map por source
class MappingLayer {
private:
    // Hash map: sourceId → binding
    // O(1) lookup promedio
    std::unordered_map<std::string, ParameterBinding> bindingsBySource_;
    
    // Secondary index: target → bindings (para overlap detection)
    std::unordered_map<std::string, std::vector<ParameterBinding*>> bindingsByTarget_;
    
public:
    // Lookup O(1) por source
    ParameterBinding* resolve(const std::string& sourceId) {
        auto it = bindingsBySource_.find(sourceId);
        if (it != bindingsBySource_.end()) {
            return &it->second;
        }
        return nullptr;
    }
    
    // Overlapping detection O(n) donde n = bindings al mismo target
    std::vector<ParameterBinding*> getBindingsToTarget(const std::string& targetId) {
        auto it = bindingsByTarget_.find(targetId);
        if (it != bindingsByTarget_.end()) {
            return it->second;
        }
        return {};
    }
};
```

**Tradeoff: Flexibilidad vs Complejidad de UI**

| Aspecto | Flexibilidad Alta | Complejidad Baja |
|---------|-------------------|------------------|
| Transform types | Todos (DIRECT, INVERT, RANGE_MAP, CURVE, STEP) | Solo DIRECT |
| Smoothing | SMA + Exponential | Ninguno |
| Target types | PARAM, TOGGLE, TRIGGER, DISCRETE | Solo PARAM |
| Overlap policy | Todas las opciones | LAST_WINS |
| Binding UI | Editor visual completo | Dropdown simple |

```
RECOMENDACIÓN: Implementar modo "Simple" y "Advanced"
  - Simple: Solo DIRECT, sin smoothing, LAST_WINS
  - Advanced: Todas las opciones
  - Usuario puede toggle entre modos
```

---

## Migration / Rollout

No migration required - greenfield implementation.

---

## Parameter Registry - Profundidad Completa

### 1. Contrato de Parameter Definition

**Estructura Parameter completa:**

```cpp
struct Parameter {
    // Identificación
    std::string id;           // Unique: "global.bpm", "pinturaFluorescente.k"
    std::string label;        // Display: "Tempo", "Noise Amount"
    
    // Tipo y estructura
    ParamType type;           // FLOAT, INT, BOOL, ENUM, TRIGGER, VEC2, VEC3
    
    // Metadata
    std::string description;  // Tooltip info
    std::string group;        // UI grouping: "Rendering", "Animation", "Audio"
    bool hidden = false;      // No mostrar en UI (parámetros internos)
    bool writable = true;     // Solo lectura vs lectura-escritura
    bool hotReloadable = true; // Cambiable en runtime
    
    // Alcance
    ParamScope scope;         // GLOBAL, SCENE
    std::string sceneId;      // Non-empty solo si scope == SCENE
    
    // Rangos y defaults
    std::optional<float> min;
    std::optional<float> max;
    float defaultValue = 0.0f;
    float currentValue = 0.0f;
    float step = 0.01f;       // Incremento para UI sliders
    std::string unit;         // "bpm", "px", "deg", "%"
    
    // Para ENUM type
    std::vector<std::string> enumOptions;  // {"option1", "option2", ...}
    
    // Smoothing integrado
    SmoothingConfig smoothing;
};
```

**Tipos de parámetro:**

```cpp
enum class ParamType {
    FLOAT,   // Valor continuo: 0.0 - 100.0
    INT,     // Entero: 0, 1, 2, ...
    BOOL,    // Booleano: true/false
    ENUM,    // Opciones discretas predefined
    TRIGGER, // Acción one-shot, no tiene "current value"
    VEC2,    // Vector 2D: {x, y}
    VEC3     // Vector 3D: {x, y, z}
};

enum class ParamScope {
    GLOBAL,  // Parámetro del sistema (BPM, masterVolume)
    SCENE    // Parámetro de una escena específica
};

struct SmoothingConfig {
    SmoothingType type = SmoothingType::NONE;
    float factor = 0.0f;        // Exponential: 0.0-1.0
    int windowSize = 0;         // Moving average
};
```

**Diferencia clave: TRIGGER vs otros tipos:**

```cpp
// TRIGGER es acción única, no mantiene estado "current"
// Se ejecuta y se olvida inmediatamente

struct TriggerParameter : public Parameter {
    TriggerParameter() {
        type = ParamType::TRIGGER;
        // NO tiene currentValue - es un evento, no un estado
    }
    
    // Ejecutar el trigger
    void fire() {
        notifyObservers();  // Notifica que se ejecutó
    }
};

// BOOL tiene currentValue (true/false)
struct BoolParameter : public Parameter {
    bool currentValue = false;
    
    void set(bool value) {
        if (value != currentValue) {
            currentValue = value;
            notifyObservers();
        }
    }
};
```

**Ejemplo de estructura completa:**

```cpp
// Parámetro global
Parameter globalBpm;
globalBpm.id = "global.bpm";
globalBpm.label = "Tempo";
globalBpm.type = ParamType::FLOAT;
globalBpm.scope = ParamScope::GLOBAL;
globalBpm.min = 20.0f;
globalBpm.max = 300.0f;
globalBpm.defaultValue = 120.0f;
globalBpm.currentValue = 120.0f;
globalBpm.step = 1.0f;
globalBpm.unit = "bpm";
globalBpm.group = "Transport";
globalBpm.description = "Beats per minute";
globalBpm.writable = true;
globalBpm.hotReloadable = true;

// Parámetro de escena con enum
Parameter sceneBlend;
sceneBlend.id = "pinturaFluorescente.blendMode";
sceneBlend.label = "Blend Mode";
sceneBlend.type = ParamType::ENUM;
sceneBlend.scope = ParamScope::SCENE;
sceneBlend.sceneId = "pinturaFluorescente";
sceneBlend.defaultValue = 0.0f;  // Index de la opción por defecto
sceneBlend.enumOptions = {"ADD", "MULTIPLY", "SCREEN", "OVERLAY"};
sceneBlend.group = "Rendering";
globalBpm.description = "Blending algorithm";
globalBpm.writable = true;

// Trigger parameter
Parameter sceneReset;
sceneReset.id = "pinturaFluorescente.reset";
sceneReset.label = "Reset Scene";
sceneReset.type = ParamType::TRIGGER;
sceneReset.scope = ParamScope::SCENE;
sceneReset.sceneId = "pinturaFluorescente";
sceneReset.group = "Debug";
sceneReset.description = "Reset scene to initial state";
sceneReset.writable = true;
```

---

### 2. Estrategia de Nombres e IDs

**Formato de ID:**

```
{prefix}.{name}

Donde:
- prefix: "global", "system", o sceneId
- name: camelCase, sin espacios
```

| Scope | ID Format | Ejemplo |
|-------|-----------|---------|
| **Global** | `global.{paramName}` | `global.bpm`, `global.masterVolume` |
| **Scene** | `{sceneId}.{paramName}` | `pinturaFluorescente.k`, `particulas.count` |
| **System** | `system.{paramName}` | `system.frameRate`, `system.memoryUsage` |

**Naming Convention:**

```cpp
// ✅ CORRECTO
global.bpm
global.timeScale
pinturaFluorescente.noiseAmount
pinturaFluorescente.blendMode
particulas.particleCount

// ❌ INCORRECTO
global.bpm value          // Espacio
Global.BPM                // Mayúsculas en nombre
pintura-fluorescente.k    // Guión en sceneId
pintura fluorescente.k    // Espacio en sceneId
```

**Prefijos reservados:**

```cpp
// Lista de prefijos que NO pueden usarse como nombres de parámetros
static const std::vector<std::string> RESERVED_PREFIXES = {
    "global.",    // Reservado para parámetros del sistema
    "system.",    // Reservado para métricas internas
    "scene.",     // Reservado para acceso genérico a escena activa
    "_internal.", // Reservado para uso interno
    "osc.",       // Reservado para input OSC
    "keyboard.",  // Reservado para input keyboard
    "kinect."     // Reservado para input kinect
};

// Validación
bool isValidParamId(const std::string& id) {
    for (const auto& prefix : RESERVED_PREFIXES) {
        if (id.find(prefix) == 0) {
            // Verificar que no es el propio prefijo exacto
            if (id == prefix.substr(0, prefix.length() - 1)) {
                return true;  // "global" es válido como sceneId
            }
            return false;  // "global.anything" no es válido
        }
    }
    return true;  // No empieza con prefijo reservado
}
```

**ID Uniqueness Enforcement:**

```cpp
class ParameterRegistry {
    bool registerParam(const Parameter& param) override {
        // 1. Verificar ID uniqueness
        if (params_.count(param.id)) {
            log_.error("Duplicate parameter ID: " + param.id);
            return false;  // O throw exception según config
            
            // Alternativa: log warning y skip
            log_.warn("Parameter already exists, skipping: " + param.id);
            return false;
        }
        
        // 2. Verificar que no es prefijo reservado
        if (!isValidParamId(param.id)) {
            log_.error("Invalid ID (reserved prefix): " + param.id);
            return false;
        }
        
        // 3. Agregar al registry
        params_[param.id] = param;
        
        // 4. Notificar observers
        notifyParamRegistered(param.id);
        
        return true;
    }
    
    std::unordered_map<std::string, Parameter> params_;  // Hash map para O(1)
};
```

---

### 3. Separación Global vs Escena

**Parámetros globales predefinidos:**

```cpp
class GlobalParameters {
    // Registrados automáticamente en init
    static void registerGlobals(ParameterRegistry& registry) {
        // Transport
        registry.registerParam({
            .id = "global.bpm",
            .label = "Tempo",
            .type = ParamType::FLOAT,
            .min = 20.0f,
            .max = 300.0f,
            .defaultValue = 120.0f,
            .unit = "bpm",
            .group = "Transport"
        });
        
        registry.registerParam({
            .id = "global.timeScale",
            .label = "Time Scale",
            .type = ParamType::FLOAT,
            .min = 0.0f,
            .max = 4.0f,
            .defaultValue = 1.0f,
            .group = "Transport"
        });
        
        registry.registerParam({
            .id = "global.masterVolume",
            .label = "Master Volume",
            .type = ParamType::FLOAT,
            .min = 0.0f,
            .max = 1.0f,
            .defaultValue = 0.8f,
            .unit = "%",
            .group = "Audio"
        });
        
        registry.registerParam({
            .id = "global.clearEachFrame",
            .label = "Clear Frame",
            .type = ParamType::BOOL,
            .defaultValue = true,
            .group = "Rendering"
        });
        
        registry.registerParam({
            .id = "global.currentPalette",
            .label = "Color Palette",
            .type = ParamType::ENUM,
            .enumOptions = {"Neon", "Pastel", "Warm", "Cool", "Monochrome"},
            .defaultValue = 0.0f,
            .group = "Visual"
        });
    }
};
```

**Parámetros de escena - auto-registro en setup():**

```cpp
class PinturaFluorescenteScene : public IBaseScene {
    void setup() override {
        // Registrar parámetros específicos de esta escena
        auto& registry = ParameterRegistry::get();
        
        registry.registerParam({
            .id = "pinturaFluorescente.k",
            .label = "Noise Amount",
            .type = ParamType::FLOAT,
            .scope = ParamScope::SCENE,
            .sceneId = "pinturaFluorescente",
            .min = 0.0f,
            .max = 100.0f,
            .defaultValue = 50.0f,
            .step = 1.0f,
            .group = "Animation"
        });
        
        registry.registerParam({
            .id = "pinturaFluorescente.speed",
            .label = "Speed",
            .type = ParamType::FLOAT,
            .scope = ParamScope::SCENE,
            .sceneId = "pinturaFluorescente",
            .min = 0.0f,
            .max = 5.0f,
            .defaultValue = 1.0f,
            .group = "Animation"
        });
        
        registry.registerParam({
            .id = "pinturaFluorescente.glowEnabled",
            .label = "Glow",
            .type = ParamType::BOOL,
            .scope = ParamScope::SCENE,
            .sceneId = "pinturaFluorescente",
            .defaultValue = true,
            .group = "Rendering"
        });
    }
};
```

**API para acceso diferenciado:**

```cpp
class ParameterRegistry {
    // Solo parámetros globales
    std::vector<Parameter> getGlobalParams() const {
        std::vector<Parameter> result;
        for (auto& [id, param] : params_) {
            if (param.scope == ParamScope::GLOBAL) {
                result.push_back(param);
            }
        }
        return result;
    }
    
    // Solo parámetros de una escena específica
    std::vector<Parameter> getSceneParams(const std::string& sceneId) const {
        std::vector<Parameter> result;
        for (auto& [id, param] : params_) {
            if (param.scope == ParamScope::SCENE && param.sceneId == sceneId) {
                result.push_back(param);
            }
        }
        return result;
    }
    
    // Todos los parámetros
    std::vector<Parameter> getAllParams() const {
        std::vector<Parameter> result;
        for (auto& [id, param] : params_) {
            result.push_back(param);
        }
        return result;
    }
    
    // Parámetros por grupo
    std::vector<Parameter> getParamsByGroup(const std::string& group) const {
        std::vector<Parameter> result;
        for (auto& [id, param] : params_) {
            if (param.group == group) {
                result.push_back(param);
            }
        }
        return result;
    }
};
```

---

### 4. Relación con Scene Engine, Mapping y UI

**Diagrama de flujo de datos:**

```
┌─────────────────────┐      ┌─────────────────────┐
│   Scene Engine      │      │   Mapping Layer     │
│                     │      │                     │
│ scene.setup()       │      │ onInput()           │
│   └→ registerParam()│      │   └→ transform()     │
│                     │      │   └→ setParam()     │
└─────────┬───────────┘      └─────────┬───────────┘
          │                            │
          │ registerParam()           │ setParam()
          ▼                            ▼
┌─────────────────────────────────────────────────┐
│              Parameter Registry                │
│  • params_ (unordered_map<string, Param>)      │
│  • observers_ (vector<IParameterObserver>)     │
│  • notifyParamChanged(paramId, newValue)       │
└─────────────────────────────────────────────────┘
          ▲                    │
          │ onChange()         │ subscribe
          │                    ▼
┌─────────┴───────────┐      ┌─────────────────────┐
│   ControlUI         │      │   Scene (read)      │
│                     │      │                     │
│ • Slider/Bool/Enum  │      │ update(deltaTime)   │
│ • Real-time update  │      │   └→ getParam(id)   │
│ • Visual feedback   │      │                     │
└─────────────────────┘      └─────────────────────┘
```

**Integración con Scene Engine:**

```cpp
class SceneEngine : public ISceneActivator {
    void activateScene(const std::string& sceneId) override {
        // 1. Cargar la escena si no está cargada
        auto scene = loadOrGetScene(sceneId);
        
        // 2. Las escenas registran sus parámetros en setup()
        // (Se hace automáticamente cuando la escena se instancia)
        scene->setup();  // ← Aquí scene.registerParam() se llama
        
        // 3. Activate la escena
        scene->activate();
        
        activeSceneId_ = sceneId;
    }
};

// La escena debe registrar parámetros en su setup
class MiEscena : public IBaseScene {
    void setup() override {
        // ESTE es el punto donde se registran los parámetros
        auto& registry = ParameterRegistry::get();
        
        registry.registerParam({
            .id = "miescena.param1",
            .label = "Parameter 1",
            // ... resto de config
        });
    }
};
```

**Integración con Mapping Layer (SOLO WRITE):**

```cpp
class MappingLayer : public IMappingLayer {
    void onInput(const std::string& sourceId, float value) override {
        auto binding = resolve(sourceId);
        if (!binding) return;
        
        // MappingLayer SOLO ESCRIBE, no crea parámetros
        // Debe verificar que el parámetro existe
        
        if (!registry_.hasParam(binding->target.paramId)) {
            log_.warn("Mapping targets non-existent param");
            return;  // Silent fail
        }
        
        float transformed = transform(value, binding->transform);
        registry_.setParam(binding->target.paramId, transformed);
        // NO llama a registerParam() - eso es responsabilidad de la escena
    }
};
```

**Integración con UI (Observer pattern):**

```cpp
class ControlUI : public IParameterObserver {
    void onParamChanged(const std::string& paramId, float newValue) override {
        // Update reactivo de la UI cuando cambia el parámetro
        auto param = registry_.getParam(paramId);
        if (!param) return;
        
        // Update el control específico para este parámetro
        auto it = controls_.find(paramId);
        if (it != controls_.end()) {
            it->second->setValue(newValue);
        }
    }
    
    void onParamRegistered(const std::string& paramId) override {
        // Crear control UI para el nuevo parámetro
        auto param = registry_.getParam(paramId);
        createControlForParam(*param);
    }
    
    void onParamUnregistered(const std::string& paramId) override {
        // Remover control UI
        controls_.erase(paramId);
    }
    
private:
    std::unordered_map<std::string, std::unique_ptr<UIControl>> controls_;
};
```

---

### 5. Validaciones

**Tabla de validaciones:**

| Condición | Detección | Respuesta | Logging |
|-----------|-----------|-----------|---------|
| ID duplicado | `params_.count(id)` | Reject + throw/warn | ERROR: "Duplicate ID" |
| min > max | Verificación en register | Reject + throw | ERROR: "min > max" |
| current fuera de rango | `current < min \|\| current > max` | Clamp silently | DEBUG: "Clamped value" |
| Invalid type | Enum check | Compile error (static_assert) | N/A |
| Scene no registrada | `sceneId` no existe | Lookup retorna empty | WARN: "Scene not found" |

**Implementación de validaciones:**

```cpp
class ParameterRegistry {
    bool registerParam(const Parameter& param) {
        // Validación 1: ID uniqueness
        if (params_.count(param.id)) {
            log_.error("Duplicate parameter ID: " + param.id);
            throw std::runtime_error("Duplicate parameter ID: " + param.id);
            // Alternativa: return false;
        }
        
        // Validación 2: min > max
        if (param.min.has_value() && param.max.has_value()) {
            if (*param.min > *param.max) {
                log_.error("Invalid range: min > max for " + param.id);
                throw std::invalid_argument("min cannot be greater than max");
            }
        }
        
        // Validación 3: scene scope requiere sceneId
        if (param.scope == ParamScope::SCENE && param.sceneId.empty()) {
            log_.error("Scene parameter without sceneId: " + param.id);
            throw std::invalid_argument("Scene parameter requires sceneId");
        }
        
        // Validación 4: enum requiere opciones
        if (param.type == ParamType::ENUM && param.enumOptions.empty()) {
            log_.error("ENUM parameter without options: " + param.id);
            throw std::invalid_argument("ENUM parameter requires enumOptions");
        }
        
        // Validación 5: ID format válido
        if (!isValidParamIdFormat(param.id)) {
            log_.error("Invalid ID format: " + param.id);
            throw std::invalid_argument("Invalid ID format");
        }
        
        // Registrar
        params_[param.id] = param;
        notifyParamRegistered(param.id);
        
        return true;
    }
    
    bool setParam(const std::string& paramId, float value) {
        auto it = params_.find(paramId);
        if (it == params_.end()) {
            log_.warn("Parameter not found: " + paramId);
            return false;
        }
        
        auto& param = it->second;
        
        // Validación: TRIGGER no tiene set
        if (param.type == ParamType::TRIGGER) {
            log_.warn("Cannot set value on TRIGGER param: " + paramId);
            return false;
        }
        
        // Clamp silently si fuera de rango
        float newValue = value;
        if (param.min.has_value() && value < *param.min) {
            newValue = *param.min;
            log_.debug("Clamped " + paramId + " to min: " + std::to_string(newValue));
        }
        if (param.max.has_value() && value > *param.max) {
            newValue = *param.max;
            log_.debug("Clamped " + paramId + " to max: " + std::to_string(newValue));
        }
        
        // Solo notificar si cambió
        if (param.currentValue != newValue) {
            param.currentValue = newValue;
            notifyParamChanged(paramId, newValue);
        }
        
        return true;
    }
    
    // Validación de tipo en compile-time
    static_assert(std::is_enum_v<ParamType>, "ParamType must be enum");
    static_assert(std::is_enum_v<ParamScope>, "ParamScope must be enum");
};
```

**Lookup para scene no registrada:**

```cpp
class ParameterRegistry {
    std::optional<Parameter> getParam(const std::string& paramId) const {
        auto it = params_.find(paramId);
        if (it != params_.end()) {
            return it->second;
        }
        // No loguear aquí - el caller puede decidir qué hacer
        return std::nullopt;
    }
    
    std::vector<Parameter> getSceneParams(const std::string& sceneId) const {
        std::vector<Parameter> result;
        
        // Verificar si hay parámetros para esta escena
        for (auto& [id, param] : params_) {
            if (param.scope == ParamScope::SCENE && param.sceneId == sceneId) {
                result.push_back(param);
            }
        }
        
        if (result.empty()) {
            log_.debug("No parameters found for scene: " + sceneId);
        }
        
        return result;
    }
};
```

---

### 6. Metadatos Útiles para UI y Mapping

**Descripción de cada metadata:**

| Campo | Propósito | UI Usage |
|-------|-----------|----------|
| `label` | Nombre para mostrar | Headers de sliders, tooltips |
| `description` | Tooltip info | Hover text, help panel |
| `group` | Organización en UI | Collapsible sections, tabs |
| `hidden` | Ocultar en UI | Parámetros internos (_internal.*) |
| `writable` | Solo lectura vs editable | Disable controles |
| `hotReloadable` | Cambio runtime vs restart | Allow/deny update |

**Ejemplo de parámetro con todos los metadatos:**

```cpp
Parameter fullMetadata;
fullMetadata.id = "pinturaFluorescente.noiseFrequency";
fullMetadata.label = "Noise Frequency";
fullMetadata.description = "Frequency of the noise pattern. Higher values = more detail.";
fullMetadata.type = ParamType::FLOAT;
fullMetadata.scope = ParamScope::SCENE;
fullMetadata.sceneId = "pinturaFluorescente";
fullMetadata.min = 0.01f;
fullMetadata.max = 10.0f;
fullMetadata.defaultValue = 1.0f;
fullMetadata.currentValue = 1.0f;
fullMetadata.step = 0.1f;
fullMetadata.unit = "Hz";
fullMetadata.group = "Noise";
fullMetadata.hidden = false;
fullMetadata.writable = true;
fullMetadata.hotReloadable = true;
fullMetadata.smoothing = {
    .type = SmoothingType::EXPONENTIAL,
    .factor = 0.15f
};
```

**UI usa los metadatos:**

```cpp
class ParameterControlFactory {
    std::unique_ptr<UIControl> createControl(const Parameter& param) {
        std::unique_ptr<UIControl> control;
        
        // Ocultar si hidden
        if (param.hidden) {
            return nullptr;  // No crear control
        }
        
        switch (param.type) {
            case ParamType::FLOAT:
                control = std::make_unique<SliderControl>(param.label);
                control->setRange(param.min.value_or(0), param.max.value_or(1));
                control->setStep(param.step);
                break;
                
            case ParamType::BOOL:
                control = std::make_unique<ToggleControl>(param.label);
                break;
                
            case ParamType::ENUM:
                control = std::make_unique<DropdownControl>(param.label);
                for (const auto& opt : param.enumOptions) {
                    control->addOption(opt);
                }
                break;
                
            case ParamType::TRIGGER:
                control = std::make_unique<ButtonControl>(param.label);
                break;
                
            case ParamType::VEC2:
                control = std::make_unique<Vec2Control>(param.label);
                break;
                
            case ParamType::VEC3:
                control = std::make_unique<Vec3Control>(param.label);
                break;
        }
        
        // Aplicar metadata
        if (control) {
            control->setTooltip(param.description);
            control->setEnabled(param.writable);
            control->setGroup(param.group);
            
            // Disable si no es hot-reloadable y la escena no está activa
            if (!param.hotReloadable && !isSceneActive(param.sceneId)) {
                control->setEnabled(false);
            }
        }
        
        return control;
    }
};
```

**UI agrupa por group:**

```cpp
class ParameterUI {
    void draw() {
        // Obtener todos los parámetros
        auto allParams = registry_.getAllParams();
        
        // Agrupar por group
        std::map<std::string, std::vector<Parameter>> grouped;
        for (auto& param : allParams) {
            if (!param.hidden) {  // Skip hidden
                grouped[param.group].push_back(param);
            }
        }
        
        // Renderizar grupos
        for (auto& [group, params] : grouped) {
            if (ImGui::CollapsibleHeader(group.c_str())) {
                for (auto& param : params) {
                    drawParameterControl(param);
                }
            }
        }
    }
};
```

---

### 7. Riesgos de Diseño

**Matriz de riesgos del Parameter Registry:**

| Riesgo | Likelihood | Impacto | Mitigation | Estado |
|--------|-----------|---------|------------|--------|
| **Singleton registry** | Alto | Medium | DI pattern, mock implementations | 🔲 Pendiente |
| **Too many parameters** | Med | Low | UI pagination, grupos claros | 🔲 Pendiente |
| **Parameter inflation** | Med | Low | Guidelines de registro, review | 🔲 Pendiente |
| **Observer side effects** | Low | High | Transacciones, order guarantees | 🔲 Pendiente |
| **Thread safety** | Low | High | Lock-free si necesario | 🔲 Pendiente |

**Riesgo 1: Singleton Registry - Testing Difficulty**

```cpp
// Problema: Singleton dificulta testing
class ParameterRegistry {
public:
    static ParameterRegistry& get() {
        static ParameterRegistry instance;
        return instance;
    }
    
private:
    ParameterRegistry() = default;
};

// Solución: Interface + mock para testing
class IParameterRegistry {
public:
    virtual ~IParameterRegistry() = default;
    virtual bool registerParam(const Parameter& param) = 0;
    virtual std::optional<Parameter> getParam(const std::string& id) = 0;
    virtual bool setParam(const std::string& id, float value) = 0;
    virtual std::vector<Parameter> getAllParams() const = 0;
    virtual void subscribe(IParameterObserver* observer) = 0;
};

class MockParameterRegistry : public IParameterRegistry {
    // Implementación in-memory para testing
    std::unordered_map<std::string, Parameter> params_;
    std::vector<IParameterObserver*> observers_;
    
public:
    MOCK_METHOD(bool, registerParam, (const Parameter& param), (override));
    MOCK_METHOD(std::optional<Parameter>, getParam, (const std::string& id), (override));
    MOCK_METHOD(bool, setParam, (const std::string& id, float value), (override));
    MOCK_METHOD(std::vector<Parameter>, getAllParams, (), (const, override));
    MOCK_METHOD(void, subscribe, (IParameterObserver* observer), (override));
};

// Test
class SceneTest : public ::testing::Test {
protected:
    MockParameterRegistry mockRegistry_;
    Scene scene_{&mockRegistry_};  // Inyectar mock
};
```

**Riesgo 2: Too Many Parameters - UI Overwhelmed**

```cpp
// Mitigation: Paginación y grupos claros
class ParameterUI {
    static constexpr size_t MAX_PARAMS_PER_PAGE = 20;
    size_t currentPage_ = 0;
    
    void drawPaged() {
        auto params = registry_.getAllParams();
        size_t totalPages = (params.size() + MAX_PARAMS_PER_PAGE - 1) / MAX_PARAMS_PER_PAGE;
        
        // Pagination controls
        ImGui::Text("Page %zu of %zu", currentPage_ + 1, totalPages);
        if (ImGui::Button("Prev") && currentPage_ > 0) currentPage_--;
        ImGui::SameLine();
        if (ImGui::Button("Next") && currentPage_ < totalPages - 1) currentPage_++;
        
        // Render page
        size_t start = currentPage_ * MAX_PARAMS_PER_PAGE;
        size_t end = std::min(start + MAX_PARAMS_PER_PAGE, params.size());
        
        for (size_t i = start; i < end; i++) {
            drawParameterControl(params[i]);
        }
    }
    
    // Alternativa: Search/filter
    std::string searchFilter_;
    
    void drawWithFilter() {
        ImGui::InputText("Search", &searchFilter_);
        
        auto params = registry_.getAllParams();
        for (auto& param : params) {
            if (matchesFilter(param, searchFilter_)) {
                drawParameterControl(param);
            }
        }
    }
};
```

**Riesgo 3: Parameter Inflation**

```cpp
// Guidelines para prevenir inflación
// 1. Solo registrar parámetros que:
//    - Se modifican runtime (no hardcodeados)
//    - Requieren control externo (mapping/UI)
//    - Son esenciales para el comportamiento visual

// 2. Review antes de agregar:
//    - ¿Es realmente necesario?
//    - ¿Ya existe un parámetro similar?
//    - ¿Justifica su lugar en la UI?

// 3. Nomenclatura consistente:
//    - NO: "pinturaFluorescente.someRandomVariable"
//    - SI: "pinturaFluorescente.noiseAmount"

// Ejemplo de buenos parámetros vs inflados
// ✅ BUENO: 5-10 parámetros por escena
Parameter goodExample[] = {
    "pinturaFluorescente.k",        // Noise amount principal
    "pinturaFluorescente.speed",    // Animation speed
    "pinturaFluorescente.colorR",   // Color componente R
    "pinturaFluorescente.colorG",   // Color componente G
    "pinturaFluorescente.colorB",   // Color componente B
    "pinturaFluorescente.glowEnabled",
};

// ❌ INFLADO: 50+ parámetros
// registerParam("pinturaFluorescente.internalTemp");
// registerParam("pinturaFluorescente.debugCounter");
// registerParam("pinturaFluorescente.unusedParam1");
// ... etc
```

**Riesgo 4: Observer Side Effects**

```cpp
// Problema: Observer callback puede causar efectos secundarios no deseados
// Solución: Transaction-like update con ordering

class ParameterRegistry {
    void setParam(const std::string& id, float value) {
        // 1. Verificar que el cambio es necesario
        auto it = params_.find(id);
        if (it == params_.end() || it->second.currentValue == value) {
            return;  // No change, no notify
        }
        
        // 2. Update primero
        it->second.currentValue = value;
        
        // 3. Notification AFTER state change
        // IMPORTANTE: Order de observers no está garantizado
        for (auto* observer : observers_) {
            // Catch exceptions para no romper cadena
            try {
                observer->onParamChanged(id, value);
            } catch (std::exception& e) {
                log_.error("Observer exception: " + std::string(e.what()));
            }
        }
    }
    
    // Alternativa: Batch update para evitar múltiples notify
    void beginBatchUpdate() {
        batchMode_ = true;
    }
    
    void endBatchUpdate() {
        batchMode_ = false;
        // Notify una sola vez con todos los cambios
        for (auto& change : pendingChanges_) {
            notifyParamChanged(change.first, change.second);
        }
        pendingChanges_.clear();
    }
    
private:
    bool batchMode_ = false;
    std::vector<std::pair<std::string, float>> pendingChanges_;
};
```

**Riesgo 5: Thread Safety**

```cpp
// OpenFrameworks es single-threaded, pero considerar para futuro
// y para facilitar testing

class ParameterRegistry {
public:
    // Thread-safe set (si OF fuera multi-threaded)
    bool setParamThreadSafe(const std::string& id, float value) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        auto it = params_.find(id);
        if (it == params_.end()) return false;
        
        it->second.currentValue = value;
        
        // En single-threaded, esto no bloquea nada
        // Pero si fuera multi-threaded, el lock protege el access
        return true;
    }
    
private:
    std::mutex mutex_;  // Solo si es necesario
    
    // Alternativa: lock-free con atomic (si fuera necesario)
    // std::atomic<float> currentValue;  // Para cada parámetro
};

// Recomendación: No agregar mutex por defecto
// Agregar SOLO si hay evidencia de necesidad (profiling)
```

---

## Control UI - Profundidad Completa

### 1. Módulos Principales de UI

Cada módulo es una clase independiente para facilitar testing unitario:

```cpp
class IUIModule {
public:
    virtual ~IUIModule() = default;
    virtual void update(float deltaTime) = 0;
    virtual void draw() = 0;
    virtual void setVisible(bool visible) = 0;
    virtual bool isVisible() const = 0;
};
```

#### ScenePanel
```cpp
class ScenePanel : public IUIModule {
public:
    // Mostrar lista de escenas disponibles
    // Indicar escena activa con highlight
    // Click para cambiar entre escenas
    
    void setScenes(const std::vector<SceneInfo>& scenes);
    void setActiveScene(const std::string& sceneId);
    void onSceneSelected(std::function<void(const std::string&)> callback);
};
```

#### ParamPanel
```cpp
class ParamPanel : public IUIModule {
public:
    // Lista de parámetros con valores actuales
    // Edición inline (click → slider)
    // Actualización reactiva desde Registry
    
    void setParameters(const std::vector<Parameter>& params);
    void onParamChanged(std::function<void(const std::string&, float)> callback);
    
    // Para pagination
    void setPage(int page);
    void setPageSize(int size);  // default: 20
    void search(const std::string& query);  // Filter por label o id
};
```

#### MappingPanel
```cpp
class MappingPanel : public IUIModule {
public:
    // Editor visual source → target
    // Lista de mappings con enable/disable
    // Click para editar binding
    
    void setMappings(const std::vector<ParameterBinding>& bindings);
    void onMappingEdit(std::function<void(const std::string&)> callback);
    
    // Filtering
    void filterBySource(const std::string& source);  // "osc", "keyboard"
    void filterByTarget(const std::string& paramId);
};
```

#### TransportPanel
```cpp
class TransportPanel : public IUIModule {
public:
    // BPM display y edición
    // Beat/Bar indicator visual
    // Play/Stop buttons
    // Sync mode selector (Internal/External/Free)
    
    void setTransportState(const TransportState& state);
    void onPlay(std::function<void()> callback);
    void onStop(std::function<void()> callback);
    void onBPMChange(std::function<void(float)> callback);
    void onSyncModeChange(std::function<void(SyncMode)> callback);
};
```

#### SchedulePanel
```cpp
class SchedulePanel : public IUIModule {
public:
    // Timeline visual de cues
    // Grid beats × bars
    // Click para agregar/editar cue
    
    void setCues(const std::vector<SceneCue>& cues);
    void setCurrentPosition(int beat, int bar);
    void onCueClick(std::function<void(const std::string& cueId)> callback);
    void onCueAdd(std::function<void(const CueConfig&)> callback);
};
```

#### ConsolePanel
```cpp
class ConsolePanel : public IUIModule {
public:
    // Log de eventos y errores
    // Auto-scroll al final
    // Filter por level (INFO/WARN/ERROR)
    
    void log(const std::string& message, LogLevel level = LogLevel::INFO);
    void clear();
    void setFilter(LogLevel minLevel);
    
    enum class LogLevel { DEBUG, INFO, WARN, ERROR };
};
```

---

### 2. Flujo de Interacción del Usuario

#### Startup
```cpp
class ControlUI {
    void initialize() {
        // 1. Cargar mappings desde JSON
        mappingLayer_.loadMappings("config/mappings.json");
        
        // 2. Conectar adapters al mapping layer
        oscAdapter_.setMappingLayer(&mappingLayer_);
        keyboardAdapter_.setMappingLayer(&mappingLayer_);
        
        // 3. Subscribe a ParameterRegistry para updates reactivos
        registry_.subscribe(this);  // ControlUI implements IParameterObserver
        
        // 4. Inicializar cada panel
        scenePanel_.setScenes(sceneEngine_.getAvailableScenes());
        paramPanel_.setParameters(registry_.getAllParams());
        mappingPanel_.setMappings(mappingLayer_.getAllBindings());
        transportPanel_.setTransportState(transport_.getState());
        schedulePanel_.setCues(scheduler_.getScheduledCues());
    }
};
```

#### Runtime
```cpp
class ControlUI {
    void update(float deltaTime) {
        // Cada panel updatea su estado interno
        scenePanel_.update(deltaTime);
        paramPanel_.update(deltaTime);
        mappingPanel_.update(deltaTime);
        transportPanel_.update(deltaTime);
        schedulePanel_.update(deltaTime);
        consolePanel_.update(deltaTime);
    }
    
    // Los valores se actualizan automáticamente via observer
    void onParamChanged(const std::string& paramId, float newValue) override {
        // Solo updatear el control específico, no todo el panel
        paramPanel_.updateParamValue(paramId, newValue);
        
        // Log para debugging
        consolePanel_.log("Param " + paramId + " = " + std::to_string(newValue));
    }
};
```

#### Edit Mode
```cpp
class ControlUI {
    void enableEditMode() {
        editMode_ = true;
        
        // Click en param → slider de edición
        paramPanel_.enableEditing(true);
        
        // Click en mapping → editor visual
        mappingPanel_.enableEditing(true);
    }
    
    void disableEditMode() {
        editMode_ = false;
        paramPanel_.enableEditing(false);
        mappingPanel_.enableEditing(false);
    }
};
```

#### Debug Mode
```cpp
class ControlUI {
    void toggleDebugMode() {
        debugMode_ = !debugMode_;
        
        if (debugMode_) {
            showFPS_ = true;
            showMessageCount_ = true;
            showLastBeatTime_ = true;
            consolePanel_.setFilter(ConsolePanel::LogLevel::DEBUG);
        }
    }
    
    void drawDebugInfo() {
        if (!debugMode_) return;
        
        ImGui::Begin("Debug Info", &showDebugInfo_);
        
        ImGui::Text("FPS: %.1f", ofGetFrameRate());
        ImGui::Text("Messages: %llu", oscAdapter_.getMessageCount());
        ImGui::Text("Last Beat: %.3f", transport_.getLastBeatTime());
        ImGui::Text("Active Mappings: %zu", mappingLayer_.getActiveBindingCount());
        
        ImGui::Separator();
        
        // Memory info
        ImGui::Text("Params: %zu", registry_.getParamCount());
        ImGui::Text("Scenes: %zu", sceneEngine_.getSceneCount());
        
        ImGui::End();
    }
};
```

---

### 3. Pantallas o Paneles Sugeridos

#### Layout Principal
```
┌────────────────────────────────────────────────────────────────────────┐
│ [≡] Menu                                                    [?] Help   │
├──────────────┬─────────────────────────────────┬──────────────────────┤
│              │                                 │  [Params][Mappings]  │
│   SCENES     │                                 │  [Transport][Schedule│
│              │                                 │                      │
│  ○ Escena 1  │                                 │  ┌────────────────┐  │
│  ● Escena 2  │      VISUALIZATION              │  │ Panel Content │  │
│  ○ Escena 3  │      (la escena renderiza)      │  │                │  │
│              │                                 │  │                │  │
│              │                                 │  └────────────────┘  │
│              │                                 │                      │
│  [+ Add]     │                                 │                      │
├──────────────┴─────────────────────────────────┴──────────────────────┤
│ Status: [120 BPM] [INT] [Escena 2] [60 FPS]            [Debug: OFF]  │
└────────────────────────────────────────────────────────────────────────┘
```

#### Detalle de Paneles

**Panel Izquierdo: Scene List**
```
┌────────────────┐
│ 🎭 SCENES      │
├────────────────┤
│ ● PinturaFlujo │ ← Active (highlighted)
│ ○ Particulas  │
│ ○ Perlin      │
│ ○ FondoLines  │
│ ○ Cuadrados   │
├────────────────┤
│ [+] New Scene │
│ [Import...]   │
└────────────────┘
```

**Panel Central: Visualization**
- Área principal de renderizado de la escena activa
- Sin controles, solo visualización
- Click derecho para contexto

**Panel Derecho: Tabs**
```
┌─────────────────────────────────────┐
│ [Params] [Mappings] [Transport] [Schedule] │ ← Tab bar
├─────────────────────────────────────┤
│                                     │
│  Contenido del tab seleccionado     │
│                                     │
│                                     │
└─────────────────────────────────────┘
```

**Bottom Bar: Status**
```
┌────────────────────────────────────────────────────────────────────────┐
│ BPM: [120.0 ▼] │ Sync: [INTERNAL ▼] │ Scene: PinturaFlujo │ FPS: 60  │
└────────────────────────────────────────────────────────────────────────┘
```

#### Keyboard Shortcuts
```cpp
class KeyboardHandler {
    void setupShortcuts() {
        // Tabs
        shortcuts_[Key::NUM_1] = Tab::PARAM_PANEL;
        shortcuts_[Key::NUM_2] = Tab::MAPPING_PANEL;
        shortcuts_[Key::NUM_3] = Tab::TRANSPORT_PANEL;
        shortcuts_[Key::NUM_4] = Tab::SCHEDULE_PANEL;
        shortcuts_[Key::NUM_5] = Tab::CONSOLE_PANEL;
        
        // Global
        shortcuts_[Key::ESCAPE] = Action::TOGGLE_UI;
        shortcuts_[Key::F1] = Action::TOGGLE_DEBUG;
        shortcuts_[Key::SPACE] = Action::PLAY_PAUSE;
        
        // Scene switching
        shortcuts_[Key::F2] = Action::NEXT_SCENE;
        shortcuts_[Key::F3] = Action::PREV_SCENE;
    }
};
```

---

### 4. Integración con Registry, Mapping y Scheduler

#### Registry: Suscripción Reactiva
```cpp
class ControlUI : public IParameterObserver {
    void onParamChanged(const std::string& paramId, float newValue) override {
        // UI updatea SOLO el control que cambió
        // No re-renderiza todo el panel
        paramPanel_.updateParam(paramId, newValue);
        
        // Update sparkline si está visible
        if (sparklinePanel_.isVisibleForParam(paramId)) {
            sparklinePanel_.addSample(paramId, newValue);
        }
    }
    
    void onParamRegistered(const std::string& paramId) override {
        paramPanel_.addParameter(paramId);
        consolePanel_.log("Param registered: " + paramId);
    }
    
    void onParamUnregistered(const std::string& paramId) override {
        paramPanel_.removeParameter(paramId);
    }
};
```

#### Mapping: API de Carga/Edición
```cpp
class MappingPanel {
    // Load/Save
    bool loadMappings(const std::string& filename) {
        return mappingLayer_.loadMappings(filename);
    }
    
    bool saveMappings(const std::string& filename) {
        return mappingLayer_.saveMappings(filename);
    }
    
    // Add/Remove
    void addMapping(const ParameterBinding& binding) {
        mappingLayer_.addBinding(binding);
        refreshList();
        consolePanel_.log("Mapping added: " + binding.bindingId);
    }
    
    void removeMapping(const std::string& bindingId) {
        mappingLayer_.removeBinding(bindingId);
        refreshList();
        consolePanel_.log("Mapping removed: " + bindingId);
    }
    
    // Enable/Disable runtime
    void setMappingEnabled(const std::string& bindingId, bool enabled) {
        mappingLayer_.setBindingEnabled(bindingId, enabled);
    }
};
```

#### Scheduler: Gestión de Cues
```cpp
class SchedulePanel {
    void addCue(const CueConfig& config) {
        scheduler_.addCue(config);
        refreshTimeline();
        consolePanel_.log("Cue added at " + 
            std::to_string(config.beat) + "/" + std::to_string(config.bar));
    }
    
    void removeCue(const std::string& cueId) {
        scheduler_.removeCue(cueId);
        refreshTimeline();
    }
    
    std::vector<SceneCue> getScheduledCues() const {
        return scheduler_.getScheduledCues();
    }
    
    void updateCurrentPosition(int beat, int bar) {
        // Highlight en timeline
        timeline_.setPosition(beat, bar);
    }
};
```

**Regla de Acoplamiento:**
```
UI SOLO muestra estado, NO toma decisiones de negocio.
Los botones de UI llaman a métodos en las capas inferiores.
```

---

### 5. Trazabilidad Visual

#### Highlight de Mapping cuando input activa
```cpp
class MappingPanel {
    void onInputReceived(const std::string& sourceId) {
        // Flash de color cuando el input activa un mapping
        auto binding = mappingLayer_.resolve(sourceId);
        if (binding) {
            highlightMap_[binding->bindingId] = HighlightState{
                .startTime = ofGetElapsedTimef(),
                .duration = 0.3f  // 300ms flash
            };
        }
    }
    
    void drawMappingItem(const ParameterBinding& binding) {
        auto it = highlightMap_.find(binding.bindingId);
        
        if (it != highlightMap_.end()) {
            float elapsed = ofGetElapsedTimef() - it->second.startTime;
            if (elapsed < it->second.duration) {
                // Flash verde/amarillo según si hay output
                ImGui::PushStyleColor(ImGuiCol_Text, 
                    IM_COL32(0, 255, 0, 255));  // Green flash
            }
        }
        
        // Draw mapping row
        ImGui::Text("%s → %s", binding.source.sourceId.c_str(),
            binding.target.paramId.c_str());
        
        if (it != highlightMap_.end()) {
            ImGui::PopStyleColor();
        }
    }
};
```

#### Waveform de valor de parámetro (Sparkline)
```cpp
class SparklineRenderer {
    void draw(const std::string& paramId, const ImVec2& size) {
        auto& history = paramHistory_[paramId];
        
        // Drawing sparkline
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        
        float step = size.x / history.size();
        
        for (size_t i = 0; i < history.size() - 1; i++) {
            float y1 = cursor.y + size.y - (history[i] * size.y);
            float y2 = cursor.y + size.y - (history[i+1] * size.y);
            
            drawList->AddLine(
                ImVec2(cursor.x + i * step, y1),
                ImVec2(cursor.x + (i + 1) * step, y2),
                IM_COL32(100, 200, 255, 255),
                1.0f
            );
        }
    }
    
    void addSample(const std::string& paramId, float value) {
        auto& history = paramHistory_[paramId];
        history.push_back(value);
        
        if (history.size() > maxHistory_) {
            history.pop_front();
        }
    }
    
private:
    static constexpr size_t maxHistory_ = 100;
    std::unordered_map<std::string, std::deque<float>> paramHistory_;
};
```

#### Beat Indicator Visual
```cpp
class BeatIndicator {
    void draw() {
        auto state = transport_.getState();
        
        // Pulsar en el beat
        float pulse = (state.phase < 0.1f) ? 1.0f : 0.5f;
        ImVec4 color = (state.beat == 1) 
            ? ImVec4(1.0f, 0.8f, 0.0f, pulse)  // Beat 1: yellow
            : ImVec4(0.0f, 1.0f, 0.0f, pulse);  // Beats 2-4: green
        
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::Button(("Beat " + std::to_string(state.beat)).c_str());
        ImGui::PopStyleColor();
        
        // Beat count visual
        for (int i = 1; i <= 4; i++) {
            if (i == state.beat) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
            }
            ImGui::Text("%d", i);
            ImGui::PopStyleColor();
            if (i < 4) ImGui::SameLine();
        }
    }
};
```

#### Log de Mapping que resolvió valor
```cpp
class MappingResolverLog {
    void logResolution(const std::string& sourceId, 
                       const std::string& paramId, float value) {
        consolePanel_.log("Mapping: " + sourceId + " → " + paramId + 
            " = " + std::to_string(value), LogLevel::DEBUG);
        
        // Also add to UI list
        recentResolutions_.push_back({
            .source = sourceId,
            .target = paramId,
            .value = value,
            .timestamp = ofGetElapsedTimef()
        });
    }
    
    void drawRecentResolutions() {
        ImGui::Begin("Recent Mappings");
        
        for (const auto& r : recentResolutions_) {
            ImGui::Text("[%.2f] %s → %s = %.3f",
                r.timestamp, r.source.c_str(), r.target.c_str(), r.value);
        }
        
        ImGui::End();
    }
};
```

#### Color Coding de Estado
```cpp
class StatusColors {
    static constexpr ImVec4 OK() { 
        return ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  // Green 
    }
    
    static constexpr ImVec4 WARNING() { 
        return ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow 
    }
    
    static constexpr ImVec4 ERROR() { 
        return ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Red 
    }
    
    // Aplicación según estado
    void drawParamStatus(const Parameter& param) {
        ImVec4 color = OK();
        
        if (param.currentValue < *param.min + epsilon) {
            color = WARNING();  // Near min
        }
        if (param.currentValue > *param.max - epsilon) {
            color = WARNING();  // Near max
        }
        if (!param.isValid) {
            color = ERROR();  // Invalid state
        }
        
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("%s: %.2f", param.label.c_str(), param.currentValue);
        ImGui::PopStyleColor();
    }
};
```

---

### 6. Riesgos de Sobrecargar la UI

#### Too Many Params → Pagination o Search
```cpp
class ParamPanel {
    void handleLargeParamSet() {
        if (params_.size() > 50) {
            // Habilitar pagination
            paginationEnabled_ = true;
            pageSize_ = 20;
            
            // O search/filter
            searchEnabled_ = true;
        }
    }
    
    void drawSearchBox() {
        static char searchBuf[128] = "";
        ImGui::InputTextWithHint("Search", "Filter params...", searchBuf, 128);
        
        if (strlen(searchBuf) > 0) {
            applySearchFilter(searchBuf);
        }
    }
};
```

#### Too Many Mappings → Filtering
```cpp
class MappingPanel {
    void setFilterOptions() {
        // Filter por source
        ImGui::Combo("Source", &selectedSource_, 
            "All\0OSC\0Keyboard\0Kinect\0");
        
        // Filter por target scene
        ImGui::Combo("Scene", &selectedScene_, sceneOptions_);
        
        // Aplicar filters
        applyFilters();
    }
};
```

#### Performance de Render → Update Solo Changed Params
```cpp
class ControlUI {
    // NO hacer esto:
    void renderAllParams() {
        for (const auto& param : registry_.getAllParams()) {
            renderSlider(param);  // Slow: re-renderiza todos
        }
    }
    
    // HACER esto:
    void onParamChanged(const std::string& paramId, float value) {
        // Solo updatear el control específico
        auto control = getControlForParam(paramId);
        if (control) {
            control->setValue(value);  // Fast: solo updatea un control
        }
    }
};
```

#### Regla de Separación de Responsabilidades

La UI sigue el principio de **"solo muestra, nunca decide"**. Esto significa que la UI NUNCA toma decisiones de negocio; simplemente refleja el estado del sistema y delega operaciones a los servicios apropiados.

**Principio Fundamental:**
```
UI → Llama métodos de servicios → Servicios决定 qué hacer → UI muestra resultado
```

**Ejemplos de Responsabilidades Separadas:**

```cpp
// ✅ CORRECTO: UI solo llama servicios, no decide
class ScenePanelUI {
    void onSceneClick(const std::string& sceneId) {
        // UI solo notifica al servicio apropiado
        // NO decide si la escena debe activarse o no
        
        // Opción 1: Llamar directamente al activator
        sceneActivator_->activateScene(sceneId);
        
        // Opción 2: Llamar al scheduler (para HYBRID mode)
        scheduler_->prepareScene(sceneId);
        
        // La LÓGICA de qué hacer está en el SERVICIO, no en la UI
    }
};

// ✅ CORRECTO: UI muestra error returned by servicio
class ParamPanelUI {
    void onParamEdit(const std::string& paramId, float newValue) {
        auto result = registry_->setParam(paramId, newValue);
        
        if (!result.success) {
            // El SERVICIO retorna el error, UI solo lo muestra
            showError(result.errorMessage);
        }
        // Si success, el observer pattern actualiza la UI automáticamente
    }
};

// ✅ CORRECTO: MappingLayer decide si el mapping es válido
class MappingPanelUI {
    void onAddMapping(const MappingBinding& binding) {
        auto result = mappingLayer_->addMapping(binding);
        
        if (!result.success) {
            // MappingLayer decidió que no se puede agregar
            showError("Invalid mapping: " + result.reason);
        }
    }
};

// ✅ CORRECTO: Transport valida y retorna resultado
class TransportPanelUI {
    void onBPMChange(float newBPM) {
        auto result = transport_->setBPM(newBPM);
        
        if (!result.success) {
            // Transport decidió que el valor es inválido
            showError("BPM must be between 20-300");
        }
        
        // UI actualiza display con el valor real (puede haber sido clamped)
        updateBPMDisplay(result.actualBPM);
    }
};

// ❌ INCORRECTO: UI decide lógica de negocio
class BadUIExample {
    void onBPMChange(float newBPM) {
        // UI decide lógica en lugar del servicio
        if (newBPM > 100) {
            // Esto NO debe estar en UI - es responsabilidad de Transport
            sceneEngine_->triggerEffect("fastMode");  // WRONG!
        }
        transport_->setBPM(newBPM);
    }
};

// ❌ INCORRECTO: UI filtra inputs sin consultar al servicio
class BadMappingUI {
    void onAddMapping(const MappingBinding& binding) {
        // UI decide unilateralmente sin consultar al servicio
        if (binding.paramId.find("internal") != string::npos) {
            // Esto NO debe estar en UI
            showError("Cannot map internal params");  // WRONG!
            return;
        }
        // ...
    }
};
```

**Flujo de datos cuando el usuario hace algo inválido:**

```
Usuario cambia parámetro a valor inválido
         │
         ▼
UI llama registry.setParam("bpm", 500)
         │
         ▼
Servicio valida y retorna { success: false, error: "BPM must be 20-300" }
         │
         ▼
UI muestra mensaje de error
         │
         ▼
El valor NO se cambia (el servicio decide el comportamiento)
```

**Lista de métodos que UI puede llamar directamente:**
- `ParameterRegistry.getParam()` - consultar valor
- `ParameterRegistry.setParam()` - solicitar cambio (puede fallar)
- `MappingLayer.addMapping()` - agregar mapping (puede fallar)
- `MappingLayer.removeMapping()` - eliminar mapping
- `Transport.setBPM()` - cambiar BPM (puede fallar)
- `Transport.play()`, `Transport.stop()`
- `SceneActivator.activateScene()` - activar escena
- `Scheduler.prepareCue()` - preparar cue

**Lo que UI NO debe hacer:**
- No validar lógica de negocio (delegar al servicio)
- No decidir transiciones de escenas (delegar al scheduler)
- No filtrar mapeos basándose en reglas de negocio (delegar al mapping layer)
- No decidir quantization o timing (delegar al scheduler)

---

#### Coupling UI ↔ Business Logic
```cpp
// ❌ INCORRECTO: UI decide lógica de negocio
class BadUI {
    void onPlayButton() {
        // UI decide cuándo cambiar escena
        if (shouldChangeScene()) {
            sceneEngine_.activateScene(nextScene_);  // WRONG
        }
    }
};

// ✅ CORRECTO: UI llama a servicios, no decide
class GoodUI {
    void onPlayButton() {
        // UI solo notifica, scheduler decide
        transport_.play();  // Transport decide timing
    }
};
```

#### Platform-Specific: ImGui vs OF Native
```cpp
// Decisión: ImGui por defecto para productividad
// Pero mantener abstracción para cambio futuro

enum class UIFramework {
    IMGUI,      // Dear ImGui - actual
    NATIVE,     // ofxUIControls - OpenFrameworks native
    QT,         // Qt - para aplicaciones standalone
    WEB         // HTML/CSS/JS - para remoto
};

class IUIPanelFactory {
public:
    virtual std::unique_ptr<IUIModule> createPanel(PanelType type) = 0;
};

// Implementación actual: ImGui
class ImGuiPanelFactory : public IUIPanelFactory {
    std::unique_ptr<IUIModule> createPanel(PanelType type) override {
        switch (type) {
            case PanelType::SCENE: return std::make_unique<ImGuiScenePanel>();
            case PanelType::PARAM: return std::make_unique<ImGuiParamPanel>();
            // ...
        }
    }
};
```

---

### 7. Resumen de Decisiones de UI

| Decisión | Rationale |
|----------|-----------|
| Cada módulo como clase independiente | Testability, reusability |
| Registry subscribe para updates reactivos | Sin polling, performance |
| UI solo muestra, servicios deciden | Clean separation of concerns |
| Keyboard shortcuts para navegación | UX productividad |
| Sparkline para params importantes | Visual feedback rápido |
| Beat indicator visual | Feedback de sync |
| Color coding de estados | Atajo visual para debugging |
| Pagination cuando >50 params | Evitar scroll infinite |
| ImGui como framework default | Productividad, prototyping rápido |

---

## Open Questions - RESOLVED

- [x] **Q1**: ¿El Scheduler debe conocer directamente a SceneEngine o comunicarse via eventos?
  - **DECISIÓN**: Interfaz `ISceneActivator` inyectada en Scheduler, implementada por SceneEngine
  -Scheduler no conoce directamente SceneEngine, usa la interfaz

- [x] **Q2**: ¿Cómo manejamos parámetros que pertenecen a escenas inactivas?
  - **DECISIÓN**: ParameterRegistry mantiene todos los parámetros registrados, inactivos tienen valores pero no se renderizan

- [x] **Q3**: ¿Soporte para múltiples escenas activas simultáneamente?
  - **DECISIÓN**: Single active scene - mantener constraint de spec

---

## Decisiones Arquitectónicas Justificadas (Resumen)

| Decisión | Rationale |
|----------|------------|
| MappingLayer como mediator | Agregar adaptadores sin modificar otras capas |
| Transport singleton | Simplicidad para volumen fijo de beats |
| ParameterRegistry con observers | UI reactiva sin polling |
| Scheduler subscribe a Transport | Desacoplamiento temporal |
| Kinect contract diferido | Progreso paralelo, contrato listo |

---

## Kinect Adapter Contract - Future Implementation

> **Nota**: Esta sección define el contrato para integración futura de Kinect. La implementación está fuera del scope actual, pero el contrato debe estar listo para que MappingLayer pueda prepararse.

### 1. Contrato Abstracto del Adaptador Kinect

```cpp
// IKinectAdapter.h - Header público sin dependencias de SDK Kinect
// Este header NO incluye Kinect.h ni ninguna biblioteca de Microsoft

#pragma once

#include <string>
#include <optional>
#include <functional>
#include <vector>

// Tipos forward-declarados para evitar dependencias
struct Vector3D {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct TrackedBody {
    int bodyId;                    // Índice del body (0-5 para Kinect V1, 0-6 para V2)
    bool isTracked;                // Si el body está siendo trackeado
    float confidence;              // 0.0 - 1.0 overall confidence
    std::vector<Joint> joints;    // Posiciones de joints
};

struct Joint {
    JointType type;
    Vector3D position;             // Normalizada 0.0-1.0
    Vector3D velocity;              // Calculada, normalizada
    float confidence;              // 0.0 - 1.0 tracking confidence
};

enum class JointType {
    HEAD,
    NECK,
    SHOULDER_LEFT,
    SHOULDER_RIGHT,
    ELBOW_LEFT,
    ELBOW_RIGHT,
    WRIST_LEFT,
    WRIST_RIGHT,
    HAND_LEFT,
    HAND_RIGHT,
    HIP_LEFT,
    HIP_RIGHT,
    KNEE_LEFT,
    KNEE_RIGHT,
    ANKLE_LEFT,
    ANKLE_RIGHT,
    SPINE_BASE,
    SPINE_MID,
    SPINE_TOP
};

enum class HandState {
    OPEN,      // Mano abierta
    CLOSED,    // Puño
    POINTING,  // Apuntando
    UNKNOWN    // No detectable
};

enum class ConnectionState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
};

// Señales que el adaptador puede generar (callbacks/events)
class IKinectAdapter {
public:
    virtual ~IKinectAdapter() = default;
    
    // ============================================
    // Lifecycle
    // ============================================
    
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual ConnectionState getConnectionState() const = 0;
    
    // ============================================
    // Body Tracking Signals (continuous)
    // ============================================
    
    // Called cuando un body nuevo aparece o desaparece
    virtual void setBodyCallback(std::function<void(const TrackedBody&)> callback) = 0;
    
    // Called cuando posición de joint cambia (30fps typical)
    virtual void setJointCallback(std::function<void(int bodyId, JointType, const Joint&)> callback) = 0;
    
    // ============================================
    // Hand Tracking (continuous)
    // ============================================
    
    virtual void setHandCallback(std::function<void(int bodyId, bool isLeft, 
        const Vector3D& position, HandState state)> callback) = 0;
    
    // ============================================
    // Gesture Events (discrete)
    // ============================================
    
    enum class GestureType {
        WAVE,       // Ondulación
        SWIPE,      // Movimiento rápido
        CLICK,      // Click gesto
        GRAB,       // Agarrar
        RELEASE     // Soltar
    };
    
    virtual void setGestureCallback(std::function<void(int bodyId, GestureType, 
        const Vector3D& position)> callback) = 0;
    
    // ============================================
    // Zone Events (discrete)
    // ============================================
    
    // Screen dividido en regions, body occupancy
    struct Zone {
        std::string zoneId;        // "zone-0-0", "zone-1-2", etc.
        int gridX;                 // 0-based column
        int gridY;                 // 0-based row
        std::string label;         // "left", "center-right", etc.
    };
    
    virtual void setZoneCallback(std::function<void(const std::string& zoneId, 
        int bodyId, bool entered)> callback) = 0;
    
    // ============================================
    // Raw Data Access (for custom processing)
    // ============================================
    
    virtual std::optional<Vector3D> getRawJointPosition(int bodyId, JointType joint) const = 0;
    virtual std::vector<TrackedBody> getAllTrackedBodies() const = 0;
    
    // ============================================
    // Configuration
    // ============================================
    
    virtual void setSmoothing(float positionFactor, float velocityFactor) = 0;
    virtual void setConfidenceThreshold(float threshold) = 0;  // Default 0.5
    virtual void setZoneGrid(int columns, int rows) = 0;
};
```

### 2. Lista Inicial de Señales Normalizadas

**Convenciones de nomenclatura:**
```
kinect.body.{bodyIndex}.{property}
kinect.hand.{left|right}.{property}
kinect.joint.{jointType}.{property}
kinect.gesture.{gestureType}
kinect.zone.{zoneId}.{property}
```

| Categoría | Signal ID | Tipo | Rango | Descripción |
|-----------|-----------|------|-------|-------------|
| **Body** | `kinect.body.{i}.position.x` | float | 0.0-1.0 | Posición X del body centroid |
| | `kinect.body.{i}.position.y` | float | 0.0-1.0 | Posición Y del body centroid |
| | `kinect.body.{i}.position.z` | float | 0.0-1.0 | Profundidad (normalizada) |
| | `kinect.body.{i}.velocity.x` | float | -1.0-1.0 | Velocidad X (derivada) |
| | `kinect.body.{i}.velocity.y` | float | -1.0-1.0 | Velocidad Y |
| | `kinect.body.{i}.velocity.z` | float | -1.0-1.0 | Velocidad Z |
| | `kinect.body.{i}.presence` | float | 0.0-1.0 | 1.0 = body presente, 0.0 = no |
| | `kinect.body.{i}.confidence` | float | 0.0-1.0 | Tracking confidence |
| **Hands** | `kinect.hand.left.position.x` | float | 0.0-1.0 | Mano izquierda X |
| | `kinect.hand.left.position.y` | float | 0.0-1.0 | Mano izquierda Y |
| | `kinect.hand.left.position.z` | float | 0.0-1.0 | Mano izquierda Z |
| | `kinect.hand.left.state` | float | 0.0-1.0 | 0=open, 0.33=closed, 0.66=pointing, 1=unknown |
| | `kinect.hand.right.position.x` | float | 0.0-1.0 | Mano derecha X |
| | `kinect.hand.right.position.y` | float | 0.0-1.0 | Mano derecha Y |
| | `kinect.hand.right.position.z` | float | 0.0-1.0 | Mano derecha Z |
| | `kinect.hand.right.state` | float | 0.0-1.0 | Estado de mano derecha |
| **Joints** | `kinect.joint.head.position.x` | float | 0.0-1.0 | Cabeza X |
| | `kinect.joint.head.position.y` | float | 0.0-1.0 | Cabeza Y |
| | `kinect.joint.head.confidence` | float | 0.0-1.0 | Tracking confidence |
| | `kinect.joint.neck.position.x` | float | 0.0-1.0 | Cuello X |
| | `kinect.joint.shoulder_left.position.x` | float | 0.0-1.0 | Hombro izquierdo X |
| | `kinect.joint.elbow_left.position.x` | float | 0.0-1.0 | Codo izquierdo X |
| | `kinect.joint.wrist_left.position.x` | float | 0.0-1.0 | Muñeca izquierda X |
| | (etc. para todos los joints) | | | |
| **Gestures** | `kinect.gesture.wave` | trigger | - | Onda detectada |
| | `kinect.gesture.swipe` | trigger | - | Swipe detectada |
| | `kinect.gesture.click` | trigger | - | Click gesto |
| | `kinect.gesture.grab` | trigger | - | Agarre detectado |
| | `kinect.gesture.release` | trigger | - | Soltar detectado |
| **Zones** | `kinect.zone.{zoneId}.occupied` | float | 0.0-1.0 | 1.0 si body en zona |
| | `kinect.zone.{zoneId}.bodyIndex` | float | 0.0-5.0 | Índice del body en zona |

**Normalización a screen space:**

```cpp
class KinectNormalizer {
    // Convierte coordenadas de Kinect (depth space) a normalized screen space
    static Vector3D toNormalizedScreen(const Vector3D& kinectPos, 
                                       int screenWidth, int screenHeight,
                                       float depthRange) {
        // Kinect depth space típico: X [-2.5, 2.5], Y [-1.5, 1.5], Z [0.5, 8.0]
        
        // Mapear X a 0.0-1.0
        float normalizedX = (kinectPos.x + 2.5f) / 5.0f;
        
        // Mapear Y a 0.0-1.0
        float normalizedY = (kinectPos.y + 1.5f) / 3.0f;
        
        // Mapear Z a 0.0-1.0 (invertido: más cerca = mayor valor)
        float normalizedZ = 1.0f - (kinectPos.z - 0.5f) / (depthRange - 0.5f);
        
        return {normalizedX, normalizedY, normalizedZ};
    }
};
```

### 3. Recomendaciones de Smoothing y Tratamiento de Ruido

**Problema:** Raw Kinect data es naturalmente noisy.

```cpp
class KinectSmoother {
    // Exponential smoothing para position
    struct PositionFilter {
        float positionSmoothing = 0.2f;  // 0.2-0.3 recomendado
        
        Vector3D update(const Vector3D& rawPosition) {
            if (!initialized_) {
                initialized_ = true;
                lastPosition_ = rawPosition;
                return rawPosition;
            }
            
            // Exponential smoothing: new = old + factor * (raw - old)
            lastPosition_.x += positionSmoothing * (rawPosition.x - lastPosition_.x);
            lastPosition_.y += positionSmoothing * (rawPosition.y - lastPosition_.y);
            lastPosition_.z += positionSmoothing * (rawPosition.z - lastPosition_.z);
            
            return lastPosition_;
        }
        
    private:
        bool initialized_ = false;
        Vector3D lastPosition_;
    };
    
    // Velocity filter (más noise, necesita más smoothing)
    struct VelocityFilter {
        float velocitySmoothing = 0.5f;  // 0.5 recomendado para velocity
        
        Vector3D update(const Vector3D& rawVelocity) {
            if (!initialized_) {
                initialized_ = true;
                lastVelocity_ = rawVelocity;
                return rawVelocity;
            }
            
            lastVelocity_.x += velocitySmoothing * (rawVelocity.x - lastVelocity_.x);
            lastVelocity_.y += velocitySmoothing * (rawVelocity.y - lastVelocity_.y);
            lastVelocity_.z += velocitySmoothing * (rawVelocity.z - lastVelocity_.z);
            
            return lastVelocity_;
        }
        
    private:
        bool initialized_ = false;
        Vector3D lastVelocity_;
    };
};
```

**Confidence threshold:**

```cpp
class KinectConfidenceFilter {
    static constexpr float DEFAULT_THRESHOLD = 0.5f;
    
    // No usar datos con confidence < threshold
    bool isValidReading(float confidence) const {
        return confidence >= threshold_;
    }
    
    // Filter joints by confidence
    std::vector<Joint> filterLowConfidenceJoints(const std::vector<Joint>& joints) {
        std::vector<Joint> validJoints;
        for (const auto& joint : joints) {
            if (joint.confidence >= threshold_) {
                validJoints.push_back(joint);
            }
        }
        return validJoints;
    }
    
    float threshold_ = DEFAULT_THRESHOLD;
};
```

**Latency compensation:**

```cpp
class KinectLatencyCompensator {
    // Buffer de frames para smoothing
    static constexpr size_t FRAME_BUFFER_SIZE = 3;  // 3-5 frames typical
    
    struct FrameBuffer {
        Vector3D positions[FRAME_BUFFER_SIZE];
        size_t writeIndex = 0;
        size_t count = 0;
    };
    
    // Promedio de últimos N frames
    Vector3D getSmoothedPosition(FrameBuffer& buffer, const Vector3D& newPosition) {
        buffer.positions[buffer.writeIndex] = newPosition;
        buffer.writeIndex = (buffer.writeIndex + 1) % FRAME_BUFFER_SIZE;
        buffer.count = std::min(buffer.count + 1, FRAME_BUFFER_SIZE);
        
        Vector3D avg = {0, 0, 0};
        for (size_t i = 0; i < buffer.count; i++) {
            avg.x += buffer.positions[i].x;
            avg.y += buffer.positions[i].y;
            avg.z += buffer.positions[i].z;
        }
        
        avg.x /= buffer.count;
        avg.y /= buffer.count;
        avg.z /= buffer.count;
        
        return avg;
    }
};
```

### 4. Integración con Mapping Layer

**Igual que OSC, pero con source="kinect":**

```cpp
class KinectAdapter : public IKinectAdapter {
public:
    void setMappingLayer(IMappingLayer* mapping) {
        mapping_ = mapping;
    }
    
    // Callback: body position change → mapping input
    void setBodyCallback(std::function<void(const TrackedBody&)> callback) override {
        bodyCallback_ = std::move(callback);
    }
    
    // Called from Kinect update loop
    void onBodyUpdate(const TrackedBody& body) {
        if (!body.isTracked) {
            // Body lost → send presence 0
            if (mapping_) {
                mapping_->onInput("kinect.body." + std::to_string(body.bodyId) + ".presence", 0.0f);
            }
            return;
        }
        
        // Send normalized position
        if (mapping_) {
            auto pos = toNormalizedScreen(body.joints[static_cast<int>(JointType::SPINE_BASE)].position);
            
            mapping_->onInput("kinect.body." + std::to_string(body.bodyId) + ".position.x", pos.x);
            mapping_->onInput("kinect.body." + std::to_string(body.bodyId) + ".position.y", pos.y);
            mapping_->onInput("kinect.body." + std::to_string(body.bodyId) + ".position.z", pos.z);
            mapping_->onInput("kinect.body." + std::to_string(body.bodyId) + ".presence", 1.0f);
            mapping_->onInput("kinect.body." + std::to_string(body.bodyId) + ".confidence", body.confidence);
        }
    }
    
    // Gesture → trigger mapping
    void onGesture(GestureType gesture, int bodyId) {
        if (mapping_) {
            std::string gestureName;
            switch (gesture) {
                case GestureType::WAVE: gestureName = "wave"; break;
                case GestureType::SWIPE: gestureName = "swipe"; break;
                case GestureType::CLICK: gestureName = "click"; break;
                case GestureType::GRAB: gestureName = "grab"; break;
                case GestureType::RELEASE: gestureName = "release"; break;
            }
            
            // Mapping de gestures a triggers
            mapping_->onInput("kinect.gesture." + gestureName, 1.0f);  // Fire trigger
        }
    }
    
private:
    IMappingLayer* mapping_ = nullptr;
    std::function<void(const TrackedBody&)> bodyCallback_;
};
```

**Ejemplo de JSON bindings para Kinect:**

```json
{
  "mappings": [
    {
      "bindingId": "kinect-hand-left-x-to-param",
      "source": {
        "sourceId": "kinect.hand.left.position.x",
        "sourceType": "KINECT",
        "channel": 0,
        "valueRange": "RANGE_0_1"
      },
      "target": {
        "paramId": "pinturaFluorescente.k",
        "targetType": "PARAM",
        "scope": "scene"
      },
      "transform": {
        "type": "RANGE_MAP",
        "inputMin": 0.0,
        "inputMax": 1.0,
        "outputMin": 0.0,
        "outputMax": 100.0
      },
      "smoothing": {
        "type": "EXPONENTIAL",
        "factor": 0.25
      }
    },
    {
      "bindingId": "kinect-gesture-wave-to-trigger",
      "source": {
        "sourceId": "kinect.gesture.wave",
        "sourceType": "KINECT",
        "channel": 0,
        "valueRange": "RANGE_0_1"
      },
      "target": {
        "paramId": "pinturaFluorescente.reset",
        "targetType": "TRIGGER",
        "scope": "scene"
      },
      "transform": {
        "type": "DIRECT"
      }
    },
    {
      "bindingId": "kinect-zone-to-param",
      "source": {
        "sourceId": "kinect.zone.center.occupied",
        "sourceType": "KINECT",
        "channel": 0,
        "valueRange": "RANGE_0_1"
      },
      "target": {
        "paramId": "pinturaFluorescente.glowEnabled",
        "targetType": "TOGGLE",
        "scope": "scene"
      },
      "transform": {
        "type": "DIRECT"
      }
    }
  ]
}
```

### 5. Riesgos Técnicos Futuros

| Riesgo | Descripción | Mitigation |
|--------|-------------|------------|
| **Latencia de tracking** | Kinect default 30fps vs render 60fps | Buffer + interpolation, predict position |
| **Occlusion** | Body parts hidden por objetos/otra persona | Confidence threshold, fallback a last known |
| **Multiple bodies** | Más de una persona en frame | Soporte multi-body con índices, priority por closest |
| **Platform** | Kinect solo Windows/Xbox, SDK variations | Abstraction layer, #ifdef platform code |
| **Fallback** | Kinect no disponible | Graceful degradation, mock adapter para desarrollo |

**Latency Mitigation:**

```cpp
class KinectLatencyHandler {
    // Predict posición basada en velocity
    Vector3D predictPosition(const Vector3D& currentPos, 
                             const Vector3D& velocity,
                             float targetFrameTime) {
        // Asumiendo tracking a 30fps, render a 60fps
        // Predictar: pos + velocity * (targetFrameTime / trackingFrameTime)
        float predictionFactor = 2.0f;  // 60fps/30fps = 2
        
        return {
            currentPos.x + velocity.x * predictionFactor,
            currentPos.y + velocity.y * predictionFactor,
            currentPos.z + velocity.z * predictionFactor
        };
    }
};
```

**Occlusion Handling:**

```cpp
class KinectOcclusionHandler {
    // Si confidence < threshold por más de N frames, marcar como occluded
    struct JointTracker {
        int occlusionCounter = 0;
        static constexpr int MAX_OCCLUSION_FRAMES = 10;
        
        bool update(float confidence) {
            if (confidence < 0.3f) {
                occlusionCounter++;
                return occlusionCounter > MAX_OCCLUSION_FRAMES;
            } else {
                occlusionCounter = 0;
                return false;
            }
        }
    };
};
```

### 6. Criterios de Escalabilidad

**Multi-body support:**

```cpp
class KinectMultiBodyHandler {
    // Máximo de bodies simultáneos
    static constexpr int MAX_BODIES = 6;  // Kinect V2 soporta hasta 6
    
    // Prioritize: closest body al sensor
    int getPrimaryBodyId(const std::vector<TrackedBody>& bodies) {
        float minZ = std::numeric_limits<float>::max();
        int primaryId = 0;
        
        for (const auto& body : bodies) {
            auto spineBase = getJoint(body, JointType::SPINE_BASE);
            if (spineBase.position.z < minZ) {
                minZ = spineBase.position.z;
                primaryId = body.bodyId;
            }
        }
        
        return primaryId;
    }
};
```

**Extensible joints:**

```cpp
// Agregar nuevos joints sin modificar interface base
// Solo agregar nuevos tipos a JointType enum
// El sistema de mapping usa string IDs, no hardcoded joints

// Ejemplo: futuro joint nuevo
// enum class JointType { ..., ELBOW_LEFT, ELBOW_RIGHT, ... }
// Mapping puede referenciar: kinect.joint.elbow_left.position.x
```

### 7. Mock para Testing

```cpp
// MockKinectAdapter para desarrollo sin hardware
class MockKinectAdapter : public IKinectAdapter {
public:
    bool initialize() override {
        setConnectionState(ConnectionState::CONNECTED);
        return true;
    }
    
    void shutdown() override {
        setConnectionState(ConnectionState::DISCONNECTED);
    }
    
    // Modo synthetic: generar movimiento programado
    void setSyntheticMotion(SyntheticMotionType type) {
        syntheticMotionType_ = type;
    }
    
    void update(float deltaTime) override {
        if (connectionState_ != ConnectionState::CONNECTED) return;
        
        // Generar datos sintéticos
        auto body = generateSyntheticBody(deltaTime);
        
        if (bodyCallback_) {
            bodyCallback_(body);
        }
    }
    
private:
    enum class SyntheticMotionType {
        WAVE_LEFT_HAND,
        CIRCLE_BODY,
        STANDING,
        SITTING
    };
    
    TrackedBody generateSyntheticBody(float deltaTime) {
        float t = ofGetElapsedTimef();
        
        TrackedBody body;
        body.bodyId = 0;
        body.isTracked = true;
        body.confidence = 1.0f;
        
        // Ejemplo: wave motion
        float handY = sin(t * 3.0f) * 0.3f + 0.5f;
        
        // Set joints...
        return body;
    }
};

// File playback: replay recorded sessions
class KinectPlaybackAdapter : public IKinectAdapter {
public:
    bool loadRecording(const std::string& filepath) override {
        // Load recorded .oni or .bd format
        recordingFile_ = filepath;
        return true;
    }
    
    void seekTo(float timestamp) override {
        currentTimestamp_ = timestamp;
        // Load frame at timestamp
    }
    
    void setPlaybackSpeed(float speed) {
        playbackSpeed_ = speed;  // 0.5x, 1.0x, 2.0x
    }
    
private:
    std::string recordingFile_;
    float currentTimestamp_ = 0.0f;
    float playbackSpeed_ = 1.0f;
};
```

**Configuración de testing:**

```cpp
// JSON config para mock
{
  "kinect": {
    "useMock": true,
    "mockType": "synthetic",
    "syntheticMotion": "wave_left_hand",
    "recording": null,
    "smoothing": {
      "positionFactor": 0.25,
      "velocityFactor": 0.5
    },
    "confidenceThreshold": 0.5,
    "zoneGrid": {
      "columns": 3,
      "rows": 3
    }
  }
}
```

---

## Open Questions - RESOLVED (continued)

- [x] **Q4**: ¿Cómo maneja MappingLayer las gestures discretas de Kinect?
  - **DECISIÓN**: Gestures se mapean como triggers (one-shot events), no valores continuos

- [x] **Q5**: ¿Confianza de tracking debe filtrar datos?
  - **DECISIÓN**: Sí, confidence < 0.5 no se usa para mapping, configurable

- [x] **Q6**: ¿Zones deben ser configurables?
  - **DECISIÓN**: Grid configurable (default 3x3), IDs generados automáticamente

- [x] **Q7**: ¿Mock es necesario para desarrollo?
  - **DECISIÓN**: Sí, MockKinectAdapter con synthetic motion y file playback para testing