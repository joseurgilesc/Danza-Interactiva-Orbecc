# KinectPanel UI Specification

## Purpose

ImGui panel implementing IUIModule for Kinect status monitoring and mapping configuration. Panel ID: `"Kinect"`. Dockable via UIManager.

## Requirements

### Requirement: IUIModule Compliance

MUST implement the IUIModule interface: `setup()`, `update()`, `draw()`, `setVisible(bool)`, `isVisible()`, `getPanelId()`. `getPanelId()` MUST return `"Kinect"`.

#### Scenario: Panel lifecycle
- GIVEN KinectPanel is added to UIManager
- WHEN UIManager calls `draw()`
- THEN ImGui renders the panel with docking support

### Requirement: Connection Status Display

MUST display a visual status indicator: green circle + "Connected" for CONNECTED, red circle + "Disconnected" for DISCONNECTED/ERROR, yellow circle + "Connecting..." for CONNECTING.

#### Scenario: Connected state UI
- GIVEN adapter `getConnectionState()` returns CONNECTED
- WHEN panel draws
- THEN a green circle and "Connected" text are visible

### Requirement: Connect/Disconnect Controls

MUST provide Connect and Disconnect buttons. Connect SHALL call `adapter->initialize()`. Disconnect SHALL call `adapter->shutdown()`. Buttons MUST reflect current state (disable Connect when already connected).

#### Scenario: Connect button
- GIVEN state is DISCONNECTED
- WHEN user clicks "Connect"
- THEN adapter `initialize()` is called; button becomes disabled; status updates

### Requirement: Tracked Bodies Table

MUST display a table of tracked bodies with columns: Body ID, Confidence, Tracking Status. Selecting a row SHALL populate the joint detail section.

#### Scenario: Body selection
- GIVEN 2 bodies are tracked
- WHEN user clicks on bodyId=3 row
- THEN the joint detail section shows joints for body 3

### Requirement: Joint Detail Display

MUST display selected body's 18 joints with X, Y, Z normalized coordinates and per-joint confidence. SHALL use color coding matching SkeletonView (green/yellow/red by confidence).

#### Scenario: Joint table
- GIVEN body 1 is selected and its HEAD joint has position (0.5, 0.2, 0.6) with confidence 0.9
- WHEN panel draws joint detail
- THEN HEAD row shows x=0.5, y=0.2, z=0.6, conf=0.9 in green text

### Requirement: Mapping Configuration Section

MUST provide: dropdown for joint+eje (jointSource), dropdown for scene+parameter (target), slider for threshold, enable checkbox. MUST provide Add/Remove mapping buttons. MUST display current mappings list with enable/disable toggles.

#### Scenario: Add mapping via UI
- GIVEN user selects "hand_right.position.y" as source and "pinturaFluorescente.flockSpeed" as target
- WHEN "Add Mapping" is clicked
- THEN a new mapping appears in the mappings list

### Requirement: Save/Load Buttons

MUST provide Save and Load buttons that invoke `KinectMappingEngine::saveToJson()` and `loadFromJson()` respectively.

#### Scenario: Save mappings
- GIVEN 5 mappings are configured
- WHEN "Save" is clicked
- THEN `config/kinect-mappings.json` is written with all 5 entries

### Requirement: Skeleton Preview Viewport

SHALL embed a mini skeleton preview viewport rendering the currently tracked skeleton using the same rendering logic as SkeletonView.

#### Scenario: Mini skeleton preview
- GIVEN one body is tracked
- WHEN panel draws the mini viewport
- THEN joints appear as circles connected by bone lines matching body anatomy

### Requirement: Contextual Control State

SHALL disable Kinect-specific controls (mapping config, body table, skeleton preview) when adapter is not connected.

#### Scenario: Controls locked
- GIVEN connection state is DISCONNECTED
- WHEN panel draws
- THEN mapping add/remove buttons are grayed out; body table is empty; skeleton preview shows "No device"
