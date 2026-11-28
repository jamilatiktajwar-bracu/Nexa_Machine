# Cross-platform frontend and recovery status

Nexa uses a portable C++20 VM core with a C ABI boundary for native mobile frontends. Desktop and mobile UIs are separate clients of the same core; each frontend owns its own VM session and host lifecycle.

## Desktop multi-session workspace

The Qt multi-session workspace supports independent VM sessions, assembly editing, run/pause/resume/step/reset/stop, breakpoints, guest keyboard input, RAM preview, disk devices, guest disk requests, and VM-plus-disk bundle snapshots.

The GUI and C ABI use the same `GuestDiskQueue` descriptor parser/FIFO implementation, but each frontend owns a separate queue instance. The GUI timer executes at most one host task or guest disk request per tick. The C ABI exposes request IDs and status/error polling, and the caller pumps at most one request with `nexa_disk_process_one`.

## Snapshot integrity and recovery

Snapshot bundles contain VM state, disk image copies and a manifest recording the bundle version, file sizes and 64-bit FNV-1a checksums. Restore validates the bundle before applying it. The C API restore path writes durable PREPARED/COMMITTED transaction journals and rollback images before replacing disk images, coordinates multi-disk rollback, and can recover an interrupted restore when a new session starts. The structured recovery report exposes success, transaction phase, affected path, reason and recommended action.

FNV-1a detects accidental corruption; it is not a cryptographic authenticity mechanism. Do not load untrusted VM state files. Durability depends on the host filesystem and storage device honouring flush/rename operations; no software journal can guarantee recovery from physical media failure.

## Session persistence

On session-tab close, the GUI saves VM state, editor source and attached disk registry below `WorkspacePaths::discover().root/sessions/<session-name>`. Reopening a tab with the same name attempts to recover the VM, source and disk attachments. Disk image files remain at their original paths; missing images are skipped and surfaced as a recovery warning. This is per-session persistence, not a full application-wide session catalogue or automatic recovery of tabs that were never explicitly closed.

## iOS SwiftUI frontend

The XcodeGen project is in `mobile/ios/project.yml`. Its pre-build script compiles `nexa_vm_core` for the active iOS device/simulator SDK and architecture, then links it through `Nexa-Bridging-Header.h`.

The SwiftUI app provides assembly editing, load/run/pause/step/reset/stop, keyboard input, guest console output, document-picker disk import, disk detach, VM-plus-disk snapshot save/restore and recovery diagnostics. Snapshots are stored in the app sandbox; the app saves when entering the background and attempts recovery on the next launch. The current UI remains a compact session frontend rather than a full desktop-style multi-session manager.

Build on macOS with Xcode and XcodeGen:

```sh
brew install xcodegen
cd mobile/ios
xcodegen generate
xcodebuild -project NexaMachine.xcodeproj -scheme NexaMachine \
  -sdk iphonesimulator -destination 'generic/platform=iOS Simulator' \
  CODE_SIGNING_ALLOWED=NO build
```

## Android Jetpack Compose frontend

The Gradle project is in `mobile/android`. Its Android NDK CMake target builds the JNI bridge against the portable VM core. The Compose app provides assembly editing, load/run/pause/step/reset/stop, keyboard input, guest console output, disk-image attachment through Android's document picker, disk detach, local VM-plus-disk bundle snapshot save/restore and recovery diagnostics.

The selected disk image is copied into app-private storage before attachment. Snapshots are stored in app-private storage and are not automatically exported or backed up; restore requires the same attached disk-device set as the saved bundle. The activity saves on stop and attempts recovery on the next launch. Instrumentation tests exercise native VM execution, fresh-context snapshot restore and OS force-stop/relaunch recovery.

Build with Android SDK/NDK, Java 17 and Gradle 8.9:

```sh
gradle -p mobile/android assembleDebug
```

## Portable core-only build

```sh
cmake -S . -B build-core \
  -DNEXA_BUILD_HOST_TOOLS=OFF \
  -DNEXA_BUILD_GUI=OFF
cmake --build build-core --target nexa_vm_core
```

## CI coverage

The GitHub Actions workflow configures, builds and tests the Qt desktop target on Linux, Windows and macOS, including GUI restart, staged installation, CPack archive inspection, extracted-package CLI/VM execution, GUI launch and SHA-256 verification. On macOS the GUI is installed as a deployable `nexa_gui.app` bundle with Qt frameworks/plugins. The iOS Simulator job cross-compiles the core, builds the SwiftUI app and runs an XCTest that saves a snapshot, terminates and relaunches the app, then checks workspace restoration and the structured success report. The Android job builds the NDK/JNI core and Compose APK, then runs native VM/disk/snapshot instrumentation. After instrumentation exits, the runner uses ADB to force-stop and relaunch the app twice, verifies `Workspace restored` and `Recovery: SUCCESS` in the UI hierarchy, and uploads test/logcat diagnostics on failure. Consult the latest workflow run for actual platform-by-platform results; a configured job or present source file alone does not prove a successful build or install.

Production Android/iOS signing is conditional on configured GitHub Actions secrets. Preview artifacts are not production-signed releases; see [release signing requirements](RELEASE_SIGNING.md).
