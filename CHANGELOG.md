# Changelog

## v2.4.67

- **A fresh install now opens on the Slate menu style, not Classic.**
  MENU STYLE still remembers whatever you pick after that, same as
  before -- this only changes the very first launch's starting point, and
  what an unreadable or corrupted settings file falls back to.

## v2.4.66

- **The CHANGE KEYS row now reads SETTINGS in the four new menu styles.**
  The panel it opens calls itself CONTROLS & SETTINGS and covers game
  speed, sound, mouse/touch aim and fullscreen as well as key bindings, so
  the old name undersold it. Classic keeps CHANGE KEYS -- that label is
  painted into the original artwork.

## v2.4.65

- **New: MENU STYLE, an eighth row on the title screen, restyles the menu.**
  Press it to cycle five looks -- Classic (the original artwork, untouched),
  Clear (the same plates with clean type instead of the hand-lettered
  labels), Slate, Ice and Pop. The original labels were bitmaps baked at
  640x480 and had gone soft on modern displays; the four new styles draw
  their type at runtime from Fredoka and Baloo 2 instead, so it stays sharp
  at any window size. Your choice is saved and comes back next launch.

## v2.4.64

- **New: hosting a network game now remembers your room settings.** Chain
  reactions, single-player targetting, victories limit, clear mode, attack
  mode, team mode/count, bot skill, and room size all carry over to the
  next room this device creates -- including after quitting and
  relaunching the app -- instead of resetting to the game's hardcoded
  defaults every time you host.

- **Fix: typing a space did nothing in chat or nickname fields on the
  browser build.** Confirmed on desktop itch.io -- every other character
  worked, but Space was silently dropped in in-game chat, lobby chat, and
  while entering a username or nickname. Emscripten's SDL3 port never
  delivers a space through the normal text-input event on this platform;
  Space is now appended directly on keydown as a targeted workaround,
  browser build only.

## v2.4.63

- **New: "Attack bubbles" is now three-way -- ON, OFF, or Blockable.** Blockable
  makes the attack you just earned pay down whatever is still queued
  against you, sending only the surplus onward. It cannot go negative:
  blocking more than you owe empties your queue and sends nothing rather
  than banking credit for the next wave, and only bubbles still queued can
  be blocked -- once one has started falling it is committed and will land.
  A "Blocked -N" toast shows when it happens. Available in both the online
  game room and local multiplayer.

  On the wire, `DISABLEMALUS` keeps its original 0/1 meaning and the third
  state rides alongside it in a new `MALUSCANCEL` field, so a client built
  before this still reads on-vs-off correctly; it simply does not cancel.

- **New: a HELP box opens a full settings guide, on both the online game
  room and the local multiplayer panel.** It sits at the right-hand end of
  the Bot skill row on each, and F1 opens it from either screen. In a room
  it is there for joiners as well as the host -- a joiner cannot change the
  settings but still has to play by them.

  Each screen gets its own page rather than one page with parts hidden,
  because the rule that matters most differs between them. Online, an
  attack is split evenly among all living opponents -- until six or more
  are alive, at which point it goes to a single opponent instead, since
  splitting would multiply one pop into an attack on up to nineteen boards
  at once. Locally, an attack is not divided at all: every living opponent
  receives the full count, which is the original game's local rule and
  makes a big local match considerably more violent than an online one of
  the same size. One shared page would have stated one of those wrongly.

## v2.4.62

- **Fix: tapping Start more than once in an online lobby could autofire a
  shot into the round.** A local game starts synchronously, too fast for
  an extra tap to matter, but an online game waits on a server round-trip
  between the tap and the round actually starting -- long enough for an
  impatient second tap to still be sitting in the input queue when the
  screen switches to the game, where it was misread as the first shot.
  Queued pointer input is now discarded at that transition, and the Start
  row itself is debounced against repeat taps while a request is already
  in flight.

## v2.4.61

- **Fix: Android phones and tablets were each locked to one screen
  orientation instead of rotating with the device.** A tablet stayed
  landscape and a phone stayed portrait no matter how it was held.
  Android only grants real rotation when the window is both resizable
  and has every allowed orientation named at once; the window was
  never created resizable, so the OS picked one orientation at startup
  and never revisited it. Both device classes now rotate freely, the
  same as iOS already does -- TV boxes are unaffected and stay
  landscape-locked.

## v2.4.60

- **Fix: a player who had already lost a round could keep firing bubbles.**
  Reported live in online multiplayer -- the board was frozen and the
  round had moved on for everyone else, but mouse/touch clicks still
  launched bubbles. Keyboard and gamepad already stopped for a player
  who had lost; mouse/touch aim and fire went through a separate path
  that skipped that check entirely.
- **Fix: a bot rejected when joining a room could sit invisibly stuck.**
  If the server ever turns away a bot's attempt to join a room (the
  room wasn't ready yet, was full, or the name collided), the bot's
  connection stayed open with no indication anything had gone wrong --
  it would show as connected in the general lobby while never actually
  appearing in the room. The client now notices the rejection, drops
  the bot, and tells the host why.

## v2.4.59

- **Fix: the WASM build showed a permanent black screen on some Android
  devices** (confirmed live on a tablet with a fractional device pixel
  ratio, in both Chrome and Brave, via itch.io and locally). The game
  itself ran completely normally underneath -- menus, input, audio,
  networking all worked -- but SDL's browser backend misjudges the
  canvas size on any device whose CSS pixels don't divide evenly into
  device pixels, sizing the window (and canvas) 0x0 so no pixel ever
  reached the screen. Corrected by re-asserting the game's real
  resolution right after the renderer is created.
- **Network-room bots can now mix difficulty levels.** A bot keeps
  whatever Bot skill was set at the moment it was added to the room,
  instead of every bot in the room being retuned to whatever skill is
  current when the round starts. Change the skill setting between adds
  and a single room can run "bot1-high" alongside "bot2-low" at the
  same time.

## v2.4.58

- **Bot difficulty renamed to Low/Med/High, and network-room bots now name
  their own skill.** The Bot skill row (local multiplayer settings and the
  network room alike) showed "easy/normal/hard"; it now reads Low/Med/High.
  A network room's bots used to get a random numeric suffix ("bot1-482");
  they're now named "bot1-low", "bot2-med", "bot1-high", etc., so the
  roster itself tells the other players what they're up against. Also
  fixes a related bug found while making this change: changing Bot skill
  after bots had already joined the lobby left their nicknames naming the
  skill they joined at, even though the game applies whatever skill is
  current at game-start time to every bot uniformly -- a stale name could
  quietly lie about what a bot would actually play at.
- **Credit where it's due:** the bot AI's shot-scoring model was ported
  from [zepr/fbjs](https://github.com/zepr/fbjs)'s `cpu.js` (GPL-3.0);
  this is now noted in `bubbleai.cpp` and in the README's Credits list.
- **Fix: the bots were too hard, even on the lowest setting.** Reported
  live as still strong enough to beat casual players consistently. Low
  now misses about 4 times out of 5 (was half the time), and a miss no
  longer has a decent chance of landing a near-optimal shot anyway --
  it's now restricted to the worse half of the ranked options. Med and
  High are unchanged.

## v2.4.57

- **Fix: the "Set name" row on the LAN/Net server-list screen was untappable
  by mouse or touch.** It was registered as a stepped row (the "<  value  >"
  look), which made every tap on it send LEFT/RIGHT instead of RETURN --
  but that row has no actual left/right-steppable value, and neither
  server-list screen has a LEFT/RIGHT handler at all, so every tap or click
  silently did nothing. A literal keyboard Enter still worked (it reaches
  the activation code directly, bypassing row taps entirely), which is why
  this stayed hidden until reported live on itch.io: "I can highlight Set
  Name but nothing happens" -- reproduced on both iPhone touch and a plain
  desktop mouse click, ruling out anything touch-specific. The row no
  longer claims to be stepped; a tap on it now activates it like any other
  action row.

## v2.4.56

- **Fix: the WASM mouse-echo debounce could lose a race under load.** The
  browser's synthesized `MOUSE_BUTTON_DOWN` echo of a real touch tap was
  only dropped if it arrived within a fixed 200ms of the tap's own
  `FINGER_UP` -- an assumption that held under light load but not always:
  on a busier frame the echo can arrive later, dispatching as its own,
  independent tap that re-activates whatever row the real tap just
  selected. For a row with no dedicated activation key (the network room's
  Bots count, where a second tap just sends RETURN) that means the row
  re-increments itself with no further tap from the player. Reported live
  on itch.io/iPhone as the Bots row occasionally getting stuck this way --
  intermittently, not every time, exactly what a timing race that only
  loses sometimes looks like. Replaced the fixed window with
  `WasmMouseEchoGuard`, which tracks "does the browser still owe this tap
  its echo" as actual state instead of a clock, so an arbitrarily late echo
  is still recognized whenever it shows up.

## v2.4.55

- **Fix: a tap missing every panel row re-activated the selected row instead
  of doing nothing.** `MainMenu::HandlePanelTap` returned `false` on a miss,
  which the caller reads as "this panel doesn't hit-test taps" and answers
  with its tap-anywhere-to-confirm fallback -- injecting RETURN, which
  activates whatever row is currently selected. On a panel that does
  register rows that turned a miss into an unintended re-activation. The
  network game room's Bots/Bot-skill rows are only 18 logical units tall (a
  couple of millimetres on a phone), so with "Bots" selected, nearly every
  tap aimed elsewhere missed, fell through, and cycled the bot count again
  instead of moving the selection -- reported live as being unable to
  navigate out of the bot-count control. The local multiplayer panel's own
  Bots row hit the identical trap, reported separately as that screen
  feeling "stuck." Neither was the swipe-vs-tap classification fixed in
  v2.4.53/v2.4.54 -- both are the same underlying defect: HandlePanelTap now
  consumes a miss (no selection change, no event) once any row was
  registered, and only falls back to tap-anywhere-to-confirm on a screen
  that registers no rows at all.

## v2.4.54

- **Fix: iOS Safari's own edge-swipe-back could beat the game's.** v2.4.53
  fixed a real bug in the swipe-vs-tap classifier, but that only covers
  touches the game's own handlers actually receive. Mobile Safari has a
  system-level "swipe from the left edge to navigate back" gesture that is
  recognized below the DOM's touch dispatch -- it can claim a leftward drag
  before SDL's finger-down/up handlers ever see it, no matter what the
  page's JS does. The game's own "swipe left to go back" competed for that
  same gesture and lost, which read as the swipe doing nothing. The WASM
  shell template now opts out via `touch-action: none` and
  `overscroll-behavior: none`, the documented way to disable it.

## v2.4.53

- **Fix: swipe-back was too easily swallowed on any stepped row.** v2.4.51's
  fix let a deliberate swipe back still fire on a stepped row, but only past
  a 100-logical-unit drift cap meant to leave room for a full edge-to-edge
  swipe. Every counter/value row (Players, Mode, Victories limit, Bots, Bot
  skill, per-player colors, ...) is a stepped row, and a normal deliberate
  swipe-back on a phone commonly travels only 60-90 logical units -- short
  of that cap. It fell through to the tap handler instead, which reads a
  release on the already-selected row as "activate" -- nudging the row's
  value rather than leaving the screen. Reported live as the "Bots" row on
  itch.io/iPhone feeling stuck: every attempt to swipe back out of it just
  changed the bot count again. The cap is now 60, still comfortably past
  ordinary tap jitter but low enough for a real swipe to clear it.

## v2.4.52

- **Fix: the aim guide display was gittery.** `SDL_GetTicks()` has
  whole-millisecond resolution, so a steady 60fps frame's own measured
  "elapsed" alternates between 16ms and 17ms every single frame -- an
  exact, persistent few-percent oscillation in `deltaScale`, not
  incidental noise. The real bubble's own incremental per-frame movement
  never shows this, but the aim guide re-simulates its entire
  multi-hundred-step preview path from scratch every frame using that
  same raw value, so the oscillation accumulated over the path length
  and visibly shimmered the dotted line every frame even while the aim
  was held perfectly still. The guide now uses a smoothed deltaScale
  that damps that alternation while still tracking a real, sustained
  change (an actual frame-rate drop, or the player changing the
  speed-multiplier setting) within a handful of frames.

## v2.4.51

- **Fix: v2.4.50 broke swipe-back on the LAN/Net screens.** v2.4.50 pinned
  the "Set name" row to the very bottom of the LAN/Net server-list panel.
  That row is a stepped row, and the swipe-vs-tap fix from v2.4.49
  unconditionally suppresses "swipe back" on any stepped row -- a rule
  meant to catch an ordinary tap that drifted a little, not a real swipe.
  With "Set name" now always occupying the bottom of the panel, a
  deliberate edge-to-edge swipe back on iPhone (itch.io) routinely
  released right on top of it, breaking swipe-back there entirely. Back
  is now only suppressed on a stepped row within accidental-tap-drift
  range; a swipe well past that fires Back regardless of what it
  released on.

## v2.4.50

- **Fix: every debug Android build could request a real ad on any
  device.** AdMob's per-device test whitelist (`admob.testDeviceId`) only
  covered devices a maintainer had thought to add ahead of time — anyone
  building from source, or CI, could still trigger a live ad request on
  an untested device. Debug builds now use Google's own published
  always-test AdMob App ID and a Google test ad unit ID
  (`AdsManager`/`BuildConfig.DEBUG`), so every debug build gets guaranteed
  test ads regardless of device; the per-device whitelist is now a
  redundant second layer rather than the only safety net. Release builds
  are unaffected.
- **Fix: "Create Game Room" room size was keyboard-only on touch.** The
  lobby's "Create Game Room  <  N players  >" row's value-adjust split ran
  across the row's own raw midpoint, which sits inside the "Create Game
  Room" label itself, leaving no safe tap target for either creating the
  room or changing its size. `menulist::List` rows can now separate a
  label's own action from the value's own adjust zone, so a tap on the
  label still creates the room while a tap on "< N players >" changes it.
- **Fix: "Set name" in the LAN/Net server lists didn't read as anchored to
  the screen.** It was the row list's own last row, so with few public
  servers listed it landed right under them near the top of an otherwise
  mostly-empty panel. It's now its own fixed "Account" section, pinned to
  the bottom of the panel regardless of how many servers are listed above
  it.

## v2.4.49

- **Fix: an undershot swipe (especially in the game room) could still
  change the currently highlighted setting's value.** v2.4.48 stopped a
  swipe from being suppressed once it traveled far enough, but a vertical
  swipe attempt that falls short of the navigation threshold releases the
  same way a tap does — and since most rows are shorter than that travel,
  it commonly lands right back on the row it started from. Because that
  row is already selected, the "second tap on the highlighted row
  activates it" rule fired, changing the value exactly as if it had been
  deliberately double-tapped (most visible on the game room's Bots/Skill
  rows, but affecting every row type). Activating an already-selected row
  from a release now additionally requires the touch stayed within
  ordinary tap-jitter range; past that it does nothing, whether or not it
  ever cleared the swipe-navigation threshold either.

## v2.4.48

- **Fix: v2.4.47 caused a black screen on iPhone on itch.io.** That
  release's landscape-rotation fix made the canvas-fit logic prefer
  `window.visualViewport` over `window.innerWidth/innerHeight`, reasoning
  that it excludes Safari's toolbar chrome — but `visualViewport` has a
  documented history of misreporting inside nested iframes on iOS Safari,
  and itch.io serves this game inside exactly that. A bad readout at first
  load collapsed the canvas to scale 0, with nothing to correct it
  afterward since nothing re-triggers a resize on a plain page load.
  Reverted to `window.innerWidth/innerHeight`, kept the (unrelated, safe)
  retry-burst behavior on rotation, and added a guard against ever
  committing an invalid scale again.
- **Fix: menu swipe up/down on touch was unreliable, and swiping down could
  change the currently selected setting instead of moving off it.** The
  gesture classifier suppressed *every* swipe direction whenever the touch
  released on a stepped row (Game speed, Victories limit, ...), but the
  underlying collision this guarded against is purely horizontal — a
  stepped row's own tap only reads which half was touched, never how far
  up or down the finger moved. A stepped row's tap band is a full row
  tall, far more travel than the vertical swipe threshold needs, so an
  intentional swipe landing on one (common on any settings screen mixing
  stepped and toggle rows) was read as a stationary tap instead — and a
  second tap on the already-selected row activates it, which is what
  changed the value instead of navigating away. Vertical swipes are no
  longer suppressed by this guard; only the horizontal "swipe back"
  gesture still is, which is the one that actually collides.

## v2.4.47

- **Fix: Android tablets could not rotate to landscape** — a single APK
  serves phones, tablets, and TV boxes, and the check used to tell a TV
  apart from a handheld (`DeviceHasTouchscreen()`) can't also tell a tablet
  apart from a phone, since both report a touchscreen. Every tablet fell
  into the phone branch and got hard-locked to portrait. A tablet is now
  detected directly (`smallestScreenWidthDp >= 600`, Android's own
  phone/tablet threshold) and given the same landscape treatment as a TV
  box. Not verified on a physical tablet — none available in this
  environment.
- **Fix: rotating to landscape on iPhone (itch.io/WASM) could leave the
  game canvas wrong-sized** — cropped or mis-scaled after rotation, while
  simulating the same rotation in a desktop browser never reproduced
  anything wrong. iOS Safari can report stale `window.innerWidth/
  innerHeight` (and even `visualViewport`) for a few frames while its own
  layout and toolbar-chrome animation settle after a rotation; the CSS
  canvas-fit logic only recomputed once per `resize` event, so a run that
  landed mid-settle locked in a wrong scale with no later event to correct
  it. It now prefers `visualViewport` dimensions and recomputes repeatedly
  for a short window after `resize`/`orientationchange` instead of trusting
  the first pass. Not verified against real iPhone hardware — the
  underlying race is a real-device timing quirk that a desktop browser's
  atomic resize can't reproduce, so this could only be checked for
  regressions on the normal (non-rotating) path.

## v2.4.46

- **Fix: Game speed (and every other stepped row) still could not be
  lowered via touch after v2.4.45** — the actual root cause, found by
  driving the deployed build with a plain mouse click (which bypasses both
  of the last two fixes' code paths entirely) and watching the value go
  the wrong way. A stepped row's `"<  value  >"` is drawn right-aligned
  near the row's right edge, but the tap target was one wide rect split
  down the row's own raw geometric middle — so on a row with a long label
  and a short value (Game speed's `"<  3.0  >"`), the visible `<` sat
  physically inside what a 50/50 split called the row's right half, and
  tapping it increased the value instead of decreasing it. `>` — further
  right, safely past either boundary — correctly increased either way,
  which is exactly why this read as "raising works, lowering doesn't"
  across two prior fix attempts. The tap target now splits at the drawn
  value block's own midpoint instead of the row's. Re-verified live: the
  exact click that increased the value on the deployed v2.4.45 build now
  decreases it.

## v2.4.45

- **Fix: Game speed (and every other stepped row) still could not be
  lowered via touch on the WASM/browser build**, reported live on itch.io
  after v2.4.44 shipped the native version of this fix. A browser fires
  both a real `FINGER_UP` and a synthesized `MOUSE_BUTTON_DOWN` for one
  physical tap; native SDL tags the synthesized one so it can be skipped,
  but Emscripten's tagging can't be trusted, so on WASM both ran —
  dispatching every menu tap and main-menu button press twice, from two
  independently computed coordinates, which is what let the two dispatches
  of one tap on a stepped row's left half disagree about which half it
  actually was. A short debounce, keyed off the same clock already used to
  dedupe a multi-finger `FINGER_UP` re-fire, now recognizes the WASM mouse
  event as that tap's own browser echo and drops it — `FINGER_UP` alone
  drives WASM menu touch, matching how the in-game aim code already picks
  one input path on that platform.

## v2.4.44

- **WASM/browser hosts can now add real network bots.** A browser tab can't
  open a raw TCP socket, so `netbot.cpp`'s WASM half was a permanent stub —
  a WASM host could add a "Bots" row but it would always fail. Replaced with
  a real transport: each bot is its own WebSocket connection to the same
  server the host is already using (fb-server already speaks WebSocket on
  its normal port), lobby lines as text frames and in-game payloads as
  binary frames. A leader now waits (up to 3s) for every bot's async
  WebSocket handshake to finish before broadcasting the level sync, closing
  a start-order gap the async connect uncovers. Verified against a real
  server + browser session: two bots played two full rounds, exchanged
  malus in every direction, correct per-round stats.
- **Bot AI scores landings on position and cluster shape, not just pops.**
  Previously any non-popping shot tied at the same score regardless of
  where it landed, so most non-popping play was effectively random, and any
  pop — however weak — always dominated by three orders of magnitude.
  Ported the scoring model (pop/detach/cluster weights, positional heatmap)
  from the zepr/fbjs clone's bot, scaled to integers: a deep pop can now
  lose to a strong cluster placed high up, and a non-popping shot has a
  real preference for its own color instead of a coin flip.
- **Fix: a stepped row (Game speed, Victories limit, bot count, ...)
  decreased via its left half, but tapping there was frequently read as a
  swipe-back gesture instead** — an ordinary tap can drift 40+ logical
  units while the finger is down, especially on a narrow phone, which is
  exactly the threshold the swipe-back heuristic used with no equivalent
  check on the right half. A stepped row's tap zone now gets first look at
  a touch landing on it before swipe gestures apply.

## v2.4.43

- **Redesigned every full-screen sub-menu around a shared `menulist::List`
  widget** (`src/menulist.h`/`.cpp`, new) — Local Multiplayer, Change Keys,
  the LAN/Net server lists, the joined-server lobby, and the game room all
  now share one scrolling row list, sidebar, header bar, and footer hint at
  a 20px type size instead of five independently hand-rolled layouts (a
  small wood popup covering under half the canvas at 15px type, plus the
  game room's own hardcoded row table). The list scrolls instead of forcing
  rows to be relocated or deleted for want of space, and every row is a tap
  target on top of the existing keyboard/gamepad navigation. The room
  screen's own per-player settings grid and roster sidebar keep their
  bespoke rendering — the shared widget wraps around them rather than
  replacing them.
- **World-map backdrop on every one of those screens**, not just the lobby
  and room that already had it — `menulist::DrawWorldMapBackdrop()` draws
  `share/gfx/back_netgame.png` full-canvas and masks its own baked-in
  watermark behind the footer hint, and every screen's panels now use a
  shared lower fill opacity (`kMapFillAlpha`) so the map actually shows
  through instead of reading as a sliver at the edges. An earlier attempt
  that lowered panel opacity without swapping the backdrop was tried and
  reverted after it made text hard to read against the busier main-menu
  artwork — the map first, then the lower opacity, is what makes it work.
- **`back_netgame.png` itself replaced** with a colorful political-boundary
  map (teal ocean, pastel per-country fills) — the previous texture, unedited
  since the project's original Perl-era "netgame first try" commit, was a
  flat monochrome red/tan/sepia image.
- Three real overlap bugs found and fixed while live-testing the above on a
  real Android device: a long server name or room-role label ran into its
  right-aligned value with no truncation (`menulist::List` now measures the
  value first and truncates the label to fit); a >5-cap room's Match Rules
  list overlapped the wider 2-column roster sidebar by 60px (the list now
  narrows to match); a long room title could overlap the header's "Start
  game!" button (the header bar now truncates the title the same way).
- Fixed the lobby's "Create Game Room" row losing tap-to-create along the
  way — an early version of the conversion made it a `splitAdjust` row for
  its "< 20 players >" display, which sends Left/Right on every tap and
  never Return, so touch players could resize the room but never actually
  create it. Restored non-`splitAdjust` behavior (tap = create) while
  keeping the same bracketed value text.
- Fixed the Local Multiplayer panel never highlighting its own selected row.
- `docs/store-assets/screenshot-1-follow-server.png` and
  `screenshot-2-game-room.png` (and their `docs/screenshots/` masters)
  recaptured against the new UI and map — the previous captures still
  showed the old wood-popup panels.
- **Windows, Linux, and the WASM browser build now use the same
  close-up-penguin-face icon macOS already had** (`tools/make-desktop-icons.py`,
  new) — `SDL_SetWindowIcon` was already loading that art natively on macOS
  (doubling as the Dock icon), so it was carried to the other three platforms
  that can use a 48x48 source without a blurry upscale: `frozen-bubble.ico`
  (Windows, compiled into the .exe), the Linux AppImage's hicolor icon, and
  a base64-inlined browser-tab favicon for the WASM build. iOS and Android
  are untouched — both need much larger masters (1024px, 512px) than this
  source could usefully upscale to, and already have dedicated sources.
- **Fix: a network room's Up/Down navigation could never reach "Start
  game!"** — `maxActions` in the room's key handler was a stale hand-count
  that predated the `Bots`/`Bot skill` rows, undercounting the true row
  total by up to two, so wraparound skipped the Start row entirely.
  Rederived from the same row-index enum the room's own renderer uses so
  the two can't drift apart again.
- **Fix: a hosted bot's malus attacks never reached the human player, and
  a human's attacks on a bot never reached the bot** — two independent bugs
  in the netcode added for in-room AI bots, both found live while
  reproducing a "no malus with bots" report and fixed together:
  - `OwnsSenderId()` (added to stop a bot's own fire/stick messages from
    being replayed onto its own board) also caught the bot's own `'g'`
    malus-attack messages, which are not a replay of the sender's board —
    they are a hit on a *different* one. `'g'` is now exempted from that
    suppression, and the receive handler now credits whichever board this
    client owns matches the destination nick, not just the local player's.
  - Separately, a bot's own connection never recognized *any* incoming game
    message as one: the check gating it assumed player ids were small
    integers, but the server actually assigns `'A'`-`'z'` (ordinary
    printable ASCII) — so the check never matched, silently leaving that
    queue empty for the bot's entire match, every match, since bots were
    added. Replaced with the same state-based gate `NetworkClient` already
    uses (`myPlayerId != 0`, i.e. past the game-start handshake) rather
    than sniffing the byte's value.
  - Both directions verified live against a local test server with a real
    hosted bot, not just by inspection.

## v2.4.42

- **Fix: Android build rejected by Play Console for not supporting 16 KB
  memory pages.** Two causes, both in `android/app/`. First, the pinned NDK
  (r25) doesn't 16 KB-align a shared library's LOAD segments by default --
  that only became automatic in NDK r28+ -- so every `.so` this build
  produces needed the linker flag explicitly (`CMakeLists.txt`). Second,
  `libc++_shared.so` is a prebuilt file NDK r25 copies into the APK
  verbatim; we never compile it, so no linker flag can reach it, and it
  stayed 4 KB-aligned regardless. Switched `ANDROID_STL` from `c++_shared`
  to `c++_static` (`build.gradle`) so libc++ is baked into each of our own
  (now-aligned) libraries instead of shipped as that separate prebuilt file
  at all. Safe here specifically because every boundary between this app's
  shared libraries is SDL3's plain C API, not C++ objects or exceptions
  crossing library edges. Verified on-device: clean launch, no
  `UnsatisfiedLinkError`, all three ABIs' `.so` files 16 KB-aligned.

## v2.4.41

- **fb-server can cap concurrent bots** (`-b`, default 20; `-b 0` refuses
  every bot). A bot identifies itself with a `BOT` command right after
  connecting -- it is otherwise an ordinary connection, indistinguishable
  from a person at the protocol level -- so this is opt-in and server-wide,
  not a limit on connections generally (that is already `-m`'s job). Unlike
  a person, a bot's shots run level generation, malus and chain-reaction
  logic on the operator's CPU on every one of them, which is the resource
  this flag is actually about. Past the cap a `BOT` command gets
  `BOT_LIMIT_REACHED`, the client shows the player why and drops that
  connection back to being a plain lobby client -- it can still join and
  play as a person, just not register as a bot. See
  [SetupServer.md](SetupServer.md#optional--limit-concurrent-bots).
- **Local multiplayer goes up to five players.** The cap was four, but the
  engine's own ceiling has always been five: `NewGame`'s case 5 is a
  hand-authored layout (one full board in the centre, four minis in the
  corners) that five-player network rooms already used, and the per-seat
  state -- `player1Keys`..`player5Keys`, `controllerInputs[5]`,
  `CTRL_SC_PLAYERS` -- was sized for five throughout. Only the local setup
  screen's own limit was lower. It now lives in one named constant rather
  than as literals spread across the menu, the clamps and the round-end
  check.

  One of those literals would have bitten: the round-completion state that
  lets you move on after a round only fired for local games of four or
  fewer, so a five-player round would have ended and then refused to
  continue. Also fixed the Team Mode label, which named a fixed
  "P1+P3 vs P2+P4" pairing regardless of the count -- it now derives from
  the same rule the teams themselves come from, so it reads
  "P1+P3+P5 vs P2+P4" at five and cannot promise a split the game does not
  play.

- **New: bots in local multiplayer.** The setup screen takes a bot count and a
  skill, and the bots fill the highest player slots so player 1 is always a
  person. They need no controller, so the "not enough controllers" warning
  only counts the human seats now. A bot aims by flying a probe bubble through
  the real launch physics -- the same movement, collision and cell-resolution
  code a fired bubble uses -- once per candidate angle, and keeps the angle
  whose landing clears the most, so it cannot drift out of step with the game
  the way a separate model of the board would. It then drives the same
  shooter controls a keyboard does rather than placing bubbles directly,
  which is what makes its launcher visibly swing onto the shot.
- **New: bots in network game rooms.** The host picks 0-4 bots and a skill
  under the room's player list, and each one joins as an ordinary member with
  its own connection to the server: everyone sees it in the roster, it counts
  against the room's cap, and it plays and takes attacks like anyone else.
  Only the host simulates them, using the same aiming as local play. Every
  board this client speaks for -- its own and its bots' -- now answers for
  itself in each part of the protocol that counts connections, including the
  end-of-round handshake, where one silent seat would have stalled the next
  round for the whole room.
- **New: `/kick p2` in the game room.** The host can remove a player by roster
  position or by nickname. The server has always supported this and enforces
  that only the room's creator may do it; no client had ever sent the command,
  and a player who was kicked was left looking at a room the server had
  already removed them from.
- **Bots plan one shot ahead, and react faster on Hard.** The bot already
  knows what colour is queued behind its current shot -- the same preview a
  person reads off the launcher -- and now credits a candidate shot for the
  combo that bubble could make next, on top of whatever it pops immediately.
  It cannot talk the bot into a worse shot for the sake of a setup: an
  immediate pop is still worth a thousand points a bubble, so the lookahead
  only ever breaks a near-tie between shots that do about the same right now.
  Hard weighs this most, Normal less, and Easy not at all. Reaction time is
  now skill-scaled too (previously the same short, semi-fixed pause for every
  bot): Hard commits to its shot in roughly a tenth of the time Easy does.
- **Fix: attack bubbles could hang in mid-air.** A malus bubble rises from the
  bottom of the board and parks one row under whatever it meets. Both halves of
  that journey scanned its column starting from row 0, which cannot tell "the
  column's lowest bubble is the ceiling row" from "the column is empty" — so a
  malus sent to a column the round had already cleared parked at row 1 with
  nothing above it. Unattached bubbles are only swept up after a pop, and a
  malus landing alone pops nothing, so it simply hung there until the player
  happened to clear something else. An empty column now sends it to the ceiling
  row, which is attached by definition. Most visible on the small side boards
  of a 3+ player game, which take attacks from every opponent at once.
- **Fix: chain-reaction bubbles on the small side boards swung far outside
  them.** A chain bubble falls until it passes a screen Y threshold, then arcs
  back up to its target. The original picks that threshold per board
  (`bin/frozen-bubble` line 2525) — 185 for the two top side boards, 415 for
  the two bottom ones, 380 for the centre — but the port used 380 for every
  board. A top side board's chain bubble therefore sank to y=424, roughly two
  hundred pixels below its own board and straight through the centre board,
  before turning around, hanging visibly at the top of the swing back.
- **Fix: blocking a player did not survive a restart.** `/block` and
  `/unblock` saved through the path that never rewrites the block list to
  disk, so the block worked for the rest of the session and was then silently
  forgotten — putting an abusive player's chat back in front of someone who
  had deliberately shut it out. Blocks now persist. The existing test called
  the save routine directly and so passed throughout.
- **Fix: `/block Alice ` (trailing space) silently blocked nobody.** The nick
  was trimmed at the front but not the back, so it never matched the nick the
  server reports on incoming chat, while the UI still confirmed the block.
- **Fix: report text is no longer able to inject protocol lines or log
  entries.** A newline pasted into `/report`'s reason split the message into
  two commands, the second of which the server would execute. The client now
  rejects control characters, and the server folds them out of `reports.log`
  independently — a hand-rolled client can no longer forge log entries that
  frame another player.
- **Fix: a paid subscription could be ignored for a whole session** (Android).
  Ad removal is derived from two independent Play queries, and either one
  coming back with a transient error stopped the *other* one's result from
  being applied at all — so a Play hiccup on the one-time-purchase lookup left
  an active, paid yearly subscriber looking at ads until the next launch. The
  entitlement is now granted on any evidence of ownership, and revoked only
  when both queries actually answered.
- **Fix: the Mobile Ads SDK was never initialized** (Android). The manifest
  disables AdMob's own auto-init (it collides with SDL's EGL surface at
  startup), and the code that was meant to take over never ran, so ad requests
  went out against an uninitialized SDK and worked only by its internal
  fallback — leaving the first request, the one the test-device allow-list
  exists to protect, outside any guarantee. Init now happens lazily on the
  first ad load, verified on-device.
- **Fix: an introductory offer would have displayed the wrong price**
  (Android). The Settings row showed the subscription's first pricing phase,
  which is the free trial or discounted period whenever one is configured in
  Play Console — printing "$0.00" beside a plan that renews at full price. It
  now shows the recurring price.
- **Fix: a network game counted you twice, and then would not start round 2.**
  The server truncates nicknames to 10 characters; the client kept the full
  one. When the server sent back its authoritative room roster after the
  start, the client compared it against its own list *by nickname* — and
  `"android_user"` didn't match the `"android_us"` the server echoed, so it
  added the local player a second time as a phantom. That inflated the player
  count (a 5-player game reported 6), drew a duplicate board, wrongly switched
  on the >5-player battle-royale HUD, and deadlocked the end-of-round
  handshake, which waited for a ready signal the phantom could never send —
  so winning a round left you unable to start the next one. Only triggered
  with a nickname longer than 10 characters, which is why short-nicked players
  never saw it. The client now clamps to the same limit the server enforces.
- **Fix: blocking a player was bypassable by changing one letter's case.**
  The block matched nicks exactly, but the server's own nick-uniqueness check
  is case-sensitive too — so `Alice` and `alice` can be connected at the same
  time as two different people, and anyone you blocked could reappear just by
  reconnecting with the case flipped, while your UI still said they were
  blocked. Matching is now case-insensitive. Nicks are also clamped to the
  10-character limit the server enforces, since a longer stored nick could
  never match what the server actually reports.
- **Fix: Android builds reported their version as `v0.0.0-nocmake`.** Android
  compiles through `android/app/CMakeLists.txt`, not the root one, so the
  `APP_VERSION` the root file defines for every other platform never reached
  it — every APK ever shipped showed the placeholder in the Settings panel.
  It now comes from `build.gradle`'s `versionName`, which the release
  checklist already bumps.
- **Fix: team-assignment control traffic could show up as chat.** The lobby's
  fallback text view didn't hide `!team:` protocol messages the way the chat
  dock did, so the same message list rendered differently depending on which
  screen was up. Both views now share one filter.

## v2.4.40

- **New: two ways to remove ads** (Android) — a $5/year auto-renewing
  subscription (`remove_ads_year`) or a $15 permanent unlock
  (`remove_ads_forever`). Both live as rows in the Settings panel, showing
  Play's own localized price; the yearly row states that it renews and where
  to cancel, before you buy.
- **Fix: ad removal could never actually be purchased.** The only way to
  trigger it was pressing `R` on the chain-reaction prompt — and nothing maps
  a touch or a controller button to `R`, so no phone, tablet, or TV box could
  reach it. (A code comment claimed it was "mapped from a controller button";
  no such mapping existed.) It is now a normal Settings row, reachable by tap,
  keyboard, and controller alike.
- **Fix: the Settings panel drew outside its own background.** The content had
  outgrown the fixed 280px box, so "Reset all settings" and the two help lines
  rendered over the title screen behind it. The panel now measures its rows
  and sizes itself, so it stays correct as rows come and go by platform and
  entitlement.
- **New: block and report abusive players.** `/block <nick>` in chat hides
  someone's messages — in the lobby and mid-match both — and takes effect
  immediately without needing anything from the server, so it works even on
  a server with no moderation at all. `/unblock <nick>` undoes it, `/blocked`
  lists them, and the list is saved per device (up to 32). A blocked player's
  in-game messages are dropped on arrival rather than hidden at draw time, so
  they don't play the chat sound either — an audible ping for a message you
  can't see would be worse than not blocking.

  `/report <nick> <reason>` sends a report to that server's operator, who
  reads it from `reports.log`. Deliberately never acted on automatically:
  nicks are chosen fresh every connect and aren't tied to any account, so
  auto-kicking on report would hand every player a way to remove anyone they
  liked. The client says the report was "sent to the server operator" rather
  than implying anything happens on its own.

  This also closes an App Store blocker — Apple's Guideline 1.2 requires
  apps with user-to-user messaging to offer blocking and reporting.
- **Fix: the server wrote `reports.log` (and would have written any relative
  path) into the filesystem root.** fb-server daemonizes with `cwd=/`, so a
  bare `fopen("reports.log", "a")` fails on any real install — reports would
  have been silently dropped while the reporter was told they'd been filed.
  Resolved to an absolute path with the same precedence as `notify.dat`
  (`FB_SERVER_REPORT_FILE`, then `$HOME/.fb-server/`, then
  `/var/lib/fb-server/`), and a write failure now returns `REPORT_FAILED` to
  the client instead of a false `OK`.

## v2.4.39

- **New: follow a server from its lobby, not just the server list**, with an
  indicator when the server doesn't support it. The header of the online
  lobby now carries the same follow toggle the LAN/Net list rows have (press
  **F**, or tap it), so it's reachable no matter how you got connected —
  picked off a list, found via LAN discovery, or typed in by hand. The client
  probes the server once per connection with a side-effect-free `NOTIFYUNREG`
  and reads back whether it was understood, so an older `fb-server` (or
  anything else answering on that port) shows "not supported" instead of the
  toggle silently doing nothing.
- **Fix: iOS builds were missing `NSLocalNetworkUsageDescription`.** Without
  it, iOS 14+ silently refuses any local-network connection attempt — LAN
  play included — with no error the app can catch. Added to
  `cmake/iOSInfo.plist.in`.
- **Fix: `notify-relay`'s APNs sender could never have delivered a real
  push.** It passed the `.p8` key's file *path* to `aioapns`, which expects
  the key's PEM *contents* — every send failed parsing the pathname as PEM
  before reaching Apple. Invisible until real credentials were supplied,
  since stub mode never constructs the sender at all. iOS push confirmed
  against live APNs on a real device once fixed — see
  [docs/PUSH_SETUP.md](docs/PUSH_SETUP.md).
- **Fix: the generated iOS entitlements file (`FrozenBubble.entitlements`)
  could never be signed with.** Its own doc comment contained a literal `--`
  sequence, which Apple's plist parser (AMFI) rejects inside an XML comment,
  so `codesign --entitlements` failed outright on every attempt. Also
  documented the real gotcha it was trying to explain: signing with only that
  file's `aps-environment` key (rather than the full entitlement set a
  provisioning profile carries) fails install with "missing
  application-identifier entitlement" — [docs/IOS.md](docs/IOS.md) now shows
  pulling the full set out of the profile itself.

## v2.4.38

- **Fix: Android TV launched in a squeezed portrait window instead of filling
  the screen.** A per-device orientation change shipped in v2.4.37 picked
  portrait or landscape based on whether the device had a touchscreen — but
  Android TV boxes register a touch device too (both because Android TV
  itself can claim the touchscreen feature, and because SDL registers a touch
  device for any virtual input device, which every Android device has), so
  every TV got portrait. The game now asks the device's UI mode instead.
- **Fix: lobby and game-room chat did nothing on Android TV**, a side effect
  of the same touchscreen check above gating the keyboard handover chat
  needs. TV boxes have no keyboard either, so they need that handover just
  as much as a phone; the check is gone. Verified on real Fire TV hardware.
- **Docs:** noted that a sluggish Fire TV is usually fixed by a reboot —
  under [Android TV](README.md#android-tv) in the README.

## v2.4.37

- **New: follow a server and be told when someone joins it.** Press **F** on a
  server in the LAN or Net list, or tap the star at the left of its row, and
  that server will notify your phone when a player joins — so a quiet server
  can tell you a game is starting instead of you checking it. Followed servers
  are saved per device, up to eight.

  The notification is delivered by the OS, so it arrives with the app
  backgrounded or fully closed; it is deliberately suppressed while the app is
  in the foreground, where the lobby already shows who is online. Rate-limited
  to one per device per 10 minutes so a busy server can't turn into a stream of
  banners.

  Server-side this is a new `NOTIFYREG`/`NOTIFYUNREG` protocol pair and a
  registration table (`server/notify.c`) that is deliberately *not* tied to a
  connection: everything else in the server is keyed by file descriptor and
  freed the moment a socket closes, which is exactly the wrong lifetime for
  something whose whole purpose is to reach a device that has disconnected.

  Actual APNs/FCM delivery is handled by a new optional sidecar,
  `server/notify-relay/`, rather than by `fb-server` itself — the game server
  has no TLS stack and runs one blocking event loop for every connected player,
  so an HTTP/2 handshake with Apple mid-round would stall the game. It fires a
  best-effort UDP datagram instead and moves on; if the relay is down, missing,
  or misconfigured, the datagram is dropped and gameplay is untouched. Operators
  who don't want the feature can simply not run it. See
  [SetupServer.md](SetupServer.md).

  Device-token acquisition is implemented on both platforms: `src/push_ios.mm`
  requests permission and registers with APNs (grafting its callbacks onto
  SDL's application delegate rather than replacing it, which would stop the app
  launching), and `PushManager.java` obtains an FCM token. Foreground
  suppression is handled per platform — a `UNUserNotificationCenterDelegate` on
  iOS, and on Android by FCM's own behaviour, which routes a notification
  payload to the app instead of the tray while it is in front.

  **Firebase is optional and off by default.** The Gradle plugin is applied only
  when `android/app/google-services.json` is present, and `PushManager` reaches
  the SDK by reflection, so a clone without credentials — including CI — builds
  and runs exactly as before. Both paths are verified.

  **Still needs credentials to deliver anything.** iOS additionally needs a
  signed build: APNs will not issue a token without the `aps-environment`
  entitlement, which free sideloading profiles do not grant. The build now emits
  `FrozenBubble.entitlements` to sign with. See
  [docs/PUSH_SETUP.md](docs/PUSH_SETUP.md) for both consoles end to end.

  **Android verified end to end on real hardware**, including two bugs only a
  real device and real credentials could surface: `androidPushToken()` looked
  up `PushManager` with `FindClass()` by name, which silently returns null
  when called from a thread the JVM didn't create — the SDL game thread is
  exactly that — so no real device ever obtained a token. Fixed by routing
  through a one-line static wrapper on `FrozenBubbleActivity` instead, reached
  via the already-valid Activity object rather than a name lookup, mirroring
  `androidFetchUrl()`. Separately, `notify-relay`'s pinned `apns2` dependency
  turned out to hard-require `PyJWT<2.0` while `firebase-admin` requires
  `PyJWT>=2.5.0` — no version of either satisfies both, so the relay image
  could never have built with real credentials at all, blocking Android
  delivery too. Replaced with `aioapns`, moving the relay's receive loop onto
  `asyncio` in the process (a persistent APNs connection instead of
  reconnecting per push, and FCM's blocking call now runs via
  `asyncio.to_thread`). iOS delivery remains unverified against a real device
  pending Apple Developer Program enrollment.

## v2.4.36

- **Touch taps and swipes now land in the right place off a 4:3 screen.** They
  were mapped onto the 640×480 playfield by scaling the raw touch position,
  which is only correct on a window shaped exactly like the canvas — true of
  every desktop window and no phone or Android TV panel. On a landscape phone
  this compressed the horizontal axis to 61% of true, and made swipe-left-to-
  go-back need a swipe 1.6x longer than intended; portrait was worse still.
  Fixed by mapping through the same letterbox-aware conversion the mouse
  already used. Affects iOS, Android TV, and any non-4:3 desktop window.
- **iOS now rotates freely** instead of being locked to landscape, matching the
  browser build (which cannot lock orientation at all). Portrait renders the
  playfield as a band across the screen rather than filling it.
- **Android now opens in the right orientation for the device**: portrait on
  phones and tablets, landscape on TV boxes, decided automatically from
  whether the device has a touchscreen — the same one APK serves both.
- **New: swipe left to leave a round in progress**, for touch devices that have
  no Escape key, gamepad B, or Android back button — iOS in particular had no
  way to leave a game at all. Confined to the bottom of the screen, level with
  the launcher or below, so it can't be triggered by mistake while aiming.
- **The game speed setting can now be changed by tapping**, not just from a
  keyboard. It's stepped with Left/Right rather than activated, so the old
  tap-to-activate gesture silently did nothing for it; a second tap on either
  half of the row now steps the value down or up.
- **Lobby and game-room chat can now be typed into by tapping.** Activating the
  chat row used to go straight to "send", which did nothing with an empty
  field and looked broken. It now raises the on-screen keyboard properly.
- **Composing a chat message no longer hides the conversation.** The whole room
  used to be replaced by a bare input box; it now grows the chat log to show
  as many recent messages as fit, with the room's map still behind it.
- **The on-screen keyboard no longer covers the field you're typing into**, on
  iOS and Android. The rect telling the OS where to shift the view for was
  being passed in canvas coordinates instead of window coordinates, so the
  shift was wrong on anything but a 4:3 screen.
- **The letterbox bars around the playfield are reliably black**, rather than
  taking on the last UI colour that happened to be drawn — most visible in
  portrait, where the bars are a third of the screen.
- **Removed the "Continue when players leave" room setting** — it's now always
  on, matching the only value most hosts used it at. Everything else in the
  game room shifted up one row's worth of internal bookkeeping only; no other
  behavior changes.
- Fixed an Android build issue where compiling the game corrupted a shared
  header in the SDL_image submodule tree, breaking the next iOS build until it
  was manually restored.

## v2.4.35

- **New: an experimental iOS build**, produced by `tools/build-ios.sh`. It is
  **unsigned**, so it has to be re-signed before a device will install it, and
  nothing is published on the releases page — to play on an iPhone today, use
  the browser build. SDL3 and its satellites are compiled from source for the
  platform, assets resolve from inside the app bundle, saves and logs go to the
  app container, and HTTP goes through NSURLSession since iOS ships no `curl`.
  Hosting a LAN game is unavailable there, as on Android and Windows. See
  [docs/IOS.md](docs/IOS.md), including what is still unverified.
- **Mouse and touch aiming now default to on where there is no keyboard** — in
  the browser, on iOS, and on Android phones and tablets — and stay off on
  desktop and Android TV. A single Android APK serves both TV boxes and phones,
  so the default is decided at runtime from whether the device has a
  touchscreen. A stored preference still wins, and keyboard and controller
  aiming keep working alongside it.
- **New: "Reset all settings"**, at the bottom of Settings → Keys. It restores
  key bindings, speed, sound and mouse aim, and asks for a second press before
  doing anything.
- **List-style panels are now tap-to-select on touch devices** — settings, the
  LAN and Net server lists, the connect form, and the online lobby and game
  room. The first tap on a row highlights it and a second tap activates it, so
  a row can be read before it changes. In the game room's per-player grid a tap
  picks the cell first, so a mis-tap no longer changes the wrong player's
  setting.
- **3-player rounds no longer keep the death-enlarged launcher** — every other
  player count put it back, and the offset also drifted a pixel per death
  instead of applying once.
- **Clearing a single-player level no longer scores twice** — a chain bubble
  still in flight when the win panel appeared could re-enter the scoring branch,
  awarding a second 1000 and writing a duplicate row into the high scores and
  the level history.
- **The training-mode clock no longer counts paused time**, matching the
  high-score timer beside it.
- **Replugging a controller now works** — each hotplug leaked a gamepad and
  consumed a slot permanently, eventually walking a pad into a slot past the
  bindings where its buttons silently stopped responding.
- **Muting no longer restarts the music** — muting stopped playback outright, so
  unmuting had to name a track and always resumed the in-game theme regardless
  of what had been playing. It now pauses and resumes.

## v2.4.34

- **Android upgrades no longer strand a half-extracted asset tree** — installing
  a new version now rebuilds the managed asset directory completely, and an
  extraction interrupted by a crash, a kill, or a full disk is detected and
  redone on the next launch instead of leaving the game running against a
  partial tree. Settings and high scores are never touched by this.
- **Browser saves now survive a reload** — settings, key bindings, level
  history, and high scores are stored in IndexedDB rather than in memory, so
  they persist across a page reload in the same browser and origin. See the
  scope note in [web/README.md](web/README.md#saved-data).
- **Settings and high scores are written the moment they change**, rather than
  only at a clean shutdown, so a crash or a closed tab no longer discards the
  session's progress.
- **Saves can no longer be lost to an interrupted write** — each file is written
  out in full and swapped into place, so a crash mid-save leaves the previous
  high-score table intact instead of a truncated one.
- **The high-score screen no longer shows blank level thumbnails** — saving
  invented an empty level for every unused id, and each one it invented made it
  invent another, so finishing a single level wrote eighteen empty grids into
  the level history.
- **Fixed a ~1 MB leak at shutdown** — the high-score manager and settings
  objects ran their cleanup but were never freed, along with the surfaces and
  textures they owned.

## v2.4.33

- **Classic, Clear, and Team matches now use the correct win conditions** — an
  empty board no longer ends Classic or Team play by itself, while Clear Mode
  reliably awards exactly one win to the player who cleared their board.
- **Simultaneous final losses now resolve as a draw** without briefly or
  permanently crediting either player with a win or showing player one's
  winner panel.
- **Player departures now honor the room's continuation, surviving-team, and
  victories-limit rules**, including when a departure makes an in-progress
  next-round wait terminal.
- **Local 2–4 player setup now exposes a reachable victories-limit setting**
  and carries the selected value into the match. Finite matches return to the
  menu when the limit is reached, while unfinished 3- and 4-player matches can
  advance after every player's round-end animation completes.
- **Chain reactions now choose valid targets on flipped boards**, reserving the
  complete connected group and cancelling invalid cross-chains.
- **High-speed shots no longer tunnel through occupied bubbles** on full-size
  or mini boards, and now attach at their first collision.
- **Levelset high scores no longer go blank** — a new high score, or just the table reflowing to fit one in, could silently wipe the name/level/time text of any entry copied in the process. Text now travels with the entry instead of being dropped.
- **Multiplayer targeting indicator now actually renders** — it never received a font, so it silently failed to draw every time.
- **Server list no longer shows dead servers as online** — a closed port could still read as a successful connection; latency checks now confirm the connection actually succeeded.
- **Fixed several settings/diagnostic messages being silently discarded** — a logging category mistake meant warnings about e.g. an unwritable settings file never appeared anywhere.
- **Hardened against corrupt or malicious input**: out-of-range window height, NaN/infinite speed multiplier, out-of-range saved key bindings, and a server-side integer overflow are all now rejected or clamped instead of propagating.
- **Fixed a gamepad button-to-player mapping bug** — the button stride was smaller than SDL3's button count, so some buttons could control the wrong player's ship. Note: this changes the stored binding layout for players 2–5, who will need to rebind their controllers once.
- **Hosting a LAN server no longer kills unrelated `fb-server` processes** on a busy port — it now reports the conflict and lets you find the real owner.
- **Client no longer crashes if a menu image asset is missing** — failures are now logged with the missing path instead of crashing the moment the panel opens.
- **Fixed a memory leak during menu background animations** and a second leak in level-editor-era candy image handling.
- **Server-side memory and correctness fixes**: an empty room name could consume a game slot invisibly and unjoinably; closing a room could leak a small allocation per remaining player; the lobby's free-player count could disagree with its own player list.
- **CI now runs the test suite under AddressSanitizer/UndefinedBehaviorSanitizer on Linux**, closing a gap where memory-safety fixes were unverifiable on macOS.
- Numerous internal hardening and cleanup changes: WebSocket handshake edge cases, a crash in the plasma menu transition when its asset is missing, dead menu code removed, and singleton lifetime/initialization cleanups.

## v2.4.32

- **Fixes the browser build, which failed to link in v2.4.31** — a diagnostic added in v2.4.31 was defined only for desktop builds, so the WebAssembly build did not link and that release went out incomplete. v2.4.31's changes are listed below and are all included here.

## v2.4.31

- **macOS download is now labelled by architecture** — the macOS build is Apple Silicon only, but shipped as `frozen-bubble-macos.dmg` with nothing saying so, and Intel Macs downloaded an app that could not launch. It is now `frozen-bubble-macos-arm64.dmg`, and the build fails rather than publishing if the binary is not the architecture the name claims. Intel Macs can build from source or play in the browser.
- **The version shown in-game is correct again** — the settings screen read v2.4.26 regardless of which release you were running. The version now comes from one place and matches the build on every platform, including the server's startup log.
- **Windows installer can no longer ship missing libraries** — the packaging step copied a fixed list of DLLs and ignored every failure, so a missing one still produced a working-looking installer that failed on the player's machine. The required libraries are now determined from the program itself, and the build fails if any is missing.
- **Server survives a busy discovery port** — if anything else was using the LAN discovery port, the server refused to start at all. It now starts, serves games normally, and explains that only broadcast discovery is unavailable.
- **A real HTTPS certificate is no longer destroyed by the setup script** — the server setup script only recognised older RSA certificates, so a current Let's Encrypt certificate was treated as invalid and overwritten with a self-signed one that browsers reject. It now recognises both, and refuses to overwrite existing certificates outright. Renewal instructions corrected.
- **Deployment credentials hardened** — the release workflow ran a third-party action from a moving branch while giving it the itch.io publishing credential. It is now pinned to a fixed, reviewed version.
- **Installed macOS builds find their assets** — a build installed outside an app bundle looked for game files in the directory it was compiled in, and failed anywhere else.
- **Android build docs match the actual build** — setup instructions described downloading SDL2 libraries by hand; the project builds SDL3 from submodules.
- **Removed dead SDL2-era files** — 97 broken links and three stale build/documentation files describing a build that no longer exists.

## v2.4.30

- **Desktop frame pacing fixed** — the frame limiter was measuring the wrong interval, so instead of holding a steady 60 fps it alternated a full-length pause with almost none, delivering frames in short/long pairs at roughly 107 fps. On a 60 Hz display about half of those were drawn and never shown, and the rest arrived out of step with the refresh, which read as stutter. Frames now arrive evenly.
- **Desktop now runs at exactly browser speed** — a side effect of the pacing bug was that the very short frames hit an internal lower limit, quietly adding about 6% to the game speed on desktop that the browser build never had. Desktop and browser now run identically at the same speed setting.
- **Display sync enabled on desktop** — the game now presents in step with the monitor instead of on its own clock, which removes a dropped or doubled frame every few seconds. Falls back to the frame limiter where the display driver does not support it.
- **Performance overlay** — press **F3** to show frames per second, frame-time range, and effective game speed against the configured speed, in the bottom-right corner. Off by default, remembered between sessions. Useful for comparing desktop against the browser build, and for telling a frame-rate problem apart from a game-speed one — they look the same while playing but have different causes.

## v2.4.29

Completes the high-severity fixes from the repository audit that v2.4.28 started.

- **Players can no longer impersonate each other** — the server relayed each in-game message with the sender byte exactly as the sending client wrote it, so a client could claim to be any other player in its room, or the room leader. The server now stamps every relayed message with the seat it assigned that connection.
- **A stray message no longer takes the server down** — a connection left in a room that had closed or kicked it could, with its next in-game message, terminate the whole server process and every unrelated game running on it. Only that connection is closed now.
- **Server privilege drop fails loudly** — when started with `-u`, a failed switch to the requested user was ignored and the daemon carried on with its original privileges. It now refuses to start, and also drops supplementary groups, which it previously kept.
- **Server rejects an implausible response length** — a hostile or broken master-server reply could steer the server's own buffer arithmetic; the value is now range-checked before use.
- **The game starts even when its settings file cannot be written** — an unwritable preferences folder previously left the game retrying forever before any window appeared, so it looked frozen. It now starts with default settings and says so.
- **A corrupt highscore or level file no longer prevents startup** — the bad entry is skipped and the rest of the file is kept, instead of the game closing during startup.
- **Long lobby listings no longer break the connection** — on a busy server, a single large message could permanently wedge the client's receive buffer: the lobby stopped updating and, in a game, moves from other players were silently discarded while boards drifted apart. No error was shown.
- **Windows: the game no longer stalls waiting for the network** — the client's per-frame receive was blocking on Windows, so the game could hang until the server sent something. *(Fixed by construction; not yet validated on Windows hardware.)*
- **Stale attacks no longer carry into a new game** — starting a different match kept attacks and counters from the previous one, which could land on a board belonging to a player who is no longer in the game.
- **WebSocket messages are sent whole** — a partially-sent message was reported as fully sent, which left browser clients misreading everything that followed on that connection.

## v2.4.28

- **Server crash on simultaneous disconnects fixed** — when several players in the same room dropped at once, the server could keep using a game it had already torn down. On the shipping build this corrupted whichever branch it read next and could write a bogus win to the stats file; under a sanitizer it aborted the process, taking every other room on the server down with it.
- **Server hardened against malformed LAN discovery packets** — a full-length discovery datagram could make the server read past the end of its receive buffer. Well-formed probes are unaffected.
- **Client hardened against malformed data from other players** — out-of-range bubble placements from a peer are now dropped instead of writing outside the board, team numbers are clamped to the valid range instead of indexing off the end of the team color table, and a non-numeric or oversized value in a room-options message no longer terminates the game.
- **Regression tests added** — the server use-after-free, the discovery over-read, and the team-number clamp each have a test that fails without its fix. The two server tests require a sanitizer build and report themselves as skipped otherwise, rather than passing without running.
- **Android releases are now upgradable** — CI previously generated a throwaway signing key on every run, so each release was signed by a different identity and Android refused to install it over the previous version. Releases are now signed with a persistent key held in repository secrets, and a tagged release fails rather than publishing an APK that cannot be upgraded. `versionCode` also advances per release instead of staying pinned. See `docs/ANDROID_SIGNING.md`.

## v2.4.27

- **Desktop game speed default raised to 3×** — new macOS, Linux, and Windows settings now match the browser default; existing saved speed preferences remain unchanged.
- **Round-stats team colors fixed** — winning players in Team Mode now retain their configured team color instead of switching to the generic green winner highlight.
- **All release builds restored** — Linux AppImage, macOS DMG, Windows installer, Android APK, and WebAssembly packages are built, attached to tagged GitHub releases, and deployed to their Itch.io channels.

## v2.4.26

- **20-player battle royale** — rooms can now hold up to 20 players (choose 5, 10, or 20 when creating a room), with new UI to handle the larger player count: an auto-ranked opponent view (the 4 visible mini-boards are kept on whoever's most relevant — targeting you, attacking, in danger, then anyone alive — with manual Tab paging as an override), a slot-relative target picker (keys 1–4 target whoever's shown in that view slot once more than 5 players are alive), a blinking attack-flash border on any board that's actually been hit, kill tracking (a new **KO** column in the round-stats table), a spectate mode for eliminated players (the same 1–4/0 keys pin an opponent's board into view instead of picking a malus target), and a compact 2-column player roster with a lobby room-list `(count/cap)` display for rooms above 5 players.
- **Team Mode** — new game mode for local multiplayer and network game rooms. Players are assigned to teams; malus attacks only go to players on other teams; the round ends when only one team remains alive. Rooms of 5 or fewer players assign teams via a per-player grid row; rooms above 5 players use a dedicated roster instead — press **A** in the game room to open it, with auto-balanced teams as the default and per-player overrides on top.
- **Clear Mode** — new game mode available in local multiplayer and network game rooms. First player to clear their entire board wins the round (last survivor also wins). Defaults to row compression off and malus disabled; both can still be toggled independently by the host.
- **Malus disable setting** — host can now disable malus attacks independently in both local multiplayer and network game rooms.
- **Online lobby and game-room refresh** — adds a persistent chat dock, larger scrollable room cards, an online-player sidebar, and grouped match settings.
- **Fix: classic single-player campaign inheriting a stale chain-reaction flag** — "Play All Levels" and "Pick Start Level" could silently inherit chain reaction being left on from an earlier Random Levels/2P/network session, even though the classic campaign is supposed to always run with it off.

## v2.4.25

- **Dead player's board now freezes mid-round** — in 3–5 player games, when a player is eliminated but the round continues, their board now ices over (and shows the frozen cap) so the elimination is clearly indicated, matching the original Frozen Bubble's `update_lost` behavior.
- **Fix: keyboard aim stuck after using the mouse** — once you aimed with the mouse, the latched mouse angle overrode the launcher every frame, so keyboard/controller aim stopped working until reload (most visible on the itch.io / WASM build). Keyboard/controller aim now reclaims control (mouse re-activates on the next mouse move).

## v2.4.24

- **Post-round stats screen** — after each multiplayer round, a per-player table shows bubbles **Fired**, **Popped**, malus sent (**Atk**) and malus received (**Def**); the round winner's row is highlighted. In network games, each client broadcasts its own round stats via the new `S` GAMEMSG opcode so every player sees exact numbers for everyone (not just themselves).
- **Lobby match summary** — when a network match ends and players return to the lobby, the host posts a summary to the chatroom: rounds played, plus each player's win count and match totals (fired / popped / atk / def).
- **Incoming-malus indicator** — when malus lands on your board, a fading toast above your launcher shows **who sent it and how many** (e.g. `dchau2  +12`). Repeated hits from the same attacker aggregate into one toast.
- **Fullscreen toggle in Settings** — Settings → Keys menu now has a Fullscreen ON/OFF row (desktop only; hidden on WASM). Persists across restarts.
- **Fix: native build broken since the malus-split fix** — `SendMalusToOpponent` referenced `currentSettings.teamMode`, a field that does not exist in this branch's `SetupSettings`, so the native target did not compile. Removed the stray reference.

## v2.4.16

- **Nickname saved** — last used nickname is remembered across restarts on desktop, Android, and web (browser uses localStorage)
- **Lobby player list** — in-lobby screen now shows names of available players (up to 9, with "+N more" for overflow)
- **Nick save fixes** — Set Name field clears before typing; nick saves immediately on confirm, not just on connect

## v2.4.15

- **Sound toggle in Settings** — Settings → Keys menu now has a Sound ON/OFF toggle; disables all music and SFX immediately and persists across restarts

## v2.4.14

- **Xbox controller: continue round fixed** — pressing A on an Xbox controller after a round ends now correctly continues to the next round in local multiplayer

## v2.4.13

- **Single player targeting fixed** — when the "Single player targeting" lobby setting is on, malus now automatically focuses on one opponent instead of splitting; manual keys 1–4 still override the auto-selection
- **Ghost player fix** — reconnecting with the same nickname no longer shows duplicate entries in the server lobby (stale connection is evicted immediately)
- **Android default nickname** — default nickname on Android TV is now `android_user` instead of `unnamed`

## v2.4.12

- **Game Speed setting** — adjustable in Settings → Keys; use LEFT/RIGHT on the "Game Speed" row to set 1.0–5.0×; saved per device to settings.ini
- **Fire TV fixes** — A button now continues to next round after game ends; minimize/resume no longer causes persistent slow speed
- **Malus bubble speed** — increased 25% and now frame-rate-independent

## v2.4.11

- See v2.4.12 (combined release)

## v2.4.10

- **Speed tuned** — native clients (macOS, Linux, Windows, Android TV) run at 1.25× base speed; browser (WebAssembly) runs at 3.0× normalized across all frame rates

## v2.4.9

- **Frame-rate-independent speed** — bubble and launcher movement now scale with delta time on all platforms; browser builds (WebAssembly) run normalized across all frame rates; native builds (macOS, Linux, Windows, Android TV) run at 1.5× speed

## v2.4.8

- **WebAssembly: swap creators fixed** — after a game ends, the server now correctly moves the player's connection back to normal lobby mode (previously left in in-game priority mode), preventing the 5-second in-game timeout and stale duplicate entries when starting a new game with swapped roles
- **WebAssembly: round 2+ sync fixed** — browser client waits for all level-sync messages to arrive before starting each subsequent round (same mechanism as the round 1 fix in v2.4.7); prevents the ~30-second disconnect at the start of round 2
- **Server: version logged at startup** — fb-server now prints its version and protocol to the log on startup, making it easier to confirm which binary is running

## v2.4.7

- **WebAssembly: game start fixed** — browser client now waits for all 40 level-sync messages to queue up before entering the game loop, preventing the immediate disconnect when a native client hosts and the web client joins

## v2.4.6

- **Menu animation fix** — graphics quality icon no longer attempts to load missing frames at startup (off-by-one in frame count guard)
- **macOS startup log fix** — suppressed spurious `[ERROR] [DEBUG] Parameter 'texture' is invalid` messages from SDL Metal renderer initialization

## v2.4.5

- **WebAssembly: join game fixed** — browser client now correctly joins multiplayer game rooms; macOS host can see the web player join
- **WebAssembly: join retry** — JOIN command automatically retries with a name suffix if the nickname is already in use (mirrors the existing CREATE retry behaviour)
- **Game room chat text color** — fixed chat messages appearing yellow on the web client

## v2.4.4

- **WebAssembly: public server list** — itch.io browser version now fetches and displays the public server list on the Net Game screen
- **Net Game loads instantly** — server list fetch and latency probing moved to a background thread (desktop); browser opens the screen immediately
- **WebAssembly: game creation fixed** — CREATE command now waits for server confirmation before entering the game room; automatically retries with a name suffix if the game name is already taken
- **Max colors** — "Colors" option renamed to "Max colors" in all game setup panels (2P, local multiplayer, LAN, net game)
- **Pause animation fix** — pause penguin animation now loads the correct frames (was off-by-one)
- **Stick effect asset fix** — missing `stick_effect_7-mini.png` added; array bounds corrected

## v2.3.1

- **Xbox controller support** — fully working in 1P and 2P local modes; bind any button in Settings → Keys
- **Reset controller defaults** — one-click reset to D-pad + A button layout per player in key bindings
- **Bubble centering fix** — all players now land at the same column when shooting straight up
- **Exit button** — replaced High Scores menu button with an Exit App button
- **Net game manual entry** — added visible Connect button; navigate with UP/DOWN, ENTER to select
- **Net game lobby text color** — fixed text appearing all red after a failed connection attempt
- **False local server in Net Game list** — fixed spurious "Local Server" entry appearing when no server is running
- **Net game keyboard** — keyboard no longer auto-opens when entering the manual IP/port entry screen; press ENTER on a field to open it
- **Android TV delete key** — improved backspace handling for text fields on Android 11+

## v2.3.0

- **Per-player lobby settings grid** — Max colors, Rows collapse, and Aim guide shown as a P1–P5 column grid; host navigates with arrow keys and Enter
- **Aim guide** — trajectory preview toggle per player
- **Row compression toggle** per player — disable rows collapsing for specific players
- **Local multiplayer** — 2 players on controllers (3–5 player local is WIP)
