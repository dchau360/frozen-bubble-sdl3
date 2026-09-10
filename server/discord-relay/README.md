# discord-relay

Posts a Discord message whenever a player joins a room on `fb-server`,
carrying the joining player's IP and self-reported geolocation (a Google
Maps link) when one is available.

`fb-server` cannot do this itself: it has no TLS stack, and it runs one
single-threaded blocking event loop for every connected player, so an HTTPS
POST to Discord in the middle of a round would stall the game. It instead
fires one best-effort UDP datagram per join at this process and moves on
immediately. If this relay is down, misconfigured, or simply not running,
the datagram is dropped and gameplay is unaffected.

## Wire format

    JOIN|<nick>|<ip>|<geoloc>|<servername>

`geoloc` is the joining player's self-reported `lat:lon` or an empty string
-- a fast joiner routinely beats their own client-side geolocation lookup
(it can take up to ~16s), so a missing location is the normal case, not an
error.

## Running

    DISCORD_RELAY_BIND=0.0.0.0:9100 python3 relay.py

and point the game server at it:

    FB_SERVER_DISCORD_RELAY=127.0.0.1:9100 fb-server ...

## Stub mode

With no webhook configured, the relay logs what it *would* have posted:

    [stub] would post: 🔔 **alice** joined **fb.servequake.com** from [this location](https://www.google.com/maps?q=37.77,-122.42) (203.0.113.5)

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

A burst of joins can hit Discord's per-webhook rate limit; a request that
gets 429'd is logged and dropped rather than queued or retried, the same
best-effort handling as any other delivery failure -- see the comment on
`_post_sync` in `relay.py`.

The IP address included in the message is a real piece of PII -- keep the
webhook URL and the channel it posts to appropriately private, the same as
you would `joiners.log` on the game server itself.
