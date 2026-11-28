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
| Android | Kotlin/Jetpack Compose VM client, document-picker disk import and private workspace recovery | Android Gradle Plugin + Java 17, NDK/CMake JNI bridge; signed APK/AAB when signing credentials are configured |

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

## Delivered implementation

- Portable C++20 VM/kernel core and C API are shared by desktop and mobile hosts.
- Desktop CLI and Qt6 GUI include multi-session workspace, disk handling, VM controls and snapshot support.
- Android has a Kotlin/Jetpack Compose frontend backed by the NDK/JNI bridge, private disk storage and VM-plus-disk snapshot recovery.
- iOS/iPadOS has a SwiftUI frontend backed by the C bridging header, document-picker disk import and VM-plus-disk snapshot recovery.
- CI is configured for Linux, macOS and Windows desktop builds/CTest/GUI and package checks, Android emulator instrumentation, and iOS Simulator recovery XCTest.

## Remaining validation and distribution work

1. Complete the current Windows CTest and extracted-package GUI/CLI validation; fix any remaining platform-specific failures.
2. Keep Android force-stop/relaunch tests and iOS terminate/relaunch XCTest green on the latest commit.
3. Configure Android and Apple distribution signing secrets, then verify signed APK/AAB and iOS archive/export signatures.
4. Complete device installation and launch checks for signed mobile artifacts and document store distribution.
5. Continue touch/accessibility polish, mobile resource-limit testing, workspace migration and cross-version snapshot compatibility.

## Current reality

The frontends and CI workflows are present in the repository; their status is determined by the latest per-platform workflow run, not by source presence alone. Preview artifacts are not production-signed releases. GitHub Release publishing is gated on both Android and iOS signing readiness and signature/checksum verification. Nexa remains an educational VM/OS simulator, not a hardware hypervisor or bootable host operating system.
