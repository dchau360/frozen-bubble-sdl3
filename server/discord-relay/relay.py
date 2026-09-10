#!/usr/bin/env python3
"""
Discord join-alert relay for fb-server.

fb-server itself has no TLS stack and runs a single-threaded blocking event
loop, so it cannot POST to a Discord webhook without risking a stall for
every connected player. Instead it fires a best-effort UDP datagram at this
process, which owns the actual HTTPS call -- and the webhook URL, which
never needs to reach fb-server at all.

Wire format, one datagram per player arriving on the server (their first
accepted NICK -- not a game-room join, which is a later and much less
actionable moment):

    JOIN|<nick>|<ip>|<geoloc>|<servername>

nick and ip are always present. geoloc is the player's self-reported
"lat:lon" (see the client's DetectGeoLocation()/GEOLOC command) or an empty
string -- in practice always empty, since the client sends GEOLOC after NICK
and the lookup behind it can take up to ~16s. The field stays in the format
anyway: it costs nothing, and this end discards it regardless. servername is
everything remaining after the fourth "|" (it can itself contain spaces or,
in principle, "|").

ip and geoloc are parsed but never posted to Discord -- see build_message(),
which explains why the location came out. Both ride along in the datagram
because fb-server already has them for free and it costs nothing to send,
not because this relay does anything with them.

Without DISCORD_WEBHOOK_URL configured this runs in stub mode: it logs what
it *would* have posted and returns. That is the intended state until an
operator supplies a webhook -- the whole pipeline stays exercisable without
one.

Environment:
    DISCORD_RELAY_BIND    host:port to listen on (default 0.0.0.0:9100)
    DISCORD_WEBHOOK_URL    Discord's channel webhook URL; unset = stub mode
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
        headers={"Content-Type": "application/json"},
    )
    # Discord's webhook rate limit (per-webhook, a handful of requests per
    # few seconds) means a burst of joins can 429. That is treated the same
    # as any other delivery failure below: logged and dropped, never
    # retried -- queuing would just let a slow patch of joins pile up
    # indefinitely, and the alert has already lost most of its value by the
    # time a retry would land anyway.
    with urllib.request.urlopen(req, timeout=10) as resp:
        resp.read()


async def handle_datagram(data, webhook_url):
    try:
        text = data.decode("utf-8", errors="replace").strip()
    except Exception:
        log.warning("undecodable datagram, dropped")
        return

    # split into at most 5 so a servername containing '|' survives intact
    parts = text.split("|", 4)
    if len(parts) != 5 or parts[0] != "JOIN":
        log.warning("malformed datagram, dropped: %r", text[:120])
        return

    _, nick, ip, geoloc, servername = parts
    # Both received, neither posted -- see build_message(). They stay in the
    # wire format because fb-server already has them and a datagram costs the
    # same either way; dropping them here rather than at the sender keeps the
    # decision in one reviewable place, and leaves an operator who forks this
    # file for a private channel something to work from.
    del ip, geoloc
    content = build_message(nick, servername)

    if not webhook_url:
        log.info("[stub] would post: %s", content)
        return

    try:
        await asyncio.to_thread(_post_sync, webhook_url, content)
        log.info("posted join alert for %s", nick)
    except Exception as exc:
        # A delivery failure must never take the relay down -- the game
        # server has already moved on and cannot be told about it anyway.
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
