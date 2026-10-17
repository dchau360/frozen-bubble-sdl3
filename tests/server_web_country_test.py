#!/usr/bin/env python3
"""Browser players get a country from the address nginx forwards.

fb-server sees nginx's docker address for a WebSocket player, so the real
client address arrives in X-Forwarded-For. server/ws.c keeps it only when the
TCP peer is a private/loopback address (a trusted proxy), and server/geoip.c
maps it to a country through the DB-IP Lite CSV named by FB_SERVER_GEOIP_FILE.
The country lands in country_tag[], the same slot the COUNTRY command fills
for native clients, so it shows in the Discord JOIN datagram and the highscores.

Pins: a forwarded address in a listed range yields its country; one outside any
range yields none; no header yields none; a garbage header yields none.
"""

import base64
import os
import socket
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

from testports import free_tcp_port

CSV = (
    '"203.0.113.0","203.0.113.255","NZ"\n'
    '"198.51.100.0","198.51.100.255","DE"\n'
    '"2001:db8::","2001:db8::ffff","JP"\n'
)


def ws_frame(text):
    payload = text.encode()
    mask = os.urandom(4)
    hdr = bytes([0x81])
    n = len(payload)
    assert n < 126
    hdr += bytes([0x80 | n]) + mask
    return hdr + bytes(b ^ mask[i % 4] for i, b in enumerate(payload))


class WebCountryTest(unittest.TestCase):
    def setUp(self):
        if len(sys.argv) < 2:
            self.skipTest("fb-server binary path not passed as argv[1]")
        self.server_path = Path(sys.argv[1])
        if not self.server_path.exists():
            self.skipTest(f"fb-server binary not found at {self.server_path}")
        self.relay = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.relay.bind(("127.0.0.1", 0))
        self.relay.settimeout(0.3)
        self.tmpdir = tempfile.TemporaryDirectory()
        geoip = Path(self.tmpdir.name) / "geoip.csv"
        geoip.write_text(CSV)
        env = dict(os.environ)
        env["FB_SERVER_DISCORD_RELAY"] = f"127.0.0.1:{self.relay.getsockname()[1]}"
        env["FB_SERVER_GEOIP_FILE"] = str(geoip)
        env["FB_SERVER_STATS_FILE"] = str(Path(self.tmpdir.name) / "stats.dat")
        env["FB_SERVER_WEEKLY_FILE"] = str(Path(self.tmpdir.name) / "weekly.dat")
        self.port = free_tcp_port()
        self.server = subprocess.Popen(
            [str(self.server_path), "-p", str(self.port), "-q", "-z", "-d"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            try:
                socket.create_connection(("127.0.0.1", self.port), timeout=0.2).close()
                break
            except OSError:
                time.sleep(0.05)
        else:
            self.server.kill()
            self.fail("server never started listening")

    def tearDown(self):
        self.relay.close()
        self.server.kill()
        self.server.wait(timeout=5)
        self.tmpdir.cleanup()

    def join_country(self, xff, nick):
        """Connect as a browser would, return the country field of the JOIN."""
        s = socket.create_connection(("127.0.0.1", self.port), timeout=3.0)
        try:
            key = base64.b64encode(os.urandom(16)).decode()
            lines = ["GET / HTTP/1.1", "Host: x", "Upgrade: websocket",
                     "Connection: Upgrade", f"Sec-WebSocket-Key: {key}",
                     "Sec-WebSocket-Version: 13"]
            if xff is not None:
                lines.append(f"X-Forwarded-For: {xff}")
            s.sendall(("\r\n".join(lines) + "\r\n\r\n").encode())
            got = b""
            deadline = time.monotonic() + 3
            while b"\r\n\r\n" not in got and time.monotonic() < deadline:
                got += s.recv(4096)
            self.assertIn(b"101", got)
            time.sleep(0.3)
            s.sendall(ws_frame(f"FB/1.3 NICK {nick}\n"))
            for _ in range(20):
                try:
                    d, _a = self.relay.recvfrom(2048)
                except socket.timeout:
                    continue
                parts = d.decode().split("|")
                if parts[0] == "JOIN" and parts[1] == nick:
                    return parts[5]
            self.fail("no JOIN datagram")
        finally:
            s.close()

    def test_listed_ipv4_gets_its_country(self):
        self.assertEqual(self.join_country("203.0.113.9", "kiwi"), "NZ")

    def test_second_range(self):
        self.assertEqual(self.join_country("198.51.100.200", "hans"), "DE")

    def test_ipv6(self):
        self.assertEqual(self.join_country("2001:db8::42", "sakura"), "JP")

    def test_chain_uses_first_address(self):
        self.assertEqual(self.join_country("203.0.113.9, 10.0.0.2", "chain"), "NZ")

    def test_unlisted_address_gets_none(self):
        self.assertEqual(self.join_country("192.0.2.1", "nowhere"), "")

    def test_no_header_gets_none(self):
        self.assertEqual(self.join_country(None, "plain"), "")

    def test_garbage_header_gets_none(self):
        self.assertEqual(self.join_country("not-an-ip!", "junk"), "")


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
