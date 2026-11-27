# Nexa platform architecture and mobile roadmap

## Shared VM core
The portable C++20 core owns instruction execution, guest memory, assembly parsing, virtual disk format, VM state serialization, workspace/profile configuration, and deterministic tests. Host UI code should call a narrow façade and never duplicate instruction semantics.

## Desktop (current prototype)
The optional Qt 6 Widgets application provides profiles, source editing, single-step, timer-driven execution, pause/resume, stop/reset, breakpoints, framebuffer display, memory inspection, named snapshots, and virtual-disk image inspection/initialization/import/export. This is a desktop prototype; packaging and GUI CI coverage remain incomplete.

## Mobile frontend (architecture only; not implemented)
- iOS/iPadOS: SwiftUI shell with a touch-first session list, source editor, compact register/status dashboard, snapshot actions, and sandbox document picker. The portable core can be wrapped through a C ABI or Objective-C++ bridge; UI and document access remain native.
- Android: Kotlin/Compose frontend with the same session-oriented façade and Storage Access Framework for images/files.
- Mobile execution should be bounded and cooperative, with background execution limitations respected; do not promise a VM keeps running after the OS suspends the app.
- The C ABI in `include/nexa/c_api.h` is now the initial native binding surface for session create/remove, source load, lifecycle, step/run, info, memory access, keyboard input, breakpoints, framebuffer access, and snapshots. Both mobile frontends can consume the same serialized profile and snapshot formats where practical, with versioning and explicit compatibility checks.

## Multi-session GUI integration (planned)
The portable `SessionManager` registry now owns independent VM instances and stable IDs, names, lifecycle state, program-loaded status, RAM metadata, execution count, and snapshot operations. It supports start, cooperative bounded run, pause/resume, stop/reset, step, breakpoint-aware run, and independent state inspection. It is not yet connected to Qt session tabs or profile/disk ownership. The GUI should switch sessions without sharing registers or guest RAM. A future public API should expose create/load/start/pause/resume/stop/reset, step, status, memory inspection, event subscription, disk attach/detach, snapshot operations, and typed errors.

## Storage safety
Disk import validates file size against configured capacity before writing. Export should be explicit and destination-selected. Profile clone/import allocates a fresh disk path. VM execution snapshots and disk images are separate until a future coordinated bundle format exists.

## Roadmap
1. Connect the new SessionManager to Qt and add independent VM tabs.
2. Add virtual disk attach/detach and persistent guest block-device integration.
3. Create native iOS/iPadOS and Android shells around the shared core.
4. Package signed desktop installers and mobile app builds, with platform-specific permissions and sandbox handling.
5. Add CI coverage for desktop GUI and mobile toolchains.


## Desktop multi-session workspace

The Qt desktop application now includes a separate **Multi-session workspace** tab. Each VM tab owns an independent `SessionManager` instance, assembly editor, execution state, RAM preview, disk-device registry, and cooperative disk-operation queue. Disk attach, detach, flush, and snapshot file operations are scheduled one task per GUI timer tick so long host-side I/O does not run in the same UI callback that handles VM controls. VM-plus-disk snapshots flush attached images before copying them and restore disk images before reloading the VM state. These bundles are convenient local checkpoints, not crash-atomic transactions.

## iOS build verification

The macOS CI job additionally cross-compiles the portable `nexa_vm_core` static library for the iOS device SDK (arm64), with host tools and Qt disabled. This verifies the portable C++ core toolchain; it does **not** claim that a native iOS application, UI, signing, or App Store packaging exists.
