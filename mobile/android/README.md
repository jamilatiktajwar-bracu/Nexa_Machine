# Nexa Android JNI bridge

The JNI bridge exposes context/session lifecycle, assembly loading, bounded execution, stepping, pause/reset, keyboard input and error retrieval. Build with the Android NDK by configuring CMake with `-DANDROID=ON -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 -DNEXA_BUILD_HOST_TOOLS=OFF -DNEXA_BUILD_GUI=OFF`. The C ABI context must be serialized from the Kotlin side; do not call it concurrently.

This is a native bridge foundation, not a complete Android application. Activity/Compose UI, Gradle packaging, lifecycle integration, storage access framework, tests on emulator/device and signing remain future work.
