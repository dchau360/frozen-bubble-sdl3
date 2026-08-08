# push-credentials

Drop APNs / FCM credential files here. The directory is mounted read-only into
the `notify-relay` container at `/credentials`.

**Nothing in here except this file should ever be committed** — see the
`.gitignore` alongside it.

Typical contents:

- `AuthKey_XXXXXXXXXX.p8` — the APNs auth key, referenced by
  `APNS_KEY_PATH=/credentials/AuthKey_XXXXXXXXXX.p8`
- `firebase-service-account.json` — referenced by
  `FCM_SERVICE_ACCOUNT_JSON=/credentials/firebase-service-account.json`

Leaving this directory empty is fine and is the default: the relay then runs in
stub mode and only logs what it would have sent.
