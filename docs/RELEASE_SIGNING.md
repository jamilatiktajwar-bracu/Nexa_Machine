# Mobile release signing

Nexa's release workflow produces preview artifacts when distribution-signing credentials are not configured. A preview APK signed with the Android debug key and an iOS Simulator build are useful for testing, but they are **not** production distribution artifacts.

## Android signing

Add these repository or environment secrets in GitHub Actions:

| Secret | Purpose |
| --- | --- |
| `NEXA_ANDROID_KEYSTORE_BASE64` | Base64-encoded release keystore file |
| `NEXA_ANDROID_STORE_PASSWORD` | Keystore password |
| `NEXA_ANDROID_KEY_ALIAS` | Signing key alias |
| `NEXA_ANDROID_KEY_PASSWORD` | Signing key password |

When all four are present, the workflow decodes the keystore into the runner's temporary directory, builds the signed APK and AAB, verifies the APK with `apksigner`, verifies the AAB with `jarsigner`, and writes/verifies SHA-256 checksums.

If any required secret is absent, the workflow builds a debug preview APK instead. The preview APK signature and checksum are still verified, but the job reports that distribution signing is not ready.

## iOS signing

Add these secrets:

| Secret | Purpose |
| --- | --- |
| `NEXA_IOS_DISTRIBUTION_CERTIFICATE_BASE64` | Base64-encoded Apple distribution certificate in PKCS#12 format |
| `NEXA_IOS_CERTIFICATE_PASSWORD` | Password for the PKCS#12 certificate |
| `NEXA_IOS_PROVISIONING_PROFILE_BASE64` | Base64-encoded App Store provisioning profile |
| `NEXA_IOS_TEAM_ID` | Apple Developer team identifier |

When all four are present, the workflow imports the certificate into a temporary keychain, installs the provisioning profile, archives the iOS app, verifies the app signature with `codesign --verify --deep --strict`, exports the App Store Connect package, and writes/verifies its SHA-256 checksum.

If any required secret is absent, the workflow builds and uploads an unsigned-for-distribution iOS Simulator preview instead.

## Publishing policy

The GitHub Release publishing job runs only for a `v*` tag and only when both the Android and iOS jobs report `signing_ready=true`. Desktop archives and mobile preview artifacts can be uploaded by the packaging workflow without satisfying that production-release gate.

The workflow never treats a debug APK or Simulator app as a signed store release. Check the Android/iOS job conclusions and uploaded artifact contents before distributing a tagged release.

## Secret handling

Configure secrets through **Repository Settings → Secrets and variables → Actions** (or an appropriately scoped environment). Do not paste certificate contents, passwords, keystores, or provisioning profiles into issues, pull requests, source files, or chat. Keep the private keystore and its passwords in an approved secure backup outside the repository.
