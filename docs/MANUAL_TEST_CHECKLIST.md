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
  setup with 2–4 players, navigate to **Victories limit**, and use Left, Right,
  and confirm/Enter through the available values and both wrap points. Repeat
  with a controller and confirm the selected row and value remain visibly in
  sync with each input.
- [ ] **Classic continuation and Clear Mode win:** empty a local multiplayer
  board in Classic mode and confirm the round continues. Repeat in Clear Mode
  and confirm exactly one win, the correct winner presentation, and the
  expected win SFX.
- [ ] **Simultaneous local loss draw:** arrange for the final surviving local
  players to cross the danger line in the same frame. Confirm the round is
  presented as a draw and neither player receives a transient or lasting win.
- [ ] **Team Mode survivor:** eliminate players until exactly one team remains.
  Confirm the surviving team receives the intended winner presentation and
  teammate credit without ending merely because one board was cleared.
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
