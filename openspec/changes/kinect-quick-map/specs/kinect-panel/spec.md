# Delta for kinect-panel

## ADDED Requirements

### Requirement: Quick Map Tab
KinectPanel MUST render a 4th tab "Quick Map" with an interactive visual skeleton. Clicking a joint SHALL create a VEC2_XY KinectMapping targeting `{activeSceneId}.{jointName}`. Mapped joints SHALL display a gold ring indicator.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Click-to-assign | Skeleton visible, scene active | User clicks shoulder_left joint circle | VEC2_XY mapping created: `{sceneId}.shoulder_left`; gold ring appears |
| Gold ring indicator | wrist_right already mapped | Skeleton renders | wrist_right joint drawn with gold ring overlay |
| Duplicate click | shoulder_left already mapped | User clicks shoulder_left again | No duplicate created; brief toast "already mapped" |

### Requirement: Quick Map Scene Selector
Quick Map tab MUST include a dropdown listing all scenes from SceneEngine. Selected scene SHALL determine `targetParameterId` prefix on click.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| Scene selection | pinturaFluorescente active; user changes to furry | User clicks joint | Target is `furry.{jointName}` |

### Requirement: Quick Map Guard States
When no tracking data exists, Quick Map SHALL display "Start tracking to see the skeleton" with ghosted skeleton and disabled clicks. When no scene is active, SHALL display "No active scene — activate a scene first" with disabled clicks.

| Scenario | GIVEN | WHEN | THEN |
|---|---|---|---|
| No tracking | getAllTrackedBodies() returns empty | Quick Map tab renders | Message shown; skeleton ghosted; clicks disabled |
| No active scene | sceneEngine_->getActiveScene() returns nullptr | Quick Map tab renders | Message shown; scene selector disabled |
