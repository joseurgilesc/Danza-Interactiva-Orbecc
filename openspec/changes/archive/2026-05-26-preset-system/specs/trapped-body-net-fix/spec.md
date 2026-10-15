# Delta for trapped-body-net-fix

## ADDED Requirements

### Requirement: tensionAlphaBoost Registration
`line.tensionAlphaBoost` MUST be registered via `addParam()` in TrappedBodyNetScene. Type: FLOAT, range: 0.0–2.0, default: 0.5.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Registry appearance | App starts with TrappedBodyNetScene | Scene registers params | `line.tensionAlphaBoost` appears in ParameterRegistry with FLOAT type, [0.0, 2.0], default 0.5 |
| OSC mapping visible | OscMappingPanel open, scene active | User browses parameter dropdown | `line.tensionAlphaBoost` appears as selectable target |
| ParamPanel visible | ParamPanel open, scene active | Panel renders | `tensionAlphaBoost` slider shown, starts at 0.5 |

### Requirement: Parameter Readback Each Frame
Scene MUST read `tensionAlphaBoost` from ParameterRegistry each frame during line tension calculation.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Value affects alpha | tensionAlphaBoost=1.5 | Lines under tension draw | Alpha boosted by 1.5× tension factor |
| Default minimal effect | tensionAlphaBoost=0.5 | Lines under tension draw | Alpha boosted by 0.5× tension factor |
