# Desktop Game-Speed Default Design

## Goal

Make 3.0 the authoritative default game-speed multiplier on desktop and
WebAssembly while retaining 1.25 on Android. Existing saved preferences,
including an explicit 2.0, must remain unchanged.

## Design

Define one platform-aware default in `gamesettings.h` and use it for the
`GameSettings::speedMultiplier` member initializer, new settings-file creation,
and the missing-key fallback in `ReadSettings()`. This removes the three
independent literals that previously drifted apart.

Default settings-file generation will serialize the shared numeric default
rather than maintaining a separate string literal. Loading continues to prefer
the value present in `settings.ini`; the shared default is used only when the
key is absent.

## Compatibility

- Desktop and WebAssembly default to 3.0.
- Android defaults to 1.25.
- Existing values from 1.0 through 5.0 remain untouched.
- Existing range clamping remains unchanged.
- Historical changelog entries remain intact; a current entry records the
  desktop default.

## Verification

Rebuild the native game and confirm existing saved preferences remain unchanged.
