# Frozen Bubble: SDL3

A C++ / SDL3 port of the classic [Frozen Bubble 2](https://en.wikipedia.org/wiki/Frozen_Bubble),
reimplementing its gameplay, network multiplayer and chain-reaction system.
The original was Linux-only; this port runs on **Linux, macOS, Windows,
Android, and in the browser**.

[**Play in your browser**](https://dchau360.itch.io/frozenbubble2) ·
[Downloads](https://github.com/dchau360/frozen-bubble-sdl3/releases/latest) ·
[Source](https://github.com/dchau360/frozen-bubble-sdl3) ·
[Privacy policy](privacy/)

![A network game room with several players](screenshots/game-room.png)

## Download

| Platform | File |
| --- | --- |
| Linux | `frozen-bubble-linux-x86_64.AppImage` — `chmod +x` and run |
| macOS | `frozen-bubble-macos-arm64.dmg` — Apple Silicon only |
| Windows | `frozen-bubble-windows-setup.exe` — unsigned, SmartScreen will warn |
| Android | `frozen-bubble-android-tv.apk` — same APK for TV boxes, phones and tablets |
| Browser | [Play on itch.io](https://dchau360.itch.io/frozenbubble2) — desktop and mobile, including iPhone |

Downloads are on the
[releases page](https://github.com/dchau360/frozen-bubble-sdl3/releases/latest);
[itch.io](https://dchau360.itch.io/frozenbubble2) hosts the browser version.

There is no iOS download. The build exists but is unsigned and experimental, so
a device will not install it without re-signing — see
[docs/IOS.md](https://github.com/dchau360/frozen-bubble-sdl3/blob/main/docs/IOS.md).
To play on an iPhone, use the browser build.

## What it does

- **Single player** through the original level set, plus a level editor.
- **Local multiplayer** for 2–4 players, with gamepad support. Three- and
  four-player games are experimental and less play-tested than the rest. For
  five or more players, use a network room.
- **Network multiplayer** against the original `fb-server` protocol, so this
  port and the original game can share a server.
- **Chain reactions**, malus attacks and the original scoring — ported against
  the Perl source rather than reimplemented from memory.

## Playing online

The game ships with no server of its own. On startup it fetches a
community-maintained list of public servers, and **anyone can run one** — the
server lives in
[`server/`](https://github.com/dchau360/frozen-bubble-sdl3/tree/main/server),
and [SetupServer.md](https://github.com/dchau360/frozen-bubble-sdl3/blob/main/SetupServer.md)
covers hosting your own.

Chat is not moderated by the developer. `/block <nick>` hides a player's
messages on your device immediately, without needing anything from the server;
`/report <nick> <reason>` sends a report to that server's operator. Reports are
never acted on automatically — nicknames are chosen fresh on every connection
and are not tied to any account, so automatic kicks on report would just hand
everyone a way to remove anyone they liked.

## Credits

This is a port, not an original work. Frozen Bubble was created by the
**Frozen-Bubble Team** — Guillaume Cottenceau (design and programming),
Alexis Younes and Amaury Amblard-Ladurantie (artwork), and Matthias Le Bidan
(soundtrack), with contributions from many others listed in
[AUTHORS](https://github.com/dchau360/frozen-bubble-sdl3/blob/main/AUTHORS).
The artwork and music here are theirs.

Licensed under the
[GNU General Public License v2](https://github.com/dchau360/frozen-bubble-sdl3/blob/main/COPYING),
the same licence as the original. The complete source is on
[GitHub](https://github.com/dchau360/frozen-bubble-sdl3), and bug reports are
welcome via [GitHub Issues](https://github.com/dchau360/frozen-bubble-sdl3/issues).
