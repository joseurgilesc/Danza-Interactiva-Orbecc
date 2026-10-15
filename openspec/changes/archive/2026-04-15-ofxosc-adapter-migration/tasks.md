# Tasks: ofxosc-adapter-migration

## Phase 1: Setup & Configuration Infrastructure

- [x] 1.1 Update `addons.make` to include `ofxOsc` exactly once and confirm regenerated project files link the addon for every target.
- [x] 1.2 Create `bin/data/config/osc.json` seeded with defaults (`enabled`, `port`, `prefix`, `logLevel`, `maxMessagesPerFrame`) plus inline comments so operators can edit safely.
- [x] 1.3 Implement `OSCSettings` + `OSCSettingsManager` in `src/adapters/OSCSettings.{h,cpp}` to load/save JSON, enforce defaults, and cache file `mtime` for hot reloads (depends on 1.2).

## Phase 2: OSCAdapter Implementation

- [x] 2.1 Expand `src/adapters/OSCAdapter.h` to declare receiver ownership, MappingLayer/Transport setters, `applySettings`, `setEnabled`, `setPort`, stats accessors, and internal helpers for prefix stripping (depends on 1.3).
- [x] 2.2 Flesh out `OSCAdapter.cpp` lifecycle: initialize `ofxOscReceiver`, implement `startListening`, `stopListening`, destructor cleanup, and guard double starts/stops (depends on 2.1).
- [x] 2.3 Implement `restartReceiver()`/`setPort()` to stop, bind the new port, manage `lastSuccessfulPort_`, and surface `ConfigStatus` errors/logs when binding fails (depends on 2.2).
- [x] 2.4 Build the `update()` polling loop that drains `hasWaitingMessages()` within `maxMessagesPerFrame`/`2 ms` budget, tracks backlog counts, and yields when over budget (depends on 2.3).
- [x] 2.5 Implement prefix stripping + `/fader|toggle|push` channel mapping helpers, including normalization/clamping and warning logs for invalid addresses (depends on 2.4).
- [x] 2.6 Add transport routing for `/bpm`, `/play`, `/stop` with debouncing and range clamps, ensuring MappingLayer callbacks and Transport commands are invoked once per packet (depends on 2.4).

## Phase 3: Application Integration & Wiring

- [x] 3.1 Introduce `std::unique_ptr<OSCAdapter> oscAdapter_` and `OSCSettingsManager oscSettings_` members plus required includes in `src/ofApp.h` (depends on 2.x).
- [x] 3.2 Update `ofApp::setup()` to load `osc.json`, instantiate the adapter, inject MappingLayer/Transport pointers, apply current settings, and call `startListening()` with success/failure logs (depends on 3.1).
- [x] 3.3 Enhance `ofApp::update()` to call `oscSettings_.refreshIfChanged()`, reapply settings/ports when diffs appear, and invoke `oscAdapter_->update()` before scene updates (depends on 3.2).
- [x] 3.4 Implement shutdown + error surfacing: in `ofApp::exit()` persist settings and stop listening, and propagate `ConfigStatus` failures to existing UI/log channels so operators know when to retry (depends on 3.2).

## Phase 4: Testing & Documentation

- [ ] 4.1 Add unit tests (e.g., `tests/osc/OSCAdapterTests.cpp`) covering prefix stripping, channel formulas, toggle/push normalization, and transport debounce edge cases (depends on 2.5-2.6).
- [ ] 4.2 Create an integration harness or script (e.g., `scripts/osc_smoke.py`) that sends `/fader`, `/bpm`, and port-change scenarios to validate runtime reconfiguration/backlog throttling (depends on 3.3).
- [ ] 4.3 Update developer docs/README to describe `osc.json` settings, restart behavior, known warnings, and the manual test checklist so QA can reproduce the spec scenarios (depends on 4.2).
