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

import logging
import os
import socket
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
    """Live APNs delivery. Constructed only when all four APNS_* vars are set."""

    def __init__(self, key_path, key_id, team_id, topic, use_sandbox):
        self.key_path = key_path
        self.key_id = key_id
        self.team_id = team_id
        self.topic = topic
        self.use_sandbox = use_sandbox
        self._client = None

    def _client_lazy(self):
        if self._client is None:
            # Imported lazily so stub mode has no third-party dependency at all.
            from apns2.client import APNsClient  # type: ignore
            from apns2.credentials import TokenCredentials  # type: ignore

            creds = TokenCredentials(
                auth_key_path=self.key_path,
                auth_key_id=self.key_id,
                team_id=self.team_id,
            )
            self._client = APNsClient(credentials=creds, use_sandbox=self.use_sandbox)
        return self._client

    def send(self, token, message):
        from apns2.payload import Payload  # type: ignore

        payload = Payload(alert=message, sound="default", badge=1)
        self._client_lazy().send_notification(token, payload, self.topic)


class FcmSender:
    """Live FCM delivery. Constructed only when FCM_SERVICE_ACCOUNT_JSON is set."""

    def __init__(self, service_account_path):
        self.service_account_path = service_account_path
        self._messaging = None

    def _messaging_lazy(self):
        if self._messaging is None:
            import firebase_admin  # type: ignore
            from firebase_admin import credentials, messaging  # type: ignore

            cred = credentials.Certificate(self.service_account_path)
            try:
                firebase_admin.get_app()
            except ValueError:
                firebase_admin.initialize_app(cred)
            self._messaging = messaging
        return self._messaging

    def send(self, token, message):
        messaging = self._messaging_lazy()
        msg = messaging.Message(
            notification=messaging.Notification(title="Frozen Bubble", body=message),
            token=token,
        )
        messaging.send(msg)


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


def handle_datagram(data, senders):
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
        sender.send(token, message)
        log.info("pushed to %s token=%s", platform, _redact(token))
    except Exception as exc:
        # A delivery failure must never take the relay down -- the game server
        # has already moved on and cannot be told about it anyway.
        log.error("push to %s token=%s failed: %s",
                  platform, _redact(token), exc)


def main():
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

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((host, port))
    log.info("notify-relay listening on %s:%d", host, port)

    while True:
        try:
            data, addr = sock.recvfrom(MAX_DATAGRAM)
        except KeyboardInterrupt:
            log.info("shutting down")
            return 0
        except OSError as exc:
            log.error("recvfrom failed: %s", exc)
            continue
        log.debug("datagram from %s", addr)
        handle_datagram(data, senders)


if __name__ == "__main__":
    sys.exit(main())
