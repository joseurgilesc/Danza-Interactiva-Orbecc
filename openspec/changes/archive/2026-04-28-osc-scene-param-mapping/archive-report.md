# Archive Report: osc-scene-param-mapping

**Date**: 2026-04-28
**Status**: ✅ Archived — 20/20 tasks complete, 9/9 requirements met, build clean

## Summary

Implemented OSC-to-scene-parameter mapping for Ableton Live integration via M4L Connection Kit. A new `OscMappingEngine` component stores `OscMapping` entries (oscAddress → targetParameterId + min/max + frequency mode) and dispatches scaled OSC values to scene parameters through `SceneEngine`. An `OscMappingPanel` (ImGui `IUIModule`) provides a 7-column table with CRUD, learning mode, and JSON persistence. The OSCAdapter now supports a `std::function` callback that fires AFTER built-in routing for all messages, enabling dual dispatch (e.g., `/rig/fader/2` triggers both the fader handler and the mapping engine).

## Files Created

| File | Description |
|------|-------------|
| `src/mapping/OscMappingEngine.h` | Header: `OscFrequencyMode`, `LearnState` enums, `OscMapping` struct, engine class |
| `src/mapping/OscMappingEngine.cpp` | Implementation: CRUD, dispatch, scaling, learning FSM, JSON I/O |
| `src/ui/OscMappingPanel.h` | Header: `IUIModule` subclass |
| `src/ui/OscMappingPanel.cpp` | Implementation: ImGui 7-column table, MAP button, toolbar |

## Files Modified

| File | Changes |
|------|---------|
| `src/adapters/OSCAdapter.h` | Added `OscMessageCallback` typedef, `setOscMessageCallback()` setter, `oscMessageCallback_` member |
| `src/adapters/OSCAdapter.cpp` | Added callback invocation after built-in routing in `update()` (1 line) |
| `src/ui/PanelIds.h` | Added `kOscMapping = "OSC Mappings"` constant |
| `src/ui/UIManager.h` | Added `oscMappingDockId_` member |
| `src/ui/UIManager.cpp` | Split `mappingDockId_` horizontally → new `oscMappingDockId_` dock zone, docked `kOscMapping` |
| `src/ofApp.h` | Added `oscMappingEngine` (unique_ptr) and `oscMappingPanel_` members, include |
| `src/ofApp.cpp` | 6 insertions: panel creation, callback wiring, update(), exit() save |

## Key Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Engine placement | Separate `OscMappingEngine` class | Neither `MappingLayer` (MIDI-oriented) nor `OSCAdapter` (transport-only) owns frequency modes + learning FSM |
| Adapter→Engine link | `std::function` callback | Minimal surface: one setter, one line in `update()`, no interface overhead |
| Callback timing | After built-in routing, all messages | Enables dual dispatch for fader coexistence (R-3) |
| Parameter resolution | Dot-split `sceneId.localParamId` | Matches existing `ofApp.cpp` pattern; direct scene access avoids registry round-trip |
| Learning mode | Single-slot FSM (10s timeout) | Per spec R-2; eliminates race conditions |
| Dock layout | Split `mappingDockId_` horizontally | Keeps MIDI mapping and OSC mapping panels visible side-by-side |
| JSON persistence | Independent `config/osc-mappings.json` via nlohmann/json | Separate schema avoids coupling with ParameterRegistry |

## Critical Bugs Found and Fixed

| # | Requirement | Bug | Fix |
|---|-------------|-----|-----|
| 1 | R-5 (THROTTLED) | `frameCounter = 0` after dispatch made `0 % N == 0` always true | Changed post-dispatch reset to `frameCounter = 1` |
| 2 | R-2 (TIMEOUT UI) | Engine transitioned TIMEOUT→IDLE in same `update()` — panel never saw TIMEOUT | Changed `if` to `else if` → TIMEOUT persists one full frame |

## Warnings

| # | Detail |
|---|--------|
| W1 | `scaleValue()` is dead-code — actual scaling done inline via `ofMap()` |
| W2 | No immediate UI feedback when OSC adapter is not listening |

## Requirement Coverage

| Req | Status |
|-----|--------|
| R-1 Mapping Storage | ✅ CRUD, duplicate rejection, mappingId generation |
| R-2 Learning Mode | ✅ Single-slot FSM, 10s timeout, cancel |
| R-3 OSC Processing | ✅ Callback after routing, dual dispatch, dot-split resolution |
| R-4 Value Scaling | ✅ `ofMap()` with param range awareness, inversion |
| R-5 Frequency Modes | ✅ EVERY_FRAME, ON_CHANGE (ε=0.001), THROTTLED |
| R-6 JSON Persistence | ✅ Round-trip, missing/corrupt handling |
| R-7 OscMappingPanel | ✅ 7-column table, MAP button, learning indicator |
| R-8 Application Wiring | ✅ Full lifecycle: setup → update → exit |
| R-9 Edge Cases | ✅ Graceful handling: unmapped, missing params, inactive scenes |

## Verification

- **Build**: `make -j4` → 0 errors, 0 new warnings
- **Verification Report**: `verify-report.md` — PASS after 2 CRITICAL fixes
- **Tasks**: 20/20 completed across 9 phases

## SDD Cycle Complete

The change has been fully planned (proposal → specs → design → tasks), implemented (apply), verified (verify), and archived. Ready for the next change.

## Traceability

| Artifact | Path |
|----------|------|
| Proposal | `archive/2026-04-28-osc-scene-param-mapping/proposal.md` |
| Specs | `archive/2026-04-28-osc-scene-param-mapping/specs/osc-scene-param-mapping/spec.md` |
| Design | `archive/2026-04-28-osc-scene-param-mapping/design.md` |
| Tasks | `archive/2026-04-28-osc-scene-param-mapping/tasks.md` (20/20 [x]) |
| Verify Report | `archive/2026-04-28-osc-scene-param-mapping/verify-report.md` |
| Main Spec | `openspec/specs/osc-scene-param-mapping/spec.md` |
