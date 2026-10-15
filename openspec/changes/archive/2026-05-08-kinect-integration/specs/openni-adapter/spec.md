# OpenNIKinectAdapter Specification

## Purpose

Concrete implementation of IKinectAdapter using ofxOpenNI + OpenNI2 + NiTE2. Drives actual Kinect v1 (Xbox 360) hardware for skeleton tracking.

## Requirements

### Requirement: Device Initialization

MUST initialize OpenNI device and NiTE user tracker on `initialize()`. Upon success, state transitions CONNECTING → CONNECTED. On failure, transitions to ERROR.

#### Scenario: Successful init
- GIVEN Kinect v1 is connected via USB/power adapter and OpenNI2 drivers are installed
- WHEN `initialize()` is called
- THEN `getConnectionState()` returns CONNECTED; NiTE user tracker is active

#### Scenario: Device not found
- GIVEN no Kinect hardware is connected
- WHEN `initialize()` is called
- THEN returns `false`; `getConnectionState()` is ERROR; ofLogWarning is emitted

### Requirement: Per-Frame Skeleton Processing

MUST read NiTE skeleton data each frame in `update()`, invoke registered callbacks for each tracked body, and cap processing to 30 fps to avoid CPU overload.

#### Scenario: Skeleton data flow
- GIVEN one user is tracked with all 15 NiTE joints visible
- WHEN `update()` is called
- THEN registered callbacks fire for each joint with normalized `Vector3D` data in 0.0–1.0 range

#### Scenario: Frame rate cap
- GIVEN app runs at 60 fps and Kinect data arrives each frame
- WHEN `update()` is called
- THEN skeleton processing happens at most 30 times per second; intermediate frames are skipped

### Requirement: Coordinate Normalization

MUST normalize raw Kinect coordinates from depth-space meters to 0.0–1.0 screen space. x-axis: horizontal position proportional to depth-space width. y-axis: vertical. z-axis: depth normalized.

#### Scenario: Coordinate mapping
- GIVEN a raw joint position at center of depth frame (~0, 0, 2000mm)
- WHEN normalized
- THEN output is approximately x=0.5, y=0.5, z=0.5 (mid-range depth)

### Requirement: Graceful Disconnection

MUST detect device disconnection and transition state to ERROR. SHALL NOT crash or hang. SHALL invoke `setBodyCallback` with `isTracked=false` for previously tracked bodies. SHALL emit ofLogWarning.

#### Scenario: Mid-session disconnect
- GIVEN Kinect is tracking normally (CONNECTED state)
- WHEN USB cable is physically disconnected
- THEN `getConnectionState()` transitions to ERROR; callbacks notify loss; no crash

### Requirement: Logging

SHALL log state transitions via ofLogNotice (CONNECTED), ofLogWarning (disconnect/errors), and ofLogVerbose (per-frame skeleton count, when verbose).

#### Scenario: Connection log
- GIVEN device initializes successfully
- WHEN state becomes CONNECTED
- THEN ofLogNotice("OpenNIKinectAdapter") << "Kinect connected" is emitted
