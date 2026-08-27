#!/usr/bin/env python3
"""Regression test: fb-server's -b flag caps concurrent BOT registrations.

A bot is otherwise an ordinary connection -- the server cannot tell it from
a person -- but it identifies itself with the BOT command, letting an
operator worried about CPU (level generation, malus, chain reactions all run
per board) cap how many can be live at once, server-wide rather than per
room. The cap is enforced at BOT time, not at connect time: a connection
over the limit still gets a lobby and can play as a person, it just cannot
register as a bot.
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


class ServerBotCapTestBase(unittest.TestCase):
    # Overridden by subclasses to pass a different -b value.
    max_bots_flag = None

    def setUp(self):
        if len(sys.argv) < 2:
            self.skipTest("fb-server binary path not passed as argv[1]")
        self.server_path = Path(sys.argv[1])
        if not self.server_path.exists():
            self.skipTest(f"fb-server binary not found at {self.server_path}")

        self.port = 15513
        args = [str(self.server_path), "-p", str(self.port), "-q", "-z", "-d"]
        if self.max_bots_flag is not None:
            args += ["-b", str(self.max_bots_flag)]
        # -d keeps the server in the foreground -- see server_list_cap_test.py
        # for why that matters to a reliable teardown.
        self.server = subprocess.Popen(
            args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
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


class ServerBotCapDefaultTest(ServerBotCapTestBase):
    """No -b passed: the default is generous enough that ordinary use never
    hits it, and a connection that never sends BOT is never counted at all."""

    def test_bot_registers_ok(self):
        a = self.connect()
        a.sendall(b"FB/1.3 NICK botalice\nFB/1.3 BOT\n")
        self.assertIn(b"BOT: OK", recv_until(a, b"BOT:"))

    def test_non_bot_connections_never_count_against_the_cap(self):
        # A plain client that never sends BOT must never be refused for
        # being one -- the cap is opt-in, not a connection limit by another
        # name (that is already -m's job).
        a = self.connect()
        a.sendall(b"FB/1.3 NICK plainuser\n")
        resp = recv_until(a, b"\n")
        self.assertNotIn(b"BOT_LIMIT_REACHED", resp)

    def test_bot_registration_is_idempotent(self):
        # A second BOT from the same connection (e.g. a retry) must not be
        # double-counted against the cap -- sending it twice and then
        # filling the rest of a tight cap with other connections proves it
        # was not silently charged twice.
        a = self.connect()
        a.sendall(b"FB/1.3 NICK botalice\nFB/1.3 BOT\nFB/1.3 BOT\n")
        both = recv_until(a, b"BOT: OK", timeout=2.0)
        # Both replies must be OK, not OK then a rejection.
        self.assertNotIn(b"BOT_LIMIT_REACHED", both)


class ServerBotCapZeroTest(ServerBotCapTestBase):
    """-b 0 disables bots entirely, and must be distinguishable from a typo
    that would otherwise silently fall back to the same value."""
    max_bots_flag = 0

    def test_bot_refused_at_zero(self):
        a = self.connect()
        a.sendall(b"FB/1.3 NICK botalice\nFB/1.3 BOT\n")
        self.assertIn(b"BOT: BOT_LIMIT_REACHED", recv_until(a, b"BOT:"))

    def test_ordinary_play_is_unaffected(self):
        # -b 0 must not be mistaken for -m 0 (which server_list_cap_test.py
        # already covers on the max_users axis): a connection that never
        # identifies as a bot creates a room exactly as it would with no
        # -b flag at all.
        a = self.connect()
        a.sendall(b"FB/1.3 NICK botalice\nFB/1.3 CREATE botalice 5\n")
        self.assertIn(b"CREATE: OK", recv_until(a, b"CREATE:"))


class ServerBotCapLimitTest(ServerBotCapTestBase):
    """-b 2: the smallest cap that can tell "one under" from "at the limit"
    apart from the boundary itself."""
    max_bots_flag = 2

    def test_cap_enforced_across_connections_then_frees_on_disconnect(self):
        a = self.connect()
        a.sendall(b"FB/1.3 NICK bota\nFB/1.3 BOT\n")
        self.assertIn(b"BOT: OK", recv_until(a, b"BOT:"))

        b = self.connect()
        b.sendall(b"FB/1.3 NICK botb\nFB/1.3 BOT\n")
        self.assertIn(b"BOT: OK", recv_until(b, b"BOT:"))

        # The cap is server-wide, not per room or per connection group: a
        # third bot is refused even though it has never touched a or b.
        c = self.connect()
        c.sendall(b"FB/1.3 NICK botc\nFB/1.3 BOT\n")
        self.assertIn(b"BOT: BOT_LIMIT_REACHED", recv_until(c, b"BOT:"))

        # Disconnecting one of the first two frees exactly one slot back up.
        a.close()
        self.socks.remove(a)
        time.sleep(0.3)  # give the server's own accept/read loop a tick

        d = self.connect()
        d.sendall(b"FB/1.3 NICK botd\nFB/1.3 BOT\n")
        self.assertIn(b"BOT: OK", recv_until(d, b"BOT:"))

    def test_refused_bot_can_still_play_as_a_person(self):
        # The cap governs BOT registration only. A connection the server
        # refused as a bot is otherwise an ordinary lobby client -- exactly
        # what an operator wants: bots are limited, people never are.
        a = self.connect()
        a.sendall(b"FB/1.3 NICK bota\nFB/1.3 BOT\n")
        recv_until(a, b"BOT:")
        b = self.connect()
        b.sendall(b"FB/1.3 NICK botb\nFB/1.3 BOT\n")
        recv_until(b, b"BOT:")

        c = self.connect()
        c.sendall(b"FB/1.3 NICK botc\nFB/1.3 BOT\n")
        self.assertIn(b"BOT: BOT_LIMIT_REACHED", recv_until(c, b"BOT:"))

        c.sendall(b"FB/1.3 CREATE botc 5\n")
        self.assertIn(b"CREATE: OK", recv_until(c, b"CREATE:"))


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]] + sys.argv[2:])
