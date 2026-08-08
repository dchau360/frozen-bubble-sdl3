#!/usr/bin/env python3
"""Regression test: LIST advertises each open room's max_players cap.

Phase 1 added an optional max_players argument to CREATE (rooms up to 20)
but the LIST payload never transmitted the cap, so clients could not render
"3/20". The server now appends ":<max_players>" immediately after each open
room's closing bracket, e.g. "[alice]:20". Legacy CREATE without the cap
argument defaults to 5 and must appear as "[bob]:5". The suffix sits in the
inter-bracket gap that both known parsers (Perl regex /\\[([^\\]]+)\\]/g and
the C++ find('[')/find(']') loop) already skip, so no version gate is
needed.
"""

import socket
import subprocess
import sys
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


class ServerListCapTest(unittest.TestCase):
    def setUp(self):
        if len(sys.argv) < 2:
            self.skipTest("fb-server binary path not passed as argv[1]")
        self.server_path = Path(sys.argv[1])
        if not self.server_path.exists():
            self.skipTest(f"fb-server binary not found at {self.server_path}")

        self.port = 15512
        # -d keeps the server in the foreground. Without it fb-server forks and
        # the parent exits, so Popen.kill() reaps only the parent and the real
        # daemon keeps the port -- the next test in this file would fail to bind
        # but still probe successfully, silently talking to the stale server.
        self.server = subprocess.Popen(
            [str(self.server_path), "-p", str(self.port), "-q", "-z", "-d"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
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
        self.server.kill()
        self.server.wait(timeout=5)

    def connect(self):
        s = socket.create_connection(("127.0.0.1", self.port), timeout=3.0)
        self.socks.append(s)
        recv_until(s, b"SERVER_READY")
        return s

    def test_list_reports_room_caps(self):
        # Room with an explicit 20-player cap.
        a = self.connect()
        a.sendall(b"FB/1.3 NICK capalice\nFB/1.3 CREATE capalice 20\n")
        self.assertIn(b"CREATE: OK", recv_until(a, b"CREATE:"))

        # Legacy-style CREATE without a cap argument: defaults to 5.
        b = self.connect()
        b.sendall(b"FB/1.3 NICK capbob\nFB/1.3 CREATE capbob\n")
        self.assertIn(b"CREATE: OK", recv_until(b, b"CREATE:"))

        # Third client lists the lobby.
        c = self.connect()
        c.sendall(b"FB/1.3 NICK capcarol\nFB/1.3 LIST\n")
        listing = recv_until(c, b"LIST:")

        self.assertIn(b"[capalice]:20", listing,
                      f"20-cap room missing its cap suffix (got: {listing!r})")
        self.assertIn(b"[capbob]:5", listing,
                      f"legacy room missing its default :5 suffix (got: {listing!r})")


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]] + sys.argv[2:])
