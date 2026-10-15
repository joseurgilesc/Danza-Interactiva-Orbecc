# Kinect Data Types Specification

## Purpose

Define shared data types for skeleton tracking — zero dependency on Kinect SDK headers. All Kinect-compatible adapters and consumers use these types as the common contract.

## Requirements

### Requirement: JointType Enumeration

`JointType` MUST enumerate exactly 18 human body joints: HEAD, NECK, SHOULDER_LEFT, SHOULDER_RIGHT, ELBOW_LEFT, ELBOW_RIGHT, WRIST_LEFT, WRIST_RIGHT, HAND_LEFT, HAND_RIGHT, HIP_LEFT, HIP_RIGHT, KNEE_LEFT, KNEE_RIGHT, ANKLE_LEFT, ANKLE_RIGHT, SPINE_BASE, SPINE_MID, SPINE_TOP. SHALL NOT include any joints beyond these 18.

#### Scenario: Joint count verification
- GIVEN the KinectTypes header is compiled
- WHEN `static_cast<int>(JointType::COUNT)` is evaluated
- THEN it equals 18

### Requirement: TrackedBody Structure

`TrackedBody` MUST contain: `bodyId` (int), `isTracked` (bool), `confidence` (float 0.0-1.0), and `joints` (fixed-size array or vector of 18 `Joint` indexed by `JointType`). The `bodyId` MUST be a positive integer assigned by the adapter.

#### Scenario: Body with all joints
- GIVEN a `TrackedBody` is constructed with `bodyId=1`
- WHEN accessing `joints[JointType::WRIST_RIGHT]`
- THEN a valid `Joint` is returned with default zero values

### Requirement: Joint Structure

Each `Joint` MUST contain: `position` (`Vector3D`), `velocity` (`Vector3D`), `confidence` (float 0.0-1.0).

#### Scenario: Joint default state
- GIVEN a default-constructed `Joint`
- WHEN `confidence` is read
- THEN it equals 0.0

### Requirement: Vector3D Normalization

`Vector3D` coordinates (x, y, z) MUST be normalized to 0.0–1.0 range, representing screen-proportional coordinates. The z-axis SHALL represent depth where 0.0 = nearest and 1.0 = farthest.

#### Scenario: Edge values
- GIVEN a `Vector3D` with x=0.0, y=0.5, z=1.0
- WHEN rendered in skeleton view
- THEN joint appears at left edge, vertical center, maximum depth

### Requirement: HandState Enumeration

`HandState` MUST enumerate: OPEN, CLOSED, POINTING, UNKNOWN.

#### Scenario: Default hand state
- GIVEN skeleton tracking is active but hand gesture not recognized
- WHEN `HandState` is queried
- THEN it returns UNKNOWN

### Requirement: ConnectionState Enumeration

`ConnectionState` MUST enumerate: DISCONNECTED, CONNECTING, CONNECTED, ERROR.

#### Scenario: State transitions
- GIVEN adapter is in DISCONNECTED state
- WHEN `initialize()` is called and device found
- THEN state transitions to CONNECTING, then CONNECTED

### Requirement: Zero SDK Dependency

KinectTypes.h MUST NOT include any Kinect SDK, OpenNI, or NiTE headers. It SHALL compile in isolation with C++17 standard library only.

#### Scenario: Compilation isolation
- GIVEN KinectTypes.h is included without OpenNI installed
- WHEN the project compiles
- THEN no compilation errors occur
