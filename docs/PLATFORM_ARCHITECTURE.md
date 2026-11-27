# Nexa platform architecture and mobile roadmap

## Shared VM core
The portable C++20 core owns instruction execution, guest memory, assembly parsing, virtual disk format, VM state serialization, workspace/profile configuration, and deterministic tests. Host UI code should call a narrow façade and never duplicate instruction semantics.

## Desktop (current prototype)
The optional Qt 6 Widgets application provides profiles, source editing, single-step, timer-driven execution, pause/resume, stop/reset, breakpoints, framebuffer display, memory inspection, named snapshots, and virtual-disk image inspection/initialization/import/export. This is a desktop prototype; packaging and GUI CI coverage remain incomplete.

## Mobile frontend (architecture only; not implemented)
- iOS/iPadOS: SwiftUI shell with a touch-first session list, source editor, compact register/status dashboard, snapshot actions, and sandbox document picker. The portable core can be wrapped through a C ABI or Objective-C++ bridge; UI and document access remain native.
- Android: Kotlin/Compose frontend with the same session-oriented façade and Storage Access Framework for images/files.
- Mobile execution should be bounded and cooperative, with background execution limitations respected; do not promise a VM keeps running after the OS suspends the app.
- The C ABI in `include/nexa/c_api.h` is now the initial native binding surface for session create/remove, source load, lifecycle, step/run, info, memory access, and snapshots. Both mobile frontends can consume the same serialized profile and snapshot formats where practical, with versioning and explicit compatibility checks.

## Multi-session design (planned)
The portable `SessionManager` registry now owns independent VM instances and stable IDs, names, lifecycle state, program-loaded status, RAM metadata, execution count, and snapshot operations. It supports start, cooperative bounded run, pause/resume, stop/reset, step, breakpoint-aware run, and independent state inspection. It is not yet connected to Qt session tabs or profile/disk ownership. The GUI should switch sessions without sharing registers or guest RAM. A future public API should expose create/load/start/pause/resume/stop/reset, step, status, memory inspection, event subscription, disk attach/detach, snapshot operations, and typed errors.

## Storage safety
Disk import validates file size against configured capacity before writing. Export should be explicit and destination-selected. Profile clone/import allocates a fresh disk path. VM execution snapshots and disk images are separate until a future coordinated bundle format exists.

## Roadmap
1. Stabilize GUI timer execution and storage operations; add Qt build/test CI when runners support Qt.
2. Extract a session API and add independent VM tabs/session registry.
3. Add virtual disk attach/detach and persistent guest block-device integration.
4. Create native iOS/iPadOS and Android shells around the shared core.
5. Package signed desktop installers and mobile app builds, with platform-specific permissions and sandbox handling.
