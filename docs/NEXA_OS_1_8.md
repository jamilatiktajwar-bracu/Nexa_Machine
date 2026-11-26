# Nexa OS 1.8 — User Accounts and File Permissions

NexaShell now has a small educational account and discretionary access-control layer.

Commands:
- `useradd USER`: create a local user (no password authentication in this prototype).
- `login USER`: switch the active shell identity.
- `whoami`: print the active identity.
- `chmod MODE PATH`: set Unix-style octal mode bits, for example `chmod 640 /notes.txt`.
- `chown USER PATH`: change file owner (root or current owner only).

New files are owned by the creating user and default to mode 0644; directories default to 0755. Permission checks apply to shell `cat`, `write`, `rm`, and `run`. Root bypasses checks. This version keeps account and file metadata in the shell's in-memory security manager; metadata is not yet persisted in VFS images, and there are no passwords, groups administration, privilege rings, or hardware-enforced isolation. Do not use this prototype as a security boundary.
