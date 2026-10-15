# Delta for preset-ui

## ADDED Requirements

### Requirement: Per-Preset Morph Controls
PresetPanel MUST display a morph duration slider (0.0–10.0s) and an easing dropdown (LINEAR, EASE_IN, EASE_OUT, EASE_IN_OUT) in the preset editor section. Changes SHALL write to PresetDef fields and persist via savePresets(). The Apply button SHALL pass 0.0 as morphDuration when preset has its own duration configured, preferring preset's stored value.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Set duration | Preset "lite" has morphDuration=0.0 | User drags slider to 2.0 | PresetDef.morphDuration=2.0; persisted to JSON |
| Set easing | Preset "lite" has morphEasing=LINEAR | User selects EASE_IN_OUT from combo | PresetDef.morphEasing=EASE_IN_OUT; persisted |
| Apply respects preset values | morphDuration=2.0, easing=EASE_IN on preset | Click Apply with duration input=0 | Morph runs 2.0s with EASE_IN easing |
| Old preset editor | Preset loaded from older JSON without morph fields | PresetPanel renders | Slider at 0.0; combo at LINEAR; no error |
