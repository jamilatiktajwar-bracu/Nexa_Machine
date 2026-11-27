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

**Save VM + disk snapshot** flushes attached disks, saves VM execution state, and copies each image to a sibling directory named after the session. **Restore VM + disk snapshot** restores the VM and copies image files back into the currently attached devices. Keep the same device IDs attached when restoring.

This GUI snapshot format is a convenient local checkpoint, not a crash-atomic transaction. It currently has no manifest, checksums, rollback journal, or cross-process locking. Back up important disk images before restoring snapshots.

## Portable core and frontend status

The VM core and C ABI are designed for reuse by native frontends. The Qt GUI is a desktop frontend. The iOS CI step checks compilation of the portable core for the iOS SDK; it does not build a Swift app, simulator UI, signing configuration, or App Store package. Native iOS and Android frontends remain future work.

The C ABI descriptor-based disk request queue is a separate cooperative host-pumped API. The current Qt tab disk buttons use a local host task queue and are not yet wired to that C ABI request queue.
