# Nexa OS 1.9 — Persistent security metadata

Nexa OS 1.9 extends VFS image persistence to include shell account and file ownership metadata.

- savefs HOST_PATH writes a versioned NEXASEC1 metadata record into the VFS image before saving.
- loadfs HOST_PATH restores users and file owner/group/mode records after loading the image.
- The current login session resets to root after metadata restoration.
- Metadata parsing validates record types, duplicate names/paths, user references, and permission mode bounds before replacing in-memory security state.
- Older images without the security record remain loadable and start with the default root account.

This remains an educational host-side policy layer. It does not provide password authentication, cryptographic integrity, secure boot, or hardware-enforced isolation. Do not treat VFS images as trusted security databases.

Run the full CMake/CTest suite to verify the persistence integration.
