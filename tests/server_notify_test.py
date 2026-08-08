#!/usr/bin/env python3
"""Follow-a-server push notifications: registration, join hook, and cooldown.

A "follow" has to outlive the connection that created it -- the entire point
is to reach a device after it has closed the app -- so unlike every other
piece of per-connection state in the server, a registration lives in a
flat-file-backed table (server/notify.c) rather than in the fd-keyed arrays
that conn_terminated() frees.

This drives the real binary over the real protocol and stands in for the
relay with a plain UDP socket, so it covers the parts that unit-testing the
table alone would miss: the command parsing, the add_player() hook, the
datagram format, and the cooldown that stops a busy server from turning into
a banner-spam machine.
"""

import os
import socket
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path


def recv_until(sock, token, timeout=5.0):
    sock.setblocking(False)
    got = b""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline and token not in got:
        try:
            d = sock.recv(4096)
            if d:
                got += d
        except (BlockingIOError, socket.error):
            pass
        time.sleep(0.02)
    return got


class ServerNotifyTest(unittest.TestCase):
    def setUp(self):
        if len(sys.argv) < 2:
            self.skipTest("fb-server binary path not passed as argv[1]")
        self.server_path = Path(sys.argv[1])
        if not self.server_path.exists():
            self.skipTest(f"fb-server binary not found at {self.server_path}")

        # Stand-in for notify-relay: a bound UDP socket the server fires at.
        self.relay = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.relay.bind(("127.0.0.1", 0))
        self.relay.settimeout(0.2)
        self.relay_port = self.relay.getsockname()[1]

        self.tmpdir = tempfile.TemporaryDirectory()
        self.notify_file = Path(self.tmpdir.name) / "notify.dat"

        env = dict(os.environ)
        env["FB_SERVER_NOTIFY_RELAY"] = f"127.0.0.1:{self.relay_port}"
        env["FB_SERVER_NOTIFY_FILE"] = str(self.notify_file)
        # Long enough that the second join in the cooldown test is definitely
        # inside the window, without making the test itself slow.
        env["FB_SERVER_NOTIFY_COOLDOWN_SECONDS"] = "3600"
        # Keep stats out of the developer's real home directory.
        env["FB_SERVER_STATS_FILE"] = str(Path(self.tmpdir.name) / "stats.dat")

        self.port = 15517
        # -d keeps the server in the foreground. Without it fb-server forks and
        # the parent exits, so Popen.kill() reaps only the parent and the real
        # daemon keeps the port -- every later test in this file would then
        # silently talk to the first test's server, with its own notify file
        # and its already-spent cooldown.
        self.server = subprocess.Popen(
            [str(self.server_path), "-p", str(self.port), "-q", "-z", "-d"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            env=env,
        )
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            try:
                probe = socket.create_connection(("127.0.0.1", self.port), timeout=0.2)
                probe.close()
                break
            except OSError:
                time.sleep(0.05)
        else:
            self.server.kill()
            self.fail("server never started listening")
        self.socks = []

    def tearDown(self):
        for s in self.socks:
            s.close()
        self.relay.close()
        self.server.kill()
        self.server.wait(timeout=5)
        self.tmpdir.cleanup()

    def connect(self):
        s = socket.create_connection(("127.0.0.1", self.port), timeout=3.0)
        self.socks.append(s)
        recv_until(s, b"SERVER_READY")
        return s

    def drain_relay(self):
        """Every datagram waiting on the stand-in relay, as decoded strings."""
        out = []
        deadline = time.monotonic() + 0.8
        while time.monotonic() < deadline:
            try:
                data, _ = self.relay.recvfrom(2048)
                out.append(data.decode())
            except socket.timeout:
                if out:
                    break
        return out

    def test_registration_is_validated(self):
        a = self.connect()

        a.sendall(b"FB/1.3 NOTIFYREG\n")
        self.assertIn(b"MISSING_ARGUMENTS", recv_until(a, b"NOTIFYREG:"))

        # Platform but no token.
        a.sendall(b"FB/1.3 NOTIFYREG ios\n")
        self.assertIn(b"MISSING_ARGUMENTS", recv_until(a, b"NOTIFYREG:"))

        # Only the two platforms that can actually receive a push are accepted;
        # anything else would sit in the table forever, never deliverable.
        a.sendall(b"FB/1.3 NOTIFYREG windows sometoken\n")
        self.assertIn(b"INVALID_PLATFORM", recv_until(a, b"NOTIFYREG:"))

        a.sendall(b"FB/1.3 NOTIFYREG ios goodtoken1\n")
        self.assertIn(b"NOTIFYREG: OK", recv_until(a, b"NOTIFYREG:"))

    def test_join_notifies_followers_once_per_cooldown(self):
        a = self.connect()
        a.sendall(b"FB/1.3 NOTIFYREG ios iostoken1\n")
        self.assertIn(b"NOTIFYREG: OK", recv_until(a, b"NOTIFYREG:"))
        a.sendall(b"FB/1.3 NOTIFYREG android droidtoken1\n")
        self.assertIn(b"NOTIFYREG: OK", recv_until(a, b"NOTIFYREG:"))

        # Registering is not itself an event worth notifying about.
        self.assertEqual(self.drain_relay(), [])

        a.sendall(b"FB/1.3 NICK notifyroom\nFB/1.3 CREATE notifyroom\n")
        self.assertIn(b"CREATE: OK", recv_until(a, b"CREATE:"))

        # Creating a room is not a join either -- only somebody else arriving is.
        self.assertEqual(self.drain_relay(), [])

        b = self.connect()
        b.sendall(b"FB/1.3 NICK joiner1\nFB/1.3 JOIN notifyroom joiner1\n")
        self.assertIn(b"JOIN: OK", recv_until(b, b"JOIN:"))

        fired = self.drain_relay()
        self.assertEqual(len(fired), 2,
                         f"expected one datagram per registered device, got {fired!r}")
        platforms = sorted(d.split("|")[1] for d in fired)
        self.assertEqual(platforms, ["android", "ios"])
        for datagram in fired:
            parts = datagram.split("|", 3)
            self.assertEqual(parts[0], "NOTIFY")
            self.assertIn(parts[2], ("iostoken1", "droidtoken1"))
            self.assertTrue(parts[3], "notification body must not be empty")

        # A second join inside the cooldown must stay silent, or a busy server
        # notifies on every single arrival.
        c = self.connect()
        c.sendall(b"FB/1.3 NICK joiner2\nFB/1.3 JOIN notifyroom joiner2\n")
        self.assertIn(b"JOIN: OK", recv_until(c, b"JOIN:"))
        self.assertEqual(self.drain_relay(), [],
                         "cooldown did not suppress the second join")

    def test_unregister_stops_notifications(self):
        a = self.connect()
        a.sendall(b"FB/1.3 NOTIFYREG ios staying\n")
        self.assertIn(b"NOTIFYREG: OK", recv_until(a, b"NOTIFYREG:"))
        a.sendall(b"FB/1.3 NOTIFYREG android leaving\n")
        self.assertIn(b"NOTIFYREG: OK", recv_until(a, b"NOTIFYREG:"))

        a.sendall(b"FB/1.3 NOTIFYUNREG leaving\n")
        self.assertIn(b"NOTIFYUNREG: OK", recv_until(a, b"NOTIFYUNREG:"))

        a.sendall(b"FB/1.3 NICK unregroom\nFB/1.3 CREATE unregroom\n")
        self.assertIn(b"CREATE: OK", recv_until(a, b"CREATE:"))
        b = self.connect()
        b.sendall(b"FB/1.3 NICK joiner3\nFB/1.3 JOIN unregroom joiner3\n")
        self.assertIn(b"JOIN: OK", recv_until(b, b"JOIN:"))

        fired = self.drain_relay()
        self.assertEqual(len(fired), 1, f"unregistered device still notified: {fired!r}")
        self.assertEqual(fired[0].split("|")[2], "staying")

    def test_registrations_persist_to_disk(self):
        a = self.connect()
        a.sendall(b"FB/1.3 NOTIFYREG ios persisted1\n")
        self.assertIn(b"NOTIFYREG: OK", recv_until(a, b"NOTIFYREG:"))

        # Written immediately, not only at shutdown: a server that is killed
        # rather than stopped cleanly must not lose everybody's follows.
        deadline = time.monotonic() + 3.0
        while time.monotonic() < deadline and not self.notify_file.exists():
            time.sleep(0.05)
        self.assertTrue(self.notify_file.exists(),
                        "registration was never written to the notify file")
        contents = self.notify_file.read_text()
        self.assertIn("persisted1", contents)
        self.assertIn("ios", contents)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]] + sys.argv[2:])
