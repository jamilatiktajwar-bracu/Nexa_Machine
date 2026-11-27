# Cross-platform frontend and recovery milestone

Nexa uses a portable C++20 VM core with a C ABI boundary for native mobile frontends. Desktop and mobile UIs are separate clients of the same core; they do not share a running VM process.

## Desktop multi-session workspace

The Qt multi-session workspace supports independent VM sessions, assembly editing, run/pause/resume/step/reset/stop, breakpoints, guest keyboard input, RAM preview, disk devices, guest disk requests, and VM-plus-disk bundle snapshots.

The GUI and C ABI use the same `GuestDiskQueue` descriptor parser/FIFO implementation, but each frontend owns a separate queue instance. The GUI timer executes at most one host task or guest disk request per tick. The C ABI exposes request IDs and status/error polling, and the caller pumps at most one request with `nexa_disk_process_one`.

## Snapshot integrity and recovery

Qt and C ABI snapshot bundles contain VM state, disk image copies, and a manifest recording bundle version, file sizes, and 64-bit FNV-1a checksums. Restore validates the bundle before applying it and uses local rollback copies if a subsequent apply/load step fails.

FNV-1a is an accidental-corruption check, not a cryptographic authenticity mechanism. Do not load untrusted VM state files. Rollback is local to one process and cannot guarantee recovery from power loss or storage-device failure during restore.

## Session persistence

On session-tab close, the GUI saves VM state, editor source and attached disk registry below `WorkspacePaths::discover().root/sessions/<session-name>`. Reopening a tab with the same name attempts to recover the VM, source and disk attachments. Disk image files remain at their original paths; missing images are skipped and surfaced as a recovery warning. This is per-session persistence, not a full application-wide session catalogue or automatic recovery of tabs that were never explicitly closed.

## iOS SwiftUI frontend

The XcodeGen project is in `mobile/ios/project.yml`. Its pre-build script compiles `nexa_vm_core` for the active iOS device/simulator SDK and architecture, then links it through `Nexa-Bridging-Header.h`.

The current SwiftUI app provides assembly editing, load/run/pause/step/reset/stop, keyboard input, and guest console output. Desktop disk-image management, workspace persistence and bundle snapshot controls are not yet exposed in the iOS UI.

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

The Gradle project is in `mobile/android`. Its Android NDK CMake target builds the JNI bridge against the portable VM core. The Compose app provides assembly editing, load/run/pause/step/reset/stop, keyboard input, and guest console output. Disk-image selection and bundle snapshot UI are not yet exposed.

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

GitHub Actions configures and builds the Qt desktop target and runs CTest on Linux, Windows and macOS. The macOS job also cross-compiles the iOS core and builds the SwiftUI app for the iOS simulator. The Linux job cross-compiles the Android JNI library and assembles the Compose debug APK. Consult the latest workflow run for actual platform-by-platform outcomes; a core-only cross-build does not prove that the complete native app builds.
