# Nexa Desktop VM Manager

The optional Qt 6 desktop frontend now includes a basic profile-aware workflow.

## Build
```sh
cmake -S . -B build -DNEXA_BUILD_GUI=ON
cmake --build build --target nexa_gui
```
Qt 6 Widgets is required. If Qt is not detected, the CLI build continues and CMake skips the desktop target.

## Controls
- Profile selector: loads the selected profile's RAM setting into a fresh VM.
- **New profile**: creates a workspace profile with default RAM/disk settings.
- **Open .nexa**: loads source text into the editor.
- **Run / Step / Reset**: execute, single-step, or reset the current VM.
- **Set breakpoint**: set an instruction-address breakpoint.
- **Save snapshot / Restore snapshot**: save or restore VM execution state under the selected profile.

Snapshots capture VM state, not the virtual disk image. Disk images and profile definitions remain separate workspace assets. The GUI currently uses profile RAM settings; disk I/O is managed by the CLI and workspace, not yet exposed as a GUI panel.

Workspace is discovered through `NEXA_HOME` or the default `~/.nexa` (Windows uses the user profile home).
