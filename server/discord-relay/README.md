# discord-relay

Posts a Discord message whenever a player arrives on `fb-server`, carrying
their nick and the server's name -- and nothing else. It also posts one at
the end of every round, carrying the game mode, the winner (or that it was a
draw), and the full player roster -- see [Round results](#round-results)
below. Same webhook, same stub/live modes, same `DISCORD_SERVER_NAME`
override; nothing extra to configure for one versus the other.

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
    RESULT|<game_id>|<game_mode>|<winner>|<roster>|<servername>

`geoloc` is the arriving player's self-reported `lat:lon` or an empty string
-- in practice always empty, since the client sends `GEOLOC` after `NICK`
and the lookup behind it can take up to ~16s. Both `ip` and `geoloc` are
parsed and then discarded -- see
`handle_datagram()` and `build_message()` in `relay.py`. They stay in the
wire format because `fb-server` already has them and a datagram costs the
same either way.

`game_id` is an opaque int identifying the room -- `fb-server` assigns it
once, monotonically, when the room is created (`g->game_id` in
`server/game.c`), and it never changes for that room's whole lifetime. It's
never displayed; the relay uses it only to group a room's rounds into one
Discord thread (see Round results below). `game_mode` is `fb-server`'s raw
0-3 `GAMEMODE` value (0 Classic, 1 Clear, 2 Race, 3 Timed), or 0 if the room
never set one. `winner` is empty on a draw. `roster` is every player
currently in the room, comma-joined -- unlike `winner`, every name in it
already passed `fb-server`'s `is_nick_ok()`, so it can never itself contain
a `|`; `winner` had any literal `|` stripped at the C-layer extraction point
(`game.c`) for the same reason, since it is *not* validated against
`is_nick_ok()` at all -- see Round results below. Both message kinds keep
`servername` last, since it is the only field with no length or charset cap.

## Running

    DISCORD_RELAY_BIND=0.0.0.0:9100 python3 relay.py

and point the game server at it:

    FB_SERVER_DISCORD_RELAY=127.0.0.1:9100 fb-server ...

## Stub mode

With no webhook configured, the relay logs what it *would* have posted:

    [stub] would post: 🔔 **alice** joined **fb.servequake.com**
    [stub] would post: 🏆 **alice** won (Race) on **fb.servequake.com** — alice, bob

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

**Showing the server's full name.** `fb-server`'s own `-n` caps at 12
characters and a restricted charset -- a limit on what's advertised in the
lobby and the public server list, not on anything here. If your real name
doesn't fit there (a full domain, say), set `DISCORD_SERVER_NAME` and every
message shows that instead of whatever the datagram carried, with no change
to `-n` or to what players see in-game:

    DISCORD_SERVER_NAME=fb.example.org python3 relay.py

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

## Round results

Fired once per round -- on the `F` opcode (round-over: a bare `"F"` is a
draw, `"F<nick>"` is a win claim), sniffed in `process_msg_prio_()`
(`server/game.c`) alongside the server's normal, unrelated relay of that
same opcode to every client in the room -- not once per full match. There is
no reliable way for the server to know when a "match" (best-of-N by
whichever win count a room's players agreed on client-side) is actually
over, so this posts at the same granularity already shown to players in the
post-round stats table.

**The winner name is not verified.** It comes straight from the reporting
client's own `F` payload, the same claim every other player's screen already
shows for that round -- `fb-server` has never validated it, alert or not.
The roster next to it, by contrast, always reflects the server's own player
list, so a modified client can claim an unearned win but cannot forge who
was actually in the room.

`build_result_message()` in `relay.py` maps `game_mode` to a display name
(`_GAME_MODE_NAMES`); a mode value a future client version sends that this
build doesn't recognize just degrades to no label rather than a crash or a
stale mapping needing an update first. `_sanitize_display()` (the same
Discord-markdown escaping and `allowed_mentions` lockdown used for `JOIN`)
applies to every name in the winner and roster fields too, since the roster
comes from `nick`s that already satisfy `is_nick_ok()` but the winner field
does not.

The server never infers or posts a result from a player disconnecting
mid-round (the stats bookkeeping that already exists for that, in
`player_part_game_()`, stays local to the server) -- a dropped connection and
a rage-quit are indistinguishable there, and publicly misattributing an
outcome to a named player would be worse than not posting one.

### Threading rounds per room

With `DISCORD_BOT_TOKEN` and `DISCORD_CHANNEL_ID` both set, every room's
first result opens a Discord thread (named after the room), and every later
result for the same room (same `game_id`) posts into that thread instead of
a fresh top-level message. Leaving either unset (the default) keeps posting
flat via `DISCORD_WEBHOOK_URL`, exactly as before this existed -- join
alerts are entirely unaffected by this setting either way, since a join
isn't part of any one room's round history.

This needs a **bot**, not the webhook everything else in this file uses:
Discord's Execute Webhook endpoint can only create a *new* thread
(`thread_name` in the request body) when the webhook's own channel is a
forum or media channel -- it cannot on an ordinary text channel, which is
what this relay assumes it's sharing with join alerts. A bot token has no
such restriction: `_post_result_via_bot_sync()` posts an ordinary message
via `POST /channels/{channel}/messages`, then converts it into a thread via
`POST /channels/{channel}/messages/{message}/threads`, both authenticated
`Authorization: Bot <token>` rather than a webhook URL. Every later result
for that room posts straight to `POST /channels/{thread}/messages` -- a
thread is addressable as an ordinary channel id once it exists, so no
webhook-style `?thread_id=` query parameter is needed here.

The bot needs **View Channel**, **Send Messages**, **Create Public
Threads**, and **Send Messages in Threads** in `DISCORD_CHANNEL_ID`. Set up
via the [Discord Developer Portal](https://discord.com/developers/applications):
create an Application, add a Bot, copy its token as `DISCORD_BOT_TOKEN`,
generate an OAuth2 URL with scope `bot` and those permissions, and use it to
add the bot to your server.

`_room_threads` (a `game_id -> thread id` dict) is purely in-memory: a relay
restart forgets every thread currently open, so the next result for a room
already in progress just opens a fresh one -- no worse than every room got
before this existed, and nothing worth persisting across a restart of a
best-effort sidecar. If posting into an already-cached thread ever fails
(the channel was deleted, a permission got revoked, anything), that cache
entry is dropped so the *next* round tries a new thread instead of repeating
the same failure forever; the round whose post actually failed is logged and
dropped, same as any other delivery failure in this file.

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
