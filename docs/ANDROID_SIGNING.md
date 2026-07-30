# Android release signing

Android identifies an app by the certificate that signed it. An update installs
over an existing app only when both are signed by the **same key**. If the key
changes, users cannot upgrade — they have to uninstall and lose their data, and
Play Store uploads are rejected outright.

CI used to run `keytool -genkeypair` on every build, so every release carried a
different certificate and no release could ever upgrade another. The workflow now
takes the key from repository secrets instead, and **fails a tagged release** if
they are missing rather than publishing an APK that cannot be upgraded.

## One-time setup

You generate the key yourself and store it as a secret. Nobody else — including
tooling — should ever hold it.

### 1. Create the keystore

Pick a strong password and keep it in your password manager. Replace the `-dname`
values with your own.

```bash
keytool -genkeypair -v \
  -keystore frozen-bubble-release.keystore \
  -alias frozenbubble \
  -keyalg RSA -keysize 2048 -validity 10000 \
  -dname "CN=Frozen Bubble, OU=Game, O=FrozenBubble, L=Unknown, S=Unknown, C=US"
```

`keytool` prompts for the store and key passwords. Using the same value for both
is fine and keeps the Gradle configuration simpler.

### 2. Back it up before doing anything else

**This file cannot be regenerated.** Losing it means never being able to update
the app again for anyone who installed it. Store a copy somewhere durable and
offline — a password manager attachment or an encrypted backup, not just this
repository's secrets.

### 3. Encode it for GitHub

```bash
base64 -i frozen-bubble-release.keystore | pbcopy
```

On Linux use `base64 -w0 frozen-bubble-release.keystore`.

### 4. Add the repository secrets

In **Settings → Secrets and variables → Actions → New repository secret**, add:

| Secret | Value |
|---|---|
| `ANDROID_KEYSTORE_BASE64` | the base64 text from step 3 |
| `ANDROID_KEYSTORE_PASSWORD` | the store password from step 1 |
| `ANDROID_KEY_ALIAS` | `frozenbubble`, unless you changed `-alias` |
| `ANDROID_KEY_PASSWORD` | the key password from step 1 |

### 5. Keep the keystore out of the repository

Never commit the `.keystore` file or its passwords. Delete your local copy only
once the backup from step 2 is verified.

## What CI does with it

- **Secrets present** — the keystore is decoded to a temporary path, used to sign
  the release APK, and deleted afterwards.
- **Secrets absent, ordinary build** — an unsigned APK is produced and a warning
  is logged. Fine for checking that the build compiles.
- **Secrets absent, tagged release** — the job fails on purpose. A published
  release signed by a throwaway key is worse than no release, because users who
  install it are stranded on that version.

## Related

`versionCode` in `android/app/build.gradle` must increase on every release, or
Android rejects the install even when the signature matches. It was pinned at
`10` across several releases; it is now bumped per release.

Both issues are recorded in the repository audit as REL-007 (signing) and
REL-004 (`versionCode`). See `docs/audit/SDL3_COMPLETE_REVIEW.md`.
