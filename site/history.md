# The history of Frozen Bubble

Frozen Bubble is one of the best-known free games of the Linux desktop, and
for many people one of the first games they played there. This page tells where it
came from, what Frozen Bubble 2 added, and why this port exists.

## The original game

Frozen Bubble was written by the **Frozen-Bubble Team**: Guillaume Cottenceau
designed and programmed it, Alexis Younes and Amaury Amblard-Ladurantie drew
the artwork, and Matthias Le Bidan wrote the soundtrack. It came out in 2002,
written in Perl on top of the SDL graphics library, and it was free software
from the start, released under the GNU General Public License.

The idea was not new. Shooting coloured bubbles at a ceiling of bubbles goes
back to Taito's arcade game *Puzzle Bobble* of 1994, sold in the West as
*Bust-a-Move*. What Frozen Bubble brought was polish. The penguins, the icy
backgrounds, the bubbles that freeze over when you lose, and a memorable
soundtrack gave it a character of its own, and its 100
levels were carefully designed rather than generated. It was small, it ran on
almost anything, and it was packaged by many Linux distributions, so a great
many people found it already installed.

## Frozen Bubble 2

Over its later releases the game grew from a puzzle game into a party game.
Frozen Bubble 2 brought **network play** for up to five players, with a lobby, chat and
a small dedicated server, `fb-server`, that anyone could run. By then it also
had **chain reactions**, where bubbles falling from a pop can fly into a spot that
completes another group, and a **multiplayer training** mode against the
clock. Attack bubbles, sent at your opponents whenever you popped or dropped
more than two bubbles at once, made every big drop a weapon.

Frozen Bubble 2 is the version most people remember, and the one this port is
based on. Its network protocol, its rules for attacks and its 100 levels are
reproduced here, checked line by line against the original Perl source rather
than remembered.

## Why a port?

The original was written for the Linux desktop of its day. It depends on an
old version of SDL and on Perl modules that are increasingly hard to install,
and it never ran on phones, on Windows or in a browser. A game that was meant
to be passed around had become hard to play at all.

**Frozen Bubble: Battle Royale** is a rewrite of the game in C++ on SDL3, the current
version of the same library the original used. Rewriting rather than wrapping
the old code means one program runs on Windows, macOS, Linux, Android (phones,
tablets and TV) and in any modern web browser. The artwork and music are the
original team's, and the code remains under the GPL, so anyone can read it,
change it or build it themselves from
[the source on GitHub](https://github.com/dchau360/frozen-bubble-sdl3).

The network side is compatible with the original: this port talks the same
protocol to the same `fb-server`, extended carefully so that older clients
still work. The original Linux game and this port can share a server and,
for the original game's rules, a room.

## What is new

Being a rewrite made room for things the original never had. Among them:

- **Online rooms for up to twenty players**, with a layout for large battles,
  target selection, and spectating once you are out.
- **Four multiplayer modes**: Classic, Clear, Race and Timed, all playable in
  teams. See [game modes](../modes/).
- **Local multiplayer for up to five**, with gamepads and bots to fill empty
  seats.
- **Swap**, a pocket that holds one bubble for later.
- **Points and online high scores** for the 1-player campaign, with weekly
  boards, a time bonus and player accounts that need no sign-up. See the
  [high scores FAQ](../faq/).
- **Tournaments**, weekly rankings, replays, mouse and touch aiming, a
  colour-blind bubble set and new candy-style bubbles.

A full list of every difference from the original, including fixes to bugs in
the original server, is kept in the project's
[comparison document](https://github.com/dchau360/frozen-bubble-sdl3/blob/main/docs/PARITY.md),
and every release is described in the [release notes](../news/).

## Credits

Frozen Bubble belongs to the people who made it. The full list of
contributors to the original game, including the many translators and
level designers, is in the
[AUTHORS](https://github.com/dchau360/frozen-bubble-sdl3/blob/main/AUTHORS)
file. This port is maintained independently and is not made or endorsed by
the original team. On Google Play the Android version is published by Solina
AI LLC, which makes no claim over the original game's code, artwork or music.
