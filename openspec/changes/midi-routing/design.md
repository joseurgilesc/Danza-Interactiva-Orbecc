# Design: MIDI Routing — Separación Cues vs Parámetros por Preset

## Technical Approach

Introduce a **MidiRouter** that dispatches incoming MIDI by channel (1→cues, 2→params), with config from `config/midi.json`. A **MidiMappingEngine** (follows OscMappingEngine pattern) handles CC/Note→parameter dispatch per preset via `IBaseScene::setParameter()`. A **MidiMappingPanel** (ImGui table, follows CueEditorPanel Learn pattern) provides the editing UI. **PresetDef** gains a `std::vector<MidiMapping> midiMappings` field (backward-compatible). The single `midiLearnCallback_` becomes a `std::vector` supporting multi-panel Learn coexistence.

## Architecture Decisions

| Decision | Choice | Alternatives | Rationale |
|----------|--------|-------------|-----------|
| Channel routing location | `MidiRouter` class, not inline in `ofApp.cpp` | Inline in `newMidiMessage()` | Follows OscMappingEngine pattern; testable in isolation; config hot-reload support |
| Config file name | `config/midi.json` | `config/midi-mappings.json` | Matches spec requirement; shorter; follows `config/osc.json` naming |
| Learn callback refactor | `vector<function<bool(ofMessage&)>>` with first-consumer-wins | Single slot with priority dispatcher | Simple iteration; CueEditor already has null-safe `registerLearn_()` calls |
| Mapping storage | Per-preset inside `PresetDef.midiMappings` | Separate per-scene config file | Preset lifecycle guarantees clean activation/deactivation; no stale mapping cleanup needed |
| Mapping matching | Type + channel + number exact match | Plus velocity/velocity ranges | Spec only requires type+channel+number; velocity filtering YAGNI |
| `MidiMappingEngine` placement | `src/midi/` directory | `src/mapping/` | `mapping/` is for internal parameter-to-parameter bindings; `midi/` groups all new MIDI code |

## Data Flow

```
ofxMidiIn → ofApp::newMidiMessage(msg)
  │
  ├─ foreach midiLearnCallbacks_[i]:
  │    if callback_i(msg) == true → return     // consumed
  │
  └─ midiRouter_->routeMessage(msg)
       │
       ├─ msg.channel == cueChannel_ (default 1)
       │    → sceneSequencer_->findCueByMidi(type, value)
       │      → triggerCue(cueIndex)
       │
       ├─ msg.channel == paramChannel_ (default 2)
       │    → mappingEngine_->processMidiMessage(msg)
       │       → match by type+channel+number
       │       → scaleValue() → scene->setParameter(id, val)
       │
       └─ else → ignored
```

Preset activation flow:
```
PresetManager::applyPreset("lite")
  → mappingEngine_->removeMappingsByScene()   (kinect — existing)
  → mappingEngine_->addMappings(preset.kinectMappings)
  → midiMappingEngine_->setActiveMappings(preset.midiMappings)   ← NEW
  → apply params (morph or instant)
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/midi/MidiRouter.h` | Create | Channel-dispatching router (~25 LOC) |
| `src/midi/MidiMappingEngine.h` | Create | Mapping engine + Learn state (~100 LOC) |
| `src/midi/MidiMappingEngine.cpp` | Create | Dispatch, scale, CRUD, persistence (~200 LOC) |
| `src/ui/MidiMappingPanel.h` | Create | ImGui panel, implements IUIModule (~50 LOC) |
| `src/ui/MidiMappingPanel.cpp` | Create | Table render, Learn UX, add/remove rows (~300 LOC) |
| `config/midi.json` | Create | Default: `{"cueChannel":1,"paramChannel":2,"portName":"IAC Driver Bus 1"}` |
| `src/ofApp.h` | Modify | Add `midiRouter_`, `midiMappingEngine_`, `midiMappingPanel_`, `MidiConfig`; change `midiLearnCallback_` to `vector`; add `addMidiLearnCallback()` / `removeMidiLearnCallback()` |
| `src/ofApp.cpp` | Modify | Load config, wire router+engine, refactor `newMidiMessage()`, register MidiMappingPanel |
| `src/presets/PresetDef.h` (inside PresetManager.h) | Modify | Add `std::vector<MidiMapping> midiMappings` to `PresetDef` |
| `src/presets/PresetManager.h` | Modify | Constructor accepts `MidiMappingEngine*`; add include |
| `src/presets/PresetManager.cpp` | Modify | Serialize/deserialize `midiMappings`; hook `setActiveMappings()` in `applyPreset()` |
| `src/ui/CueEditorPanel.h` | Modify | Constructor accepts `addCallback`/`removeCallback` instead of `setCallback` |
| `src/ui/CueEditorPanel.cpp` | Modify | Uses `addMidiLearnCallback_`/`removeMidiLearnCallback_` |
| `src/ui/PanelIds.h` | Modify | Add `static const char* kMidiMapping = "MIDI Mappings";` |

## Interfaces / Contracts

### MidiMapping struct (in `src/midi/MidiMappingEngine.h`)

```cpp
enum class ControlMode : uint8_t { CONTINUOUS, TOGGLE, TRIGGER, MOMENTARY };

struct MidiMapping {
    std::string mappingId;          // auto-generated: "midi-N"
    std::string type;               // "cc" or "note"
    int         channel;            // 1–16
    int         number;             // 0–127 (CC number or note number)
    std::string targetParameterId;  // "sceneId.localParamId"
    float       minInput = 0.0f;    // MIDI input range min
    float       maxInput = 127.0f;  // MIDI input range max
    float       minOutput = 0.0f;   // mapped parameter range min
    float       maxOutput = 1.0f;   // mapped parameter range max
    ControlMode controlMode = ControlMode::CONTINUOUS;
    bool        enabled = true;
    // Runtime state (not persisted):
    float       lastValue = 0.0f;
    bool        noteOn = false;     // for TOGGLE/TRIGGER/MOMENTARY tracking
};
```

### MidiConfig (in `src/midi/MidiRouter.h`)

```cpp
struct MidiConfig {
    int cueChannel = 1;
    int paramChannel = 2;
    std::string portName = "IAC Driver Bus 1";
    bool load(const std::string& path);   // returns false with defaults+warning on error
    bool save(const std::string& path) const;
};
```

### MidiRouter public API

```cpp
class MidiRouter {
public:
    MidiRouter(SceneSequencer*, MidiMappingEngine*, const MidiConfig&);
    void routeMessage(const ofxMidiMessage& msg);
    void setCueChannel(int ch);     // clamps to 1–16
    void setParamChannel(int ch);
    void reloadConfig(const MidiConfig& cfg);
};
```

### MidiMappingEngine minimal public API

```cpp
class MidiMappingEngine {
public:
    explicit MidiMappingEngine(SceneEngine*);
    void processMidiMessage(const ofxMidiMessage& msg);  // matched+scaled dispatch
    void setActiveMappings(const std::vector<MidiMapping>& mappings);
    std::vector<MidiMapping>& getActiveMappings();
    // Learn (engine-level double-learn guard)
    bool startLearning();                   // false if already learning
    void cancelLearning();
    LearnState getLearnState() const;
};
```

### PresetManager constructor change

```cpp
PresetManager(SceneEngine* sceneEngine, KinectMappingEngine* mappingEngine,
              MidiMappingEngine* midiMappingEngine = nullptr);
```

### PresetDef change (inside PresetManager.h, line 40)

```cpp
std::vector<MidiMapping> midiMappings;  // after kinectMappings
```

### Callback registration (ofApp.h)

```cpp
std::vector<std::function<bool(ofxMidiMessage&)>> midiLearnCallbacks_;
void addMidiLearnCallback(std::function<bool(ofxMidiMessage&)> cb);
void removeMidiLearnCallback(const std::function<bool(ofxMidiMessage&)>* cb); // by pointer identity
```

`removeMidiLearnCallback` matches by target pointer of `std::function` object — consumer passes address of the stored function to scan and remove.

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Unit | `MidiRouter::routeMessage()` — channel dispatch correctness | Instantiate with mock SceneSequencer+MappingEngine, feed messages on various channels, assert which mock received call |
| Unit | `MidiMappingEngine::scaleValue()` — linear interpolation, inverted range, clamping | Feed boundary values (0, 64, 127), assert outputs; test inverted min/max |
| Unit | `MidiConfig::load()` — defaults on missing file, validation (same channel, 0, 17) | Write temp JSON files, call load(), check error paths |
| Integration | Preset apply → active mappings lifecycle | Load preset with midiMappings, apply, send CC on param channel, verify `setParameter` called on active scene |
| Integration | Learn callback vector behavior — CueEditor + MidiMappingPanel coexistence | Register both callbacks, send PC (consumed by Cue), verify MidiMapping callback skipped |
| Build | `make` compilation — all new files link, existing cue editor works | Standard build verification per config.yaml |

## Migration / Rollout

No migration required. Existing preset JSONs lack `midiMappings` → deserialization defaults to empty vector. Existing CueEditorPanel code is compatible with the new callback API: `registerLearn_` just wraps `add`/`remove` instead of `set`. The single `config/midi.json` file is created automatically with defaults if missing.

## Error Handling & Edge Cases

- **Malformed midi.json**: `MidiConfig::load()` returns false; defaults applied; `ofLogError` with path and parse error.
- **paramChannel == cueChannel**: `MidiConfig::load()` rejects; defaults used; `ofLogError` logged.
- **Invalid channel (0 or >16)**: Rejected during load; defaults used.
- **Target parameter doesn't exist**: `SceneEngine::getScene()` returns null or `getParameter()` returns null → `ofLogWarning` once per mapping; message silently skipped.
- **Double Learn**: `MidiMappingEngine::startLearning()` returns false if already in LEARNING state; panel shows toast "Learn already active".
- **NoteOff dispatch for TOGGLE**: Ignored — TOGGLE only reacts to NoteOn. TRIGGER and MOMENTARY track NoteOff for release.

## Open Questions

- [ ] Should `config/midi.json` also store the MIDI port name, and should the app auto-open it? (Spec says portName, current code opens port 0.)
