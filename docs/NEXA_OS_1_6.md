# Nexa OS 1.6 — User-space shell and application launcher

Nexa OS 1.6 adds a small command interpreter backed by the Nexa kernel and in-memory VFS.

## Start the shell

```sh
nexa --os
```

## Commands

- `help` — show commands
- `ls [DIR]` — list a VFS directory
- `cat PATH` — display a file
- `touch PATH` — create an empty file
- `mkdir PATH` — create a directory
- `write PATH TEXT` — create or replace a text file
- `rm PATH` — remove a file
- `run PATH` — assemble and launch a guest assembly program from the VFS, schedule it, and display its output
- `ps` — show process IDs, states, and instruction ticks
- `exit` — leave the shell

Example session:

```text
nexa-os$ mkdir /apps
nexa-os$ write /apps/hello.nexa LOAD A, 1
nexa-os$ ls /apps
nexa-os$ run /apps/hello.nexa
```

The shell is a user-space-style host process interface over the educational kernel API. The VFS starts in memory and is not automatically persistent across launches; use the VFS image API from host code to save it. The shell does not yet implement users, pipelines, redirection, a POSIX ABI, or a hardware boot path.
