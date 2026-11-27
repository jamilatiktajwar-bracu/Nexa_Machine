# Native C API

The portable engine can be built without CLI, OpenSSL, or Qt host dependencies:

```sh
cmake -S . -B build-core -DNEXA_BUILD_HOST_TOOLS=OFF -DNEXA_BUILD_GUI=OFF
cmake --build build-core --target nexa_vm_core
```

This produces a static library containing the VM, assembler, session registry, and C ABI. C ABI declarations are in `include/nexa/c_api.h`. All functions return 0 for success and -1 for failure (context creation returns null on failure); call `nexa_last_error` for details.

## Virtual disk sector API\n\nThe C ABI can attach one host-managed image to a VM session with `nexa_disk_attach`. Capacity must be a multiple of 512 bytes. `nexa_disk_sector_count`, `nexa_disk_read_sector`, and `nexa_disk_write_sector` expose bounded 512-byte sectors; call `nexa_disk_flush` to persist writes. Sector indices are zero-based and out-of-range access returns `-1`. Existing images must match the configured capacity exactly; missing images are initialized as zero-filled disks. Call `nexa_disk_flush` before removing a session because session removal releases the attachment without an implicit flush. Guest programs can also issue block I/O through `SYS`: set register A to 4 (read) or 5 (write), register B to the zero-based sector number, then execute `SYS`. Reads copy 512 bytes into guest RAM addresses 0–511; writes copy those bytes from guest RAM to the disk image. This ABI requires at least 512 bytes of guest RAM and currently addresses sectors 0–255 (the sector register is 8-bit). Guest writes remain in memory-backed image state until the host calls `nexa_disk_flush`.\n\n## Session enumeration

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


## Multi-device block storage (C API)

A session may attach multiple disk images with unique 32-bit device IDs using `nexa_disk_attach_device`. The legacy `nexa_disk_attach` is device ID 0. Query attached device count with `nexa_disk_device_count`; use the `*_device` sector and flush functions to address a specific device. Detach with `nexa_disk_detach(context, session, device, flush_before_detach)`; pass nonzero to flush pending writes before releasing the image. Detaching device 0 also disables the legacy guest `SYS A=4/5` handler. Session removal releases all attached devices.

For arbitrary guest RAM buffers, `nexa_disk_read_to_guest` and `nexa_disk_write_from_guest` transfer exactly one 512-byte sector at a checked 16-bit guest address. The complete buffer must fit in RAM. These host-mediated calls are synchronous; asynchronous queueing and interrupt completion are a later milestone. VM snapshots still do not include disk image contents, so coordinated VM+disk snapshots are not yet atomic.


## Guest descriptor I/O and cooperative completion

A guest can submit a disk operation with `nexa_disk_submit_guest`. The descriptor is 11 bytes, little-endian: byte 0 is operation (0=read, 1=write), bytes 1–4 are the 32-bit device ID, bytes 5–8 are the 32-bit sector index, and bytes 9–10 are the guest RAM buffer address. The 512-byte buffer must fit in guest RAM. Submission returns a monotonically increasing request ID and queues the request; it does not perform disk I/O immediately. The host scheduler calls `nexa_disk_process_one` to process at most one request. Poll `nexa_disk_request_status`: 0 pending, 1 completed, -1 failed. On successful completion the VM receives the caller-selected interrupt vector through its existing interrupt controller. Failed requests expose a message through `nexa_disk_request_error` and do not trigger a completion interrupt. This is cooperative asynchronous architecture: queue submission and completion are separated, but processing is host-pumped and not a background thread.
