# Frozen Bubble: SDL3

A C++ / SDL3 port of the classic [Frozen Bubble 2](https://en.wikipedia.org/wiki/Frozen_Bubble),
reimplementing its gameplay, network multiplayer and chain-reaction system.
The original was Linux-only; this port runs on **Linux, macOS, Windows,
Android, and in the browser**.

[**Get it on Google Play**](https://play.google.com/store/apps/details?id=org.frozenbubble) ·
[Play in your browser](https://dchau360.itch.io/frozenbubble2) ·
[Downloads](https://github.com/dchau360/frozen-bubble-sdl3/releases/latest) ·
[Source](https://github.com/dchau360/frozen-bubble-sdl3) ·
[Privacy policy](https://dchau360.github.io/frozen-bubble-sdl3/privacy/)

![A network game room with several players](screenshots/game-room.png)

## Download

| Platform | File |
| --- | --- |
| Android | [Google Play](https://play.google.com/store/apps/details?id=org.frozenbubble) — phones, tablets and TV |
| Linux | `frozen-bubble-linux-x86_64.AppImage` — `chmod +x` and run |
| macOS | `frozen-bubble-macos-arm64.dmg` — Apple Silicon only |
| Windows | `frozen-bubble-windows-setup.exe` — unsigned, SmartScreen will warn |
| Android (sideload) | `frozen-bubble-android-tv.apk` — same APK as Play, for TV boxes and devices without Play |
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
- **Local multiplayer** for 2–5 players, with gamepad support. Games above
  two players are experimental and less play-tested than the rest. Empty
  seats can be filled with bots. Above five players, use a network room.
- **Network multiplayer** against the original `fb-server` protocol, so this
  port and the original game can share a server.
- **Chain reactions**, malus attacks and the original scoring — ported against
  the Perl source rather than reimplemented from memory.

## Screens

![The main menu](screenshots/main-menu.png)

![A local two-player game](screenshots/local-2player.png)

![The post-round statistics table](screenshots/round-stats.png)

## Playing online

The game ships with no server of its own. On startup it fetches a
community-maintained list of public servers, and **anyone can run one** — the
server lives in
[`server/`](https://github.com/dchau360/frozen-bubble-sdl3/tree/main/server),
and [SetupServer.md](https://github.com/dchau360/frozen-bubble-sdl3/blob/main/SetupServer.md)
covers hosting your own.

Chat is not moderated by the developer. `/block <nick>` hides a player's
messages on your device immediately, without needing anything from the server;
if you host the room, `/kick p2` removes that player from it outright.
`/report <nick> <reason>` sends a report to that server's operator. Reports are
never acted on automatically — nicknames are chosen fresh on every connection
and are not tied to any account, so automatic kicks on report would just hand
everyone a way to remove anyone they liked.

## Community

[Join the Discord](https://discord.gg/uE4dq8fqGW) — the same invite the
**Join Discord server** row on the NET GAME server list and the online
lobby opens in your browser. There's an alert whenever someone connects to
a public server, plus a result alert at the end of every round — game
mode, winner (or draw), and the full player roster — if you're looking for
an opponent or just keeping an eye on how things are going.

Running your own server? It can post its own join and round-result alerts
to a Discord channel of your choosing — see
[SetupServer.md](https://github.com/dchau360/frozen-bubble-sdl3/blob/main/SetupServer.md#optional--discord-join--result-alerts).

## Publisher and contact

Frozen Bubble for Android is published on Google Play by
**Solina AI LLC**, a Texas limited liability company. The app is
free software and the company makes no claim over the original game's code,
artwork or music — see Credits below.

There is no support email. Questions, bug reports, privacy and
data-removal requests all go to the public issue tracker, which is read by
the maintainer:
[github.com/dchau360/frozen-bubble-sdl3/issues](https://github.com/dchau360/frozen-bubble-sdl3/issues).
Using the tracker rather than mail keeps the history public, so an answer
to one player is visible to the next one with the same question.

The [privacy policy](https://dchau360.github.io/frozen-bubble-sdl3/privacy/)
covers what the app stores and what the ads in the Android build collect.

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
