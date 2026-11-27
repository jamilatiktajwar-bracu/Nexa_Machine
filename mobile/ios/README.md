# Nexa iOS frontend prototype

This SwiftUI shell drives the portable C ABI through a bridging header importing `nexa/c_api.h`. Add the compiled iOS `nexa_vm_core` static library and its public headers to an Xcode target, set the bridging-header path, and link required C++ runtime libraries. The app deliberately uses bounded instruction batches on a 15 ms main-run-loop timer.

Current scope: one local VM session, assembly load, start/pause/stop/reset/step and keyboard input. Disk-image document picker, coordinated snapshot UI, background execution policy, signing, simulator/device packaging and App Store configuration remain to be completed. The iOS core cross-compile CI job alone does not build this app.
