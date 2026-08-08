# notify-relay

Delivers the push notifications for fb-server's "follow a server" feature.

`fb-server` cannot do this itself: it has no TLS stack, and it runs one
single-threaded blocking event loop for every connected player, so an HTTP/2
handshake with Apple in the middle of a round would stall the game. It instead
fires one best-effort UDP datagram per notification at this process and moves
on immediately. If this relay is down, misconfigured, or simply not running,
the datagram is dropped and gameplay is unaffected.

## Wire format

    NOTIFY|<platform>|<token>|<message>

`platform` is `ios` or `android`.

## Running

    NOTIFY_RELAY_BIND=0.0.0.0:9099 python3 relay.py

and point the game server at it:

    FB_SERVER_NOTIFY_RELAY=127.0.0.1:9099 fb-server ...

## Stub mode

With no credentials configured, the relay logs what it *would* have sent:

    [stub] would push to ios token=abc123...ef01: A player just joined myserver!

This is the default and needs no dependencies beyond Python 3 — the APNs and
FCM libraries are imported lazily, only once the corresponding credentials are
present. It exists so the entire pipeline (client → game server → relay) can be
exercised before anyone has an Apple Developer account or a Firebase project.

## Live delivery

**iOS** — all four required:

| Variable | Meaning |
|---|---|
| `APNS_KEY_PATH` | path to the `.p8` auth key |
| `APNS_KEY_ID` | the key's identifier |
| `APNS_TEAM_ID` | Apple developer team id |
| `APNS_TOPIC` | the app's bundle id |
| `APNS_USE_SANDBOX` | optional; `1` targets the APNs sandbox |

**Android** — `FCM_SERVICE_ACCOUNT_JSON`, the path to a Firebase service-account
JSON file.

Then install the delivery libraries:

    pip install -r requirements.txt

Tokens are redacted in all log output.
