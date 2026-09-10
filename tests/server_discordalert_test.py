#!/usr/bin/env python3
"""Discord join-alert relay hookup: the join hook and the datagram format.

Replaces server_notify_test.py (the old "follow a server" push feature,
removed). Unlike that feature this one has no registry -- it's a single
operator's standing webhook, not per-player registrations that have to
outlive a connection -- so there is nothing to persist and nothing to
validate about a registration command. What's left to cover is narrower:
does every room join fire exactly one datagram, in the right format, with
the joining player's IP and self-reported geolocation (when there is one)?

This drives the real binary over the real protocol and stands in for the
relay with a plain UDP socket, so it covers the parts that unit-testing a
formatting function alone would miss: the add_player() hook actually firing,
and the wire format field order/split behavior real fb-server output takes.
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


class ServerDiscordAlertTest(unittest.TestCase):
    def setUp(self):
        if len(sys.argv) < 2:
            self.skipTest("fb-server binary path not passed as argv[1]")
        self.server_path = Path(sys.argv[1])
        if not self.server_path.exists():
            self.skipTest(f"fb-server binary not found at {self.server_path}")

        # Stand-in for discord-relay: a bound UDP socket the server fires at.
        self.relay = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.relay.bind(("127.0.0.1", 0))
        self.relay.settimeout(0.2)
        self.relay_port = self.relay.getsockname()[1]

        self.tmpdir = tempfile.TemporaryDirectory()

        env = dict(os.environ)
        env["FB_SERVER_DISCORD_RELAY"] = f"127.0.0.1:{self.relay_port}"
        # Keep stats out of the developer's real home directory.
        env["FB_SERVER_STATS_FILE"] = str(Path(self.tmpdir.name) / "stats.dat")

        self.port = 15518
        # -d keeps the server in the foreground. Without it fb-server forks and
        # the parent exits, so Popen.kill() reaps only the parent and the real
        # daemon keeps the port -- every later test in this file would then
        # silently talk to the first test's server.
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

    def test_join_fires_one_datagram_with_ip_and_no_geoloc(self):
        a = self.connect()
        a.sendall(b"FB/1.3 NICK alertroom\nFB/1.3 CREATE alertroom\n")
        self.assertIn(b"CREATE: OK", recv_until(a, b"CREATE:"))

        # Creating a room is not a join -- only somebody else arriving is.
        self.assertEqual(self.drain_relay(), [])

        b = self.connect()
        b.sendall(b"FB/1.3 NICK joiner1\nFB/1.3 JOIN alertroom joiner1\n")
        self.assertIn(b"JOIN: OK", recv_until(b, b"JOIN:"))

        fired = self.drain_relay()
        self.assertEqual(len(fired), 1, f"expected exactly one datagram, got {fired!r}")
        parts = fired[0].split("|", 4)
        self.assertEqual(parts[0], "JOIN")
        self.assertEqual(parts[1], "joiner1")
        self.assertEqual(parts[2], "127.0.0.1")
        self.assertEqual(parts[3], "", "no GEOLOC was sent, so this field must be empty")
        self.assertTrue(parts[4], "servername field must not be empty")

    def test_join_carries_self_reported_geoloc_when_sent_first(self):
        a = self.connect()
        a.sendall(b"FB/1.3 NICK geoloroom\nFB/1.3 CREATE geoloroom\n")
        self.assertIn(b"CREATE: OK", recv_until(a, b"CREATE:"))

        b = self.connect()
        b.sendall(b"FB/1.3 NICK joiner2\nFB/1.3 GEOLOC 37.77:-122.42\n")
        recv_until(b, b"GEOLOC:")
        b.sendall(b"FB/1.3 JOIN geoloroom joiner2\n")
        self.assertIn(b"JOIN: OK", recv_until(b, b"JOIN:"))

        fired = self.drain_relay()
        self.assertEqual(len(fired), 1, f"expected exactly one datagram, got {fired!r}")
        parts = fired[0].split("|", 4)
        self.assertEqual(parts[1], "joiner2")
        self.assertEqual(parts[3], "37.77:-122.42")

    def test_every_join_fires_no_cooldown(self):
        # Unlike the old per-device follow feature (which throttled repeat
        # notifications to the same device), this is one operator's channel:
        # every join is its own event, with nothing to suppress.
        a = self.connect()
        a.sendall(b"FB/1.3 NICK tworoom\nFB/1.3 CREATE tworoom\n")
        self.assertIn(b"CREATE: OK", recv_until(a, b"CREATE:"))

        b = self.connect()
        b.sendall(b"FB/1.3 NICK joiner3\nFB/1.3 JOIN tworoom joiner3\n")
        self.assertIn(b"JOIN: OK", recv_until(b, b"JOIN:"))
        self.assertEqual(len(self.drain_relay()), 1)

        c = self.connect()
        c.sendall(b"FB/1.3 NICK joiner4\nFB/1.3 JOIN tworoom joiner4\n")
        self.assertIn(b"JOIN: OK", recv_until(c, b"JOIN:"))
        fired = self.drain_relay()
        self.assertEqual(len(fired), 1)
        self.assertEqual(fired[0].split("|", 4)[1], "joiner4")


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]] + sys.argv[2:])
