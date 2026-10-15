# Delta for ofapp-integration

## ADDED Requirements

### Requirement: mouseMoved Event Routing

ofApp MUST declare `void mouseMoved(int x, int y) override` in its header and implement it in `ofApp.cpp`. Implementation SHALL forward: `inputRouter->mouseMoved(x, y)` and `uiManager.mouseMoved(x, y)`. Without this override, GLFW mouse-move events are silently dropped by openFrameworks.

#### Scenario: mouseMoved reaches active scene

- GIVEN app running, scene "furry" active, useMouseInput=true
- WHEN user moves mouse to (500, 400) without pressing buttons
- THEN FurryScene::mouseMoved(500, 400) is called via InputRouter→SceneEngine chain
- AND UIManager receives mouseMoved(500, 400) for ImGui hover tracking

#### Scenario: mouseMoved on ofApp.h missing (regression test)

- GIVEN ofApp.h is being reviewed
- WHEN inspecting the class declaration
- THEN `void mouseMoved(int x, int y) override;` MUST be present alongside the other mouse overrides

### Requirement: useMouseInput Toggle Routing

ofApp MUST store a `bool useMouseInput_` member (default: `true`). SceneEngine SHALL expose `setUseMouseInput(bool)` and `isMouseInputEnabled()` methods. When toggled, active scene MUST be notified to stop/start accepting mouse-driven interaction points.

#### Scenario: Toggle OFF blocks mouse-to-scene path

- GIVEN `useMouseInput_=false` and scene active
- WHEN mouseMoved(400, 300) fires
- THEN InputRouter forwards the event but SceneEngine does NOT populate a "mouse" interaction point on the active scene

#### Scenario: Toggle ON restores mouse path

- GIVEN `useMouseInput_` was false, now set to true
- WHEN mouse moves
- THEN subsequent mouseMoved events populate "mouse" point on active scene normally
