# Proposal: OSC-to-Scene-Parameter Mapping

## Intent

Enable RECEIVING OSC from Ableton Live (M4L Connection Kit: OSC Send, OSC MIDI Send) and mapping
to scene params. OSCAdapter drops unknown addresses — operators cannot bind arbitrary OSC to visuals.

## Scope

### In Scope
- `OscMappingEngine` — stores `OscMapping` structs (oscAddress → targetParamId + min/max + frequency mode), learning-mode FSM, `processOscMessage()` dispatcher via `SceneEngine`
- OSCAdapter callback — `std::function<void(string, float)>` fired AFTER built-in routing for ALL messages
- `OscMappingPanel` (IUIModule) — ImGui table: OSC Address | Target | Min | Max | Freq | Enabled | [MAP]; add/remove/save/load
- JSON persistence: `config/osc-mappings.json`
- New dock slot (`kOscMapping`) in UIManager

### Out of Scope
- SEND (oF → Ableton) — deferred
- Changes to `MappingLayer`, `ParameterBinding`, or scene code
- Auto-discovery, test automation

## Approach

Callback fires in `OSCAdapter::update()` for every message. Engine normalizes M4L int (0-127) to float (0-1),
scales via per-mapping min/max, dispatches to active scene. Learning mode: UI activates, next OSC message
auto-assigns address. Three frequency modes: every-frame, on-change (±epsilon), throttled (cooldown).

## Affected Areas

| Area | Impact |
|------|--------|
| `src/adapters/OSCAdapter.h/.cpp` | Modified — callback member + ~5 lines in update() |
| `src/mapping/OscMappingEngine.h/.cpp` | New — core engine |
| `src/ui/OscMappingPanel.h/.cpp` | New — ImGui table implementing IUIModule |
| `src/ui/PanelIds.h` | Modified — add `kOscMapping` |
| `src/ui/UIManager.h/.cpp` | Modified — new `oscMappingDockId_` dock slot |
| `src/ofApp.h/.cpp` | Modified — instantiate + wire engine/panel |

## Risks

| Risk | Mitigation |
|------|------------|
| M4L sends int 0-127, not float 0-1 | Auto-detect range; normalize in engine |
| Learning mode races (2 params in learn) | Single slot; UI disables other [MAP] buttons |
| Existing OSC routing breaks | Callback fires AFTER built-in routing, not replacing it |
| JSON persistence conflicts | Separate file, independent schema from ParameterRegistry |

## Rollback Plan

1. Remove `oscMessageHandler_` from OSCAdapter
2. Delete `OscMappingEngine.*` + `OscMappingPanel.*`
3. Remove from `ofApp.*` + `UIManager.*` + `PanelIds.h`
4. `make` to verify

## Dependencies

- `ofxOsc` + `ofxImGui` already in `addons.make`
- `SceneEngine::getActiveScene()` → `IBaseScene::setParameter()`
- No external libs

## Success Criteria

- [ ] OSC from Ableton changes scene parameter values in real-time
- [ ] Learning mode auto-assigns address < 1s
- [ ] Min/max scaling maps correctly
- [ ] All 3 frequency modes work as specified
- [ ] Mappings survive restart via JSON
- [ ] Existing fader/toggle/push/bpm/play/stop routing unchanged
- [ ] `make` builds without errors
