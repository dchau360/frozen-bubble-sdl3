# discord-relay

Posts a Discord message whenever a player arrives on `fb-server`, carrying
their nick and the server's name -- and nothing else.

"Arrives" means joining the *server* (their first accepted `NICK`, the point
at which the lobby can see them), not joining a game room. A player sitting
alone in a room they just opened is exactly who an alert should summon
company for, and by the time somebody has joined their room the two have
already found each other.

The joining player's IP and their self-reported geolocation both arrive in
the datagram from `fb-server` (see Wire format below); neither is ever
included in the Discord message. The IP never was. The location was, as a
Google Maps link, up until the game itself began advertising a community
Discord to players from the NET GAME list and the online lobby: a channel
the game recruits players into is one where every joining player's
approximate whereabouts would be visible to anyone who took up the offer,
which is not something a player agreed to by letting the client geolocate
them for the lobby's world map. The map is unaffected -- that data simply
stops here.

If you are running a genuinely private, operators-only channel and want the
location back, `build_message()` in `relay.py` is where it was, and
`handle_datagram()` still has the value in hand. Weigh it against who can
actually read the channel, and remember that a webhook URL is one paste away
from a wider audience than you planned.

`fb-server` cannot do this itself: it has no TLS stack, and it runs one
single-threaded blocking event loop for every connected player, so an HTTPS
POST to Discord in the middle of a round would stall the game. It instead
fires one best-effort UDP datagram per join at this process and moves on
immediately. If this relay is down, misconfigured, or simply not running,
the datagram is dropped and gameplay is unaffected.

## Wire format

    JOIN|<nick>|<ip>|<geoloc>|<servername>

`geoloc` is the arriving player's self-reported `lat:lon` or an empty string
-- in practice always empty, since the client sends `GEOLOC` after `NICK`
and the lookup behind it can take up to ~16s. Both `ip` and `geoloc` are
parsed and then discarded -- see
`handle_datagram()` and `build_message()` in `relay.py`. They stay in the
wire format because `fb-server` already has them and a datagram costs the
same either way.

## Running

    DISCORD_RELAY_BIND=0.0.0.0:9100 python3 relay.py

and point the game server at it:

    FB_SERVER_DISCORD_RELAY=127.0.0.1:9100 fb-server ...

## Stub mode

With no webhook configured, the relay logs what it *would* have posted:

    [stub] would post: 🔔 **alice** joined **fb.servequake.com**

This is the default and needs no dependencies at all -- the standard library
covers everything a Discord webhook POST needs (it's a plain JSON POST, no
OAuth/JWT dance the way APNs or FCM needed). It exists so the pipeline
(game server → relay) can be exercised before a webhook exists.

## Live delivery

Create a webhook in Discord (Server Settings → Integrations → Webhooks →
New Webhook, pick the channel it should post to) and copy its URL:

    DISCORD_WEBHOOK_URL=https://discord.com/api/webhooks/... python3 relay.py

That one URL is both the credential and the channel selector -- there is no
separate "which channel" setting here; create a different webhook (and point
a different relay instance, or swap the env var) to alert a different
channel.

**If you swap out the HTTP client**, keep a real `User-Agent` header.
Discord's edge WAF rejects the stock `urllib` default
(`Python-urllib/3.x`) with a bare 403 on every request, GET included, not
just POST -- confirmed against the live endpoint. `_post_sync()` already
sets one; a rewrite that drops it fails on the very first join with
nothing in the response to explain why, indistinguishable in the log from
a bad or revoked webhook URL.

A burst of joins can hit Discord's per-webhook rate limit; a request that
gets 429'd is logged and dropped rather than queued or retried, the same
best-effort handling as any other delivery failure -- see the comment on
`_post_sync` in `relay.py`.

The message carries neither an IP nor a location, but it does carry a nick,
and the stream of them is a record of who plays where and when -- keep the
webhook URL somewhere sensible all the same. Anyone holding it can post
anything to that channel.

## Collecting joins from servers you don't run

Anyone can run an `fb-server`, so a channel that announces joins across
several of them means letting other operators post into it. Use **one
webhook per operator**, never a bot token:

- A webhook can only create messages in the single channel it was made for.
  A bot token is one shared credential that grants far more, everywhere the
  bot is installed, and cannot be withdrawn from one holder without
  rotating it for all of them.
- Per-operator webhooks make revocation surgical: delete that one webhook
  and that one operator stops posting, with nobody else disturbed.
- Discord caps a channel at 15 webhooks, which is the practical ceiling on
  this approach.

The operator's side needs no special build -- they set the URL you issued
as their `DISCORD_WEBHOOK_URL`, exactly as above.

**What issuing a webhook actually grants.** Whoever holds it can post
anything to that channel, not just what this relay would send; nothing on
Discord's side constrains a URL holder to running this code. In particular
`servername` is self-asserted -- it comes from `net_servername()`, which
imposes no charset rule (unlike `nick`, which fb-server's `is_nick_ok()`
restricts to `[A-Za-z0-9_-]{1,10}`) -- so a hostile or compromised operator
can claim to be a server they are not. `build_message()` escapes Discord
markdown in both fields and `_post_sync()` sends
`allowed_mentions: {"parse": []}`, which together stop a name injecting a
`[label](url)` link, forging a second alert with a newline, or pinging the
channel; `tests/discord_relay_message_test.py` holds that line. None of
that makes the *claim* trustworthy. Issue a webhook only to an operator you
would vouch for.

If you would rather not extend that trust, invert the design: run one
ingest service that holds the webhook and issue each operator an API key
pointing at it, so the channel name each alert displays is one you map from
the key rather than one the sender asserts. That service must build the
message from structured fields -- passing a sender-supplied `content`
string through to Discord would hand back exactly the trust you set out to
withhold.
