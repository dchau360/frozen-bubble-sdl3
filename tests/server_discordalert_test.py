#!/usr/bin/env python3
"""Discord join-alert relay hookup: the arrival hook and the datagram format.

Replaces server_notify_test.py (the old "follow a server" push feature,
removed). Unlike that feature this one has no registry -- it's a single
operator's standing webhook, not per-player registrations that have to
outlive a connection -- so there is nothing to persist and nothing to
validate about a registration command. What's left to cover is narrower:
does every player arriving on the server fire exactly one datagram, in the
right format?

"Arriving" means the first accepted NICK, not a room join. That distinction
is the point of most of this file. A player alone in a room they just made is
precisely who an alert should summon company for, and by the time somebody
has joined their room the two have already found each other -- so the hook
sits in the NICK handler, and the tests below pin it there from both
directions: it fires for a player who never touches a room at all, and it
does not fire a second time when one is finally joined.

This drives the real binary over the real protocol and stands in for the
relay with a plain UDP socket, so it covers the parts that unit-testing a
formatting function alone would miss: the hook actually firing, and the wire
format field order/split behavior real fb-server output takes.
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


class _FbServerTestBase(unittest.TestCase):
    """Boots a real fb-server per test with a UDP socket standing in for
    discord-relay. Shared by ServerDiscordAlertTest (arrival alerts) and
    ServerDiscordResultAlertTest (round-end alerts) -- same server, same
    stand-in relay, different wire messages driving it."""

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


class ServerDiscordAlertTest(_FbServerTestBase):
    def test_nick_alone_fires_one_datagram_with_ip(self):
        # The whole point of the feature: a player who has done nothing but
        # arrive is already worth announcing. No room is created or joined
        # anywhere in this test.
        a = self.connect()
        a.sendall(b"FB/1.3 NICK arriver1\n")
        self.assertIn(b"NICK: OK", recv_until(a, b"NICK:"))

        fired = self.drain_relay()
        self.assertEqual(len(fired), 1, f"expected exactly one datagram, got {fired!r}")
        parts = fired[0].split("|", 4)
        self.assertEqual(parts[0], "JOIN")
        self.assertEqual(parts[1], "arriver1")
        self.assertEqual(parts[2], "127.0.0.1")
        self.assertEqual(parts[3], "", "GEOLOC arrives after NICK, so this is empty")
        self.assertTrue(parts[4], "servername field must not be empty")

    def test_bare_connection_without_a_nick_fires_nothing(self):
        # Opening a TCP connection is not arriving -- a port scanner, a health
        # check, or the server list's own probe all get this far. The player
        # is announced when they have a name, because that is when the lobby
        # can see them.
        self.connect()
        self.assertEqual(self.drain_relay(), [])

    def test_creating_and_joining_a_room_fire_nothing_further(self):
        # Both halves of the old behavior, pinned as silent. The arrival
        # datagram for each player is drained as it happens, so anything left
        # at the end came from the room activity itself.
        a = self.connect()
        a.sendall(b"FB/1.3 NICK host1\n")
        self.assertIn(b"NICK: OK", recv_until(a, b"NICK:"))
        self.assertEqual(len(self.drain_relay()), 1)

        a.sendall(b"FB/1.3 CREATE host1\n")
        self.assertIn(b"CREATE: OK", recv_until(a, b"CREATE:"))
        self.assertEqual(self.drain_relay(), [], "creating a room is not an arrival")

        b = self.connect()
        b.sendall(b"FB/1.3 NICK guest1\n")
        self.assertIn(b"NICK: OK", recv_until(b, b"NICK:"))
        self.assertEqual(len(self.drain_relay()), 1)

        b.sendall(b"FB/1.3 JOIN host1 guest1\n")
        self.assertIn(b"JOIN: OK", recv_until(b, b"JOIN:"))
        self.assertEqual(self.drain_relay(), [],
                         "the room join must not fire a second datagram for a "
                         "player already announced on arrival")

    def test_rename_on_the_same_connection_does_not_re_announce(self):
        # A second NICK is a rename, not an arrival. Firing again would put
        # one player in the channel twice under two names.
        a = self.connect()
        a.sendall(b"FB/1.3 NICK before\n")
        self.assertIn(b"NICK: OK", recv_until(a, b"NICK:"))
        fired = self.drain_relay()
        self.assertEqual(len(fired), 1)
        self.assertEqual(fired[0].split("|", 4)[1], "before")

        a.sendall(b"FB/1.3 NICK after\n")
        recv_until(a, b"NICK:")
        self.assertEqual(self.drain_relay(), [])

    def test_reconnect_replacing_a_ghost_does_not_re_announce(self):
        # A silent TCP drop leaves the old fd seated until gracetime fires, so
        # the redial arrives while the previous connection is still listed and
        # gets it evicted (the "ghost player fix" in the NICK handler). That
        # is the same player coming back, not a new arrival -- without this
        # guard a flapping connection redials the channel every time the link
        # blinks.
        a = self.connect()
        a.sendall(b"FB/1.3 NICK flappy\n")
        self.assertIn(b"NICK: OK", recv_until(a, b"NICK:"))
        self.assertEqual(len(self.drain_relay()), 1)

        # A ghost is specifically a connection that went *quiet*, not one that
        # closed: closing it (even with SO_LINGER forcing an RST) hands the
        # server an event it acts on at once, and the fd is gone before the
        # redial lands -- no ghost, nothing to evict. So leave the socket open
        # and say nothing until conn_recently_active() stops vouching for it,
        # which is a 10-second window (server/net.c, widened 2026-09-11 to
        # absorb startup jitter across freshly-launched clients that
        # collide on the same default nickname -- see the comment there).
        time.sleep(10.5)

        b = self.connect()
        b.sendall(b"FB/1.3 NICK flappy\n")
        self.assertIn(b"NICK: OK", recv_until(b, b"NICK:"))
        self.assertEqual(self.drain_relay(), [],
                         "a reconnect that evicted its own ghost is not a new "
                         "arrival")

    def test_every_arrival_fires_no_cooldown(self):
        # Unlike the old per-device follow feature (which throttled repeat
        # notifications to the same device), this is one operator's channel:
        # every distinct player arriving is its own event, with nothing to
        # suppress. Only the two non-arrivals above are filtered.
        a = self.connect()
        a.sendall(b"FB/1.3 NICK arriver3\n")
        self.assertIn(b"NICK: OK", recv_until(a, b"NICK:"))
        self.assertEqual(len(self.drain_relay()), 1)

        b = self.connect()
        b.sendall(b"FB/1.3 NICK arriver4\n")
        self.assertIn(b"NICK: OK", recv_until(b, b"NICK:"))
        fired = self.drain_relay()
        self.assertEqual(len(fired), 1)
        self.assertEqual(fired[0].split("|", 4)[1], "arriver4")

    def test_rejected_nick_fires_nothing(self):
        # is_nick_ok() refuses it, so nobody arrived. The alert must hang off
        # the success path, not off the command being received.
        #
        # No space in the bad nick: the handler truncates args at the first
        # one, so "bad nick!" would arrive at the validator as plain "bad" and
        # be accepted, testing nothing.
        a = self.connect()
        a.sendall(b"FB/1.3 NICK bad!nick\n")
        recv_until(a, b"NICK:")
        self.assertEqual(self.drain_relay(), [])


class ServerDiscordResultAlertTest(_FbServerTestBase):
    """Round-end alerts: the 'F' opcode sniffed in process_msg_prio_
    (game.c), fired at discord-relay as a RESULT datagram alongside the
    existing JOIN one.

    Reaching that sniff needs a room in GAME_STATUS_PLAYING with the sending
    connection in "prio" mode (add_prio(), triggered by OK_GAME_START) --
    prio is what makes a raw, non-"FB/"-prefixed line reach
    process_msg_prio instead of the text command parser (see net.c). Once
    there, the real client's own framing ({seat id byte}{opcode}{payload}\\n)
    doesn't matter for the id byte specifically: the server stamps over
    whatever byte 0 is with the sender's real seat id before relaying, so
    these tests use a placeholder ('?') the same way other prio opcodes in
    game.c's own comments do (e.g. "?p\\n", "?!\\n").
    """

    def _start_two_player_game(self, room, guest_nick="guest1", mode=None):
        """Gets a 2-player room to GAME_STATUS_PLAYING with player A (the
        room's creator) in prio mode, so raw round-end bytes sent on A's own
        connection reach the sniff. Drains every datagram fired getting
        there, so each test starts from a clean relay. Returns (a, b).

        The creator's roster identity is `room` itself, not whatever NICK
        they used to connect: CREATE's argument doubles as both the room's
        name (what JOIN and LIST look it up by) and g->players_nick[0] --
        the same quirk the original fb protocol has always had. A joiner's
        roster identity is JOIN's second argument instead, which also
        overwrites their nick[fd]. Both are capped at 10 chars by
        is_nick_ok(), same as a lobby NICK, so callers must keep `room` and
        `guest_nick` within that or truncation will desync the roster
        assertions from what was actually requested.
        """
        a = self.connect()
        a.sendall(b"FB/1.3 NICK creator\n")
        self.assertIn(b"NICK: OK", recv_until(a, b"NICK:"))

        b = self.connect()
        b.sendall(f"FB/1.3 NICK {guest_nick}\n".encode())
        self.assertIn(b"NICK: OK", recv_until(b, b"NICK:"))

        self.drain_relay()  # the two NICK arrivals above, not under test here

        a.sendall(f"FB/1.3 CREATE {room}\n".encode())
        self.assertIn(b"CREATE: OK", recv_until(a, b"CREATE:"))

        b.sendall(f"FB/1.3 JOIN {room} {guest_nick}\n".encode())
        self.assertIn(b"JOIN: OK", recv_until(b, b"JOIN:"))

        if mode is not None:
            a.sendall(f"FB/1.3 SETOPTIONS GAMEMODE:{mode}\n".encode())
            self.assertIn(b"SETOPTIONS: OK", recv_until(a, b"SETOPTIONS:"))

        a.sendall(b"FB/1.3 START\n")
        self.assertIn(b"START: OK", recv_until(a, b"START:"))

        a.sendall(b"FB/1.3 OK_GAME_START\n")
        self.assertIn(b"OK_GAME_START: OK", recv_until(a, b"OK_GAME_START:"))

        self.assertEqual(self.drain_relay(), [],
                         "none of CREATE/JOIN/SETOPTIONS/START/OK_GAME_START "
                         "should fire a result alert on their own")
        return a, b

    def test_round_win_fires_result_with_mode_and_roster(self):
        a, b = self._start_two_player_game("winroom", "guest1", mode=2)  # Race

        # The winner need not be the sender -- any client in the room can be
        # the one whose 'F' lands first (see the multi-sender comment on the
        # sniff itself), so this deliberately reports the OTHER player.
        a.sendall(b"?Fguest1\n")
        fired = self.drain_relay()
        self.assertEqual(len(fired), 1, f"expected exactly one RESULT datagram, got {fired!r}")
        parts = fired[0].split("|", 5)
        self.assertEqual(parts[0], "RESULT")
        self.assertTrue(parts[1].isdigit(), "game_id must be a plain int")
        self.assertEqual(parts[2], "2", "GAMEMODE:2 (Race) should flow through")
        self.assertEqual(parts[3], "guest1")
        self.assertEqual(set(parts[4].split(",")), {"winroom", "guest1"})
        self.assertTrue(parts[5], "servername field must not be empty")

    def test_draw_fires_with_an_empty_winner_field(self):
        a, b = self._start_two_player_game("drawroom")
        a.sendall(b"?F\n")  # bare F -- fb-server's own draw signal
        fired = self.drain_relay()
        self.assertEqual(len(fired), 1)
        parts = fired[0].split("|", 5)
        self.assertEqual(parts[0], "RESULT")
        self.assertEqual(parts[3], "", "a draw must post with no winner name")

    def test_second_f_before_any_n_does_not_re_fire(self):
        # Multiple clients can each send their own 'F' for the same round --
        # the real client's CommitRoundWin ignores every one after the
        # first; result_posted is this function's equivalent.
        a, b = self._start_two_player_game("deduproom")
        a.sendall(b"?Fwinner\n")
        self.assertEqual(len(self.drain_relay()), 1)

        a.sendall(b"?Fwinner\n")
        self.assertEqual(self.drain_relay(), [],
                         "a second F before any 'n' must not double-post "
                         "the same round")

        a.sendall(b"?n\n")  # ready for next round -- resets the dedup guard
        self.assertEqual(self.drain_relay(), [], "'n' itself must not fire anything")

        a.sendall(b"?Fwinner2\n")
        fired = self.drain_relay()
        self.assertEqual(len(fired), 1, "a new round's F after 'n' must post again")
        self.assertEqual(fired[0].split("|", 5)[3], "winner2")

    def test_pipe_in_winner_payload_cannot_corrupt_the_datagram_fields(self):
        # winner is lifted straight from a client's 'F' payload with none of
        # is_nick_ok()'s charset restriction behind it -- unlike every other
        # field in this datagram. A literal '|' in it would otherwise shift
        # this and every field after it one column to the right.
        a, b = self._start_two_player_game("pipetest")
        a.sendall(b"?Fevil|injected\n")
        fired = self.drain_relay()
        self.assertEqual(len(fired), 1)
        parts = fired[0].split("|", 5)
        self.assertEqual(parts[0], "RESULT")
        self.assertEqual(parts[3], "evil injected")
        self.assertEqual(set(parts[4].split(",")), {"pipetest", "guest1"})

    def test_game_id_is_stable_across_rounds_of_the_same_room(self):
        # The whole point of carrying game_id at all: the relay groups every
        # round from one room into a single Discord thread by this value
        # (see server/discord-relay/relay.py's _room_threads), so it must
        # not change just because a new round started.
        a, b = self._start_two_player_game("stableroom")

        a.sendall(b"?Fwinner\n")
        first_game_id = self.drain_relay()[0].split("|", 5)[1]

        a.sendall(b"?n\n")
        self.drain_relay()

        a.sendall(b"?Fwinner2\n")
        second_game_id = self.drain_relay()[0].split("|", 5)[1]

        self.assertEqual(first_game_id, second_game_id)

    def test_different_rooms_get_different_game_ids(self):
        a1, b1 = self._start_two_player_game("roomone", "guest1")
        a2, b2 = self._start_two_player_game("roomtwo", "guest2")

        a1.sendall(b"?Fwinner\n")
        game_id_1 = self.drain_relay()[0].split("|", 5)[1]

        a2.sendall(b"?Fwinner\n")
        game_id_2 = self.drain_relay()[0].split("|", 5)[1]

        self.assertNotEqual(game_id_1, game_id_2)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]] + sys.argv[2:])
