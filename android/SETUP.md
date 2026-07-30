# Android TV Build Setup

## Prerequisites

| | Version | Notes |
|---|---|---|
| JDK | 17 | Temurin is what CI uses |
| Android SDK | compileSdk / targetSdk **34** | via Android Studio SDK Manager |
| NDK | **25.2.9519653** | pinned in `app/build.gradle` (`ndkVersion`) |
| CMake | **3.22.1** | pinned in `app/build.gradle`; install via the SDK Manager |

`minSdk` is 21. The APK is built for `arm64-v8a`, `armeabi-v7a` and `x86_64`.

## SDL3 sources

SDL3 and its satellite libraries are **git submodules**, not prebuilt binaries.
There is nothing to download by hand — initialise them and the NDK build
compiles them from source:

```bash
git submodule update --init --recursive android/app/jni/SDL3*
```

That populates `android/app/jni/` with `SDL3/`, `SDL3_image/`, `SDL3_mixer/` and
`SDL3_ttf/`. The Java glue comes from those submodules too; nothing needs to be
copied into `app/src/main/java/org/libsdl` by hand.

If those directories are empty the CMake configure step fails — that is almost
always a missing or partial submodule checkout rather than a real build error.

## Build

```bash
cd android
./gradlew assembleDebug     # debug APK
./gradlew assembleRelease   # release APK
```

Output lands in `app/build/outputs/apk/`. You can also open `android/` in
Android Studio and click Run.

A local `assembleRelease` produces `app-release-unsigned.apk` unless you supply
signing configuration, because the release build type declares no
`signingConfig`. That is deliberate — release keys live in CI secrets, not in
the repository. See [../docs/ANDROID_SIGNING.md](../docs/ANDROID_SIGNING.md) for
how tagged releases are signed with a persistent key, and why a tagged build
fails outright rather than shipping an APK that Android would refuse to install
over the previous version.

## Versioning

`versionCode` and `versionName` live in `app/build.gradle` and are **not**
derived from CMake — they are one of the three places a release bump must touch
(see the release checklist in [../CLAUDE.md](../CLAUDE.md)). `versionCode` must
strictly increase or Android rejects the upgrade.

## Installing on a device

```bash
adb install -r app/build/outputs/apk/release/app-release.apk
```

For Fire TV / Android TV boxes, enable ADB debugging in Developer Options and
connect over the network:

```bash
adb connect <device-ip>:5555
```

The README also documents installing a released build through the Downloader
app, which needs no adb at all.

## Testing with the Android TV emulator

In Android Studio's AVD Manager, create a device:
- Category: TV
- Size: 1080p
- API Level: 28+

## Controller Mapping

| Controller Button | Game Action |
|---|---|
| D-pad Up/Down | Menu navigation |
| D-pad Left/Right | Rotate bubble |
| A button | Fire / Select |
| B button | Back / Escape |
| Start | Pause |

## Notes

- Local server hosting is not available on Android (no fork/exec)
- Use `start-server.sh` on another machine — it enables both TCP (Net Game / direct IP) and UDP broadcast (LAN Game discovery) by default
- Public server list is fetched via `FrozenBubbleActivity.fetchUrl()` using `HttpURLConnection` (JNI call from C++)
