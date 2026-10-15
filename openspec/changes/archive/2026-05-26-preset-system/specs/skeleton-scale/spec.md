# Delta for skeleton-view

## ADDED Requirements

### Requirement: Global Skeleton Scale Parameter
A global `skeleton.scale` (FLOAT, 0.25–3.0, default 1.0) SHALL be registered in ParameterRegistry. SkeletonView MUST read this parameter each frame and multiply joint radii and bone thicknesses by it.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Default (1.0) | scale=1.0 | SkeletonView draws | Joint radius and bone thickness unchanged |
| Double (2.0) | scale=2.0 | SkeletonView draws | Joint radius doubled; bone thickness doubled |
| Half (0.5) | scale=0.5 | SkeletonView draws | Joint radius halved; bone thickness halved |
| Min clamp | Value=0.1 | Applied | Clamped to 0.25 |

### Requirement: ParamPanel Visibility
`skeleton.scale` SHALL appear in ParamPanel under a "Global" group separate from per-scene parameters.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Global group | ParamPanel open | Panel renders parameter groups | "Global" group contains `skeleton.scale` slider |

## MODIFIED Requirements

### Requirement: Joint Rendering
(Previously: "Circle radius MUST be proportional to joint confidence (range: 3px at 0.0 confidence to 12px at 1.0).")

Base circle radius MUST be proportional to joint confidence (range: 3px at 0.0 to 12px at 1.0). Final radius SHALL be `baseRadius * skeleton.scale`.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| High-conf, scaled | confidence=0.95, scale=2.0 | Draw | Circle radius ≈ 22.8px |
| Low-conf, default | confidence=0.2, scale=1.0 | Draw | Circle radius ≈ 4.8px (unchanged) |
