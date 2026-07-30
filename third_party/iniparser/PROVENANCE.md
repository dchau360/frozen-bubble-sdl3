# iniparser — vendored dependency

| | |
|---|---|
| Upstream | https://github.com/ndevilla/iniparser |
| Author | Nicolas Devillard and contributors |
| Licence | MIT — see [LICENSE](LICENSE) |
| Vendored files | `iniparser.c`, `iniparser.h`, `dictionary.c`, `dictionary.h` |
| Upstream version | **not recorded when vendored** — see below |

Used by `src/gamesettings.cpp` to read and write `settings.ini` under
`SDL_GetPrefPath()`. Compiled directly into the game target, so it ships inside
every released artifact (AppImage, DMG, Windows installer, APK, WASM bundle).

## On the missing version

The exact upstream release these four files were copied from was not recorded,
and the sources carry no version macro, so this file does not claim one. The
API surface present here — `iniparser_set_error_callback`, the `int64`/`uint64`
getters — indicates a 4.x release rather than the older 3.x series, but that is
an inference from the header, not a verified provenance record.

Before changing anything here, diff against a known upstream tag to establish
which one this matches, and record it in this file. Any local modifications
should be listed here too; none are currently known to exist, which is itself
unverified for the same reason.

## Why this file exists

Audit finding REL-014: the vendored copy shipped in every artifact with no
licence text, version, or provenance. MIT requires that "the above copyright
notice and this permission notice shall be included in all copies or
substantial portions of the Software", so distributing these sources without
`LICENSE` alongside them did not satisfy the licence.
