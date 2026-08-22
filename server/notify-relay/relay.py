#!/usr/bin/env python3
"""
Push-notification relay for fb-server's "follow a server" feature.

fb-server itself has no TLS stack and runs a single-threaded blocking event
loop, so it cannot talk to APNs (HTTP/2 + ES256 JWT) or FCM (OAuth2) without
risking a stall for every connected player. Instead it fires a best-effort UDP
datagram at this process, which owns the actual delivery.

Wire format, one datagram per notification:

    NOTIFY|<platform>|<token>|<message>

platform is "ios" or "android". The message is free text already formatted by
the server.

Without credentials this runs in stub mode: it logs what it *would* have sent
and returns. That is the intended state until an operator supplies an APNs key
and/or a Firebase service account -- the whole pipeline stays exercisable
without them.

Environment:
    NOTIFY_RELAY_BIND     host:port to listen on (default 0.0.0.0:9099)
    APNS_KEY_PATH         .p8 auth key file            } all four required
    APNS_KEY_ID           key identifier               } for live iOS
    APNS_TEAM_ID          Apple developer team id      } delivery
    APNS_TOPIC            app bundle id                }
    APNS_USE_SANDBOX      "1" to target the APNs sandbox instead of production
    FCM_SERVICE_ACCOUNT_JSON  path to a service-account JSON, for live Android
"""

import asyncio
import logging
import os
import sys

LOG_FORMAT = "%(asctime)s %(levelname)s %(message)s"
logging.basicConfig(level=logging.INFO, format=LOG_FORMAT, stream=sys.stdout)
log = logging.getLogger("notify-relay")

MAX_DATAGRAM = 2048


def _redact(token):
    """Tokens are credentials of a sort -- never log one in full."""
    if not token:
        return "(empty)"
    if len(token) <= 12:
        return token[:2] + "..."
    return f"{token[:6]}...{token[-4:]}"


class ApnsSender:
    """Live APNs delivery. Constructed only when all four APNS_* vars are set.

    Uses aioapns rather than the more commonly seen `apns2` package: apns2 has
    not been released in years and hard-pins PyJWT<2.0, which is flatly
    incompatible with firebase-admin's PyJWT>=2.5.0 -- the two cannot appear in
    the same requirements.txt at all, let alone the same virtualenv. aioapns is
    async (a natural fit once FCM's blocking call forced this module onto an
    event loop anyway) and keeps one persistent HTTP/2 connection rather than
    reconnecting per notification.
    """

    def __init__(self, key_path, key_id, team_id, topic, use_sandbox):
        self.topic = topic
        self.use_sandbox = use_sandbox
        # Imported lazily so stub mode has no third-party dependency at all.
        from aioapns import APNs  # type: ignore

        # aioapns wants the PEM *contents* here, not a path: `key` is handed
        # straight to jwt.encode(key=...), and the only open() anywhere in the
        # package is for cert_file. Passing the path instead fails at the first
        # send with "Unable to load PEM file ... MalformedFraming" -- PyJWT
        # parsing the pathname itself as PEM -- which reads like a corrupt .p8
        # and sends you looking at the wrong thing entirely.
        #
        # APNS_KEY_PATH stays a path because that is what a container mount
        # gives you; the read happens here instead.
        try:
            with open(key_path, "r") as f:
                key_data = f.read()
        except OSError as exc:
            raise RuntimeError(
                f"APNS_KEY_PATH could not be read ({key_path}): {exc}"
            ) from exc

        self._client = APNs(
            key=key_data,
            key_id=key_id,
            team_id=team_id,
            topic=topic,
            use_sandbox=use_sandbox,
        )

    async def send(self, token, message):
        from aioapns import NotificationRequest  # type: ignore

        request = NotificationRequest(
            device_token=token,
            message={"aps": {"alert": message, "sound": "default", "badge": 1}},
            apns_topic=self.topic,
        )
        result = await self._client.send_notification(request)
        if not result.is_successful:
            raise RuntimeError(f"APNs rejected the push: {result.status} {result.description}")


class FcmSender:
    """Live FCM delivery. Constructed only when FCM_SERVICE_ACCOUNT_JSON is set."""

    def __init__(self, service_account_path):
        # Imported lazily so stub mode has no third-party dependency at all.
        import firebase_admin  # type: ignore
        from firebase_admin import credentials  # type: ignore

        cred = credentials.Certificate(service_account_path)
        try:
            firebase_admin.get_app()
        except ValueError:
            firebase_admin.initialize_app(cred)

    async def send(self, token, message):
        # firebase-admin's messaging.send() is a blocking HTTP call with no
        # async variant; run it off the event loop thread so one slow FCM
        # request cannot delay every other pending notification (APNs
        # included, since both senders now share this loop).
        from firebase_admin import messaging  # type: ignore

        def _send_sync():
            msg = messaging.Message(
                notification=messaging.Notification(title="Frozen Bubble", body=message),
                token=token,
            )
            messaging.send(msg)

        await asyncio.to_thread(_send_sync)


def build_senders():
    """Returns {platform: sender_or_None}. None means stub mode for that platform."""
    senders = {"ios": None, "android": None}

    key_path = os.environ.get("APNS_KEY_PATH", "")
    key_id = os.environ.get("APNS_KEY_ID", "")
    team_id = os.environ.get("APNS_TEAM_ID", "")
    topic = os.environ.get("APNS_TOPIC", "")
    if key_path and key_id and team_id and topic:
        senders["ios"] = ApnsSender(
            key_path, key_id, team_id, topic,
            os.environ.get("APNS_USE_SANDBOX", "") == "1",
        )
        log.info("APNs configured (topic=%s, sandbox=%s)",
                 topic, senders["ios"].use_sandbox)
    else:
        log.info("APNs not configured -- iOS notifications run in stub mode")

    fcm_json = os.environ.get("FCM_SERVICE_ACCOUNT_JSON", "")
    if fcm_json:
        senders["android"] = FcmSender(fcm_json)
        log.info("FCM configured (service account=%s)", fcm_json)
    else:
        log.info("FCM not configured -- Android notifications run in stub mode")

    return senders


async def handle_datagram(data, senders):
    try:
        text = data.decode("utf-8", errors="replace").strip()
    except Exception:
        log.warning("undecodable datagram, dropped")
        return

    # split into at most 4 so a message containing '|' survives intact
    parts = text.split("|", 3)
    if len(parts) != 4 or parts[0] != "NOTIFY":
        log.warning("malformed datagram, dropped: %r", text[:120])
        return

    _, platform, token, message = parts
    if platform not in senders:
        log.warning("unknown platform %r, dropped", platform)
        return
    if not token:
        log.warning("empty token, dropped")
        return

    sender = senders[platform]
    if sender is None:
        log.info("[stub] would push to %s token=%s: %s",
                 platform, _redact(token), message)
        return

    try:
        await sender.send(token, message)
        log.info("pushed to %s token=%s", platform, _redact(token))
    except Exception as exc:
        # A delivery failure must never take the relay down -- the game server
        # has already moved on and cannot be told about it anyway.
        log.error("push to %s token=%s failed: %s",
                  platform, _redact(token), exc)


class _RelayProtocol(asyncio.DatagramProtocol):
    """Fans each datagram out to its own task rather than awaiting it inline,
    so one slow push (APNs/FCM both do real network I/O) can never delay the
    next datagram from being picked up off the socket."""

    def __init__(self, senders):
        self.senders = senders

    def datagram_received(self, data, addr):
        log.debug("datagram from %s", addr)
        asyncio.ensure_future(handle_datagram(data, self.senders))


async def async_main():
    bind = os.environ.get("NOTIFY_RELAY_BIND", "0.0.0.0:9099")
    if ":" not in bind:
        log.error("NOTIFY_RELAY_BIND must be host:port, got %r", bind)
        return 1
    host, _, port_s = bind.rpartition(":")
    try:
        port = int(port_s)
    except ValueError:
        log.error("NOTIFY_RELAY_BIND has a non-numeric port: %r", bind)
        return 1

    senders = build_senders()

    loop = asyncio.get_running_loop()
    transport, _ = await loop.create_datagram_endpoint(
        lambda: _RelayProtocol(senders),
        local_addr=(host, port),
    )
    log.info("notify-relay listening on %s:%d", host, port)

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
