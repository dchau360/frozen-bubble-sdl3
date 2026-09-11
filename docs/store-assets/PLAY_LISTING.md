# Google Play listing — draft content

Everything here is text/config you paste into Play Console yourself — account
creation, AdMob signup, and the console forms are all things only you can do.
This is the drafted content so you're not starting from a blank form.

---

## Store listing text

**App name:** Frozen Bubble

**Promo text** (Play's separate promotional-text field, 80 chars max — this
one is 78):

```
Aim, fire, pop -- 4 multiplayer modes, teams, and online play up to 20 strong.
```

**Short description** (max 80 chars — this one is 78):

```
4 multiplayer modes, team play, and online rooms up to 20. Free & open source.
```

**Full description** (max 4000 chars — this one is 2600, updated to lead with
the multiplayer modes, teams, and online play, per the game's v2.4.95 Race
and Timed mode addition, plus a mention of Discord join/result alerts):

```
Frozen Bubble is a free, open-source arcade puzzle game: aim, fire, and pop
chains of colored bubbles before they reach the bottom. A faithful port of
the beloved 2002 classic, rebuilt from scratch for modern devices -- with
four multiplayer modes and full team play, local or online.

FOUR WAYS TO PLAY MULTIPLAYER
- Classic -- last player or team standing wins.
- Clear Mode -- first to clear your entire board wins the round.
- Race -- first to pop a target number of bubbles (50 by default) wins.
- Timed -- most bubbles popped when the clock runs out (30 seconds by
  default) wins.
Every mode plays the same whether you're local or online, and the host can
adjust each mode's numbers to fit the room.

TEAMS
Join a team, or stay a free agent, from any room's Set Teams screen -- teams
work in all four modes above, not just one. Teammates share the win when
one of you takes the round, and in Timed mode your team's pops even pool
together, so a coordinated pair can out-pop a solo player. Hosts get
one-tap Auto-balance buttons to split the room into 2, 3, 4, or 5 teams
instantly.

ONLINE MULTIPLAYER
Play with friends or strangers over the internet, 2-20 players per room, on
the included dedicated server. Rooms bigger than 5 players get a
battle-royale layout showing your most relevant opponents, quick-target
hotkeys, and a spectate mode once you're out. The host controls chain
reactions, attack bubbles, per-player aim assist, mouse/touch aim, and
more -- every joined player sees the rules update live. Follow a quiet
server and get notified the moment someone joins it, or join our Discord
for the same join alerts (plus round results) without the app even open.

LOCAL MULTIPLAYER
2-5 players, one device -- keyboard, controller, or a mix. Empty seats can
be filled with bots at three skill levels, so a bigger match doesn't need a
full house of controllers. (3-5 player local games are still experimental
and less tested than 2-player.)

SINGLE PLAYER
100 levels of classic bubble-popping, with scoring and chain reactions.

CONTROLLER SUPPORT
Full gamepad support with per-player rebindable controls, built for both
handheld play and the living-room TV.

OPEN SOURCE
Frozen Bubble's source is entirely public and GPL-licensed. No account
required, no tracking SDK, no data sold. See exactly what the app does at
github.com/dchau360/frozen-bubble-sdl3.

This is an independent, fan-made continuation of the original Frozen Bubble
project and is not affiliated with or endorsed by the original authors.

Contains ads. Remove them for a year, or permanently, with an in-app
purchase.
```

**Release notes / "What's new"** (Play's per-release field, 500 chars max —
this one is 447; covers `v2.4.75`-`v2.4.82`, the changes since this listing
was last drafted):

```
Teams are now a per-player setting in every mode, not a separate mode of
their own -- join a team (or stay free-for-all) from the new Set Teams
screen, with one-tap Auto-balance for the host. Fixed illegible text on
the Set Teams screen, and the room's HELP guide being unreachable with
Up/Down on a keyboard. Faster rendering and lighter logging during
multiplayer, plus highscore-screen alignment fixes.
```

(v2.4.82 itself is a CI-only fix with no player-facing change from v2.4.81
-- nothing to call out separately.)

**Category:** Games > Puzzle
**Contact email:** (your email — this is shown publicly on the listing)
**External marketing / website:** `https://github.com/dchau360/frozen-bubble-sdl3`
**Privacy policy URL:** `https://dchau360.github.io/frozen-bubble-sdl3/privacy/`
**Website URL:** `https://dchau360.github.io/frozen-bubble-sdl3/`

---

## Content rating questionnaire (IARC)

Play's rating form is a short questionnaire, not free text — here's what the
honest answers are, based on what the app actually does:

| Question area | Answer | Why |
|---|---|---|
| Violence | None / very mild | Popping bubbles, no blood, gore, or realistic violence |
| Sexual content | None | — |
| Profanity (in fixed app content) | None | The app itself contains no profanity |
| Controlled substances | None | — |
| Gambling | No | No real-money wagering or loot-box mechanics |
| **User interaction / communication** | **Yes — unmoderated** | Network multiplayer has free-text chat between players, with **no profanity filter or moderation** in this codebase. Answer honestly here — this is the one question worth not glossing over, since IARC/Play specifically ask about *unmoderated* chat and it nudges the rating up a notch (typically still in the Teen range at most, not Mature) |
| Shares location | **Yes — approximate, not precise** | See below |
| Shares personal info with other users | Only your chosen nickname (not tied to real identity) | — |
| Users can spend real money | Yes | Two ad-removal purchases via Google Play Billing: a yearly subscription and a one-time permanent unlock |

On location specifically: the game calls an IP-geolocation service
(`ipinfo.io`/`ip-api.com`) at startup to get a rough lat/lon from your IP
address — this is **not** GPS or any device location permission, and is
cached to one decimal place (roughly city/region accuracy, not street-level
or precise). It's shown as a pin on a world map in the network lobby, so
other players can see approximately where players and servers are. Answer
"approximate location" (not "precise location") on the Data Safety form's
location question, and "yes" to sharing it with other users — the world map
is exactly that.

Expect this to land around **Teen** (or your platform's equivalent) purely
because of the unmoderated chat question — not because of any actual violent
or mature content. That's normal for any game with open text chat (the same
reason most chat-enabled games rate Teen rather than Everyone).

---

## Data Safety form

This maps directly from [PRIVACY_POLICY.md](../PRIVACY_POLICY.md). Play's
categories change their exact wording occasionally, so treat this as a
strong draft to check against the live form rather than a copy-paste-blind
answer key.

| Play category | Collected? | Shared? | Purpose | Notes |
|---|---|---|---|---|
| **Location** | **Yes — approximate location** | **Yes, with other users** | App functionality (shown on a world map in the lobby) | Derived from IP address via ipinfo.io/ip-api.com, not GPS or a device location permission. Cached to one decimal place (~city/region accuracy). See the content-rating section above for detail |
| **Personal info** (name, email, address, phone, User IDs) | No | — | — | Nickname is free-text, unverified, not linked to real identity — Google's own guidance treats this as not requiring declaration here |
| **Financial info** | No (by the app) | — | — | Google Play Billing handles the purchase; the app never receives payment details, only a purchase token |
| **Health & fitness** | No | — | — | |
| **Messages** (in-app messaging) | Yes | Yes — with other players in your match, and the server you're connected to | App functionality | Only while playing network multiplayer; not stored by the developer |
| **Photos/videos/audio/files** | No | — | — | |
| **Calendar / Contacts** | No | — | — | |
| **App activity** (app interactions, in-app search history, etc.) | No | — | — | No analytics SDK; gameplay isn't reported anywhere |
| **Web browsing** | No | — | — | |
| **App info & performance** (crash logs, diagnostics) | No | — | — | No crash-reporting SDK |
| **Device or other IDs** — advertising ID | Yes (Android, via AdMob SDK) | Yes — with Google/AdMob | Advertising or marketing | Not collected directly by the developer; handled inside Google's SDK. Optional in the sense that "Remove Ads" stops ads (but the identifier collection is AdMob SDK behavior, not something toggled off by that purchase) |
| **Device or other IDs** — push token | Yes (opt-in, when you follow a server) | Yes — with that specific server's operator only | App functionality | Only sent to servers you explicitly follow; removed on unfollow |

**Data deletion:** No account exists to delete. Uninstalling removes all
local data. For a followed server's push registration, unfollowing removes
it, or contact that server's operator (this is disclosed in the privacy
policy already).

**Encryption in transit:** Not uniformly — answer **No** on "is all user
data encrypted in transit," or use the per-category breakdown if the form
offers one:

- **Native TCP clients (desktop, Android) — not encrypted.** The game
  protocol is a raw socket connect (`networkclient.cpp`); nickname, chat,
  the IP-derived location, and gameplay state all travel in the clear.
  This isn't an operator misconfiguration — the reference `docker-compose.yml`
  deliberately exposes port 1511 for native clients as plain TCP, and only
  port 443 (the browser/WebSocket path) gets TLS via nginx. Anyone can run
  a server, and most third-party ones will have no more encryption than
  the reference setup does.
- **Browser/WASM clients — encrypted**, when connecting through a server
  that terminates TLS on its WebSocket endpoint (the official server does;
  a self-hosted one might not).
- **First-party SDK traffic (AdMob, Play Billing, Firebase, APNs) —
  encrypted.** All HTTPS/TLS by platform requirement, not something the
  app controls either way.
- **Geolocation lookup — mostly encrypted, one gap.** `ipinfo.io/loc` is
  HTTPS; the fallback, `ip-api.com`, is plain HTTP.

Nothing sensitive (no passwords, no payment data) is in the unencrypted
paths, but "no" is still the accurate answer to a blanket yes/no question.

---

## AdMob — done

Account created 2026-08-24, real IDs wired in (commit `710cd088`):

| | |
|---|---|
| App ID | `ca-app-pub-7736855769799322~9200045587` (in `AndroidManifest.xml`) |
| Interstitial ad unit | `ca-app-pub-7736855769799322/5410693019` (in `AdsManager.java`) |

**The account is still pending Google's approval** — a new-account review,
separate from Play Console verification. Until it clears, ad requests fail
with `Account not approved yet`, which is expected and not a bug.

The owner's test tablet is registered as an AdMob test device via
`admob.testDeviceId` in `android/local.properties` (git-ignored), so local
builds always get safe test creatives rather than real impressions — tapping
your own live ads is invalid traffic and can get a new account flagged. To
register another device, run a debug build, watch logcat for the SDK's
"Use RequestConfiguration..." line, and add that hash to that file.

---

## Play Console — the two ad-removal products

Create both under **Monetize** once the account is verified. The product IDs
must match `BillingManager.java` exactly and **cannot be changed after
creation**:

| What | Product ID | Where in Play Console | Price |
|---|---|---|---|
| 1 year, auto-renewing | `remove_ads_year` | Monetize → **Subscriptions** | $5/year |
| Permanent | `remove_ads_forever` | Monetize → Products → **In-app products** | $15 one-time |

**The yearly one is a subscription, not a one-time product** — it belongs
under Subscriptions, and needs a **base plan** with a yearly billing period
(the app reads the first offer on it). A one-time "1 year pass" was considered
and rejected: without a backend, its expiry can only come from the purchase
timestamp, and allowing a second year means consuming the first, which throws
that timestamp away and loses the entitlement on the next reinstall. Play
tracks subscription state itself, so `queryPurchasesAsync(SUBS)` answers
correctly across reinstalls and new devices with no server of ours.

Prices above are what you set in Play Console for your home currency; Play
converts for other countries. The app never hardcodes a price — it shows
Play's own localized string, and `...` until Play answers.

No app code change is needed for either — both IDs are already wired in.

### Testing purchases before launch

Purchases can't be tested until the app is uploaded to a track (internal
testing is enough) and your account is added under **Setup → License
testing**, which makes purchases free and instant for those accounts. Until
then the app logs `Product not available` for both, which is the expected
state and not a bug.
