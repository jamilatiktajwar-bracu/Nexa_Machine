# Nexa OS 2.1 — Account recovery and groups

Nexa OS 2.1 extends the educational account model.

## Account lockout and recovery

- Five consecutive failed password attempts lock a non-root account; root is exempt so an administrative recovery path remains available.
- A locked account cannot log in, even with the correct password.
- Root can reset a user's password with `passwd USER NEW_PASSWORD`; this clears the lockout.
- Root can clear lockout without changing credentials using `unlock USER`.
- Successful authentication clears the failed-attempt counter.

## Groups

- `groupadd GROUP` creates a group.
- `usermod -g GROUP USER` changes a user's primary group.
- `chgrp GROUP PATH` changes a file's group owner (root or the file owner).
- Group membership is used by the existing owner/group/other mode-bit checks.
- New accounts initially receive a same-named primary group.

## Persistence and compatibility

Security metadata format `NEXASEC3` stores groups, primary group assignments, password hashes, failed-attempt counters, lockout state, and file ownership/mode metadata. The loader remains compatible with `NEXASEC1` and `NEXASEC2`; older accounts are assigned groups inferred from their existing metadata.

## Security scope

This is an educational host-side policy implementation. The lockout is persistent but has no timed expiry; root must unlock or reset the account. Passwords remain command-line arguments and may appear in shell history or process input. This is not a PAM replacement, production identity service, or hardware-enforced security boundary. Disk images are not authenticated or encrypted.
