# iOS

The iOS build is **experimental and unsigned**. It produces a real arm64 iOS
app bundle and `.ipa`, but nothing signs it, so getting it onto a device is a
step you have to do yourself (see [Installing](#installing)).

## Building

Requires full Xcode — the Command Line Tools alone do not ship the iPhoneOS SDK —
and the SDL3 submodules:

```bash
git submodule update --init --recursive android/app/jni/SDL3 android/app/jni/SDL3_image android/app/jni/SDL3_mixer android/app/jni/SDL3_ttf
```

```bash
tools/build-ios.sh
```

That writes `build-ios/frozen-bubble-sdl3-unsigned.ipa` (~27 MB). Other modes:

```bash
tools/build-ios.sh --simulator   # runs in the iOS Simulator; no .ipa
tools/build-ios.sh --clean       # discard the build tree first
```

To run the simulator build:

```bash
xcrun simctl install booted build-ios-sim/FrozenBubble.app
xcrun simctl launch --console-pty booted org.frozenbubble.sdl3
```

## How it differs from the other platforms

**SDL is compiled from source.** iOS has no package manager to install SDL3
from, and a `find_package` on a Mac would happily match the host's Homebrew
*macOS* build. The iOS build compiles SDL3, SDL3_image, SDL3_mixer and SDL3_ttf
from the pinned submodules under `android/app/jni` — the same sources the
Android build uses — configured in [`cmake/iOSDeps.cmake`](../cmake/iOSDeps.cmake).
Everything links statically, so the `.app` contains no embedded frameworks;
each embedded framework would need its own signature, which an unsigned build
cannot supply.

Only the codecs the game actually loads are enabled (PNG/GIF via SDL_image,
Ogg Vorbis via stb_vorbis, FreeType without HarfBuzz). The first build takes a
few minutes because of this; later ones are incremental.

**Assets live in the bundle.** There is no `DATA_DIR` compile-time path on iOS —
one baked in from the build machine could only ever be wrong on a device.
`InitDataDir()` resolves `SDL_GetBasePath()`, which on iOS is the `.app`
directory itself (iOS bundles are flat — no `Contents/Resources`).

**Saves and logs go to the app container.** `settings.ini`, the highscore files
and `frozen-bubble.log` are written under
`Library/Application Support/frozen-bubble/` inside the app's data container.
The working directory on iOS is the bundle, which is read-only and signed on a
real device, so nothing may be written relative to it.

**HTTP goes through NSURLSession.** The desktop code shells out to
`popen("curl ...")` for the public server list and the geolocation lookup. iOS
forbids `fork`/`exec` inside the app sandbox and ships no `curl`, so
[`src/platform_ios.mm`](../src/platform_ios.mm) supplies the same blocking
fetch via `NSURLSession` — the counterpart of the Android JNI `fetchUrl()`.
Two of those endpoints are plaintext HTTP and get named App Transport Security
exceptions in the Info.plist rather than a blanket `NSAllowsArbitraryLoads`.

**Rotates freely, portrait first.** The app allows portrait and both landscapes,
matching the browser build, which cannot lock orientation at all. Portrait is
listed first, which is as close to "prefer portrait" as iOS gets: the starting
orientation comes from how the device is being held, not from an app preference.
The playfield is a fixed 640×480 canvas, so portrait renders it as a band across
the middle of the screen rather than filling it — smaller, but playable.

Android differs: there a phone is portrait-*locked*, because SDL resolves a
fixed-size window to a single orientation and cannot offer the choice.

Allowing it takes two changes, not one. The Info.plist orientations are
necessary but not sufficient: SDL intersects them with a mask of its own, and
derives that mask from the requested window size whenever `SDL_HINT_ORIENTATIONS`
is unset. The window is 640×480, so SDL decides landscape-only by itself and the
plist never gets a say. `frozenbubble.cpp` sets that hint before creating the
window.

Android differs in a second way: only a TV box is locked, to landscape,
decided once at startup from whether the device has a touchscreen — a single
APK serves both, and the manifest cannot express a per-device answer, so the
choice is made in code instead (`DeviceHasTouchscreen()` in `frozenbubble.cpp`,
the same check the mouse/touch-aim default already used). A phone or tablet
rotates freely, same as iOS, but needs a second change beyond naming both
orientations: Android's `SDLActivity.setOrientationBis` only grants free
rotation (`SCREEN_ORIENTATION_FULL_USER`) when the window is also created with
`SDL_WINDOW_RESIZABLE` — without it, a fixed window with both orientations
named just breaks the tie by aspect once at startup and never revisits it.

**Mouse/touch aim is on by default.** There is no keyboard to aim with, and with
it off the only gesture is tapping a screen half, which cannot pick an angle.
Keyboard and controller aiming still work — whichever you used last wins — and
the setting is stored, so changing it sticks.

**The app icon is compiled at build time.** iOS 11 and later read the icon from
a compiled `Assets.car`, which only `actool` can produce, so
[`tools/ios-appicon.sh`](../tools/ios-appicon.sh) runs after the link and merges
actool's own `CFBundleIcons` keys into the Info.plist. The 1024 image in
`ios/AppIcon.xcassets` is committed, and is a resize of the same 1024 master
every platform's icon derives from
(`share/icons/frozen-bubble-icon-1024x1024.png`); regenerate every platform's
icon together, iOS included, with `python3 tools/make-app-icons.py` (needs
Pillow — nothing else does).

**Leaving a round needs a gesture.** Escape, gamepad B and Android's back button
all reach `QuitToTitle()`; iOS has none of the three, so touch had no way out of
a game at all. Swiping left across the bottom of the playfield — level with the
launcher or below, the band `HandleMouseAim` already ignores — now does it. The
band matters: a false positive here quits a game in progress, so the gesture is
kept out of the area where a long drag could just as well be someone aiming.

**The soft keyboard no longer covers the field you're typing into.** iOS (and
Android) already shift the view up so an active text field clears the
keyboard — SDL implements it, driven by the rect passed to
`SDL_SetTextInputArea`. That rect was being passed in 640×480 logical canvas
coordinates, which only coincide with the window coordinates SDL expects on a
4:3 window; on a letterboxed phone screen they are nowhere near each other, so
the view shifted by the wrong amount or not at all. `SetTextInputAreaLogical()`
in `platform.cpp` now converts through the renderer before handing the rect to
SDL.

**Composing a chat message keeps the conversation on screen.** Opening the chat
input used to replace the whole room with a bare "Send Chat Message" box, which
lost the log at the exact moment it mattered for reading what you're replying
to. It now grows the chat dock over the room's settings instead, keeping the
world map behind and showing as many recent messages as fit.

**Following a server registers this device for join notifications.** Marking a
server with **F**, or tapping the star at the left of its row, stores it in
`settings.ini` and hands the server this device's push token. The server keeps
that token in a file rather than in per-connection state, precisely because the
notification has to reach a device that is no longer connected. Delivery is the
OS's job, so a banner arrives whether the app is backgrounded or closed; while
the app is in the foreground it is suppressed, since the lobby already shows
who is online.

`push_ios.mm` asks for notification permission at startup and registers with
APNs. It deliberately does not replace SDL's `UIApplicationDelegate` — SDL owns
that and the app would stop launching — and instead grafts the two APNs
callbacks onto SDL's existing delegate class at runtime. The banner is
suppressed while the app is active by a `UNUserNotificationCenterDelegate` that
returns no presentation options.

A token still requires all three of: a signature carrying the
`aps-environment` entitlement (the default unsigned build has none, and free
sideloading profiles do not grant it), the player allowing the prompt, and APNs
answering. Any of those failing is an ordinary outcome reported as `""`, and
callers skip the registration rather than treating it as an error. The build
writes `build-ios/FrozenBubble.entitlements` for you to sign with; see
[PUSH_SETUP.md](PUSH_SETUP.md).

## What does not work

- **Hosting a LAN game.** "Start LAN game" as a *host* needs `fb-server` as a
  separate process, and iOS forbids `fork`/`exec` in the sandbox. Joining a game
  hosted elsewhere works — that is a plain TCP socket. Same restriction as the
  Android and Windows builds.
- **Receiving a follow notification on an unsigned build.** APNs registration
  needs the `aps-environment` entitlement, which requires signing with a paid
  Apple Developer profile — free sideloading does not grant it. Following a
  server still works and persists; the device just never gets a token.

## Installing

An unsigned `.ipa` will **not** install on a stock iPhone — iOS verifies the
code signature at install time and there is nothing here to verify. Options:

- **Sideloading tools** (AltStore, Sideloadly) re-sign the app with your own
  Apple ID. Free accounts expire the signature after 7 days.
- **Your own certificate**: sign the `.app` with a development or distribution
  identity and an embedded provisioning profile, then repackage:
  ```bash
  codesign -f -s "Apple Development: you@example.com" --entitlements your.entitlements build-ios/FrozenBubble.app
  ```
  `your.entitlements` needs to be the **profile's own entitlements**, not just
  `build-ios/FrozenBubble.entitlements` on its own — that file only carries
  `aps-environment`, and signing with only that omits `application-identifier`
  and the team ID, which the installer requires and normally gets auto-derived
  by Xcode. Pull the real set out of the profile instead (PlistBuddy needs a
  seekable file, not a pipe, hence the intermediate file):
  ```bash
  security cms -D -i embedded.mobileprovision > /tmp/profile.plist
  /usr/libexec/PlistBuddy -x -c "Print :Entitlements" /tmp/profile.plist > your.entitlements
  ```
  Also copy the profile itself into the bundle before signing (a stock device
  refuses to install without it):
  ```bash
  cp embedded.mobileprovision build-ios/FrozenBubble.app/embedded.mobileprovision
  ```
- **Xcode**: point a project at the built `.app`, or build with the Xcode
  generator and let Xcode manage signing.
- **TrollStore / a jailbroken device** installs unsigned bundles directly.

Signing is deliberately left out of the build: baking in a specific team ID
would produce an artifact that only installs for whoever built it.

## Status

Verified in the iOS Simulator: the app launches, resolves its assets from the
bundle, creates a Metal renderer, renders the title screen at the correct 4:3
letterbox, and reads back its settings on a second launch.

The app icon renders correctly on the simulator's home screen, and mouse/touch
aim comes up enabled in a freshly written `settings.ini`.

Portrait was verified by screenshot in the simulator: the canvas renders upright
and centred, matching the source artwork, rather than rotated.

Not yet verified: touch input and gameplay on iOS specifically (the touch
handling is platform-independent code shared with the Android build, and the
settings-panel tap behaviour was verified through the WebAssembly build, which
runs the same code), audio output, netplay, and anything at all on physical
hardware — the build is unsigned, so it has never been installed on a device.

Taps could not be injected into the simulator to confirm the letterbox mapping
end-to-end, so `tests/touch_letterbox_test.cpp` pins the arithmetic instead,
across every aspect ratio the game actually runs at.

Also unverified on a device: the keyboard-avoidance shift while composing chat,
the expanded chat dock, and the game room's settings list after a renumbering
that removed one row (see [CHANGELOG.md](../CHANGELOG.md)) — all built and unit
tested, none of them seen running on hardware.

Follow notifications are verified as far as they can be without credentials:
`tests/server_notify_test.py` drives the real server over the real protocol and
checks registration, the join hook, the datagram format, the cooldown,
unregistration and on-disk persistence, and the relay was confirmed by hand to
receive and log those datagrams.

On the iOS side specifically, the simulator confirms that the app links against
UserNotifications, launches without crashing, and shows the permission prompt
over the title screen with the expected Alert|Sound options. Neither APNs
callback fires there — a simulator running an unsigned build cannot complete
registration — so **the device token path itself has never executed**, and
nothing downstream of it (a real `NOTIFYREG` from a phone, an actual banner) has
been seen working. That needs a signed build on hardware.
