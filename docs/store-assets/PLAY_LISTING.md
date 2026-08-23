# Google Play listing — draft content

Everything here is text/config you paste into Play Console yourself — account
creation, AdMob signup, and the console forms are all things only you can do.
This is the drafted content so you're not starting from a blank form.

---

## Store listing text

**App name:** Frozen Bubble

**Short description** (max 80 chars — this one is 79):

```
Pop chains of bubbles solo, with friends locally, or online. Free & open source.
```

**Full description** (max 4000 chars):

```
Frozen Bubble is a free, open-source arcade puzzle game: aim, fire, and pop
chains of colored bubbles before they reach the bottom. A faithful port of
the beloved 2002 classic, rebuilt from scratch for modern devices.

SINGLE PLAYER
100 levels of classic bubble-popping, with scoring and chain reactions.

LOCAL MULTIPLAYER
2-4 players, same device — keyboard, controller, or a mix. Perfect for
couch play.

ONLINE MULTIPLAYER
Play with friends or strangers over the internet, 2-20 players per room.
Choose Classic (last player standing), Clear Mode (first to clear their
board wins), or Team Mode. The host controls chain reactions, attack
bubbles, aim assist, and more — everyone sees the rules live.

FOLLOW A SERVER
Mark a quiet server as followed and get a notification when someone joins
it, so you don't have to keep checking back.

CONTROLLER SUPPORT
Full gamepad support with per-player rebindable controls, built for both
handheld play and the living-room TV.

OPEN SOURCE
Frozen Bubble's source is entirely public and GPL-licensed. No account
required, no tracking SDK, no data sold. See exactly what the app does at
github.com/dchau360/frozen-bubble-sdl3.

This is an independent, fan-made continuation of the original Frozen Bubble
project and is not affiliated with or endorsed by the original authors.

Contains ads. A one-time in-app purchase removes them permanently.
```

**Category:** Games > Puzzle
**Contact email:** (your email — this is shown publicly on the listing)
**External marketing / website:** `https://github.com/dchau360/frozen-bubble-sdl3`
**Privacy policy URL:** `https://dchau360.github.io/frozen-bubble-sdl3/`

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
| Shares location | No | — |
| Shares personal info with other users | Only your chosen nickname (not tied to real identity) | — |
| Users can spend real money | Yes | One-time "Remove Ads" purchase via Google Play Billing |

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
| **Location** | No | — | — | |
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

**Encryption in transit:** Network multiplayer traffic (game protocol,
chat) is **plain TCP, not encrypted** — worth answering honestly on
whichever question asks this. Nothing sensitive (no passwords, no payment
data) travels over it, but it isn't TLS.

---

## AdMob — real account setup (manual, your account)

1. Go to <https://apps.admob.com> and sign up (uses your Google account —
   no separate signup, but you're accepting AdMob's own terms).
2. **Apps → Add app** → Android → is it published on Play yet? No (you can
   link it later) → name it "Frozen Bubble".
3. AdMob issues an **App ID** (`ca-app-pub-XXXXXXXXXXXXXXXX~XXXXXXXXXX`).
4. **Ad units → Add ad unit → Interstitial** (matches what `AdsManager.java`
   already implements) → note the **Ad unit ID**
   (`ca-app-pub-XXXXXXXXXXXXXXXX/YYYYYYYYYY`).
5. Send me both IDs and I'll wire them into
   [`AndroidManifest.xml`](../../android/app/src/main/AndroidManifest.xml)
   (replacing the Google test ID currently there) and
   [`AdsManager.java`](../../android/app/src/main/java/org/frozenbubble/AdsManager.java).

Keep testing with the **test ad unit ID** (already in place) until you're
ready to actually publish — serving real ads before the app is live on Play
violates AdMob policy.

---

## Play Console — remove_ads in-app product (manual, once account exists)

1. Play Console → your app → **Monetize → Products → In-app products**.
2. **Create product**. Product ID must be exactly `remove_ads` (matches what
   `BillingManager.java` already queries for) — Play product IDs can't be
   changed after creation, so get this exact.
3. Set a price, title ("Remove Ads"), description ("Removes all ads
   permanently").
4. Activate it. No app code change needed — `BillingManager.java` already
   queries for this exact product ID.

---

## Closed testing (the 12-tester/14-day gate for a Personal account)

### What the track actually needs

A common misconception is that "closed testing" is a stripped-down, informal
mode — it isn't. Play Console requires most of the same setup as a
production release before it will let a closed track go out at all:

- **Content rating questionnaire** — done (above).
- **Data Safety form** — done (above).
- **Ads declaration** — "Yes, my app contains ads" (Play Console asks this
  as its own checkbox separately from the Data Safety form).
- **Target audience & content** — age groups your app targets; pick 13+
  given the unmoderated chat (matches the content rating reasoning above).
- **Store listing** (title, descriptions, icon, screenshots, feature
  graphic) — all of it, not a subset. Testers see the real listing. Already
  drafted above.
- **Countries/regions** — pick where the track is available; "all
  countries" is fine for testing.
- **US export laws / content guidelines declarations** — standard
  checkboxes, no encryption export complications here (see the encryption
  note in the Data Safety section above).

None of this is testing-track-specific busywork — it's the same review
Google runs before *any* release. Filling it in once now means production
is just "promote this release" later, not a second round of the same forms.

### Setting up the track

1. Play Console → your app → **Testing → Closed testing**.
2. **Create track** (or use the default one Play Console offers).
3. Upload the release `.aab` — the CI-built one from
   [Android-AAB in the latest release](https://github.com/dchau360/frozen-bubble-sdl3/releases/latest)
   works directly, no separate testing build needed.
4. **Testers** tab → choose **Email list** (simplest option — no Google
   Group needed for 12 people):
   - Paste tester emails, one per line or comma-separated. Each must be a
     real Google account (Gmail or Workspace) — Play Console doesn't
     validate this until the person tries to opt in.
5. Save. Play Console generates an **opt-in URL** for this track — that's
   what you send testers, not a direct Play Store link. They:
   - Open the URL while signed into the Google account you listed.
   - Click **Become a tester**.
   - Follow the link to the Play Store listing and install from there.
6. **The 14-day clock starts when a tester opts in**, not when you invite
   them — so stagger reminders if people are slow to click through, since a
   tester who opts in on day 5 doesn't help you reach day 14 until their
   own day 14.
7. Once you have 12 testers opted in continuously for 14 days, Play Console
   unlocks the option to apply for production access on that app.

Being opted in is the literal requirement, but it's worth asking testers to
actually **install and open the app at least once** — real usage is what
surfaces bugs, and an opted-in-but-never-installed tester list looks odd if
Google ever reviews it manually.

### Where to find 12 testers

You don't need 12 strangers — friends and family with any Google account
qualify. If you want more (or don't want to ask people you know to install
a beta), some places that work well for a small open-source game:

- **Your own itch.io page's comment section** (`dchau360.itch.io/frozenbubble2`)
  — people already playing the browser build are the most likely to bother
  installing an Android beta too.
- **The GitHub repo itself** — a pinned issue or a note in the README asking
  for closed-testing volunteers; open-source-curious people are usually
  happy to help for a small, transparent project.
- **r/AndroidApps** and **r/AppHookup** on Reddit — both regularly have
  beta-tester-wanted threads; check each sub's rules on self-promotion
  first.
- **r/AndroidGaming** — same idea, gaming-specific audience.
- A **Discord server** for retro/arcade games or indie/open-source dev, if
  you're in one — a quick "testing a Frozen Bubble Android port, need 12
  people for 14 days" post usually gets more than enough volunteers.

12 is a small number — realistically one or two of the above sources is
enough; no need to hit all of them.
