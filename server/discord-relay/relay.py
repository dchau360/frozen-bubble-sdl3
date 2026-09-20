#!/usr/bin/env python3
"""
Discord join-alert and match-result relay for fb-server.

fb-server itself has no TLS stack and runs a single-threaded blocking event
loop, so it cannot POST to a Discord webhook without risking a stall for
every connected player. Instead it fires a best-effort UDP datagram at this
process, which owns the actual HTTPS call -- and the webhook URL, which
never needs to reach fb-server at all.

Two datagram kinds, both `|`-delimited with the type name first:

    JOIN|<nick>|<ip>|<geoloc>|<platform>|<country>|<servername>

One per player arriving on the server (their first accepted NICK -- not a
game-room join, which is a later and much less actionable moment). nick and
ip are always present. geoloc is the player's self-reported "lat:lon" (see
the client's DetectGeoLocation()/GEOLOC command) or an empty string -- in
practice always empty, since the client sends GEOLOC after NICK and the
lookup behind it can take up to ~16s. The field stays in the format anyway:
it costs nothing, and this end discards it regardless. platform is a single
char naming the client's OS (W/M/L/A/I/B -- see _PLATFORM_BADGES), or empty
for a client too old to send the PLATFORM command; unlike geoloc it *is*
posted, for the reasons in build_message(). country is an ISO 3166-1 alpha-2
code from the COUNTRY command (empty when the client never sent one, which
includes every client older than 1.4 and every browser build), posted as a
flag emoji -- see _flag() for why a country goes where coordinates do not.

    RESULT|<game_id>|<round>|<mode>|<winner>|<roster>|<wins>|<victories_limit>|<platforms>|<inputs>|<countries>|<servername>

One per round-end, sniffed from the 'F' opcode server-side (game.c). game_id
is an opaque int identifying the room, monotonically assigned by fb-server at
CREATE and stable for the room's whole lifetime (see g->game_id's comment in
server/game.c) -- used here only to group every round from the same room
into one Discord thread (see "Round-result threading" below), never
displayed. round is g->round_number, a 1-based per-room counter incremented
once per posted result -- purely a display label ("Round 3"), no gameplay
meaning. mode is fb-server's raw 0-3 GAMEMODE value (src/gamemode.h);
winner is the reporting client's win claim, empty for a draw, and -- unlike
everything else in either datagram -- is not validated against is_nick_ok()
at all, so it gets no more trust here than servername does; roster is every
current player's nick, comma-joined, each one already
is_nick_ok()-constrained when its owner connected or joined. wins is each
player's current win count this match, comma-joined in the same order as
roster (build_wins_csv() in game.c, already incremented for this round's
winner by the time it's sent) -- used to render a small text win-count bar
chart under the round message (see build_result_message()/
_build_win_chart()). victories_limit is the room's own win-count target
(g->victories_limit, 0 meaning no limit was ever set) -- when positive, the
chart scales its bars against it and labels each line "current/limit"
("first to N"); when 0, the chart falls back to scaling against whoever
currently leads, exactly as it did before this field existed. platforms and
inputs are per-seat single-char tags, index-aligned with roster the same way
wins is: the former is each player's OS (as JOIN's), the latter which device
they actually shot with this round (K/M/T/G -- keyboard, mouse, touch,
gamepad), from the 'i' opcode fb-server sniffs alongside 'F'. countries is
the same again, two chars per seat, from the COUNTRY command. Every one of
the three is empty per seat for a player whose client never reported it, so a
room mixing client versions badges whoever it can and leaves the rest bare.

Both datagram kinds are also accepted in their older shapes -- pre-1.4
entirely, and 1.4-without-country -- see handle_datagram(), which tells them
apart by field count plus a sanity check on the tag fields themselves, since
a servername containing '|' can otherwise fake the higher count.

    MATCH|<game_id>|<wins>|<mode>|<champion>|<servername>

At most one per round-end -- fired immediately after (never instead of) the
RESULT above for the same round, and only when that round's winner has just
reached the room's own VICTORIESLIMIT (see g->victories_limit in game.c).
game_id is the same room key as RESULT, so this always threads into the
identical Discord thread as every round before it (the RESULT that
triggered it already created or reused that thread moments earlier -- see
"Round-result threading" below). wins is the champion's final win count,
mode is the same raw 0-3 GAMEMODE value, and champion carries the same
trust posture as RESULT's winner field.

servername is everything remaining after the last "|" in every format above
(it can itself contain spaces or, in principle, "|") -- see build_message(),
build_result_message() and build_match_message() for what actually reaches
Discord from each.

ip and geoloc are parsed but never posted to Discord -- see build_message(),
which explains why the precise location came out (the country field above is
deliberately a different, far coarser thing; see _flag()). Both ride along in the datagram
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

# Single-char tags fb-server sends for a player's OS (the PLATFORM command)
# and for the device they last actually shot with (the 'i' opcode), mapped to
# the emoji that stands in for each in a Discord message. Both sets are closed
# and validated server-side (is_platform_tag_ok / is_input_tag_ok, game.c), so
# an unknown key here means either a future client this relay predates or a
# forged datagram -- both render as no badge at all, never as a guess.
#
# Emoji rather than uploaded custom emoji or an attached image: a webhook can
# post these into any channel with no setup, no asset hosting and no
# permissions beyond the one it already has, and they survive an operator
# forking this file. They are also why nothing here needs OS trademarks.
_PLATFORM_BADGES = {"W": "🪟", "M": "🍎", "L": "🐧", "A": "🤖", "I": "📱", "B": "🌐"}
_INPUT_BADGES = {"K": "⌨️", "M": "🖱️", "T": "👆", "G": "🎮"}


def _flag(country_tag):
    """An ISO 3166-1 alpha-2 code as its flag emoji, or "" if it is not one.

    No lookup table: a flag emoji *is* its two letters written as regional
    indicator symbols, so this works for every country without this file
    knowing any of them, and a well-formed code for something that is not a
    country degrades to two harmless letter glyphs rather than an error.

    What reaches here is a country, never coordinates. The lat/lon in the same
    datagram is still dropped unread -- see build_message() for the standing
    decision behind that, which this does not reopen: a flag says which of ~200
    countries somebody is in, which is the granularity a server list shows
    anyway, while a map pin says where they are.
    """
    if not country_tag or len(country_tag) != 2 or not country_tag.isascii():
        return ""
    if not country_tag.isalpha() or not country_tag.isupper():
        return ""
    return "".join(chr(0x1F1E6 + ord(c) - ord("A")) for c in country_tag)


def _badges(platform_tag, input_tag="", country_tag=""):
    """The badge string for one player: flag, platform, then input device.

    Any of the three may be absent -- an older client that never sends PLATFORM
    or COUNTRY, a player who has not fired a shot yet this round, or any tag
    this build does not recognize. Returns "" when there is nothing to show, so
    callers can concatenate unconditionally without producing stray spaces.
    """
    return (_flag(country_tag)
            + _PLATFORM_BADGES.get(platform_tag, "")
            + _INPUT_BADGES.get(input_tag, ""))


def _country_csv_ok(field):
    """True when `field` could be one of fb-server's per-seat country CSVs.

    Same disambiguation job _tag_csv_ok does, for the wider shape: every
    element is an uppercase ASCII letter pair or empty. A server name is not.
    """
    return all(part == "" or (len(part) == 2 and part.isascii()
                              and part.isalpha() and part.isupper())
               for part in field.split(","))


def _tag_csv_ok(field, valid):
    """True when `field` could be one of fb-server's own per-seat tag CSVs.

    Used to tell a new-format datagram from an old one whose trailing
    servername happens to contain a '|' -- the one case where field *count*
    alone is ambiguous (see handle_datagram). Every element is a single char
    from the closed set or empty, which no realistic server name is.
    """
    return all(part == "" or (len(part) == 1 and part in valid)
               for part in field.split(","))

# Width, in block characters, of the longest bar _build_win_chart() draws.
# When a room has a win-count target this is what "full" means; otherwise
# every bar is scaled relative to the match's current leader instead, so
# this is purely a display constant, not a cap on win count itself.
_CHART_WIDTH = 10


def _build_win_chart(roster_csv, wins_csv, victories_limit=0):
    """A monospace win-count bar chart, one row per player, leader first.

    roster_csv/wins_csv are index-aligned CSVs straight off the wire (see
    the module docstring's RESULT entry) -- roster from build_roster_csv(),
    wins from build_wins_csv(), both game.c. Returns "" (never posted) when
    there is nothing worth charting yet: a mismatched/unparsable pair (a
    stray older fb-server that never sends wins, or a malformed datagram),
    fewer than two players, or a match where nobody has won a round yet --
    an all-zero chart is a wall of empty bars with no information in it.

    victories_limit is the room's own win-count target (g->victories_limit,
    game.c), 0 meaning no limit was ever set -- by far the common case, most
    rooms never configure one. When positive, bars scale against it (not the
    leader) and each line is labelled "current/limit" under a "First to N"
    header, so the chart answers "how many more wins does the leader need,"
    not just "who's ahead right now." A count that ever exceeds the limit
    (only possible if VICTORIESLIMIT is lowered mid-room) still clamps its
    bar at full rather than overflowing it. When 0, the chart falls back to
    the leader-relative scaling it used before this parameter existed, with
    no "/limit" suffix and no header, since there is no target to show.
    victories_limit itself is a plain int fb-server computed (no untrusted
    text), so it needs no sanitization the way every name here does.
    """
    names = [n for n in roster_csv.split(",") if n] if roster_csv else []
    try:
        counts = [int(w) for w in wins_csv.split(",")] if wins_csv else []
    except ValueError:
        return ""
    if len(names) != len(counts) or len(names) < 2:
        return ""
    if max(counts) <= 0:
        return ""
    try:
        victories_limit = int(victories_limit)
    except (TypeError, ValueError):
        victories_limit = 0
    limited = victories_limit > 0
    peak = victories_limit if limited else max(counts)
    rows = sorted(zip(names, counts), key=lambda pair: pair[1], reverse=True)
    display_names = [_sanitize_display(n) for n, _ in rows]
    name_width = max(len(n) for n in display_names)
    lines = [f"First to {victories_limit}"] if limited else []
    for name, (_, count) in zip(display_names, rows):
        filled = min(_CHART_WIDTH, round(count * _CHART_WIDTH / peak))
        bar = "█" * filled + "░" * (_CHART_WIDTH - filled)
        label = f"{count}/{victories_limit}" if limited else str(count)
        lines.append(f"{name:<{name_width}} {bar} {label}")
    return "```\n" + "\n".join(lines) + "\n```"


def build_result_message(round_number, mode, winner, roster_csv, wins_csv, victories_limit,
                          servername, *, platforms_csv="", inputs_csv="",
                          countries_csv=""):
    """Round over: which round, who won (or a draw), what mode, who was
    playing, where.

    round_number is g->round_number, a 1-based per-room counter with no
    gameplay meaning -- purely a display label. A non-positive or otherwise
    unusable value (see handle_datagram's own parsing) degrades to omitting
    the label entirely rather than printing "Round 0" or similar.

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

    wins_csv and victories_limit both go straight to _build_win_chart() --
    wins_csv index-aligned with roster_csv (build_wins_csv(), game.c),
    victories_limit the room's win-count target (0 = none) -- see there for
    when a chart is actually appended versus omitted, and how a positive
    limit changes what the bars mean.

    platforms_csv, inputs_csv and countries_csv are index-aligned with
    roster_csv the same way (build_tags_csv()/build_country_csv(), game.c) and
    decorate each name in the roster line with a flag, an OS badge and a "what
    they actually played this round with" badge.
    Both are per-seat and independently optional: a room mixing this build
    with an older one shows badges for the players who reported and nothing
    for the rest, rather than dropping the feature for everyone or guessing
    at the missing seats. A length that does not match the roster is treated
    as no tags at all -- misaligning these would attribute the wrong platform
    to a named player, which is worse than showing none.
    """
    round_label = f"Round {round_number} — " if round_number and round_number > 0 else ""
    mode_label = _GAME_MODE_NAMES.get(mode, "")
    mode_label = f" ({mode_label})" if mode_label else ""
    servername = _sanitize_display(servername)
    names = [n for n in roster_csv.split(",") if n] if roster_csv else []
    platforms = platforms_csv.split(",") if platforms_csv else []
    inputs = inputs_csv.split(",") if inputs_csv else []
    countries = countries_csv.split(",") if countries_csv else []
    if len(platforms) != len(names):
        platforms = [""] * len(names)
    if len(inputs) != len(names):
        inputs = [""] * len(names)
    if len(countries) != len(names):
        countries = [""] * len(names)
    roster = ", ".join(
        (_sanitize_display(n) + (f" {b}" if (b := _badges(p, i, c)) else ""))
        for n, p, i, c in zip(names, platforms, inputs, countries)
    )
    if winner:
        headline = f"🏆 **{_sanitize_display(winner)}** won"
    else:
        headline = "🤝 Draw"
    content = f"{round_label}{headline}{mode_label} on **{servername}** — {roster}"
    chart = _build_win_chart(roster_csv, wins_csv, victories_limit)
    if chart:
        content = f"{content}\n{chart}"
    return content[:MAX_DISCORD_CONTENT]


def build_match_message(wins, mode, champion, servername):
    """Match over: the champion who just reached the room's VICTORIESLIMIT.

    Posted as a second, separate message right after the RESULT alert for
    the same round -- see MATCH's own doc in the module docstring for why
    this is additive, not a replacement. wins and champion carry the same
    trust/sanitization posture as build_result_message()'s winner field
    (game_mode too); there is no roster here, since the champion is the
    whole point and the room's roster was already shown in the RESULT
    message moments earlier.
    """
    mode_label = _GAME_MODE_NAMES.get(mode, "")
    mode_label = f" ({mode_label})" if mode_label else ""
    servername = _sanitize_display(servername)
    content = (f"🏁 **{_sanitize_display(champion)}** wins the match "
               f"with {wins} round win{'s' if wins != 1 else ''}{mode_label} "
               f"on **{servername}**!")
    return content[:MAX_DISCORD_CONTENT]


def build_message(nick, servername, *, platform="", country=""):
    """The whole message: who joined, from what, and where. Nothing else.

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

    platform is the one field of that kind that does get posted. It is which
    OS the player's client runs on, not where they are or who they are: it
    says nothing about the person, it is the same for everyone using that
    build, and it is exactly what someone reading the channel to decide
    whether to go play wants to know. Empty (an older client, or one that
    sent a tag this relay does not recognize) renders as no badge.

    country is the same kind of thing one step coarser -- an ISO alpha-2 code,
    rendered as a flag. It is posted for the same reason platform is and the
    lat/lon beside it is not: which country somebody is playing from is what a
    reader deciding whether to go play wants, and it locates nobody. See
    _flag().

    Keyword-only, deliberately: the arity of the positional part of this
    signature is itself the guard that a location can never be handed to it
    by accident, and tests/discord_relay_message_test.py pins that. A new
    field earning its way in here does not get to weaken that check.
    """
    nick = _sanitize_display(nick)
    servername = _sanitize_display(servername)
    badge = _badges(platform, country_tag=country)
    badge = f" {badge}" if badge else ""
    content = f"🔔 **{nick}**{badge} joined **{servername}**"
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
        # maxsplit one higher than the old 4-field format needs, so the
        # platform tag can be picked off when it's there. An fb-server old
        # enough not to send one, whose servername itself contains '|', also
        # yields 5 parts -- _tag_csv_ok settles which it is, since no
        # realistic server name is a bare single char from that closed set.
        parts = rest.split("|", 5)
        platform = country = ""
        if (len(parts) == 6 and _tag_csv_ok(parts[3], _PLATFORM_BADGES)
                and _country_csv_ok(parts[4])):
            nick, ip, geoloc, platform, country, servername = parts
        elif len(parts) >= 5 and _tag_csv_ok(parts[3], _PLATFORM_BADGES):
            # Protocol 1.4 without the country field, or a 1.4 servername
            # containing '|'. Either way the platform tag is real and the rest
            # is the name.
            nick, ip, geoloc, platform = parts[0], parts[1], parts[2], parts[3]
            servername = "|".join(parts[4:])
        elif len(parts) >= 4:
            nick, ip, geoloc = parts[0], parts[1], parts[2]
            servername = "|".join(parts[3:])
        else:
            log.warning("malformed datagram, dropped: %r", text[:120])
            return
        # Both received, neither posted -- see build_message(). They stay in
        # the wire format because fb-server already has them and a datagram
        # costs the same either way; dropping them here rather than at the
        # sender keeps the decision in one reviewable place, and leaves an
        # operator who forks this file for a private channel something to
        # work from.
        del ip, geoloc
        if DISCORD_SERVER_NAME:
            servername = DISCORD_SERVER_NAME
        content = build_message(nick, servername, platform=platform, country=country)
        log_label = f"join alert for {nick}"
        game_id = None

    elif kind == "RESULT":
        # Same shape as JOIN above: two extra fields on current fb-server,
        # disambiguated from an old datagram with a '|' in its servername by
        # checking that both candidates actually look like tag CSVs.
        parts = rest.split("|", 10)
        platforms_csv = inputs_csv = countries_csv = ""
        tagged = (len(parts) >= 10 and _tag_csv_ok(parts[7], _PLATFORM_BADGES)
                  and _tag_csv_ok(parts[8], _INPUT_BADGES))
        if tagged and len(parts) == 11 and _country_csv_ok(parts[9]):
            (game_id_s, round_s, mode_s, winner, roster_csv, wins_csv,
             victories_limit_s, platforms_csv, inputs_csv, countries_csv,
             servername) = parts
        elif tagged:
            # 1.4 without the country column, or a servername with a '|'.
            (game_id_s, round_s, mode_s, winner, roster_csv, wins_csv,
             victories_limit_s, platforms_csv, inputs_csv) = parts[:9]
            servername = "|".join(parts[9:])
        elif len(parts) >= 8:
            (game_id_s, round_s, mode_s, winner, roster_csv, wins_csv,
             victories_limit_s) = parts[:7]
            servername = "|".join(parts[7:])
        else:
            log.warning("malformed datagram, dropped: %r", text[:120])
            return
        try:
            game_id = int(game_id_s)
        except ValueError:
            # Never seen from an unmodified fb-server -- game_id is always a
            # plain int it assigned itself. Post the result anyway rather
            # than drop a real round-end over a threading key alone; it
            # just cannot be grouped into a thread without one.
            game_id = None
        try:
            round_number = int(round_s)
        except ValueError:
            round_number = 0  # unrecognized -- build_result_message omits the label
        try:
            mode = int(mode_s)
        except ValueError:
            mode = -1  # unrecognized -- build_result_message labels nothing
        try:
            victories_limit = int(victories_limit_s)
        except ValueError:
            victories_limit = 0  # unrecognized -- chart falls back to leader-relative
        if DISCORD_SERVER_NAME:
            servername = DISCORD_SERVER_NAME
        content = build_result_message(round_number, mode, winner, roster_csv, wins_csv,
                                        victories_limit, servername,
                                        platforms_csv=platforms_csv, inputs_csv=inputs_csv,
                                        countries_csv=countries_csv)
        log_label = f"result for {winner or 'a draw'}"
        # Best-effort label for the thread's title only, not the message
        # itself -- see build_roster_csv()/players_nick[0] in game.c for why
        # this can drift from the room's original CREATE name over a long
        # room's life, and why that is an acceptable, cosmetic-only cost.
        room_label = _sanitize_display(roster_csv.split(",")[0] if roster_csv else servername)

    elif kind == "MATCH":
        parts = rest.split("|", 4)
        if len(parts) != 5:
            log.warning("malformed datagram, dropped: %r", text[:120])
            return
        game_id_s, wins_s, mode_s, champion, servername = parts
        try:
            game_id = int(game_id_s)
        except ValueError:
            # Same reasoning as RESULT above: post anyway, just ungrouped.
            game_id = None
        try:
            wins = int(wins_s)
        except ValueError:
            wins = 0
        try:
            mode = int(mode_s)
        except ValueError:
            mode = -1
        if DISCORD_SERVER_NAME:
            servername = DISCORD_SERVER_NAME
        content = build_match_message(wins, mode, champion, servername)
        log_label = f"match win for {champion}"
        # Only used if this ever had to create a fresh thread -- in practice
        # it never does, since the RESULT for the same round already created
        # or reused one moments earlier (see the module docstring's MATCH
        # entry). Still a reasonable label on its own if that ever changes.
        room_label = _sanitize_display(champion or servername)

    else:
        log.warning("malformed datagram, dropped: %r", text[:120])
        return

    if kind in ("RESULT", "MATCH") and game_id is not None and _bot_mode_enabled():
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
