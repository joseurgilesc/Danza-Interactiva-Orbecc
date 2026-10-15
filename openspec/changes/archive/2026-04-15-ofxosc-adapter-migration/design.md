# Design: ofxosc-adapter-migration

## Technical Approach

Implementar un `OSCAdapter` que envuelve `ofxOscReceiver` y mapea mensajes OSC entrantes hacia `MappingLayer` (channels) y `TransportController` (BPM, play, stop). El diseño sigue el patrón decorador: el adapter se injecta en `ofApp` y polled cada frame.

## Architecture Decisions

### Decision: Wrapper sobre ofxOscReceiver en lugar de subclassing

**Choice**: Crear una clase `OSCAdapter` que contiene y controla un `ofxOscReceiver` privado.
**Alternatives considered**: Subclassing `ofxOscReceiver` o heredando de él.
**Rationale**: `ofxOscReceiver` no está diseñado para herencia (métodos no virtuales, acoplamiento interno a `ofBaseGLRenderer`). El wrapper permite control total sobre el lifecycle (start/stop/restart) sin modificar el addon.

### Decision: Budget de mensajes por frame en lugar de threading

**Choice**: Limitar mensajes procesados por frame (`maxMessagesPerFrame`) en el polling loop.
**Alternatives considered**: Hilo separado para recibir mensajes, cola de mensajes asíncrona.
**Rationale**: OpenFrameworks no es thread-safe para acceso a UI desde hilos secundarios. Un frame budget simple evita congelar la UI con backlog masivo, con warning logs cuando se excede.

### Decision: Prefix stripping en el adapter, no en el receptor

**Choice**: El adapter recibe mensajes completos y hace strip del prefix antes de mapear.
**Alternatives considered**: Configurar `ofxOscReceiver` para ignorar el prefix.
**Rationale**: El receiver pasa mensajes sin filtrar, y tener el prefix en el adapter permite logging más claro ("Received /rig/fader/1" vs "/fader/1").

## Data Flow

```
[External OSC] ──> ofxOscReceiver (port 9000)
                      │
                      ▼
              OSCAdapter::update()
                      │
          ┌───────────┴───────────┐
          │                       │
     Prefix Strip           Transport
     /rig/fader/1 ──>      /bpm, /play, /stop
     channel mapping           │
          │                    ▼
          ▼              TransportController
    MappingLayer                 │
          │                      │
          ▼                      ▼
    [Scene Engine]          [Playback]
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/adapters/OSCAdapter.h` | Create | OSCAdapter class declaration |
| `src/adapters/OSCAdapter.cpp` | Create | OSCAdapter implementation |
| `src/ofApp.h` | Modify | Agregar `unique_ptr<OSCAdapter>`, `OSCSettingsManager` |
| `src/ofApp.cpp` | Modify | setup(): init adapter; update(): poll adapter; exit(): cleanup |
| `src/adapters/OSCSettings.h` | Existing | Ya implementado en Phase 1 |
| `src/adapters/OSCSettings.cpp` | Existing | Ya implementado en Phase 1 |
| `bin/data/config/osc.json` | Existing | Ya creado en Phase 1 |

## Interfaces / Contracts

```cpp
// En OSCAdapter.h
class OSCAdapter {
public:
    enum class ConfigStatus { OK, ERROR_PORT_IN_USE, ERROR_BIND_FAILED };
    
    OSCAdapter(OSCSettingsManager& settings,
               MappingLayer& mapping,
               TransportController& transport);
    
    ConfigStatus startListening();
    void stopListening();
    void update(); // Called every frame
    
    // Stats
    int getBacklogCount() const;
    bool isListening() const;
    
private:
    int mapAddressToChannel(const std::string& addr) const;
    void handleTransportMessage(const ofxOscMessage& msg);
    // ... member variables
};
```

```cpp
// En ofApp.h (requerido)
#include "adapters/OSCAdapter.h"

class ofApp {
    // ...
    std::unique_ptr<modular::OSCAdapter> oscAdapter_;
    modular::OSCSettingsManager oscSettings_;
    modular::MappingLayer* mappingLayer_;  // existente
    modular::TransportController* transport_;  // existente
};
```

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Unit | Prefix stripping, channel mapping formulas | Test casos: `/rig/fader/1` → channel 0, `/rig/toggle/3` → channel 2 |
| Unit | Transport routing: BPM range clamp, debounce | Verificar 60-200 BPM clamping |
| Integration | Port change hot-reload | Cambiar port en JSON, verificar restart |
| Manual | Full rig connectivity | Conectar controlador real a port 9000 |

## Migration / Rollout

No migration required — это новая funcionalidad sin efectos en datos existentes.

## Open Questions

- [ ] ¿Soporte para múltiples prefijos OSC? (actualmente solo uno)
- [ ] ¿Mapping de canales configurable via JSON o solo addresses fixes?