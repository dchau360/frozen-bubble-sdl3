# Privacy Policy — Frozen Bubble: SDL3

**Effective date:** August 16, 2026

Frozen Bubble: SDL3 ("the app") is a free, open-source game
([GPLv2 licensed](../LICENSE), source at
[github.com/dchau360/frozen-bubble-sdl3](https://github.com/dchau360/frozen-bubble-sdl3)).
This policy covers the Android build distributed on Google Play; the same
practices apply to the other platform builds except where noted (ads and
in-app purchases are Android-only).

The app has no user accounts, no analytics SDK, and does not collect your
name, email, or any other real-world identity. What it does handle is
described below.

## Information collected

**Nickname.** You choose a display nickname in Settings. It's stored only
on your device and sent to whichever multiplayer server you connect to, so
other players in your game can see it. It is not tied to any account and
isn't sent anywhere else.

**Network connection data.** Playing network multiplayer means connecting
to a game server over TCP — your IP address is visible to that server the
same way it is for any internet connection, and the reference server
implementation ([`server/`](../server)) writes IP address, nickname, and
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

**"Follow a server" push token (opt-in, Android/iOS only).** If you tap the
star on a server to follow it, the app registers a Firebase Cloud Messaging
(Android) or APNs (iOS) device token with **that specific server**, so its
operator's optional push relay can notify you when someone joins. The token
is sent only to servers you explicitly follow, is stored alongside your
platform (`ios`/`android`) and a last-notified timestamp for cooldown
purposes, and is removed when you unfollow. See
[docs/PUSH_SETUP.md](PUSH_SETUP.md) for the full mechanism.

**Advertising identifiers (Android only).** The Android build shows an
interstitial ad via Google AdMob when entering the multiplayer lobby.
AdMob's SDK collects device and advertising identifiers under Google's own
policies — see
[Google's Privacy Policy](https://policies.google.com/privacy) and
[How Google uses information from sites or apps that use our services](https://policies.google.com/technologies/partner-sites).
The developer does not separately collect or receive this data.

**Purchase data (Android only).** A one-time "Remove Ads" purchase is
processed entirely by Google Play Billing. The app never sees your payment
method, card number, or billing address — it only receives a purchase
token from Google confirming entitlement, which is stored locally on your
device to keep ads off.

**Crash or diagnostic data.** The app does not integrate any crash-reporting
or analytics SDK, so none is collected by the developer.

## How information is used

- Nickname and network data: to run the multiplayer match you're playing.
- Push token: solely to deliver the join notification for servers you
  followed; never used for advertising or analytics.
- Advertising identifiers: handled entirely within Google's AdMob SDK to
  select and measure ads; not accessed by the developer directly.
- Purchase token: to keep the "ads removed" state accurate on your device.

## Third-party services

- [Google AdMob](https://policies.google.com/privacy) — ads (Android)
- [Google Play Billing](https://policies.google.com/privacy) — in-app
  purchases (Android)
- [Firebase Cloud Messaging](https://firebase.google.com/support/privacy) —
  push delivery for followed servers (Android)
- Apple Push Notification service — push delivery for followed servers
  (iOS)

## Data retention

- Nickname and settings live only in local app storage until you clear app
  data or uninstall.
- Followed-server push tokens are removed from a server's registry when you
  unfollow, or can be requested removed by contacting that server's
  operator.
- Server-side connection logs and match statistics are retained at the
  discretion of whoever operates that particular server.

## Children's privacy

The app is not directed at children under 13 and does not knowingly collect
personal information from them. Nicknames are free-text and players should
avoid entering real names or other identifying information.

## Your choices

- Turn off ads permanently with the one-time "Remove Ads" purchase.
- Unfollow any server to stop push registration for it.
- Play local single-player or local multiplayer to avoid any network data
  transmission entirely.
- Uninstalling the app removes all locally stored settings and nicknames.

## Changes to this policy

This policy may be updated as the app changes (for example, if a new
platform or feature is added). Material changes will be noted in
[CHANGELOG.md](../CHANGELOG.md).

## Contact

This is an independent open-source project with no company behind it.
Questions or data-removal requests: open an issue at
[github.com/dchau360/frozen-bubble-sdl3/issues](https://github.com/dchau360/frozen-bubble-sdl3/issues).
