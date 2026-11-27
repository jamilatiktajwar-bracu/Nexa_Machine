# Nexa iOS frontend prototype

This SwiftUI shell drives the portable C ABI through a bridging header importing `nexa/c_api.h`. Add the compiled iOS `nexa_vm_core` static library and its public headers to an Xcode target, set the bridging-header path, and link required C++ runtime libraries. The app deliberately uses bounded instruction batches on a 15 ms main-run-loop timer.

Current scope: one local VM session, assembly load, start/pause/stop/reset/step and keyboard input. Disk-image document picker, coordinated snapshot UI, background execution policy, signing, simulator/device packaging and App Store configuration remain to be completed. The iOS core cross-compile CI job alone does not build this app.


## Generate and build the native iOS app

Install Xcode command-line tools, CMake and XcodeGen (`brew install cmake xcodegen`). From this directory run `xcodegen generate`, then open `NexaMachine.xcodeproj` in Xcode. The target pre-build phase cross-compiles `nexa_vm_core` for the selected iOS device or simulator SDK and links the SwiftUI app against it. For a simulator build, run `xcodebuild -project NexaMachine.xcodeproj -scheme NexaMachine -sdk iphonesimulator -destination 'generic/platform=iOS Simulator' CODE_SIGNING_ALLOWED=NO build`. The app currently implements one local session; persistence, disk image import/export, background execution and device signing are still pending.
