# Apply Progress — ofxosc-adapter-migration

## 2026-04-09 — Phase 1 foundation
- Updated `addons.make` so `ofxOsc` is always generated into Xcode/Make exports; no more manual addon toggles before regenerating project files.
- Seeded `bin/data/config/osc.json` with operator-facing notes plus default values (`enabled`, `port`, `prefix`, `logLevel`, `maxMessagesPerFrame`) aligned with the design defaults (`true`, `9000`, `"rig"`, `info`, `64`).
- Added `OSCSettings` + `OSCSettingsManager` under `src/adapters/`: loads/saves JSON via `ofJson`, sanitizes prefixes/ports, tracks `lastWriteTime` for hot reloads, exposes runtime overrides, and broadcasts changes through lightweight listener IDs so ofApp/OSCAdapter can subscribe next phase.

## 2026-04-15 — Phase 2 & 3 (Implementation complete)

### Phase 2: OSCAdapter Implementation
- Created `OSCAdapter.h` with receiver wrapper, MappingLayer/Transport references, settings application, stats accessors, and internal helpers for prefix stripping.
- Created `OSCAdapter.cpp` with full lifecycle: `startListening()`, `stopListening()`, `restartReceiver()`, `setPort()`.
- Implemented `update()` polling loop with `maxMessagesPerFrame` budget, backlog tracking, and warning logs when exceeded.
- Implemented prefix stripping (`/rig/fader/1` → `/fader/1`) and channel mapping for `/fader`, `/toggle`, `/push` addresses.
- Added transport routing for `/bpm` (40-240 range clamp), `/play`, `/stop` with debouncing.

### Phase 3: Application Integration
- Added `OSCAdapter.h` + `OSCSettings.h` includes to `ofApp.h`
- Added `oscSettings_` (OSCSettingsManager) and `oscAdapter_` (unique_ptr<OSCAdapter>) members to ofApp
- Wired in `setup()`: load settings → instantiate adapter → start listening → log status
- Wired in `update()`: call `oscAdapter_->update()` BEFORE scene updates for low-latency OSC processing
- Wired in `exit()`: stop listening + persist settings on shutdown

### Testing
- Not run — repository rule: "Never build after changes." Reasoned via static analysis.

### Follow-ups / Caveats
- MappingLayer API calls are TODO placeholders — needs integration with actual `mappingLayer_.setChannelValue(channel, normalized)` when ParameterBinding API is ready.
- Phase 4 (tests, docs) pending — requires infrastructure that doesn't exist in OF.
