# Cross-platform frontend and recovery milestone

## Desktop multi-session scheduler

The Qt multi-session tab now offers a Submit guest disk I/O workflow. The user writes an 11-byte descriptor into guest RAM, enters its address, chooses a completion interrupt vector and guest handler PC, and queues the request. The GUI timer executes at most one queued host task per tick. Descriptor fields match the C ABI format: operation, 32-bit device ID, 32-bit sector, and 16-bit guest buffer address (little-endian). Successful transfers trigger the configured VM interrupt. The UI currently owns this cooperative queue; it is not yet a shared NexaContext queue and does not expose C ABI request IDs/status polling.

## Snapshot integrity and recovery

Qt snapshot bundles contain a VM state file, disk image copies, and manifest.txt. The manifest records bundle version, file sizes, and 64-bit FNV-1a checksums. Restore validates all files and the attached device set before overwriting active images. It creates a local rollback copy of the current VM and disks and attempts to restore that copy if applying the snapshot fails.

FNV-1a is an accidental-corruption check, not a cryptographic authenticity mechanism. Do not load untrusted VM state files. The rollback is local to one process and cannot guarantee recovery from power loss or storage-device failure during restore.

## Session persistence

On session-tab close, the GUI saves VM state, editor source and attached disk registry below WorkspacePaths::discover().root/sessions/<session-name>. On creating a tab with the same name, it attempts to recover the VM, source and disk attachments. Disk image files remain at their original paths; missing images are skipped and surfaced as a recovery warning. This is per-session persistence, not a full application-wide session catalogue or automatic recovery of tabs that were never explicitly closed.

## iOS SwiftUI starter

mobile/ios contains a SwiftUI single-session frontend, a C bridging header and a model using the portable C API. Add the compiled iOS static core to an Xcode target to build it. The workflow's iOS job cross-compiles the core only; it does not sign, package, launch or test this SwiftUI application.

## Android JNI starter

mobile/android contains a JNI bridge and Kotlin native declarations. CMake builds nexa_android_jni when configured with the Android NDK toolchain. The bridge supports context/session creation, assembly load, bounded run, step, pause/reset, keyboard input and error retrieval. A complete Compose/Activity app, Gradle project, storage picker, emulator tests and signing remain to be implemented.

## CI coverage

The CI matrix installs Qt6 and configures the desktop GUI on Linux, Windows and macOS. The macOS job cross-compiles the portable core for iOS. The Linux job cross-compiles the Android JNI shared library for arm64-v8a. These jobs validate build artifacts, not native mobile UI behavior.
