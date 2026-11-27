# Cross-platform host diagnostics

The platform module exposes portable host information without GUI, OS SDK, or filesystem dependencies.

- PlatformInfo reports OS, CPU architecture, compiler, build mode, byte order and desktop/mobile classification.
- platformSummary() formats it for CLI and bug reports.
- `nexa --platform` exits without starting a VM or touching guest files.
- Tests verify required fields and byte-order values.

CI now builds/tests on Ubuntu, Windows and macOS. Qt Widgets is exercised on Ubuntu only; other matrix legs compile the portable CLI/core. iOS remains a separately targeted frontend and is not covered by this desktop matrix.
