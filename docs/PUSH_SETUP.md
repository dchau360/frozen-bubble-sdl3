# Setting up push credentials

The "follow a server" feature needs two things from you that no amount of code
can supply: an **APNs auth key** from Apple for iOS, and a **Firebase project**
for Android. Everything else — the client UI, the protocol, the server
registry, the relay — is already built and works without them, logging what it
*would* have sent.

This is the operator/developer side. Players need none of it.

> **Status:** Android has been verified end to end on a real device — a real
> FCM token, a real join event, a real banner. iOS is built the same way but
> has not yet been confirmed against a real APNs delivery (needs a paid
> Apple Developer account); everything short of the live Apple round-trip
> works today.

> **Cost:** Firebase Cloud Messaging is free. APNs requires a **paid Apple
> Developer Program membership, $99/year** — free Apple IDs cannot create push
> keys at all.

---

## Part 1 — Apple (iOS)

### What you need first

- A paid Apple Developer Program membership.
- The app's bundle id: **`org.frozenbubble.sdl3`** (set by `FB_IOS_BUNDLE_ID` in
  `CMakeLists.txt`; change it there if you use your own).

### 1. Register the App ID with Push enabled

1. Go to <https://developer.apple.com/account/resources/identifiers/list>.
2. **+** → **App IDs** → **App** → Continue.
3. Description: `Frozen Bubble`. Bundle ID: **Explicit**, `org.frozenbubble.sdl3`.
4. In the Capabilities list, tick **Push Notifications**.
5. Continue → Register.

If the App ID already exists, open it and tick **Push Notifications**, then Save.

### 2. Create the APNs auth key (.p8)

A `.p8` *auth key* is preferred over the older `.p12` *certificate*: one key
works for every app on your team, for both sandbox and production, and it never
expires.

1. Go to <https://developer.apple.com/account/resources/authkeys/list>.
2. **+** → name it e.g. `Frozen Bubble Push`.
3. Tick **Apple Push Notifications service (APNs)** → Continue → Register.
4. **Download** the `.p8`. Apple lets you download it **exactly once** — if you
   lose it you must revoke and create a new one.
5. Note the **Key ID** shown on that page (10 characters).
6. Note your **Team ID** from <https://developer.apple.com/account> → Membership.

### 3. Put the values where the relay can read them

Copy the `.p8` into `docker/push-credentials/` on the server (the directory is
git-ignored) and set, in a `.env` beside `docker-compose.yml`:

```bash
APNS_KEY_PATH=/credentials/AuthKey_XXXXXXXXXX.p8   # the filename Apple gave you
APNS_KEY_ID=XXXXXXXXXX                             # step 2.5
APNS_TEAM_ID=YYYYYYYYYY                            # step 2.6
APNS_TOPIC=org.frozenbubble.sdl3                   # the bundle id, exactly
```

Add `APNS_USE_SANDBOX=1` while testing with a development build — a token from a
development build is **not** valid against the production APNs host, and vice
versa. This mismatch is the single most common reason a correctly configured
push silently never arrives.

### 4. Signing — the part that catches people out

Push notifications only work on a build signed with a provisioning profile that
carries the `aps-environment` entitlement. That has consequences for how this
app is distributed:

- The unsigned `.ipa` this repo produces by default **cannot** receive
  notifications.
- Free sideloading (AltStore / Sideloadly with a free Apple ID) **also cannot** —
  free provisioning profiles do not include the push entitlement.
- You need a development or distribution profile created under your paid
  membership, for the App ID from step 1.

The build emits `cmake/FrozenBubble.entitlements` with `aps-environment` set;
pass it when you sign:

```bash
codesign -f -s "Apple Development: you@example.com" \
  --entitlements build-ios/FrozenBubble.entitlements \
  build-ios/FrozenBubble.app
```

Use `aps-environment = development` for a development profile, `production` for
distribution. The generated file defaults to `development`.

---

## Part 2 — Firebase (Android)

Free, and there is no signing complication.

### 1. Create the project

1. Go to <https://console.firebase.google.com> → **Create a project**.
2. Name it (e.g. `frozen-bubble`). Google Analytics is optional — decline it if
   you don't want it; nothing here depends on it.

### 2. Register the Android app

1. In the project, click the **Android** icon to add an app.
2. **Android package name:** `org.frozenbubble` — this must match
   `applicationId` in `android/app/build.gradle` exactly, or the token will be
   issued for a package that doesn't exist and delivery silently fails.
3. Nickname and SHA-1 are optional for Cloud Messaging; skip them.
4. Download **`google-services.json`** and place it at:

   ```
   android/app/google-services.json
   ```

   The build detects this file. **With it present** the Firebase Messaging SDK is
   compiled in; **without it** the app builds exactly as before and simply never
   obtains a token. So this file is what switches Android push on.

   It is git-ignored — it identifies your project and should not be committed.

### 3. Create the service account for the relay

The relay sends *through* Firebase, which needs server credentials — not the
`google-services.json`, which is the client half.

1. Firebase Console → gear icon → **Project settings** → **Service accounts**.
2. **Generate new private key** → confirm. A JSON file downloads.
3. Copy it to `docker/push-credentials/` on the server and set in `.env`:

```bash
FCM_SERVICE_ACCOUNT_JSON=/credentials/firebase-service-account.json
```

> Treat this file like a password — it can send notifications to every user of
> your app. It is git-ignored; never commit it.

---

## Part 3 — Turn it on

On the server:

```bash
cd docker
docker compose up -d --build notify-relay
docker compose logs notify-relay
```

The startup log tells you what it picked up:

```
INFO APNs configured (topic=org.frozenbubble.sdl3, sandbox=False)
INFO FCM configured (service account=/credentials/firebase-service-account.json)
INFO notify-relay listening on 0.0.0.0:9099
```

Anything still unconfigured says `... run in stub mode` instead, and that
platform keeps logging rather than delivering. The two halves are independent —
Android can go live while iOS waits for your membership.

---

## Verifying end to end

1. Install the app on a real device (a simulator/emulator cannot receive a real
   push).
2. Open it once — the token is requested at startup and sent when you follow a
   server.
3. **Net Game** or **LAN Game**, highlight your server, press **F** (or tap the
   star). The star fills in.
4. Confirm the server took it:
   ```bash
   docker compose exec fb-server cat /var/lib/fb-server/notify.dat
   ```
   You should see one line: `<platform> <token> <registered> <last-notified>`
   (`ios` or `android`).
5. Background or close the app.
6. From another device, join a room on that server.
7. The banner should arrive within a few seconds. `docker compose logs
   notify-relay` shows either `pushed to ios token=...` or the error Apple/Google
   returned.

Notifications are rate-limited to one per device per 10 minutes
(`FB_SERVER_NOTIFY_COOLDOWN_SECONDS`), so wait that out before expecting a
second one.

---

## When nothing arrives

| Symptom | Usual cause |
|---|---|
| `notify.dat` has no line for your device | The app never got a token. iOS: not signed with the push entitlement, or the permission prompt was declined. Android: `google-services.json` missing at build time, or package name mismatch. |
| Relay logs `[stub] would push…` | That platform's credentials are unset or incomplete — APNs needs **all four** variables. |
| Relay logs a `BadDeviceToken` / `403` from Apple | Sandbox/production mismatch. Flip `APNS_USE_SANDBOX`. |
| Relay logs `SenderId mismatch` from FCM | The `google-services.json` in the app and the service account belong to different Firebase projects. |
| Banner appears only when the app is open | Nothing is wrong with delivery — foreground suppression may not be wired for that build. Backgrounding the app is the real test. |
| Nothing at all on Android after force-stopping | Expected. FCM does not deliver to an app the user force-stopped from Settings until it is opened again. Swiping away from recents is fine. |
