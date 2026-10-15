# Proposal: ofxosc-adapter-migration

## Intent

Reemplazar el parser OSC manual implementado con strings crudos por un `OSCAdapter` robusto basado en `ofxOsc`. Este cambio introduce configuración basada en JSON, hot-reload, y mapeo completo de mensajes OSC (faders, toggles, push, transport, BPM) hacia las capas existentes del Scene Engine.

## Scope

### In Scope
- [ ] Implementar `OSCAdapter.{h,cpp}` con receptor, prefix stripping, y channel mapping
- [ ] Integrar `OSCSettingsManager` en `ofApp` para hot-reload de configuración
- [ ] Routing de mensajes `/fader`, `/toggle`, `/push`, `/bpm`, `/play`, `/stop`
- [ ] Estadísticas de backlog y throttling por frame
- [ ] Documentación para operadores y QA

### Out of Scope
- UI visual para edición de settings (futuro work)
- Persistencia de presets de mapping (futuro work)
- Testing automatizado (infraestructura no disponible en OF)

## Approach

1. **Phase 1**: Setup y configuración (YA COMPLETADO)
   - `addons.make` incluye `ofxOsc`
   - `osc.json` con defaults
   - `OSCSettings` + `OSCSettingsManager` funcionando

2. **Phase 2**: Implementar `OSCAdapter`
   - Receptor OSC con start/stop/restart
   - Polling loop con budget de mensajes
   - Prefix stripping y channel mapping
   - Transport routing (BPM, play, stop)

3. **Phase 3**: Integrar en `ofApp`
   - Inicializar adapter en `setup()`
   - Hot-reload en `update()`
   - Cleanup en `exit()`

4. **Phase 4**: Docs y testing manual

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `addons.make` | Modified | Ya incluye ofxOsc |
| `bin/data/config/osc.json` | New | Ya creado |
| `src/adapters/OSCSettings.{h,cpp}` | New | Ya implementado |
| `src/adapters/OSCAdapter.{h,cpp}` | New | Por implementar |
| `src/ofApp.h` | Modified | Agregar miembros OSC |
| `src/ofApp.cpp` | Modified | Wiring del adapter |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Port ya en uso | Medium | Loguear error y fallback a port anterior |
| Mensajes malformed | Low | Warn + skip, no crash |
| Backlog masivo | Low | Throttling por frame budget |

## Rollback Plan

```bash
git revert HEAD -- src/adapters/OSCAdapter.* src/ofApp.* bin/data/config/osc.json
make
```

## Dependencies

- OpenFrameworks 0.12.1 con `ofxOsc` addon
- Scene Engine existente (MappingLayer, Transport)

## Success Criteria

- [ ] `make` compila sin errores
- [ ] OSC messages `/rig/fader/1` se mapean correctamente a channels
- [ ] Hot-reload de `osc.json` funciona sin restart
- [ ] Transport commands (/bpm, /play, /stop) responden