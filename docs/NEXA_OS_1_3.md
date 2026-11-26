# Nexa OS 1.3 — Persistent virtual filesystem

The VFS can now save its complete directory/file tree into a portable host-side image and restore it later.

## API

```cpp
nexa::VirtualFileSystem fs;
fs.createDirectory("/bin");
fs.createFile("/bin/app", {0x01, 0x02});
fs.saveImage("system.vfs");

nexa::VirtualFileSystem restored;
restored.loadImage("system.vfs");
```

Image format uses the `NEXAVFS1` signature, little-endian 32-bit counts and lengths, UTF-8 path bytes, and raw file data. The loader validates paths, parent directories, duplicates, entry and file size limits, truncation, and trailing bytes. It loads into temporary containers and only replaces the live VFS after the complete image passes validation.

Limits: up to 100,000 total entries, 4,096 bytes per path, and 64 MiB per file. This is an educational VFS image, not a journaled or crash-safe production disk filesystem. The image is stored on the host filesystem; guest processes do not receive arbitrary host path access.

## Build and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```
