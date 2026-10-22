# High scores, accounts and FAQ

Answers to the questions players ask most about the high score boards, player
accounts, and the game in general. If yours is not here, ask on the
[issue tracker](https://github.com/dchau360/frozen-bubble-sdl3/issues) or the
[Discord](https://discord.gg/uE4dq8fqGW).

## High scores

### What boards are there?

The game keeps two kinds of record for the 1-player campaign. **Furthest
level** ranks how far a run got, with the faster time winning a tie; reaching
the end of all 100 levels is shown as cleared. **Most points** ranks the best
single life, since losing a life sets your score back to zero. Each board has
an all-time list and a this-week list that resets every Monday at 00:00 UTC.

Your own device keeps its own tables too, under **MY SCORES** on the High
Scores screen. The online lists are on the **POINTS** and **LEVEL** tabs in
the game and on the [Highscores](../scores/) page here.

### Why are there keyboard and mouse/touch boards?

Aiming with a mouse or a finger is a different skill from steering the
launcher with keys or a gamepad, so the two are ranked separately. A level
counts as keyboard/gamepad only if every shot in it was made that way; one
mouse or touch shot puts that level on the mouse/touch board. You can show
either board or both together, and every entry is marked with its input.

### What doesn't count?

A run stops reaching any high score table, local or online, as soon as you:

- turn the **aim guide** on, or
- choose **Continue** after losing a life (or *Restart level* from the in-game
  menu).

Whatever the run had already recorded before that still stands. **Start over**
begins a new run that counts again. Only campaign runs that start at level 1 of
the default levelset can reach the online boards; picked levels and random
levels count locally only.

### How do I see the details of a score?

Tap a score on the High Scores screen, or press Space (Y on a gamepad) and
move with the arrows. A strip under the list shows the whole record: name,
level, time, shots, input, country and the date it was set. On this website,
click or tap a row.

### Can I turn sending off?

Yes. Switch off **Online highscores** in the 1-player menu and nothing more is
sent. You can still read the boards.

### Are the scores checked?

Only loosely. The server rejects runs that are impossible on their face, such
as clearing levels faster than a second each, and an operator can remove a
cheating account, but scores are not replayed to prove they happened.

## Accounts

### Do I need to sign up?

No. The game makes an account for you on its
own: a random 16-character **account code** stored on your device. There is no
email address and no password. The code is turned into a signing key, and the
server only ever sees a public key and the signatures it makes, so it can tell
your scores apart from everyone else's without knowing who you are.

### How do I keep my scores on a new device?

Open **Account code** (in the 1-player menu, at the bottom of the NET GAME
server list, or on the High Scores screen) and either:

- copy the code and type it into the new device with **Use another code**, or
- choose **Set PIN** to pick a 4-to-8-digit PIN under your name, then
  **Link with PIN** on the other device. The second device then plays as your
  account and its own scores are merged into yours.

Write your code down somewhere safe. If a device is lost and you never copied
the code or set a PIN, the account cannot be recovered.

### Why does my name have a #tag?

Two players can choose the same name. The four characters after the `#` come
from the account, so "bob#1a2b" and "bob#9f00" are clearly different people.

### How do I delete my account?

Open **Account code** and choose **Delete account**. It removes your lines
from this port's high score server and starts a fresh account on your device.
The [privacy policy](../privacy/#delete-account) describes exactly what is
stored and what deletion removes.

## General questions

### Is the game free?

Yes. It is free software under the GNU General Public License, the same
licence as the original. The Android version shows an occasional ad, for
example when you join an online lobby or continue after losing.

### Which platforms can I play on?

Android (phones, tablets and TV) from Google Play, Windows, macOS and Linux
from the [download page](https://github.com/dchau360/frozen-bubble-sdl3/releases/latest),
and any modern browser, including on an iPhone, from
[itch.io](https://dchau360.itch.io/frozenbubble2). There is no App Store
version.

### Can I play against someone using a different device?

Yes. Every version, including the browser one, connects to the same servers,
so a phone player can share a room with someone on Windows.

### Is this the official Frozen Bubble?

No. It is a fan-made port of the original game, rewritten so it runs on
current systems. The original game, its artwork and its music are the work of
the Frozen-Bubble Team; the [history page](../history/) tells the story.

### I found a bug. Where do I report it?

On the [GitHub issue tracker](https://github.com/dchau360/frozen-bubble-sdl3/issues).
Say which platform you play on and, if you can, what you did just before it
happened.
