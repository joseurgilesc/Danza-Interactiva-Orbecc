## ImGui Integration Overview

This project now embeds [ofxImGui](https://github.com/jvcleave/ofxImGui) so the legacy UI panels can be rebuilt with a single immediate-mode stack.

### Dependencies

1. Install `ofxImGui` inside your openFrameworks `addons/` directory (build scripts read `addons.make`).
2. Ensure macOS builds use the GL3 renderer (default in OF 0.12.1).

### Theme + Fonts

- Global styling lives in `src/ui/StudioTheme.{h,cpp}`.
- Fonts load from `bin/data/fonts/Roboto-Regular.ttf` and `Roboto-Bold.ttf`. We ship placeholders in repo; replace them with the actual Roboto files before release. Missing fonts trigger a runtime warning and fall back to the default ImGui font.

### Dock Layout

`UIManager` creates a dockspace with this default layout:

- Scenes panel docked on the left column.
- Parameter panel docked on the right column.
- Transport panel docked along the bottom.
- Mapping/InputPorts fill the center.

To reset the layout, delete `bin/data/imgui.ini` and restart.

### Remaining Work

- Scene/Transport/Param/Mapping/InputPorts panels still need ImGui rewrites.
- InputRouter must respect `UIManager::wantsMouseCapture/KeyboardCapture` before dispatching events to scenes.
