# IKinectAdapter Interface Specification

## Purpose

Abstract interface for Kinect skeleton tracking, following the deferred contract from modular-scene-system. All concrete adapters (OpenNI, mock, future Azure Kinect) implement this contract. Zero dependency on Kinect SDK headers.

## Requirements

### Requirement: Lifecycle Management

IKinectAdapter MUST provide `initialize()` and `shutdown()` methods. `initialize()` returns `bool` indicating success. `shutdown()` MUST release device resources. Header SHALL NOT include any Kinect SDK headers.

#### Scenario: Successful init
- GIVEN hardware is connected and drivers loaded
- WHEN `initialize()` is called
- THEN returns `true`; `getConnectionState()` returns CONNECTED

#### Scenario: Init failure
- GIVEN no Kinect hardware is connected
- WHEN `initialize()` is called
- THEN returns `false`; `getConnectionState()` returns ERROR

### Requirement: Connection State Query

MUST provide `getConnectionState()` returning `ConnectionState` enum.

#### Scenario: State polling
- GIVEN adapter is initializing
- WHEN `getConnectionState()` is called
- THEN returns CONNECTING

### Requirement: Callback Registration

MUST provide five callback setters: `setJointCallback`, `setBodyCallback`, `setHandCallback`, `setGestureCallback`, `setZoneCallback`. Each takes a `std::function` with appropriate signature. Callbacks SHALL be invoked from the adapter's update loop.

#### Scenario: Joint callback
- GIVEN a `std::function<void(int bodyId, JointType joint, Joint data)>` is registered via `setJointCallback`
- WHEN skeleton data is processed
- THEN the callback is invoked once per tracked joint

#### Scenario: Callback overwrite
- GIVEN a body callback is already registered
- WHEN a new body callback is registered via `setBodyCallback`
- THEN the new callback replaces the old one

### Requirement: Polling Access

MUST provide `getAllTrackedBodies()` returning `std::vector<TrackedBody>` and `getRawJointPosition(bodyId, jointType)` returning `Vector3D`.

#### Scenario: Polling after update
- GIVEN one body is tracked
- WHEN `getAllTrackedBodies()` is called after `update()`
- THEN returns a vector with one `TrackedBody` where `isTracked=true`

### Requirement: Configuration Methods

MUST provide `setSmoothing(float factor)`, `setConfidenceThreshold(float min)`, and `setZoneGrid(int cols, int rows)`.

#### Scenario: Smoothing applied
- GIVEN smoothing factor is set to 0.5 via `setSmoothing(0.5)`
- WHEN skeleton data is read in subsequent frames
- THEN joint positions are interpolated between consecutive frames with weight 0.5

### Requirement: No External SDK Dependency

IKinectAdapter.h MUST NOT include any OpenNI, NiTE, or other Kinect SDK headers. It SHALL be a pure C++ abstract class with only standard library includes and KinectTypes.h.

#### Scenario: Compilation isolation
- GIVEN IKinectAdapter.h is compiled in environment without OpenNI installed
- WHEN the build runs
- THEN IKinectAdapter compiles without errors
