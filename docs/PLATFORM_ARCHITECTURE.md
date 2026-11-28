# Nexa platform architecture and mobile status

## Shared VM core

The portable C++20 core owns instruction execution, guest memory, assembly parsing, virtual-disk operations, VM state serialization, workspace/profile configuration and deterministic tests. Desktop and mobile hosts call the shared C API where practical; host UI code must not duplicate guest instruction semantics.

Nexa is an educational VM and OS-kernel simulator. It is not a type-1/type-2 hypervisor, does not boot a general-purpose guest OS, and does not promise unrestricted background execution on mobile platforms.

## Desktop frontends

The Qt 6 Widgets application provides profiles, source editing, single-step, timer-driven execution, pause/resume, stop/reset, breakpoints, framebuffer display, memory inspection, named snapshots, virtual-disk inspection/import/export, and a separate multi-session workspace.

The CI workflow builds the GUI on Linux, macOS and Windows, runs CTest and GUI launch/restart smoke checks, stages an installation, creates a CPack archive, extracts that archive and launches its packaged CLI/GUI, and verifies the archive checksum. On macOS the Qt GUI is a real `nexa_gui.app` bundle; Qt's generated deployment script copies its frameworks/plugins into the bundle before packaging. Windows Qt 6.12.1 installation uses a pinned upstream `aqtinstall` revision containing the current flattened repository and Qt 6.12 metadata handling. These are workflow checks; consult the latest run for each platform's current result.

## Mobile frontends

### iOS/iPadOS — SwiftUI

The XcodeGen project in `mobile/ios/project.yml` builds the portable core for the active iOS device or simulator architecture and links it through `Nexa-Bridging-Header.h`. The SwiftUI app currently provides assembly editing, load/run/pause/step/reset/stop, guest keyboard input, console output, disk-image import, disk detach, snapshot save/restore and recovery diagnostics.

The app stores its VM and snapshot bundle in its app sandbox. It saves when the scene moves to the background and attempts recovery when the app launches again. The UI-testing target exercises snapshot save, app termination, relaunch, workspace restoration and the structured `Recovery: SUCCESS` report.

### Android — Jetpack Compose and JNI

The Gradle project in `mobile/android` builds `nexa_android_jni` against the same C++ core using the Android NDK. The Compose app currently provides assembly editing, load/run/pause/step/reset/stop, keyboard input, console output, disk-image import through the Storage Access Framework, disk detach and local snapshot save/restore with recovery diagnostics.

Disk images and snapshots are copied into app-private storage. The app saves its workspace during the activity stop lifecycle and attempts recovery on the next launch. Android instrumentation covers native VM execution, disk/snapshot operations, fresh-context restore and OS force-stop/relaunch recovery.

### Mobile execution boundaries

Both frontends use cooperative instruction budgets and remain subject to their host OS lifecycle and resource limits. A backgrounded or force-stopped app is not promised to continue running; persistence relies on a successful snapshot before suspension/termination and subsequent recovery. User data stays in the app sandbox unless the user explicitly imports or exports it.

## Snapshot and recovery safety

Snapshot bundles contain VM state, disk images and a manifest with file sizes and 64-bit FNV-1a checksums. Restore validates the bundle before applying it. The C API restore path uses durable PREPARED/COMMITTED transaction journals, rollback images, coordinated multi-disk recovery and a structured recovery report containing phase, affected path, reason and recommended action.

FNV-1a detects accidental corruption; it is not a cryptographic authenticity mechanism. Do not load untrusted VM state files. Durability depends on the host filesystem and storage device honouring flush operations; the simulator cannot guarantee recovery from physical media failure.

## Multi-session desktop workspace

The Qt application includes a separate **Multi-session workspace** tab. Each VM tab owns an independent session, editor/execution state, RAM preview, disk-device registry and cooperative disk-operation queue. Disk attach, detach, flush and snapshot operations are scheduled from the GUI timer so host-side I/O does not block the same callback that handles VM controls.

## Build entry points

Build the Qt desktop frontend with CMake and Qt 6. Build the portable core alone with:

```sh
cmake -S . -B build-core \
  -DNEXA_BUILD_HOST_TOOLS=OFF \
  -DNEXA_BUILD_GUI=OFF
cmake --build build-core --target nexa_vm_core
```

Build the iOS Simulator app on macOS with Xcode and XcodeGen:

```sh
brew install xcodegen
cd mobile/ios
xcodegen generate
xcodebuild -project NexaMachine.xcodeproj -scheme NexaMachine \
  -sdk iphonesimulator -destination 'generic/platform=iOS Simulator' \
  CODE_SIGNING_ALLOWED=NO build
```

Build the Android debug APK with Android SDK/NDK, Java 17 and Gradle 8.9:

```sh
gradle -p mobile/android assembleDebug
```

## Current validation and remaining work

CI is configured for Linux/macOS/Windows desktop build, CTest, GUI restart and package-install smoke; Android NDK/Gradle build and emulator instrumentation; and iOS core/SwiftUI simulator build plus app terminate/relaunch XCTest. The latest per-platform workflow status is authoritative—source presence alone does not constitute a successful build or install.

Remaining work is to keep the Windows CTest and package-install path green, complete mobile simulator/emulator recovery tests on the latest commit, provide the Android and Apple distribution-signing secrets, and verify signed artifacts on device before any store distribution. See [release signing requirements](RELEASE_SIGNING.md) for the exact secret names and release gate.
