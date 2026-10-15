# Apply Progress — modular-scene-system

## 2026-04-07
- Removed the legacy particle structs/pools from `ofApp` so the app always routes rendering + stats through `SceneEngine` and `PinturaFluorescenteScene`.
- PinturaFluorescenteScene now initializes its parameter definitions once, registers them with `ParameterRegistry` *only* on `activate`, and unregisters them on `deactivate`/destruction to avoid duplicate IDs.
- Updated the debug overlay to read `SceneDebugStats` exclusively from the active scene and kept UI/global hotkeys functional (U toggle, S screenshot, D debug overlay).

### Testing
- `make` (macOS 14.4, OF 0.12.1) ✅ — confirms the build succeeds without the legacy particle code path.

### Follow-ups / Caveats
- Manual UI smoke-test still recommended: toggle UI (`U`), interact with ParamPanel sliders to confirm values propagate once ParameterRegistry-to-scene wiring is implemented.
- When adding new scenes, ensure they follow the same register/unregister pattern to keep the registry clean.
