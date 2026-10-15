# SkeletonView Renderer Specification

## Purpose

Visual skeleton renderer that draws tracked body joints and bone connections over the main viewport. Uses ofCircle/ofDrawCircle for joints and ofPolyline/ofDrawLine for bones.

## Requirements

### Requirement: Joint Rendering

Base circle radius MUST be proportional to joint confidence (range: 3px at 0.0 confidence to 12px at 1.0). Final radius SHALL be `baseRadius * skeleton.scale`.

#### Scenario: High-confidence joint, default scale
- GIVEN a joint with confidence 0.95 at normalized position (0.5, 0.3)
- WHEN SkeletonView draws
- THEN a circle of radius ~11.4px is drawn at center of viewport, slightly above midline

#### Scenario: High-confidence joint, doubled scale
- GIVEN confidence=0.95, scale=2.0
- WHEN SkeletonView draws
- THEN circle radius ≈ 22.8px

#### Scenario: Low-confidence joint, default scale
- GIVEN confidence=0.2, scale=1.0
- WHEN SkeletonView draws
- THEN circle radius ≈ 4.8px (unchanged)

### Requirement: Bone Connections

MUST draw bone connections as lines between connected joint pairs using `ofDrawLine`. Connections MUST follow human anatomy: head→neck→spine_mid→spine_base; shoulders→elbows→wrists→hands; spine_base→hips→knees→ankles; left-right shoulder connected; left-right hip connected.

#### Scenario: Upper body bones
- GIVEN HEAD, NECK, SHOULDER_LEFT, SHOULDER_RIGHT are all tracked
- WHEN SkeletonView draws bones
- THEN lines connect HEAD→NECK, NECK→SHOULDER_LEFT, NECK→SHOULDER_RIGHT, SHOULDER_LEFT→SHOULDER_RIGHT

### Requirement: Confidence Color Coding

MUST color-code joints and bones: green (> 0.7 confidence), yellow (0.3–0.7), red (< 0.3).

#### Scenario: Mixed confidence body
- GIVEN HEAD has confidence 0.9, LEFT_ANKLE has confidence 0.2
- WHEN skeleton renders
- THEN HEAD is drawn green; LEFT_ANKLE is drawn red

### Requirement: Tracking Status Overlay

MUST draw text overlay: "Tracking: ON" when at least one body is tracked, "Tracking: LOST" when no bodies tracked. Text SHALL be white, positioned near top-left of viewport.

#### Scenario: Active tracking
- GIVEN `getAllTrackedBodies()` returns one body with `isTracked=true`
- WHEN SkeletonView draws
- THEN "Tracking: ON" overlay is visible

#### Scenario: Lost tracking
- GIVEN `getAllTrackedBodies()` returns empty or all bodies have `isTracked=false`
- WHEN SkeletonView draws
- THEN "Tracking: LOST" overlay is visible

### Requirement: Normalized Coordinate Space

MUST render joints in normalized screen coordinates (0.0–1.0), matching Kinect data coordinate system. x=0.0 is left edge, x=1.0 right edge, y=0.0 top, y=1.0 bottom.

#### Scenario: Space mapping
- GIVEN a joint at (0.0, 0.0)
- WHEN rendered on a 1920×1080 viewport
- THEN circle is drawn at pixel (0, 0) — top-left corner

### Requirement: Visibility Toggle

SHALL support `setVisible(bool)` to show or hide the skeleton overlay. Default MUST be visible.

#### Scenario: Toggle off
- GIVEN skeleton is visible
- WHEN `setVisible(false)` is called
- THEN next draw call renders nothing

### Requirement: Global Skeleton Scale Parameter

A global `skeleton.scale` (FLOAT, 0.25–3.0, default 1.0) SHALL be registered in ParameterRegistry. SkeletonView MUST read this parameter each frame and multiply joint radii and bone thicknesses by it.

#### Scenario: Default (1.0)
- GIVEN scale=1.0
- WHEN SkeletonView draws
- THEN Joint radius and bone thickness unchanged

#### Scenario: Double (2.0)
- GIVEN scale=2.0
- WHEN SkeletonView draws
- THEN Joint radius doubled; bone thickness doubled

#### Scenario: Half (0.5)
- GIVEN scale=0.5
- WHEN SkeletonView draws
- THEN Joint radius halved; bone thickness halved

#### Scenario: Min clamp
- GIVEN value=0.1
- WHEN applied
- THEN Clamped to 0.25

### Requirement: ParamPanel Visibility

`skeleton.scale` SHALL appear in ParamPanel under a "Global" group separate from per-scene parameters.

#### Scenario: Global group
- GIVEN ParamPanel open
- WHEN Panel renders parameter groups
- THEN "Global" group contains `skeleton.scale` slider
