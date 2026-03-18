# Android TV Build Setup

## Prerequisites

1. **Android Studio** — https://developer.android.com/studio
2. **NDK** (r25c or later) — install via Android Studio SDK Manager
3. **CMake 3.22+** — install via Android Studio SDK Manager
4. **SDL2 Android prebuilt libraries** — see below

## SDL2 Android Libraries

Download prebuilt SDL2 Android libraries and place them in `android-libs/` next to this directory:

```
android-libs/
  SDL2/
    include/        (SDL2 headers)
    lib/
      arm64-v8a/libSDL2.so
      x86_64/libSDL2.so
  SDL2_image/
    include/
    lib/arm64-v8a/libSDL2_image.so
    lib/x86_64/libSDL2_image.so
  SDL2_mixer/
    include/
    lib/arm64-v8a/libSDL2_mixer.so
    lib/x86_64/libSDL2_mixer.so
  SDL2_ttf/
    include/
    lib/arm64-v8a/libSDL2_ttf.so
    lib/x86_64/libSDL2_ttf.so
```

**Download from:** https://github.com/libsdl-org/SDL/releases (look for SDL2-x.x.x-android.zip)
Same for SDL2_image, SDL2_mixer, SDL2_ttf.

## SDL2 Java Source

Copy SDL2's Android Java glue into the project:

```bash
# From SDL2 source distribution:
cp -r SDL2/android-project/app/src/main/java/org/libsdl \
      android/app/src/main/java/org/libsdl
```

SDL2 source: https://github.com/libsdl-org/SDL/releases

## Build

1. Open `android/` in Android Studio
2. Connect Android TV device or start Android TV emulator
3. Click Run

## Testing with Android TV Emulator

In Android Studio AVD Manager, create a device:
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
- LAN discovery works if server is started with `-l` flag on another machine
- Public server list fetching requires Java HTTP implementation (TODO)
- Assets are bundled in the APK via the `assets/` source set pointing to `share/`
