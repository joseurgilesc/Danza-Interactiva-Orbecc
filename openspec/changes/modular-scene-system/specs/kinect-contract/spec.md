# Kinect Adapter Contract Specification

## Purpose

Defines the interface for future Kinect integration. This is a deferred implementation contract - no actual Kinect code in initial release.

## Requirements

### Requirement: IKinectAdapter Interface

The system MUST define a pure virtual interface for Kinect adapters.

```cpp
class IKinectAdapter {
public:
    virtual ~IKinectAdapter() = default;
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() = 0;
    virtual std::vector<Body> getBodies() = 0;
    virtual HandState getLeftHandState(BodyId) = 0;
    virtual HandState getRightHandState(BodyId) = 0;
    virtual Joint getJoint(BodyId, JointType) = 0;
};
```

- GIVEN IKinectAdapter interface defined
- WHEN concrete implementation provided
- THEN MappingLayer can use it identically to OSCAdapter

### Requirement: Connection Management

The IKinectAdapter MUST provide connection lifecycle methods.

- GIVEN IKinectAdapter implementation
- WHEN `connect()` called
- THEN initialize Kinect sensor and begin tracking

- GIVEN connected IKinectAdapter
- WHEN `disconnect()` called
- THEN release sensor resources

- GIVEN IKinectAdapter
- WHEN `isConnected()` called
- THEN return true if sensor active, false otherwise

### Requirement: Body Tracking

The IKinectAdapter SHALL provide body/ skeleton data.

- GIVEN connected Kinect sensor
- WHEN `getBodies()` called
- THEN return vector of tracked bodies with IDs and positions

- GIVEN tracked body
- WHEN `getJoint(bodyId, JointType::Head)` called
- THEN return 3D position of head joint

### Requirement: Hand State Detection

The IKinectAdapter MUST provide hand open/closed/pointer states.

- GIVEN tracked body
- WHEN `getLeftHandState(bodyId)` called
- THEN return HandState: Open, Closed, Pointer, or Unknown

- GIVEN tracked body
- WHEN `getRightHandState(bodyId)` called
- THEN return corresponding hand state

### Requirement: Gesture Signals

The IKinectAdapter SHOULD emit gesture events for MappingLayer consumption.

- GIVEN IKinectAdapter implementation
- WHEN hand raised above shoulder threshold
- THEN emit onHandGesture event with type (leftRaise, rightRaise)

- GIVEN IKinectAdapter implementation
- WHEN body pose changes significantly
- THEN emit onBodyPose event (armsUp, leanLeft, leanRight)

### Requirement: Mock Implementation Support

The system MUST allow mock Kinect adapter for testing without hardware.

- GIVEN MockKinectAdapter implementing IKinectAdapter
- WHEN used in tests or simulation mode
- THEN produces deterministic body/gesture data

---

## Verification Scenarios

### Interface Compliance

- GIVEN any class implementing IKinectAdapter
- WHEN passed to MappingLayer
- THEN works identically to OSC adapter (polymorphism)

### Mock Usage

- GIVEN MockKinectAdapter in simulation mode
- WHEN body tracking queried
- THEN return configurable test data

### Future Integration

- GIVEN RealKinectAdapter implemented later
- WHEN integrated
- THEN no changes needed to MappingLayer