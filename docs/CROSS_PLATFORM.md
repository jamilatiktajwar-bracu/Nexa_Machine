# Nexa Cross-Platform Plan

## Product goal
Nexa is intended to be usable on **Windows, macOS, Linux, and iOS**, with the same VM/kernel behavior and a platform-appropriate interface. Users should choose their host without changing guest images or learning a different Nexa workflow.

## Architecture rules
- **Portable core:** VM, assembler, executable format, virtual memory, kernel, scheduler, VFS, and guest ABI stay in standard C++20 wherever possible.
- **Host adapters:** host filesystem, storage location, keyboard, display, lifecycle, and platform permissions are isolated behind interfaces. Platform code must not leak into guest semantics.
- **One image format:** guest executable and disk formats are versioned and documented. Host byte order, path syntax, and native filesystem APIs must not alter image compatibility.
- **Optional front ends:** CLI remains the dependable baseline. Desktop UI is optional and uses Qt where supported. Mobile UI is a separate frontend, not a fork of the VM.
- **Capability-aware UX:** mobile exposes only OS-permitted capabilities (such as file import/export and touch drawing). Unsupported capabilities are clearly surfaced.
- **No hypervisor claims:** Nexa is an educational simulator, not hardware virtualization.

## Platform targets

| Host | Target experience | Build / packaging direction |
|---|---|---|
| Windows 10/11 x64 | CLI + Qt desktop, native file dialogs, keyboard/mouse | CMake/MSVC; installer or MSIX later |
| macOS (Apple Silicon + Intel where practical) | CLI + Qt desktop, native windowing | CMake/Apple Clang; signed/notarized DMG later |
| Linux x86_64 (and ARM64) | CLI + Qt desktop, distro-friendly | CMake/GCC/Clang; AppImage/Flatpak/deb later |
| iOS/iPadOS | Touch-first profile/image manager and constrained interactive simulator | Xcode + Qt for iOS or native Swift shell over defined C++ core; sandbox and resource constraints respected |

iOS is not a desktop port: background execution, storage, memory, JIT restrictions, and App Store rules require a mobile-specific frontend/runtime. Initial mobile scope is profiles, importing/exporting images, and supported simulator workloads; arbitrary native executable loading and unrestricted host access are out of scope.

## User-controlled settings
- Layout: host-native, compact, or touch.
- Theme: system, light, dark.
- Keyboard mapping and shortcuts.
- VM profiles (CPU steps/quantum, RAM, swap policy, disk image).
- Display scaling and pixel canvas.
- Storage/import-export through OS file picker.
- Save-state policy and autosave interval.
- Optional telemetry disabled by default; guest data is never uploaded.

## Milestones
1. Portable build baseline; remove platform assumptions; add Windows/macOS CI and reproducible builds.
2. Desktop UI polish: VM controls, terminal, display/canvas, settings, shortcuts, native file dialogs.
3. Portable data: versioned profiles, safe image import/export, stable paths, migration and save-state checks.
4. Mobile-core audit: isolate dependencies; remove desktop-only APIs from VM/kernel.
5. iOS frontend: touch controls, profiles, import/export, foreground execution limits, suspend/resume recovery.
6. Distribution: signed builds, release artifacts, checksums and platform installation instructions.

## Current reality
The repository currently builds a C++20 CLI and optional Qt6 desktop executable. The iOS frontend, release packaging, and automated Windows/macOS builds are **planned, not yet implemented**. Keep status accurate in release notes.
