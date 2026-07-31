# Fix instructions: regressions found in the IMP-### batch work

A review of the three commits landed for the IMP-### improvement pass
(`95ceee2b`, `53eccc5a`, `4c3fb45c`) found real regressions, not just style
issues. This doc gives precise, self-contained fix instructions for each.
Read the "why" in each section before changing code — these are subtle
enough that a surface-level patch can look right and still be wrong.

None of this needs re-litigating what IMP-### item it maps to; just fix the
bugs below. Build and `ctest --test-dir build --output-on-failure` must stay
green throughout, and each fix should be its own commit.

---

## Fix 1 (critical) — WebSocket handshake fragmented across reads breaks the upgrade

**File:** `server/net.c`, `handle_incoming_data_generic()`'s `ws_pending`
branch (currently around line 284-325); also `server/ws.c`'s
`ws_try_upgrade_from_data()` and its declaration in `server/ws.h`.

**The bug:** `ws_try_upgrade_from_data()` requires the full `\r\n\r\n`
header terminator to be present in the data it's given. The caller does
exactly one `recv()` per newly-accepted connection and calls this function
once; if that single `recv()` doesn't happen to contain the whole HTTP
header (completely legal — TCP is a stream, and this specifically can
happen through the nginx WSS-termination path this project's own
`docker-compose.yml` production deployment uses), the function returns 0,
and the caller immediately and permanently classifies the connection as
plain native TCP. There is no retry: `ws_pending[fd]` is already cleared to
0 before this check runs.

**Confirmed reproduction:** split a real WebSocket upgrade request into two
`send()` calls 300ms apart (forcing two separate server-side reads). The
server never returns `HTTP/1.1 101`; instead it responds with the plain-text
`FB/1.3 PUSH: SERVER_READY ...` greeting, then logs
`[fd] GET / HTTP/1.1 -> MISSING_FB_PROTOCOL_TAG` and closes the connection
when it tries to parse the partial HTTP request line as a game command.

**The fix — distinguish "not WebSocket" from "not enough data yet":**

1. In `ws_try_upgrade_from_data()` (`server/ws.c`), change the incomplete-header
   case to return a distinct sentinel instead of `0`. Suggested contract:
   - Return `>0`: bytes consumed, upgrade completed.
   - Return `0`: definitely not a WebSocket request (e.g. doesn't start with
     `"GET "` — at least 4 bytes were available and they don't match).
   - Return `-1`: incomplete — the data seen so far is consistent with a
     WebSocket handshake in progress (starts with `"GET "` or fewer than 4
     bytes have arrived yet) but the `\r\n\r\n` terminator hasn't arrived.
     Update the doc comment in `server/ws.h` to describe this three-way
     contract.

2. In `handle_incoming_data_generic()`'s `ws_pending` branch, when the result
   is "incomplete" (new `-1`), do **not** treat the connection as
   classified. Keep accumulating: append the newly-read bytes to whatever
   was read before (reuse `incoming_data_buffers[fd]` as scratch storage
   for the still-unclassified data — it's not used for anything else before
   classification completes) and leave `ws_pending[fd]` set so the next
   read for this fd goes through the same branch again with the fuller
   buffer.

3. Bound this: a real WebSocket handshake header is always well under a few
   KB. If the accumulated pending data exceeds some reasonable cap (e.g.
   4096 bytes) without completing, that's not a legitimate stalled
   handshake — terminate the connection (`conn_terminated`) rather than
   buffering forever.

4. When the result is "definitely not WebSocket" (`0`), keep the existing
   behavior: classify as plain TCP immediately, send the greeting as plain
   text, and hand any already-read bytes to the normal path — no need to
   wait, since a plain native client never starts with `"GET "`.

**Verify:** re-run the reproduction (split the handshake into two sends with
a delay) and confirm the server now returns `HTTP/1.1 101 Switching
Protocols`. Also verify the existing single-read case (handshake arrives
whole, the common case) still upgrades correctly — don't regress the
already-working path.

---

## Fix 2 (critical) — data pipelined right after the handshake gets corrupted

**File:** `server/net.c`, same `ws_pending` branch, the "WebSocket upgrade
completed" success path (currently around line 296-304).

**The bug:** when a client's first `recv()` contains the HTTP upgrade
request *plus* extra bytes beyond it (the client pipelines its first WS
frame instead of waiting for the 101 response — or simply because two
writes coalesced into one read, which can happen even for well-behaved
clients under some network/timing conditions), those extra bytes are the
start of the client's first **WebSocket-framed** message: still masked,
still needing `ws_decode_inplace()`. The current code instead
`memcpy`s them straight into `incoming_data_buffers[fd]` — the buffer
whose own comment two lines above (line ~333) states holds *only
already-decoded game bytes*, specifically so `ws_decode_inplace` is never
called on non-WS-encoded text and vice versa. This breaks that invariant:
those bytes never get decoded, and on the next read they get handed
straight to the plain-text command parser as if they were already-decoded
game protocol text.

**Confirmed reproduction:** sent a valid WS handshake immediately followed
(same `sendall()` call) by `b"MARKERMARKERMARKER\n"`. Server log:
`[fd] MARKERMARKERMARKER -> MISSING_FB_PROTOCOL_TAG` then
`Closing connection: process_msg said to shutdown this connection` — the
raw marker text reached the plain-text parser verbatim, proving it was
never decoded.

**The fix:** route the leftover bytes to `ws_raw_frame_buf[fd]` /
`ws_raw_frame_len[fd]` instead of `incoming_data_buffers[fd]` — that's
exactly the buffer pair the rest of the codebase already uses for "raw
partial WS frame bytes waiting for the rest of the frame" (see the existing
logic a few lines below, around line 341-348, which prepends
`ws_raw_frame_buf[fd]` before the next `recv()` and only then calls
`ws_decode_inplace`). Concretely, in the success branch:

```c
if (consumed > 0) {
        send_line_log_push(fd, get_greets_msg());
        if (peeklen > consumed) {
                memcpy(ws_raw_frame_buf[fd], peekbuf + consumed,
                       (size_t)(peeklen - consumed));
                ws_raw_frame_len[fd] = (int)(peeklen - consumed);
        }
        return;
}
```

Do **not** touch `incoming_data_buffers_count[fd]` in this branch — leave it
at 0, since there is no already-decoded data yet.

**Verify:** send a handshake request immediately followed (same write) by a
valid masked WS text frame carrying a real command (e.g. `TALK hello\n`).
Confirm the server's debug log shows the command being processed normally
(not `MISSING_FB_PROTOCOL_TAG`), and that a plain marker string sent the
same way is *not* silently accepted as a command either (it should now be
rejected by `ws_decode_inplace` as a protocol error, or simply not parse as
valid game text — either is fine, as long as it isn't routed around the
decoder).

---

## Fix 3 (critical) — plasma transition effect crashes when plasma.raw is missing

**File:** `src/shaderstuff.cpp` — `plasma_init()` (already fixed to degrade
gracefully) and `plasma_effect()` (not fixed to match).

**The bug:** `plasma_init()` was correctly changed to warn-and-return
instead of `exit(1)`/`abort()` when `plasma.raw` is missing, unreadable, or
allocation fails. But it returns *before* allocating `plasma2` and
`plasma3` too (they're allocated later in the same function, gated on the
file read having already succeeded) — so **every failure path leaves
`plasma`, `plasma2`, and `plasma3` all null**, not just `plasma`.
`plasma_effect()` was never updated to check for this: it randomly picks
one of three `plasma_type` values (1, 2, or 3) via its own internal
`rand_(3)` call, and *all three* branches dereference one of these
now-possibly-null buffers unconditionally — type 1 reads `plasma[...]`,
type 2 reads `plasma2[...]`, and type 3 *writes* to `plasma3[...]` during
its brightness precompute, which runs unconditionally near the top of the
function whenever `plasma_type == 3`. So this isn't a 1-in-3 chance of
crashing — every call to `plasma_effect()` after a failed `plasma_init()`
crashes, regardless of which type gets picked.

**Confirmed reproduction:** compiled a small harness linking the real
`shaderstuff.cpp`, called `plasma_init()` with a bogus data directory
(forcing the "file missing" path), then called `plasma_effect()` — it
segfaulted on the very first call.

**The fix:** add a single guard at the very top of `plasma_effect()`:

```c
void plasma_effect(SDL_Surface *s, SDL_Surface *img, SDL_Renderer *rend, SDL_Texture *tex)
{
    if (!plasma) return;  // plasma_init() failed or hasn't run; nothing to animate
    ...
```

This is sufficient because `plasma`, `plasma2`, and `plasma3` are only ever
all-null or all-non-null together (they're allocated sequentially in one
function, gated on the same success path) — checking `plasma` alone
correctly implies the other two are also safe to use.

**Known tradeoff, and it's fine:** when this guard fires, the transition
that would have played the plasma effect simply doesn't animate that frame
(a jump cut instead of a fancy transition) rather than falling back to a
different effect. That's an acceptable degrade — don't try to make
`effect()` pick a different transition type instead; that's a bigger,
judgment-heavy change to the effect-selection distribution and out of scope
for this fix.

**Verify:** re-run (or re-create) the harness: `plasma_init()` with a bad
path, then call `plasma_effect()` several times in a loop — it should
return immediately every time with no crash. Also confirm the normal case
(real `plasma.raw` present) is unaffected — `plasma` is non-null and the
function behaves exactly as before.

---

## Fix 4 — CREATE room-cap "clamp" doesn't actually clamp

**File:** `server/game.c`, the `CREATE` handler (currently around line
802-810).

**The bug:**

```c
if (mp >= 2 && mp <= MAX_PLAYERS_PER_GAME)
        max_players = mp;
else if (mp > MAX_PLAYERS_PER_GAME)
        l2(OUTPUT_TYPE_INFO, "CREATE room cap %d clamped to %d", mp, MAX_PLAYERS_PER_GAME);
else
        l1(OUTPUT_TYPE_INFO, "CREATE room cap %d ignored (minimum 2)", mp);
```

The `else if` branch logs `"clamped to %d"` but never actually sets
`max_players = MAX_PLAYERS_PER_GAME`. `max_players` stays at its
initial value of 5 (the legacy default) in that branch — the room is
still silently created with a 5-seat cap while the log claims a clamp to
(e.g.) 20 that didn't happen. This is a correctness bug in the fix itself:
the diagnostic doesn't match the behavior.

**The fix:** make the log true — actually clamp:

```c
if (mp >= 2 && mp <= MAX_PLAYERS_PER_GAME)
        max_players = mp;
else if (mp > MAX_PLAYERS_PER_GAME) {
        max_players = MAX_PLAYERS_PER_GAME;
        l2(OUTPUT_TYPE_INFO, "CREATE room cap %d clamped to %d", mp, MAX_PLAYERS_PER_GAME);
} else
        l1(OUTPUT_TYPE_INFO, "CREATE room cap %d ignored (minimum 2)", mp);
```

**Verify:** `tools/server_tests/test_room_caps.py` already exercises
`CREATE` with various sizes — run it, and add (or confirm it already
covers) a case sending `CREATE cap21 21` and asserting the room's
advertised cap is `MAX_PLAYERS_PER_GAME` (20), not 5.

---

## Fix 5 — finish IMP-012 (it was skipped)

**File:** primarily `src/mainmenu.h`, `src/mainmenu.cpp`,
`src/frozenbubble.h`, and wherever `controllerInputs[5]` and the gamepad
dispatch live.

The prior pass's commit for this item only noted that
`FrozenBubble::menuText` was "already removed in a prior commit" (true, but
that happened years before this audit and is unrelated) and stopped there.
The actual scope of IMP-012 was never done. Still present and still
confirmed unreachable (verified by `grep -rn` across `src/` immediately
before writing this doc):

- The two-player panel (`showing2PPanel` in `src/mainmenu.h`) — check
  whether it's ever set `true` anywhere before removing the panel itself;
  if it's genuinely dead, remove the panel, its render/input handling, and
  the flag.
- The network setup panel, if it has the same reachability gap (check
  first — don't assume; the ledger for this whole audit specifically warns
  that some findings describe state that's since changed, so re-verify
  reachability against current `main` before deleting anything, exactly as
  the original handoff doc said).
- The `editor` button and the `LevelEditor`/`Netplay` `FrozenBubble` states
  (`src/frozenbubble.h`), if unreachable.
- `selectedGameIndex` (`src/mainmenu.h:195`) if it has no live reader/writer
  pair.
- `controllerInputs[5]` and the "diverted gamepad branch" the original
  finding described — locate via `grep -rn "controllerInputs"`.
- The throwing static settings initializer the finding named — locate via
  the audit's own text in `FINDINGS.md` (search `IMP-012`) for the exact
  description if the symbol name isn't obvious from the code.

**Before deleting anything:** `grep -rn` for every symbol first and confirm
zero live callers/readers against the *current* tree — do not rely on the
audit's original claim alone, since it was written earlier in this
project's life and at least one other finding in this same audit (BUG-045's
neighbor, `menuText`) turned out to already be stale.

**Verify:** full build (`cmake --build build --parallel`) and
`ctest --test-dir build --output-on-failure` after each deletion, not just
at the end — if something turns out not to be dead, you want to know
immediately which specific removal broke it.

---

## Summary checklist

- [ ] Fix 1: WS fragmented-header handling (server/net.c, server/ws.c, server/ws.h)
- [ ] Fix 2: WS post-handshake leftover bytes routed to `ws_raw_frame_buf` (server/net.c)
- [ ] Fix 3: `plasma_effect()` null guard (src/shaderstuff.cpp)
- [ ] Fix 4: CREATE room-cap actually clamps (server/game.c)
- [ ] Fix 5: finish IMP-012's dead-code removal (src/mainmenu.h, src/mainmenu.cpp, src/frozenbubble.h, etc.)

Build and the existing `ctest` suite passing is necessary but **not
sufficient** proof for Fixes 1-3 — none of them are covered by the existing
test suite. For Fixes 1 and 2, re-run the manual reproduction described in
each section (split-handshake, pipelined-marker) against the rebuilt
server and confirm the described-as-fixed behavior actually shows up. For
Fix 3, re-run the plasma harness (or write an equivalent one) and confirm
no crash across at least a few dozen calls.
