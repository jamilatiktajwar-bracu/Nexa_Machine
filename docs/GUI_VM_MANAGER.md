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
- **Clone**: copies profile settings to a new profile and allocates a fresh disk path.
- **Delete**: removes the profile JSON while retaining its disk image.
- **Import / Export**: transfer profile JSON settings; import creates a fresh disk image.
- **New profile**: prompts for profile name, RAM size (1–65,536 bytes), and disk size (1–128 MiB).
- **Open .nexa**: loads source text into the editor.
- **Run / Step / Reset**: execute, single-step, or reset the current VM.
- **Set / Clear breakpoints**: configure or clear instruction-address breakpoints.
- **Send input**: queue keyboard bytes for guest INPUT instructions; Enter appends a newline.
- **Export log**: save the current console transcript to a text file.
- **Start / Pause / Resume / Stop / Reset**: timer-driven execution lifecycle controls. Stop halts the guest; Reset creates a fresh VM.
- **Disk info / Flush disk**: inspect configured image metadata or initialize/flush a disk image.
- **Import disk / Export disk**: import an image only if it fits configured capacity; export to a chosen file.
- **Save / Restore / Delete snapshot**: save, restore, or remove VM execution state under the selected profile.

Snapshots capture VM state, not the virtual disk image. Disk images and profile definitions remain separate workspace assets. The GUI currently uses profile RAM settings; disk I/O is managed by the CLI and workspace, not yet exposed as a GUI panel.

Workspace is discovered through `NEXA_HOME` or the default `~/.nexa` (Windows uses the user profile home).

See [platform architecture and mobile roadmap](PLATFORM_ARCHITECTURE.md) for the shared-core design and future native frontends.
