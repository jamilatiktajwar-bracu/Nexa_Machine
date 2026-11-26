# Nexa OS 2.3 — Directory permissions and session security

- Interactive terminal prompts for login, useradd, and passwd disable terminal echo (POSIX termios and Windows console input).
- Sessions expire after 15 minutes of inactivity; expired sessions are logged out and reset to /.
- Directory traversal requires execute permission on every ancestor.
- Directory listing requires read and execute permission.
- Creating/deleting entries requires write and execute permission on the parent directory.
- File reads and execution require traversable parent directories.

## Limitations
This is an educational host-side policy layer, not an operating-system security boundary. Passwords supplied through NexaShell's direct API remain command text; the interactive CLI collects them without echo. No hardware MMU protection or hostile-host isolation is provided.
