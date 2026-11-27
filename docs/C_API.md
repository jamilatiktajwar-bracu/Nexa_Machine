# Native C API

The portable engine can be built without CLI, OpenSSL, or Qt host dependencies:

```sh
cmake -S . -B build-core -DNEXA_BUILD_HOST_TOOLS=OFF -DNEXA_BUILD_GUI=OFF
cmake --build build-core --target nexa_vm_core
```

This produces a static library containing the VM, assembler, session registry, and C ABI. C ABI declarations are in `include/nexa/c_api.h`. All functions return 0 for success and -1 for failure (context creation returns null on failure); call `nexa_last_error` for details.

## Session enumeration

Use `nexa_session_list(context, nullptr, 0, &count)` to query the required array size, allocate `count` entries, then call again with the buffer and capacity. The function returns the required count in `out_count`; an undersized non-null buffer returns `-1` and sets the context error.

## Swift / Objective-C++ bridge
Wrap the opaque `NexaContext*` and session handle in a Swift class. Use a bridging header importing `nexa/c_api.h`; keep assembly parsing and execution in the core. Call `nexa_session_step` from a bounded UI timer to keep the UI responsive, and update view state from `nexa_session_info`. File paths for snapshots should come from the iOS document picker and remain inside the app sandbox unless the user exports them.

## Android JNI
Link the same static core into a native C++ JNI layer. Kotlin/Compose calls a narrow wrapper; do not expose raw pointers or C++ exceptions across JNI. Use Storage Access Framework URIs to copy selected files into app-private storage before passing local paths to the C ABI.

## Safety and limits
- Context and sessions are owned by the caller; destroy contexts when finished.
- The C API is synchronous and not thread-safe. Serialize calls per context.
- Use bounded `nexa_session_run` budgets or repeated `nexa_session_step` calls.
- Guest RAM access is checked by the VM and errors are returned.
- Snapshots store VM execution state, not disk images or workspace profiles.
- The mobile frontends and platform packaging are not yet implemented.
