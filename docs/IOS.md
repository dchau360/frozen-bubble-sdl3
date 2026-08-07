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

**Rotates freely.** The app allows portrait and both landscapes, matching the
browser build, which cannot lock orientation at all. The playfield is a fixed
640×480 canvas, so portrait renders it as a band across the middle of the screen
rather than filling it — smaller, but playable.

Allowing it takes two changes, not one. The Info.plist orientations are
necessary but not sufficient: SDL intersects them with a mask of its own, and
derives that mask from the requested window size whenever `SDL_HINT_ORIENTATIONS`
is unset. The window is 640×480, so SDL decides landscape-only by itself and the
plist never gets a say. `frozenbubble.cpp` sets that hint before creating the
window.

Note that the Android build is still locked to landscape in its manifest
(`android:screenOrientation="landscape"`), so the two mobile platforms differ
here.

**Mouse/touch aim is on by default.** There is no keyboard to aim with, and with
it off the only gesture is tapping a screen half, which cannot pick an angle.
Keyboard and controller aiming still work — whichever you used last wins — and
the setting is stored, so changing it sticks.

**The app icon is compiled at build time.** iOS 11 and later read the icon from
a compiled `Assets.car`, which only `actool` can produce, so
[`tools/ios-appicon.sh`](../tools/ios-appicon.sh) runs after the link and merges
actool's own `CFBundleIcons` keys into the Info.plist. The 1024 master in
`ios/AppIcon.xcassets` is committed; regenerate it from the game's title art
with `python3 tools/make-ios-icon.py` (needs Pillow — nothing else does).

## What does not work

- **Hosting a LAN game.** "Start LAN game" as a *host* needs `fb-server` as a
  separate process, and iOS forbids `fork`/`exec` in the sandbox. Joining a game
  hosted elsewhere works — that is a plain TCP socket. Same restriction as the
  Android and Windows builds.

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
