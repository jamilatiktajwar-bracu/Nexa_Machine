# Nexa OS 2.2 — Sessions and supplementary groups

## New capabilities

- Accounts may belong to a primary group and multiple supplementary groups.
- `usermod -aG GROUP USER` adds a supplementary membership.
- `usermod -rG GROUP USER` removes a supplementary membership.
- `groups [USER]` displays primary and supplementary memberships.
- File permission checks use supplementary group membership as well as the primary group.
- `logout` closes the current shell session. Until a successful login, only `help`, `whoami`, and `login` are available.
- Logout resets the shell working directory to `/`.
- Metadata format `NEXASEC4` persists supplementary groups and remains backward compatible with versions 1–3.

## Security notes

This is an educational host-side policy layer, not a hardware isolation boundary. Passwords are still passed as shell command text; use only test credentials. There is no production-grade terminal authentication, PAM integration, encrypted disk image, or session timeout.
