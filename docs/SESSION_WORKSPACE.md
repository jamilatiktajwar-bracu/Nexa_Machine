# Multi-session desktop workspace

The optional Qt6 desktop application exposes two top-level workspaces:

- **Profile VM** keeps the existing single-profile workflow.
- **Multi-session workspace** opens independent VM sessions in tabs. Use **New VM session** and **Close session** to manage tabs.

Each session has its own assembly editor, VM state, console, RAM preview, breakpoint set, keyboard queue, disk registry, and cooperative host task queue.

## Execution controls

- **Start** assembles the editor contents (if not already loaded) and begins timer-driven execution.
- **Pause / Resume** suspend or continue the current session.
- **Step** executes one instruction.
- **Stop** halts the VM; use **Reset** before starting it again.
- **Reload source** stops timer-driven execution, pauses a running session if necessary, assembles the current editor contents, and replaces the loaded guest program.
- **Set breakpoint** accepts a decimal guest program-counter address. **Clear breakpoints** removes all breakpoints.
- **Send input** queues text followed by a newline for the guest keyboard device.

The UI scheduler runs bounded VM work and at most one queued host disk-management task per 15 ms timer tick. It is cooperative and does not create a worker thread.

## Disk devices and snapshots

Attach or create a disk image from a session tab. Device IDs are assigned from the lowest unused nonnegative ID. The device table shows ID, image filename, configured capacity and attachment state. Flush writes all attached in-memory disk changes to their images. Detach optionally flushes first.

**Save VM + disk snapshot** flushes attached disks, saves VM execution state, and copies each image to a sibling directory named after the session. It writes a `manifest.txt` containing the VM and per-device image sizes and FNV-1a checksums. **Restore VM + disk snapshot** validates the manifest, sizes, checksums, and attached device IDs before changing the current VM or disk images. It creates a temporary rollback copy of the current VM and disks; if applying the new bundle fails, it attempts to restore that copy. Keep the same device IDs attached when restoring.

The manifest detects accidental corruption; FNV-1a is not a cryptographic authenticity check. The rollback copy is a best-effort in-process recovery mechanism, not a crash-atomic transaction. A power loss during file replacement can still leave a partial restore. Back up important disk images before restoring snapshots.

## Portable core and frontend status

The VM core and C ABI are designed for reuse by native frontends. The Qt GUI is a desktop frontend. The iOS CI step checks compilation of the portable core for the iOS SDK; it does not build a Swift app, simulator UI, signing configuration, or App Store package. Native iOS and Android frontends remain future work.

The C ABI exposes a descriptor-based asynchronous disk request queue that a host frontend pumps cooperatively. The Qt multi-session tab also has a guest descriptor queue processed by its 15 ms scheduler, alongside the host disk-management task queue. A failed guest request is reported in the session status and no longer stops the entire scheduler; subsequent queued work can continue. Session shutdown flushes attached disks before persisting the VM state and device registry.

Native iOS SwiftUI and Android Compose frontends are present. CI builds the iOS VM core and SwiftUI app on macOS, and the Android JNI core and Compose APK on Linux. These frontends are functional starting points, not yet full desktop feature parity; platform-specific file import/export and richer device management remain areas for development.
