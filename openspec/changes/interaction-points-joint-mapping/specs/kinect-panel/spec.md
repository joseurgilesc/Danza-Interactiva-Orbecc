# Delta for kinect-panel

## ADDED Requirements

### Requirement: useMouseInput Checkbox

KinectPanel MUST render an ImGui checkbox labeled "Mouse Input" in the device status section. Checked state SHALL correspond to `useMouseInput=true` (default). Unchecking SHALL disable mouse-driven interaction points globally. Toggle state MUST persist across scene changes and panel hide/show cycles. Checkbox SHALL be always visible regardless of connection state.

#### Scenario: Checkbox visible in status bar

- GIVEN KinectPanel is drawn with any connection state
- WHEN user views the device tab status section
- THEN an ImGui checkbox labeled "Mouse Input" is visible

#### Scenario: Uncheck disables mouse globally

- GIVEN checkbox is checked, mouse input is active
- WHEN user unchecks "Mouse Input"
- THEN SceneEngine::setUseMouseInput(false) is called
- AND active scene stops using mouse-based interaction points

#### Scenario: Checkbox state survives scene change

- GIVEN "Mouse Input" is unchecked
- WHEN user switches from FurryScene to PinturaFluorescenteScene
- THEN the checkbox remains unchecked
- AND PinturaFluorescente does NOT receive mouse interaction points
