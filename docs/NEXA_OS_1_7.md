# Nexa OS 1.7 — Persistent shell workspace

Nexa OS 1.7 extends the 1.6 shell with a working directory and explicit VFS image persistence.

## New commands

- `pwd` — print the current VFS directory
- `cd DIR` — change directory; supports absolute and relative paths and `cd ..`
- `savefs HOST_PATH` — save the current VFS image to a host path
- `loadfs HOST_PATH` — restore a VFS image from a host path

Existing commands (`ls`, `cat`, `touch`, `mkdir`, `write`, `rm`, `run`, `ps`) now resolve relative paths against the current directory.

## Example

```text
nexa-os$ mkdir /apps
nexa-os$ cd /apps
nexa-os$ pwd
/apps
nexa-os$ write hello.nexa HALT
nexa-os$ run hello.nexa
nexa-os$ cd ..
nexa-os$ savefs nexa.vfs
```

The shell is still a host-side educational interface. The persistence commands intentionally accept host paths supplied by the operator; they are not a sandbox boundary. VFS images preserve files and directories, not process state or user accounts. Multi-user login, ownership/ACL metadata, pipelines, and a bootable hardware OS are not part of this release.
