# Google Play listing — draft content

Everything here is text/config you paste into Play Console yourself — account
creation, Appodeal signup, and the console forms are all things only you can do.
This is the drafted content so you're not starting from a blank form.

---

## Store listing text

**App name:** Frozen Bubble: Fan Edition (26 of 30 chars)

Renamed 2026-10-01 from "Frozen Bubble 2", which is the original game's own
title. A different developer publishing under exactly that name looks like
impersonation to Google's policy checks (a suspect while AdMob's review was
stuck), and the description's "fan-made, not affiliated" line only helps a
reviewer who reads that far. The phone's home-screen label stays the short
"Frozen Bubble" (`android/app/src/main/res/values/strings.xml`), which fits
under a launcher icon.

**Promo text** (Play's separate promotional-text field, 80 chars max — this
one is 78):

```
Aim, fire, pop -- 4 multiplayer modes, teams, and online play up to 20 strong.
```

**Short description** (max 80 chars — this one is 78):

```
4 multiplayer modes, team play, and online rooms up to 20. Free & open source.
```

**Full description** (max 4000 chars — this one is 2855, updated to lead with
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
Lose a level and choose to continue or start over. A shot counter tracks
every bubble you fire, and your best runs -- most points in one life,
furthest level, fastest finish -- can go on an online highscore board
shared with every player, on the web too.

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
this one is 406; covers v2.4.128 and what's new since the listing's
last notes, which dated from v2.4.82):

```
New in 1-player: a shot counter under your score, kept with every high
score, and a Shots column on the online Level board. High Scores now shows
Points before Level. Plus: Continue or start over after losing a level, an
online highscore board for classic runs (also at fb.servequake.com/scores),
an account code to carry your scores between devices or delete them, and
weekly rankings in the online lobby.
```

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
answer key. Last checked against the submitted form's CSV export on
2026-10-06 (the Appodeal build). Play Console's Data safety page can export
and import the form as CSV, which is quicker than clicking through it: export,
change the `Response value` column, import, and review the summary.

| Play category | Collected? | Shared? | Ephemeral? | Required? | Purpose | Notes |
|---|---|---|---|---|---|---|
| **Location** | **Yes — approximate location** | **Yes, with other users** | **No** | Required | Collected and shared: App functionality (shown on a world map in the lobby), Advertising or marketing, Analytics, Fraud prevention/security/compliance | Derived from IP address via ipinfo.io/ip-api.com, not GPS or a device location permission. Cached to one decimal place (~city/region accuracy). See the content-rating section above for detail. Not ephemeral: the lobby map's coordinates are held only while connected, but the country code from the same lookup is posted to a server's Discord, which keeps it. The ad purposes are the ad networks': Unity Ads, Liftoff/Vungle and DT Exchange all derive approximate location from the IP address |
| **Personal info** — name, email, address, phone | No | — | — | — | — | Nickname is free-text, unverified, not linked to real identity — Google's own guidance treats this as not requiring declaration here |
| **Personal info** — User IDs | **Yes** (when you play online) | **Yes — with other players, the server's operator, and its Discord channel** (the short `#xxxx` tag beside your name) | **No** | Required | Collected: App functionality, Account management. Shared: App functionality | The anonymous account (v2.4.118+): servers receive a public key derived from the on-device recovery code and store a 16-hex account id with that week's counts; the first 4 hex digits appear as `name#xxxx` in rankings and Discord posts. Not linked to name, email, device ID or ad ID. Declared under User IDs rather than Device IDs because the same code works on every device the player enters it on |
| **Financial info** — payment info, credit score | No | — | — | — | — | Google Play Billing handles the purchase; the app never receives payment details, only a purchase token |
| **Financial info** — purchase history | **Yes** (Android, via Unity Ads) | Yes — with the ad networks | **No** | Required | Collected and shared: Advertising or marketing, Analytics | Unity Ads declares it. The game itself never passes purchases to Appodeal (`trackInAppPurchase` is not called) |
| **Health & fitness** | No | — | — | — | — |  |
| **Messages** (in-app messaging) | Yes | Yes — with other players in your match, and the server you're connected to | Yes | Required | App functionality | Only while playing network multiplayer; not stored by the developer |
| **Photos/videos/audio/files** | No | — | — | — | — |  |
| **Calendar / Contacts** | No | — | — | — | — |  |
| **App activity** — Other actions (gameplay) | **Yes** (online highscores; missing from the live form until 2026-10-06) | **Yes — with other users** (listed publicly on the online board, in-game and on the web) | **No** | **Optional** (the "Online highscores" toggle in the 1-player menu, on by default) | App functionality | The best classic single-player run (furthest level, time, input type) and best single-life score (points, level reached), sent with the nickname to fb.servequake.com under the anonymous account. Weekly best cleared Mondays; all-time kept until removal is requested. No analytics SDK; nothing else about gameplay is reported |
| **App activity** — app interactions | **Yes** (Android, via Appodeal and its ad networks) | Yes — with Appodeal and the ad networks | **No** | Required | Collected and shared: Advertising or marketing, Analytics, Fraud prevention/security/compliance | Ad impressions and clicks (Appodeal, Unity Ads, DT Exchange). The game itself still has no analytics SDK |
| **App activity** — in-app search history, installed apps, etc. | No | — | — | — | — |  |
| **Web browsing** | No | — | — | — | — |  |
| **App info & performance** — diagnostics, other performance data | **Yes** (Android, via Appodeal and its ad networks) | Yes — with Appodeal and the ad networks | **No** | Required | Collected and shared: App functionality, Analytics, Advertising or marketing | Device model, memory, storage and user agent (Appodeal); ad response times and display status (DT Exchange) |
| **App info & performance** — crash logs | **Yes** (Android, via DT Exchange) | No | **No** | Required | Collected: App functionality, Analytics | DT Exchange collects crashes in its own SDK. The game has no crash-reporting SDK of its own |
| **Device or other IDs** — advertising ID | Yes (Android, via Appodeal and its ad networks) | Yes — with Appodeal and the ad networks | **No** | Required | Collected and shared: App functionality, Advertising or marketing, Analytics, Fraud prevention/security/compliance | Advertising ID, IP address, MCC-MNC and network type (Appodeal); App functionality is Unity Ads' and DT Exchange's frequency capping and user counts. Not collected directly by the developer. Required, not optional: "Remove Ads" is a purchase, not a data-collection toggle |

The ad rows above are the union of
[Appodeal's Data safety page](https://docs.appodeal.com/android/data-protection/app-privacy-details)
(the Appodeal SDK only) and the pages the networks in
`android/app/build.gradle` publish:
[Unity Ads](https://docs.unity.com/en-us/grow/ads/privacy/google-data-safety),
[DT Exchange](https://docs.digitalturbine.com/dt-exchange/getting-started-with-dt-exchange/privacy/google-data-safety-form),
[BidMachine](https://developers.bidmachine.io/sdk/general/android/app-privacy-details-on-the-google-play)
and [Liftoff/Vungle](https://support.vungle.com/hc/en-us/articles/5035287250331-Google-Play-data-safety-Questionnaire-for-Vungle).
AppLovin, Mintegral and InMobi publish none that could be found (2026-10-06).
Appodeal's "optional" lines (precise location, user IDs, purchase history)
apply only when the app grants a location permission, calls `setUserId` or
calls `trackInAppPurchase`; this one does none of those. Adding or removing an
adapter means checking that network's page against these rows again.

**Advertising ID** (App content, a separate form): **Yes**, the app uses it,
for Advertising or marketing, Analytics and Fraud prevention/security/
compliance. The release build's merged manifest carries
`com.google.android.gms.permission.AD_ID` from the ad SDKs, and Play rejects
a release whose manifest and this answer disagree.

**Data deletion:** The anonymous account counts as an account for Play's
deletion policy (it follows the player across devices):

- **Account creation method:** Other ("An anonymous account is created
  automatically on first online play...").
- **In app:** NET GAME (or LAN GAME) → **Account code** → **Delete account**
  (v2.4.126+). It removes the account's highscore and weekly lines from
  this port's server, then starts a new account on the device. **New
  account** alone only replaces the code on the device.
- **Delete account URL:** `https://llmfinder.net/privacy/#delete-account`,
  the same site as the listing's privacy policy URL.
- **"Request that their data is deleted" (without deleting the account):**
  the live form answers **No**. Yes would also be defensible (the
  highscores can be removed on request through the issue tracker), with the
  same URL as the delete data URL.
- **What's deleted / kept:** other servers' weekly lines age out at their
  Monday 00:00 UTC reset. Discord messages already posted in a server's
  channel stay there. The ad networks hold their own data under their own
  policies; the advertising ID can be reset from Android's settings.

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
- **First-party SDK traffic (Appodeal, Play Billing, Firebase, APNs) —
  encrypted.** All HTTPS/TLS by platform requirement, not something the
  app controls either way.
- **Geolocation lookup — mostly encrypted, one gap.** `ipinfo.io/loc` is
  HTTPS; the fallback, `ip-api.com`, is plain HTTP.

Nothing sensitive (no passwords, no payment data) is in the unencrypted
paths, but "no" is still the accurate answer to a blanket yes/no question.

---

## Ads: Appodeal (replaced AdMob, 2026-10-06)

AdMob turned the account down with no policy named (no Policy center entry,
no email), so ads now go through Appodeal's mediation instead. The AdMob
interstitial unit (`ca-app-pub-7736855769799322/5410693019`) is no longer
used. The AdMob App ID (`ca-app-pub-7736855769799322~9200045587`) is still in
`AndroidManifest.xml`: Appodeal's consent goes through Google's UMP SDK, which
reads it, and the consent messages themselves are published in AdMob's
Privacy & messaging.

To switch it on:

1. Sign up at appodeal.com and add the app (Android, package
   `org.frozenbubble`, linked to the Play listing).
2. Paste the app key from the app's page into `appodealAppKeyDefault` in
   `android/app/build.gradle`. It is not a secret: it ships inside every APK.
   Until it is set, the app makes no ad requests at all. (Done.)
3. Replace `site/app-ads.txt` with the app-ads.txt list from Appodeal's
   dashboard, and the copy in the `dchau360.github.io` repo too, keeping the
   `google.com, pub-7736855769799322` line Appodeal's list leaves as a
   placeholder. (Done.)
4. Update the Data safety form (rows above) and the Advertising ID
   declaration, and publish the updated privacy policy, before the build
   that carries Appodeal goes live. Set the privacy policy URL
   (`https://llmfinder.net/privacy/`) in Appodeal's dashboard too.
5. Set up payouts in Appodeal (the LLC's payment details and tax form, as
   for AdMob).

Debug builds always run Appodeal in test mode (`Appodeal.setTesting`), so
no device needs registering as a test device, and the old
`admob.testDeviceId` line in `android/local.properties` does nothing now.
Google's UMP consent form shows where the law asks for one (GDPR/UK and US
state privacy laws), the first time an ad would load, and the **Ad privacy
choices** row in Controls & Settings reopens it.

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
