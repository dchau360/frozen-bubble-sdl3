# Privacy Policy — Frozen Bubble: SDL3

**Effective date:** October 1, 2026

Frozen Bubble: SDL3 ("the app") is a free, open-source game
([GPLv2 licensed](https://github.com/dchau360/frozen-bubble-sdl3/blob/main/COPYING), source at
[github.com/dchau360/frozen-bubble-sdl3](https://github.com/dchau360/frozen-bubble-sdl3)).
This policy covers the Android build distributed on Google Play; the same
practices apply to the other platform builds except where noted (ads and
in-app purchases are Android-only).

The app has no sign-up, no analytics SDK, and does not collect your name,
email, or any other real-world identity. Its only "account" is an anonymous
random code created on your device (below). What it does handle is described
below.

## Information collected

**Nickname.** You choose a display nickname in Settings. It's stored only
on your device and sent to whichever multiplayer server you connect to, so
other players in your game can see it. It is not tied to any account and
isn't sent anywhere else.

**Anonymous player account.** The first time you play online, or use the
online highscore board with Online highscores on (below), the app creates a random 16-character recovery code and saves it only on your
device. A cryptographic key derived from that code lets a server recognise
you from one visit to the next, so your weekly ranking and online highscores
stay yours even if someone else uses the same nickname. Servers receive only the key's public
half and a one-off signature, never the code itself, and each server works
out the same short account ID from the public key (the first four
characters are shown after your nickname in rankings, like `bob#7f3a`). The
account is not linked to your name, email, device ID, or ad ID, and there is
no central account database. You can see your code under **Account code**
on the LAN GAME and NET GAME screens, in the 1-player menu, and on the High
Scores screen's online tabs; copy it, enter it on another device to move your
account there, start a new account, or delete the account.

**Network connection data.** Playing network multiplayer means connecting
to a game server over TCP — your IP address is visible to that server the
same way it is for any internet connection, and the reference server
implementation ([`server/`](https://github.com/dchau360/frozen-bubble-sdl3/tree/main/server)) writes IP address, nickname, and
connect/disconnect times to a local server log for operational purposes
(abuse investigation, debugging). Per-nickname match statistics (wins,
bubbles fired/popped) are also persisted server-side so they can be shown
across sessions. **Anyone can run a Frozen Bubble server** — the app ships
with no server of its own and fetches a community-maintained list of public
servers on startup (see the
[frozen-bubble-servers](https://github.com/dchau360/frozen-bubble-servers)
repo). The developer only controls the logging behavior of the reference
server code; a third-party server operator's own practices are outside this
policy's scope.

**Approximate location.** On startup the app asks a third-party IP-geolocation
service (ipinfo.io, falling back to ip-api.com) for a rough position derived
from your IP address — not GPS, and not anything requiring a location
permission. The result is cached to one decimal place, roughly city or
region accuracy rather than a precise address, and is shown as a pin on a
world map in the network lobby so other online players can see approximately
where players and servers are. It is not stored beyond the current session.

**Platform, input device, and approximate country.** Your client
self-reports which operating system it's running on (Windows, macOS, Linux,
Android, iOS, or browser) and, once per round, whether you're playing with a
keyboard, mouse, touchscreen, or gamepad. Both are shown as small badges
next to your name to the other players in that match, and to a server's
Discord relay if one is running (below) — never stored beyond the current
session. Separately, the same third-party lookup behind "Approximate
location" above is also asked for your country only (not the finer
coordinates used for the map). That country code is sent to a server's
Discord relay if one is running, and it is never shown in the lobby or on
its world map. On desktop and Android the app also remembers the last
country it found, so that it can go with your online highscores (below).
None of these three is verified by the server — they are exactly what your
own client claims.

**Discord join alerts (server-operator feature, not controlled by the
developer).** A server's operator can optionally run a relay that posts a
message to a Discord channel of their choosing every time a player connects
to their server. **That message contains your nickname (with your short
account tag, like `bob#7f3a`, if you signed in), the server's name, and, if
your client reported them, your platform badge and your country flag** (both described just above). Your IP address and the more
precise coordinates behind the lobby's world map are never included: both
briefly reach the relay as part of the connect event (the same information
the server already receives under "Network connection data" and
"Approximate location" above), and the relay discards both rather than
forwarding them anywhere. This is opt-in **per server operator**, not per
player — there is no in-app setting to disable it, because the decision
belongs to whichever server you choose to join. Whether a given server does
this, and what channel it posts to, is outside the developer's control; see
[SetupServer.md](https://github.com/dchau360/frozen-bubble-sdl3/blob/main/SetupServer.md)
for the mechanism, and ask a server's operator directly if you want to know
whether they've enabled it. Note that the relay is ordinary source code an
operator runs themselves, so an operator who modifies their copy could post
more than the stock version does — as could any server software that is not
this one. What is described here is what the shipped relay sends.

**Discord round-result alerts (same server-operator feature, not controlled
by the developer).** The same optional relay can also post a message at the
end of every round: which game mode it was played in, who won or that it
ended in a draw, and the nicknames (with account tags, as above) — plus, if
reported, platform badge,
input-device badge, and country flag — of every player who was in that
room. **Nothing beyond nicknames, the game mode, those three badges, and the
server's name is included** — no IP address, no precise location, same as
the join alert above. The winner named is whatever the reporting player's
own client claimed, the same claim already shown on every other player's
screen for that round —
the server does not verify it, with or without this alert enabled. This
posts once per round, not once per full match, and only for players
actually seated in that room; a player who disconnects mid-round is never
named in one of these messages. Depending on the operator's setup, a room's
round-by-round messages may be grouped into one Discord thread rather than
posted as separate top-level messages — an organizational choice about
where in the channel a message appears, not a change to who can see it or
what it contains. As with join alerts, this is opt-in per server operator,
not per player, with no in-app setting to disable it, and what a given
server actually posts is outside the developer's control — see
[SetupServer.md](https://github.com/dchau360/frozen-bubble-sdl3/blob/main/SetupServer.md#round-result-alerts).

**Weekly rankings.** The reference server keeps, per anonymous account ID
(above) together with the nickname you last played under, how many rounds
you won and lost and how many bubbles you popped this week (Monday
00:00 UTC to the next), and ranks players by each. The counts are cleared
every Monday. Every player on that server can see the top 10 in the online
lobby's "Weekly rankings" screen, and the reference server's top 10 is also
public on the web page at [/weekly/](../weekly/). If the server runs the Discord relay
above, your join alert also shows your own weekly counts and ranks, and a
daily message lists the top 5 in each category. **Only nicknames, the short
account tag, and those counts are included** — nothing else about you. Computer-controlled
bots are never counted.

**Online highscores.** When you clear a level in a classic single-player
game started from level 1, the app keeps your best run so far — the furthest
level cleared, how long it took, how many shots you fired, and whether you
played with keyboard/gamepad or mouse/touch (a level that used both counts
as mouse/touch) — and your best score from a
single life,
with the level that life reached. Once you're back in a menu, it sends these
with your nickname to fb.servequake.com, the server run by this port's
developer, signed with your anonymous account. If the app has looked up
your country for online play (see "Platform, input device, and approximate
country" above), it sends that country code too. The app never makes the
lookup just for this, so a player who only plays solo, or plays in a
browser, sends no country. That server keeps each account's bests all-time
and this week, along with the last country it was sent, and lists them
publicly as `nickname#tag` with a country flag or code in the game's High
Scores → POINTS and LEVEL tabs and on the web page at
[/scores/](../scores/). Nothing else is sent: not your levels or how you
played them. This is **on by default**; turn off
**Online highscores** in the 1-player menu and nothing is sent at all (you
can still view the board, which then asks the server without signing in).
Weekly bests are cleared every Monday 00:00 UTC; all-time bests stay until
you delete your account (see "Deleting your account").
{: #world-highscores }

**Opening the community Discord.** The NET GAME server list and the online
lobby each offer a "Join our Discord" row. It is a link and nothing more:
selecting it hands a fixed invite URL to your browser, and no information
about you is sent anywhere by the act of opening it. What happens after
that is between you and Discord, under
[Discord's own privacy policy](https://discord.com/privacy). Nothing is
sent if you never select the row.

**Advertising identifiers (Android only).** The Android build shows an
interstitial ad via Google AdMob when entering the multiplayer lobby.
AdMob's SDK collects device and advertising identifiers under Google's own
policies — see
[Google's Privacy Policy](https://policies.google.com/privacy) and
[How Google uses information from sites or apps that use our services](https://policies.google.com/technologies/partner-sites).
The developer does not separately collect or receive this data.

**Purchase data (Android only).** The two ad-removal purchases — a yearly
subscription and a one-time permanent unlock — are processed entirely by
Google Play Billing. The app never sees your payment
method, card number, or billing address — it only receives a purchase
token from Google confirming entitlement, which is stored locally on your
device to keep ads off.

**Crash or diagnostic data.** The app does not integrate any crash-reporting
or analytics SDK, so none is collected by the developer.

## How information is used

- Nickname and network data: to run the multiplayer match you're playing.
- Platform and input-device badges: shown to other players in your current
  match, and included in a server's Discord alerts if it runs one (see
  above).
- Nickname: also used, at a server operator's discretion, to post a Discord
  alert when you connect to that server, and again — alongside the game
  mode, win/draw outcome, and the badges above — at the end of each round
  you play (see above). Your IP and your precise coordinates both briefly
  reach the same relay for the connect alert; neither is part of either
  posted message. Your country code reaches the relay too, and is part of
  both messages.
- Account public key: to recognise the same player across visits, so that
  weekly rankings belong to an account rather than to whoever uses a name.
- Nickname and round results: counted into the server's weekly rankings,
  shown in the lobby and, if the server runs the relay, on Discord (see
  above).
- Nickname, best single-player run and best single-life score: listed on
  the online highscore boards,
  in the game and on the web (see "Online highscores"). Not posted to
  Discord.
- Advertising identifiers: handled entirely within Google's AdMob SDK to
  select and measure ads; not accessed by the developer directly.
- Purchase token: to keep the "ads removed" state accurate on your device.

## Third-party services

- [Google AdMob](https://policies.google.com/privacy) — ads (Android)
- [Google Play Billing](https://policies.google.com/privacy) — in-app
  purchases (Android)
- [ipinfo.io](https://ipinfo.io/privacy-policy) / [ip-api.com](https://ip-api.com/docs/legal) —
  approximate location and country from your IP address, for the network
  lobby's world map and, for the country only, a server's Discord alerts
  and the online highscores
- [Discord](https://discord.com/privacy) — some servers relay an alert when
  you connect, and another at the end of each round (nickname(s), server
  name, game mode/outcome, and platform/input/country badges only), to a
  channel the server's operator chooses; not run or controlled by the
  developer. Discord's policy also applies if you follow the game's "Join
  our Discord" link.

## Deleting your account {#delete-account}

Your account is the recovery code on your device (see "Anonymous player
account" above). Servers keep nothing about an account except its weekly
ranking line (the short account tag, the nickname last played under, and
that week's counts) and, on fb.servequake.com, its online highscore line (the
same tag and nickname, the country last sent, and the account's best run
all-time and this week).

To delete your account, open **Account code** (in the 1-player menu, on the
High Scores screen's online tabs, or on the LAN GAME / NET GAME screens) and
choose **Delete account**. The game signs in to fb.servequake.com as that
account one last time and asks it to delete the account's online highscore
line and weekly ranking line there, then erases the code from your device
and gives it a new one, so nobody can sign in as the old account again. If
the server can't be reached, nothing is changed and you can try again.

**New account** on the same screen, clearing the app's data, or
uninstalling the app only erases the code from the device. The server's
lines then stay until they age out: the weekly line and the week's best run
at the next Monday 00:00 UTC reset, the all-time best not at all. To have
those removed without the code, open an issue on the tracker under Contact
below with your account tag (the `#xxxx` after your name on the board).

Other servers you played on keep at most a weekly ranking line for the
account, deleted at their next Monday 00:00 UTC reset.

## Data retention

- Nickname, settings and your account's recovery code live only in local
  app storage until you clear app data or uninstall. Losing the code loses
  the account; there is no way to recover it from a server.
- Online highscores on fb.servequake.com: the week's bests until the
  Monday reset, the all-time bests until you delete your account (see
  "Deleting your account").
- Server-side connection logs, match statistics, and any Discord channel a
  server's join alerts are posted to are retained at the discretion of
  whoever operates that particular server.

## Children's privacy

The app is not directed at children under 13 and does not knowingly collect
personal information from them. Nicknames are free-text and players should
avoid entering real names or other identifying information.

## Your choices

- Turn off ads for a year, or permanently, with an in-app purchase. The
  yearly one is an auto-renewing subscription you can cancel any time in the
  Play Store.
- Play local single-player or local multiplayer to avoid multiplayer
  network traffic entirely — this also means no location lookup happens,
  since it only runs before network play. With **Online highscores** turned
  off in the 1-player menu, single-player sends nothing at all.
- Uninstalling the app removes all locally stored settings, nicknames and
  your account's recovery code.
- Start a new account, move yours to another device, or delete it, from
  **Account code** (see "Deleting your account" above).

## Changes to this policy

This policy may be updated as the app changes (for example, if a new
platform or feature is added). Material changes will be noted in
[CHANGELOG.md](https://github.com/dchau360/frozen-bubble-sdl3/blob/main/CHANGELOG.md).

## Contact

The Android build on Google Play is published by **Solina AI LLC**, a Texas
limited liability company, which is the data controller for the purposes of
this policy. The game itself remains an independent open-source project:
the code is GPLv2, the original game's artwork and music belong to the
Frozen-Bubble Team, and the company claims no ownership over either.

Questions, bug reports, and privacy or data-removal requests all go to the
public issue tracker, which the maintainer reads:
[github.com/dchau360/frozen-bubble-sdl3/issues](https://github.com/dchau360/frozen-bubble-sdl3/issues).
There is no support email address; using the tracker keeps answers visible
to the next player with the same question.
