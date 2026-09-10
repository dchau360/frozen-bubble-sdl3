#!/usr/bin/env python3
"""
Discord join-alert and match-result relay for fb-server.

fb-server itself has no TLS stack and runs a single-threaded blocking event
loop, so it cannot POST to a Discord webhook without risking a stall for
every connected player. Instead it fires a best-effort UDP datagram at this
process, which owns the actual HTTPS call -- and the webhook URL, which
never needs to reach fb-server at all.

Two datagram kinds, both `|`-delimited with the type name first:

    JOIN|<nick>|<ip>|<geoloc>|<servername>

One per player arriving on the server (their first accepted NICK -- not a
game-room join, which is a later and much less actionable moment). nick and
ip are always present. geoloc is the player's self-reported "lat:lon" (see
the client's DetectGeoLocation()/GEOLOC command) or an empty string -- in
practice always empty, since the client sends GEOLOC after NICK and the
lookup behind it can take up to ~16s. The field stays in the format anyway:
it costs nothing, and this end discards it regardless.

    RESULT|<game_id>|<mode>|<winner>|<roster>|<servername>

One per round-end, sniffed from the 'F' opcode server-side (game.c). game_id
is an opaque int identifying the room, monotonically assigned by fb-server at
CREATE and stable for the room's whole lifetime (see g->game_id's comment in
server/game.c) -- used here only to group every round from the same room
into one Discord thread (see "Round-result threading" below), never
displayed. mode is fb-server's raw 0-3 GAMEMODE value (src/gamemode.h);
winner is the reporting client's win claim, empty for a draw, and -- unlike
everything else in either datagram -- is not validated against is_nick_ok()
at all, so it gets no more trust here than servername does; roster is every
current player's nick, comma-joined, each one already
is_nick_ok()-constrained when its owner connected or joined.

servername is everything remaining after the last "|" in both formats (it
can itself contain spaces or, in principle, "|") -- see build_message() and
build_result_message() for what actually reaches Discord from each.

ip and geoloc are parsed but never posted to Discord -- see build_message(),
which explains why the location came out. Both ride along in the datagram
because fb-server already has them for free and it costs nothing to send,
not because this relay does anything with them.

Without DISCORD_WEBHOOK_URL configured this runs in stub mode: it logs what
it *would* have posted and returns. That is the intended state until an
operator supplies a webhook -- the whole pipeline stays exercisable without
one.

Round-result threading (optional, on top of the above): with
DISCORD_BOT_TOKEN and DISCORD_CHANNEL_ID both set, every RESULT for the same
room posts as a reply in one Discord thread instead of a fresh top-level
message per round -- the first result for a game_id opens the thread, every
later one for that game_id posts into it. This needs a bot, not a plain
webhook: Discord's webhook API can only create a new thread when the webhook
is attached to a forum/media channel, and this relay is designed to share an
ordinary text channel with join alerts. JOIN alerts are unaffected either
way -- they always post as plain top-level messages via the webhook, since a
join is not part of any one room's result history. Leaving either bot
variable unset (the default) keeps RESULT alerts posting flat via the
webhook too, exactly as before this feature existed.

Environment:
    DISCORD_RELAY_BIND    host:port to listen on (default 0.0.0.0:9100)
    DISCORD_WEBHOOK_URL    Discord's channel webhook URL; unset = stub mode
    DISCORD_SERVER_NAME    overrides the server name shown in Discord,
                            independent of fb-server's own -n (which caps at
                            12 chars); unset = use whatever fb-server sent
    DISCORD_BOT_TOKEN      enables round-result threading (see above); needs
                            DISCORD_CHANNEL_ID too, and Send Messages +
                            Create Public Threads + Send Messages in Threads
                            in that channel
    DISCORD_CHANNEL_ID     the text channel id round-result threads are
                            created in; ignored unless DISCORD_BOT_TOKEN is
                            also set
"""

import asyncio
import json
import logging
import os
import sys
import urllib.request

LOG_FORMAT = "%(asctime)s %(levelname)s %(message)s"
logging.basicConfig(level=logging.INFO, format=LOG_FORMAT, stream=sys.stdout)
log = logging.getLogger("discord-relay")

MAX_DATAGRAM = 2048
# Discord returns 400 past this length; truncate rather than let a long
# custom server name (or, in principle, a hostile one) get the message
# rejected outright.
MAX_DISCORD_CONTENT = 1900
# Per-field cap for the two names that get interpolated into a message. Well
# past any honest server name, and short enough that one field cannot crowd
# out the rest of the line.
MAX_DISPLAY = 64

# Overrides the server name shown in every Discord message, independent of
# what fb-server sent on the wire. fb-server's own -n flag caps at 12
# characters and *charset* too ([a-zA-Z0-9.-]) -- a limit that constrains
# what's advertised to players in the lobby and the public server list, not
# anything on this side of the pipe. An operator whose real name doesn't fit
# there (a full domain, say) sets what should actually appear in Discord
# here instead; -n stays short for the in-game UI, this can be anything.
# Empty (the default) means use whatever fb-server sent, unchanged.
DISCORD_SERVER_NAME = os.environ.get("DISCORD_SERVER_NAME", "").strip()

# Round-result threading (see the module docstring). Both must be set for
# any of it to activate -- see _bot_mode_enabled().
DISCORD_BOT_TOKEN = os.environ.get("DISCORD_BOT_TOKEN", "").strip()
DISCORD_CHANNEL_ID = os.environ.get("DISCORD_CHANNEL_ID", "").strip()

DISCORD_API = "https://discord.com/api/v10"

# game_id (see the RESULT wire format) -> the Discord thread (itself just a
# channel id) that room's results have been posted into so far. Populated
# lazily on a room's first result; nothing ever proactively evicts an entry
# -- see _post_result_via_bot_sync()'s own comment for what happens when a
# cached thread turns out to be gone by the time it's needed again. Purely
# in-memory: a relay restart forgets every room currently in progress, and
# each of their next results simply opens a fresh thread, which is no worse
# than what every room got before this feature existed.
_room_threads = {}


def _bot_mode_enabled():
    return bool(DISCORD_BOT_TOKEN and DISCORD_CHANNEL_ID)

# Discord markdown metacharacters, escaped rather than stripped so an honest
# name containing one still reads correctly. "[" and "(" are the pair that
# actually matter: without them a display string can inject a [label](url)
# link that renders as innocent text pointing anywhere.
_MD_ESCAPE = str.maketrans({c: "\\" + c for c in "\\*_~`|>[]()#-"})


def _sanitize_display(text):
    """Neutralise a display string that came from outside this process.

    Both fields this is applied to arrive over the wire: nick from a player
    (fb-server's is_nick_ok() already limits it to [A-Za-z0-9_-]{1,10}, but
    this relay is not entitled to assume the sender is an unmodified
    fb-server) and servername from whoever runs that server, which
    net_servername() does not constrain at all. Once one webhook is shared
    with other operators -- see README.md -- servername is somebody else's
    free-form config arriving in your channel.

    Control characters are dropped outright rather than escaped: a newline
    would let a single alert forge a second, arbitrary-looking one.
    """
    text = "".join(ch for ch in text if ch.isprintable())
    # Truncate before escaping, or a cut can land mid-escape and leave a
    # trailing backslash that eats the character after it.
    return text[:MAX_DISPLAY].translate(_MD_ESCAPE)


_GAME_MODE_NAMES = {0: "Classic", 1: "Clear", 2: "Race", 3: "Timed"}


def build_result_message(mode, winner, roster_csv, servername):
    """Round over: who won (or a draw), what mode, who was playing, where.

    mode is the raw 0-3 value from fb-server's SETOPTIONS GAMEMODE field
    (src/gamemode.h). A value this relay doesn't recognize -- a future mode
    it predates, or simply garbage -- degrades to no mode label rather than
    crashing or guessing.

    winner is exactly what the reporting client's 'F' payload said and is
    NOT validated against is_nick_ok on the server side at all (unlike nick
    in build_message, which at least started out constrained) -- a modified
    client could claim a win it did not earn. Empty means a draw
    (fb-server's own bare "F"). roster_csv, by contrast, is fb-server's own
    bookkeeping (build_roster_csv() in game.c): every name in it already
    passed is_nick_ok() when its owner connected or joined, so it could not
    have been forged the same way -- only sanitized here as routine defense
    in depth, the same as every other field arriving over this datagram.
    """
    mode_label = _GAME_MODE_NAMES.get(mode, "")
    mode_label = f" ({mode_label})" if mode_label else ""
    servername = _sanitize_display(servername)
    roster = ", ".join(_sanitize_display(n) for n in roster_csv.split(",") if n)
    if winner:
        headline = f"🏆 **{_sanitize_display(winner)}** won"
    else:
        headline = "🤝 Draw"
    content = f"{headline}{mode_label} on **{servername}** — {roster}"
    return content[:MAX_DISCORD_CONTENT]


def build_message(nick, servername):
    """The whole message: who joined, and where. Nothing else.

    Neither the joining player's IP nor their location appears here, and both
    arrive in the datagram -- see handle_datagram(), which drops them.

    The IP was never posted. The location was, as a Google Maps link, back
    when this fed a private operators-only channel. It came out when the game
    itself started advertising the channel to players ("Join our Discord" on
    the NET GAME list and in the lobby): a channel the game recruits players
    into is one where every joining player's approximate location would be on
    show to everyone who took up the offer, which is not a thing a player
    agreed to by letting the client geolocate them for the lobby's world map.
    The map still works exactly as before -- that data simply stops here.
    """
    nick = _sanitize_display(nick)
    servername = _sanitize_display(servername)
    content = f"🔔 **{nick}** joined **{servername}**"
    return content[:MAX_DISCORD_CONTENT]


def _post_sync(webhook_url, content):
    """Blocking HTTPS POST -- run this off the event loop thread (see
    handle_datagram) so one slow request can never delay the next datagram
    from being picked up off the socket."""
    body = json.dumps({
        "content": content,
        # Nothing this relay posts may ping anyone, ever. Belt to
        # _sanitize_display's braces: that stops a name *rendering* as a
        # mention, this stops one being delivered as a notification even if
        # some future edit reintroduces an unescaped path. A channel that
        # can be @everyone'd by naming a server is one nobody leaves on.
        "allowed_mentions": {"parse": []},
    }).encode("utf-8")
    req = urllib.request.Request(
        webhook_url, data=body, method="POST",
        headers={
            "Content-Type": "application/json",
            # Discord's edge WAF rejects urllib's default User-Agent
            # ("Python-urllib/3.x") outright with a 403 -- on every request,
            # not just POST, and with no body explaining why. Confirmed by
            # hand: a GET to the same URL succeeds with curl's UA and 403s
            # with urllib's. A stock deploy with no other overrides hits this
            # on the very first join, unconditionally.
            "User-Agent": "fb-server-discord-relay/1.0 "
                          "(+https://github.com/dchau360/frozen-bubble-sdl3)",
        },
    )
    # Discord's webhook rate limit (per-webhook, a handful of requests per
    # few seconds) means a burst of joins can 429. That is treated the same
    # as any other delivery failure below: logged and dropped, never
    # retried -- queuing would just let a slow patch of joins pile up
    # indefinitely, and the alert has already lost most of its value by the
    # time a retry would land anyway.
    with urllib.request.urlopen(req, timeout=10) as resp:
        resp.read()


def _bot_request_sync(method, path, body):
    """Blocking Discord bot REST call, authenticated with DISCORD_BOT_TOKEN
    rather than a webhook token -- same off-thread contract as _post_sync,
    and the same User-Agent requirement (see its comment). Returns the
    parsed JSON response body, or {} for a response with no body (Discord's
    204s, e.g. on some deletes -- not used by this relay today, but cheaper
    to handle once here than to special-case every call site later)."""
    req = urllib.request.Request(
        f"{DISCORD_API}{path}", data=json.dumps(body).encode("utf-8"), method=method,
        headers={
            "Content-Type": "application/json",
            "Authorization": f"Bot {DISCORD_BOT_TOKEN}",
            "User-Agent": "fb-server-discord-relay/1.0 "
                          "(+https://github.com/dchau360/frozen-bubble-sdl3)",
        },
    )
    with urllib.request.urlopen(req, timeout=10) as resp:
        raw = resp.read()
    return json.loads(raw) if raw else {}


def _post_result_via_bot_sync(game_id, content, room_label):
    """Post one round-result as a bot message, threading it under the same
    room's earlier rounds when there is one -- see "Round-result threading"
    in the module docstring for why this needs a bot token and not just the
    webhook everything else here uses.

    The first result for a game_id creates the thread: post an ordinary
    message to DISCORD_CHANNEL_ID, then convert that message into a public
    thread named after room_label via Discord's Start Thread From Message
    endpoint. Every later result for the same game_id posts straight into
    the cached thread id instead -- a thread is itself just a channel id as
    far as message-create is concerned, so no separate "which thread"
    parameter is needed the way the webhook-only ?thread_id= mechanism would
    require.

    If posting into a *cached* thread fails -- the channel was deleted, a
    permission was revoked, anything -- the cache entry is dropped and this
    re-raises, so the caller's own log-and-drop still applies to this
    round's alert, but the next round for the same room tries a fresh thread
    instead of repeating the same failure forever.
    """
    thread_id = _room_threads.get(game_id)
    if thread_id is not None:
        try:
            _bot_request_sync(
                "POST", f"/channels/{thread_id}/messages",
                {"content": content, "allowed_mentions": {"parse": []}})
            return
        except Exception:
            _room_threads.pop(game_id, None)
            raise

    message = _bot_request_sync(
        "POST", f"/channels/{DISCORD_CHANNEL_ID}/messages",
        {"content": content, "allowed_mentions": {"parse": []}})
    thread = _bot_request_sync(
        "POST", f"/channels/{DISCORD_CHANNEL_ID}/messages/{message['id']}/threads",
        # Discord caps a thread name at 100 characters; room_label already
        # went through _sanitize_display's own (much shorter) MAX_DISPLAY
        # cap at the call site, so this is just a hard backstop.
        {"name": room_label[:100]})
    _room_threads[game_id] = thread["id"]


async def handle_datagram(data, webhook_url):
    try:
        text = data.decode("utf-8", errors="replace").strip()
    except Exception:
        log.warning("undecodable datagram, dropped")
        return

    # Split off the type name first and interpret the rest per kind, rather
    # than one shared maxsplit for both -- JOIN and RESULT don't have the
    # same field count, and a single global limit would either truncate
    # RESULT's fields or, if raised to fit RESULT, start cutting into JOIN's
    # trailing servername on the rare server whose name itself contains '|'.
    kind, _, rest = text.partition("|")

    if kind == "JOIN":
        parts = rest.split("|", 3)
        if len(parts) != 4:
            log.warning("malformed datagram, dropped: %r", text[:120])
            return
        nick, ip, geoloc, servername = parts
        # Both received, neither posted -- see build_message(). They stay in
        # the wire format because fb-server already has them and a datagram
        # costs the same either way; dropping them here rather than at the
        # sender keeps the decision in one reviewable place, and leaves an
        # operator who forks this file for a private channel something to
        # work from.
        del ip, geoloc
        if DISCORD_SERVER_NAME:
            servername = DISCORD_SERVER_NAME
        content = build_message(nick, servername)
        log_label = f"join alert for {nick}"
        game_id = None

    elif kind == "RESULT":
        parts = rest.split("|", 4)
        if len(parts) != 5:
            log.warning("malformed datagram, dropped: %r", text[:120])
            return
        game_id_s, mode_s, winner, roster_csv, servername = parts
        try:
            game_id = int(game_id_s)
        except ValueError:
            # Never seen from an unmodified fb-server -- game_id is always a
            # plain int it assigned itself. Post the result anyway rather
            # than drop a real round-end over a threading key alone; it
            # just cannot be grouped into a thread without one.
            game_id = None
        try:
            mode = int(mode_s)
        except ValueError:
            mode = -1  # unrecognized -- build_result_message labels nothing
        if DISCORD_SERVER_NAME:
            servername = DISCORD_SERVER_NAME
        content = build_result_message(mode, winner, roster_csv, servername)
        log_label = f"result for {winner or 'a draw'}"
        # Best-effort label for the thread's title only, not the message
        # itself -- see build_roster_csv()/players_nick[0] in game.c for why
        # this can drift from the room's original CREATE name over a long
        # room's life, and why that is an acceptable, cosmetic-only cost.
        room_label = _sanitize_display(roster_csv.split(",")[0] if roster_csv else servername)

    else:
        log.warning("malformed datagram, dropped: %r", text[:120])
        return

    if kind == "RESULT" and game_id is not None and _bot_mode_enabled():
        try:
            await asyncio.to_thread(_post_result_via_bot_sync, game_id, content, room_label)
            log.info("posted %s (threaded)", log_label)
        except Exception as exc:
            # Same best-effort contract as the webhook path below: the game
            # server has already moved on and cannot be told about it.
            log.error("bot post to Discord failed: %s", exc)
        return

    if not webhook_url:
        log.info("[stub] would post: %s", content)
        return

    try:
        await asyncio.to_thread(_post_sync, webhook_url, content)
        log.info("posted %s", log_label)
    except Exception as exc:
        # A delivery failure must never take the relay down -- the game
        # server has already moved on and cannot be told about it either way.
        log.error("post to Discord failed: %s", exc)


class _RelayProtocol(asyncio.DatagramProtocol):
    """Fans each datagram out to its own task rather than awaiting it inline,
    so one slow POST can never delay the next datagram from being picked up
    off the socket."""

    def __init__(self, webhook_url):
        self.webhook_url = webhook_url

    def datagram_received(self, data, addr):
        log.debug("datagram from %s", addr)
        asyncio.ensure_future(handle_datagram(data, self.webhook_url))


async def async_main():
    bind = os.environ.get("DISCORD_RELAY_BIND", "0.0.0.0:9100")
    if ":" not in bind:
        log.error("DISCORD_RELAY_BIND must be host:port, got %r", bind)
        return 1
    host, _, port_s = bind.rpartition(":")
    try:
        port = int(port_s)
    except ValueError:
        log.error("DISCORD_RELAY_BIND has a non-numeric port: %r", bind)
        return 1

    webhook_url = os.environ.get("DISCORD_WEBHOOK_URL", "")
    if webhook_url:
        log.info("Discord delivery configured")
    else:
        log.info("DISCORD_WEBHOOK_URL not set -- join alerts run in stub mode")

    if _bot_mode_enabled():
        log.info("round-result alerts will thread per room in channel %s", DISCORD_CHANNEL_ID)
    elif DISCORD_BOT_TOKEN or DISCORD_CHANNEL_ID:
        log.warning(
            "DISCORD_BOT_TOKEN and DISCORD_CHANNEL_ID must both be set to "
            "thread round-result alerts; only one is -- falling back to "
            "flat result alerts via DISCORD_WEBHOOK_URL")

    loop = asyncio.get_running_loop()
    transport, _ = await loop.create_datagram_endpoint(
        lambda: _RelayProtocol(webhook_url),
        local_addr=(host, port),
    )
    log.info("discord-relay listening on %s:%d", host, port)

    try:
        await asyncio.Event().wait()   # run until killed
    except asyncio.CancelledError:
        pass
    finally:
        transport.close()
    return 0


def main():
    try:
        return asyncio.run(async_main())
    except KeyboardInterrupt:
        log.info("shutting down")
        return 0


if __name__ == "__main__":
    sys.exit(main())
