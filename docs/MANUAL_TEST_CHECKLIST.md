# Manual test checklist

Fixes with a remaining hands-on or platform-specific verification gap, and what
to do to confirm each. An unchecked box means the procedure has not been run;
automated test coverage does not imply that the visual or multi-client check
was performed.

Grouped by how much effort the check takes.

---

## 1. Round and match state cluster

None of these procedures was executed during the automated remediation work.

- [ ] **Local victories row, keyboard and controller:** open Local Multiplayer
  setup with 2–5 players, navigate to **Victories limit**, and use Left, Right,
  and confirm/Enter through the available values and both wrap points. Repeat
  with a controller and confirm the selected row and value remain visibly in
  sync with each input.
- [ ] **Local finite-match continuation:** start 3-, 4- and 5-player matches
  with a victories limit of 2. After the first round, wait for every player's
  round-end animation and confirm the next round starts. After one player
  reaches the second win, confirm Enter/fire returns to the menu instead of
  silently starting another round. (Five players is the case that used to be
  excluded from the round-completion check, so it is the one worth repeating
  after any change there.)
- [ ] **Classic continuation and Clear Mode win:** empty a local multiplayer
  board in Classic mode and confirm the round continues. Repeat in Clear Mode
  and confirm exactly one win, the correct winner presentation, and the
  expected win SFX.
- [ ] **Simultaneous local loss draw:** arrange for the final surviving local
  players to cross the danger line in the same frame. Confirm the round is
  presented as a draw and neither player receives a transient or lasting win.
- [ ] **Teams survivor:** with players split into teams via Set Teams (teams
  are a per-player setting, not a separate mode — try this in both Classic and
  Clear Mode), eliminate players until exactly one team remains. Confirm the
  surviving team receives the intended winner presentation and teammate credit
  without ending merely because one board was cleared.
- [ ] **Native departure continuation:** connect two native clients, exercise a
  player departure with **Continue game when players leave** both enabled and
  disabled, and confirm the remaining client continues or ends the match as
  configured without an unintended next-round restart.
- [ ] **Remote clear ordering:** with two native clients, exercise both visible
  remote-clear orders (win announcement before replicated stick resolution,
  then replicated stick resolution before the announcement). Confirm each
  order shows one clear win and increments the remote winner exactly once.
- [ ] **Maximum-speed occupied-bubble collisions:** at maximum game speed, fire
  both bank shots and vertical shots toward occupied bubbles on full-size and
  mini boards. Confirm every shot attaches at first contact and none tunnels
  through to a ceiling or non-neighboring cell.

## 2. Verified in code but never executed

These have a correct fix and a clean build, but the code path was not reached
during testing. They are the ones most worth your attention.

### BUG-051 — oversized level block (out-of-bounds write)

`LoadLevelset` writes into a fixed 10-row array with no bound check, so a level
block with more than 10 rows wrote past the end. Excess rows are now dropped.

**Why untested:** levels load when a game starts, not at launch, and driving the
menu into a game needs real keyboard input.

**To check:** append a level block with ~14 non-blank rows to `share/data/levels`,
then start a 1P game. Expected: it launches and logs
`Level block in … has more than 10 rows`. Previously this was undefined
behaviour — it may or may not have visibly crashed.

Same function: a non-numeric token in `data/levels` used to terminate the game
via an unguarded `stoi`. It should now log `Bad value '…'` and treat the cell as
empty.

### BUG-044 — missing single-player menu asset

A missing `txt_*_text.png` crashed the client when the single-player panel
opened. All three dereference sites are guarded now.

**Why untested:** startup was verified with all five assets deleted (client
starts, logs ten errors), but opening the panel needs input.

**To check:** temporarily rename one `share/gfx/menu/txt_*_text.png`, launch, and
open **Start 1P Game**. Expected: the panel opens, that entry renders without its
overlay effect, no crash.

### BUG-043 — multiplayer targeting indicator

`targetingText` never had a font loaded, so the "who you are targeting" label
could never render at all.

**To check:** start a network or local game with more than 5 players, or with
single-player targeting enabled. Expected: a small `> nickname` label appears
near the shooter. It has **never** appeared before, so its absence is not a
regression — but its position and size are unverified and may need nudging.

### BUG-045 — levelset highscore text lost on insert

`HighscoreData` stored a `TTFText` by value, and `TTFText`'s copy constructor
and copy assignment silently discarded the rendered font/texture instead of
copying or freeing it. Every `push_back` into `levelsetScores` — and, it turns
out, every `std::vector` reallocation of it — blanked the text on whichever
rows got copied. Fixed by making `TTFText` move-only with real ownership
transfer, so relocating an entry now carries its texture along instead of
losing it.

**Why untested:** the path only fires when a new levelset high score is set
and the score screen renders it, not at launch.

**To check:** play a level to completion with a time/level that qualifies for
the levelset high score table (fewer than 10 entries so far, or better than
the current 10th), then open the levelset high score screen. Expected: the
new entry and every existing entry shows its name, level, and time text — none
render as a blank tile. Previously the rendered text was silently dropped on
insert.

---

## 3. Behaviour changes you should sanity-check

### BUG-036 / BUG-035 — controller bindings (⚠ known consequence)

The virtual-scancode stride changed from 20 to 26 to match SDL3's actual gamepad
button count, because buttons 20+ were aliasing into the next player's slot.

**Consequence:** saved controller bindings for **players 2–5 will decode
differently and need rebinding.** Player 1 (slot 0) and all keyboard bindings are
unaffected.

**To check:** with a gamepad, confirm player 1 still works, then rebind players
2+ in Settings → Keys. Tell me if losing those bindings is unacceptable — it is
reversible, at the cost of leaving the aliasing bug in place.

### BUG-033 — hosting a LAN server on a busy port

Hosting used to run `pkill -x fb-server`, killing **every** `fb-server` you own —
including servers for other projects. It now refuses and reports the conflict.

**To check:** start a server on a port, then try to host on the same port from
the client. Expected: a clear "Port N is already in use" message and **no**
processes killed. Previously it would have killed your other servers silently.

### BUG-016 — server reachability

Refused endpoints used to appear online with a plausible latency.

**To check:** put an unreachable host/port in the server list. Expected: shown as
unreachable rather than online. (The underlying logic was proven against a closed
port; only the UI path is unverified.)

---

## 4. Cannot be verified on macOS at all

### BUG-046 — Android asset deployment across an upgrade

The transactional deployment is covered by JVM unit tests and by APK hash parity
against `share/`, both running in CI. **No emulator or device was run at any
point during this remediation**, so the runtime path — the one that actually
performs the swap on a real install-over-install — has never executed.

**Needs:** two APKs signed by the same key, the second with a strictly higher
`versionCode` (Android refuses the upgrade otherwise — see
[ANDROID_SIGNING.md](ANDROID_SIGNING.md)). Between building them, change one
asset's contents, delete another, and add a new one.

**To check**, installing the second APK over the first:

- [ ] the **changed** asset is refreshed to its new contents
- [ ] the **deleted** asset is gone from the managed tree
- [ ] the **new** asset is present
- [ ] a **truncated but non-empty** asset is repaired — truncate one in the
      managed tree before launching, and confirm it is restored in full rather
      than left short (a zero-length file is the easy case; a partial one is the
      case that matters)
- [ ] `g_dataDir` in the startup log is the exact extracted path, not a
      `/data/data/...` vs `/data/user/0/...` variant of it
- [ ] settings and high scores from the first install are **preserved** — the
      asset tree is rebuilt, preferences are not

### REL-003 — Windows non-blocking receive

Fixed by construction in v2.4.29, still never run on Windows hardware.

**To check:** run the Windows installer, join a network game, confirm no stall
waiting for the server.

---

## 5. Also worth a quick look

- **BUG-030** — a `nan` speed multiplier no longer breaks all movement. Clamp
  logic proven in isolation; the in-game path is inferred.
- **BUG-029** — an out-of-range saved window height is now clamped.
- **BUG-050** — the lobby `free:` count was verified live against a running
  server and now agrees with the listed open players.
- **BUG-008** — creator-led room closure was verified under a sanitizer with no
  diagnostics.
- **BUG-048** — needs no manual check. The browser reload is automated:
  `tools/test-wasm-persistence.mjs` drives the packaged bundle in headless
  Chrome, reloads it, and asserts settings and high scores survived. It runs in
  CI on every push.

## 6. Two-browser WASM network playtest (async networking rearchitecture)

See `docs/ASYNC_NETWORKING_HANDOFF.md` for the full background. Nothing in
this repo can spin up `BubbleGame`/`MainMenu` headlessly and drive a real
multi-round network match, so the async connect/lobby/game-start/level-sync
rewrite (stages 1–3 of that doc) has no automated two-client coverage — this
is the recipe for the hands-on check that stands in for it.

**Setup:**

1. Build the WASM client (README → "Building WASM locally").
2. Build an ASan/UBSan-instrumented server (see this repo's `CLAUDE.md` →
   Tests, the `cmake -B build-asan …` block) — running the server under the
   sanitizer is what makes a silent memory-safety regression show up as a
   loud one instead of a random hang or corrupted game state.
3. Serve the WASM build: `python3 tools/serve-wasm.py` (prints the URL; COOP/
   COEP headers are required for audio).
4. Start the server in the foreground so its log is visible, e.g.:
   `ASAN_OPTIONS=detect_leaks=0:fast_unwind_on_malloc=0 UBSAN_OPTIONS=print_stacktrace=1 ./build-asan/fb-server -d -p 1511`
   (`detect_leaks=0` is required on macOS/Darwin — leak detection isn't
   supported there at all and aborts every run under `detect_leaks=1`; use
   `1` on Linux). WASM talks WebSocket, not raw TCP, so it needs a
   `websockify`-style proxy bridging to this port — see the README's WASM
   networking notes if one isn't already part of your local setup.
5. Open two browser tabs at the served WASM URL — ideally on two separate
   devices/networks; two tabs on one machine is a lesser but still useful
   substitute (this is what the 2026-09-08 run below used).

**Play through:**

6. Tab A: pick a nickname, create a room. Tab B: pick a different nickname,
   join it.
7. Start the game as the room leader; play at least **two full rounds** —
   round 2+ is the specific case stages 3b/3c exist to fix (previously
   stalled a full 5s every round after the first).
8. Trade fire/malus attacks in both directions so the relay path is
   exercised both ways, not just leader→joiner.
9. Optionally: back out to the lobby mid-connect elsewhere in the menu and
   cancel with ESC/tap while a "Connecting…" indicator is showing (stage 2's
   cancel path) — the playtest below didn't cover this.

**Checks:**

- [ ] Both clients reach the lobby with no visible render-loop freeze (no
      browser "page unresponsive" warning, input keeps working throughout)
- [ ] NICK/CREATE/JOIN resolve (including a deliberate nickname collision
      between the two tabs) without getting stuck in a pending state
- [ ] Round 1 starts promptly
- [ ] Round 2 (and later) starts promptly — no multi-second stall before the
      new round's board appears
- [ ] Fire/malus actions from each client visibly land on the other client's
      board
- [ ] The server's own log has no `AddressSanitizer` or `runtime error:`
      lines for the whole session
- [ ] Cancelling an in-flight connect (ESC or tap, per CLAUDE.md's
      input-parity rule) actually stops it rather than leaving a zombie
      attempt

**Already run once, informally** — 2026-09-08, two tabs on one machine, per
`docs/ASYNC_NETWORKING_HANDOFF.md`'s "live two-browser WASM playtest"
section: lobby entry, NICK/CREATE/JOIN, and several full rounds all
confirmed with no stalls and a clean sanitizer log. Not covered by that run:
the ESC/tap cancel-mid-connect check above, and testing across genuinely
separate devices/networks rather than two tabs on one machine — both still
worth doing before tagging a release.
