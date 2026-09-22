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

    def drain_relay(self, timeout=0.8):
        """Every datagram waiting on the stand-in relay, as decoded strings.

        timeout defaults to comfortably above ordinary scheduling jitter but
        well under PENDING_STATS_TIMEOUT_SECS (game.c) -- callers waiting out
        that deadline (see ServerDiscordResultAlertTest's timeout-path test)
        pass a longer one explicitly.
        """
        out = []
        deadline = time.monotonic() + timeout
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

    def _start_two_player_game(self, room, guest_nick="guest1", mode=None, victories_limit=None):
        """Gets a 2-player room to GAME_STATUS_PLAYING with BOTH connections
        in prio mode, so raw round-end bytes sent on either one reach the
        sniff -- add_prio() (net.c) is per-connection, armed only by that
        connection's own OK_GAME_START, so B needs to send it too, not just
        the room's creator, or B's own 'S' reports (see _report_stats below)
        would fall through to the ordinary text-command parser instead of
        process_msg_prio_ and never register. Drains every datagram fired
        getting there, so each test starts from a clean relay. Returns (a, b).

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

        if mode is not None or victories_limit is not None:
            options = []
            if mode is not None:
                options.append(f"GAMEMODE:{mode}")
            if victories_limit is not None:
                options.append(f"VICTORIESLIMIT:{victories_limit}")
            a.sendall(f"FB/1.3 SETOPTIONS {','.join(options)}\n".encode())
            self.assertIn(b"SETOPTIONS: OK", recv_until(a, b"SETOPTIONS:"))

        a.sendall(b"FB/1.3 START\n")
        self.assertIn(b"START: OK", recv_until(a, b"START:"))

        a.sendall(b"FB/1.3 OK_GAME_START\n")
        self.assertIn(b"OK_GAME_START: OK", recv_until(a, b"OK_GAME_START:"))

        b.sendall(b"FB/1.3 OK_GAME_START\n")
        self.assertIn(b"OK_GAME_START: OK", recv_until(b, b"OK_GAME_START:"))

        self.assertEqual(self.drain_relay(), [],
                         "none of CREATE/JOIN/SETOPTIONS/START/OK_GAME_START "
                         "should fire a result alert on their own")
        return a, b

    def _report_stats(self, conn, popped=0):
        """Send this seat's own round-end 'S' opcode with a plausible
        popped count: S{fired}:{popped}:{sent}:{recv}:{kills}:{blocked}
        (src/bubblegame_net.cpp) -- only the 'popped' field (2nd) is read
        server-side (game.c), so the rest are filler zeros. A value this
        small needs no matching 'f' opcodes sent first: the plausibility
        ceiling's flat POP_CEILING_GRACE alone already covers anything <= 10
        even with zero server-observed shots this round (see
        MAX_POPS_PER_SHOT/POP_CEILING_GRACE, game.c)."""
        conn.sendall(f"?S0:{popped}:0:0:0:0\n".encode())

    def _finish_round(self, a, b, winner="", popped_a=0, popped_b=0):
        """Send 'F' then both seats' 'S' -- the deferred post's fast path
        (see maybe_fire_pending_result(), game.c), so the RESULT (and any
        MATCH) datagram fires immediately instead of waiting out
        PENDING_STATS_TIMEOUT_SECS. Exists so every test below that isn't
        specifically about the deferred timing itself can ignore it, the
        same way they could when the post was still synchronous. Returns
        drain_relay()'s result."""
        a.sendall(f"?F{winner}\n".encode())
        self._report_stats(a, popped_a)
        self._report_stats(b, popped_b)
        return self.drain_relay()

    def test_round_win_fires_result_with_mode_and_roster(self):
        a, b = self._start_two_player_game("winroom", "guest1", mode=2)  # Race

        # The winner need not be the sender -- any client in the room can be
        # the one whose 'F' lands first (see the multi-sender comment on the
        # sniff itself), so this deliberately reports the OTHER player. Both
        # seats report stats so this exercises the fast path (see
        # _finish_round) rather than incidentally waiting out the deadline.
        fired = self._finish_round(a, b, "guest1", popped_a=3, popped_b=7)
        self.assertEqual(len(fired), 1, f"expected exactly one RESULT datagram, got {fired!r}")
        parts = fired[0].split("|", 12)
        self.assertEqual(parts[0], "RESULT")
        self.assertTrue(parts[1].isdigit(), "game_id must be a plain int")
        self.assertEqual(parts[2], "1", "first round in this room")
        self.assertEqual(parts[3], "2", "GAMEMODE:2 (Race) should flow through")
        self.assertEqual(parts[4], "guest1")
        roster = parts[5].split(",")
        wins = parts[6].split(",")
        self.assertEqual(set(roster), {"winroom", "guest1"})
        self.assertEqual(dict(zip(roster, wins)), {"winroom": "0", "guest1": "1"},
                         "wins is index-aligned with roster and already reflects "
                         "this round's winner")
        self.assertEqual(parts[7], "0", "no VICTORIESLIMIT was set for this room")
        popped = dict(zip(roster, parts[11].split(",")))
        self.assertEqual(popped, {"winroom": "3", "guest1": "7"},
                         "popped is index-aligned with roster too, and both "
                         "seats reported in time for the fast path")
        self.assertTrue(parts[12], "servername field must not be empty")

    def test_draw_fires_with_an_empty_winner_field(self):
        a, b = self._start_two_player_game("drawroom")
        fired = self._finish_round(a, b, "")  # bare F -- fb-server's own draw signal
        self.assertEqual(len(fired), 1)
        parts = fired[0].split("|", 12)
        self.assertEqual(parts[0], "RESULT")
        self.assertEqual(parts[4], "", "a draw must post with no winner name")
        self.assertEqual(set(parts[6].split(",")), {"0"}, "a draw credits nobody a win")

    def test_second_f_before_any_n_does_not_re_fire(self):
        # Multiple clients can each send their own 'F' for the same round --
        # the real client's CommitRoundWin ignores every one after the
        # first; result_posted is this function's equivalent.
        a, b = self._start_two_player_game("deduproom")
        self.assertEqual(len(self._finish_round(a, b, "winner")), 1)

        a.sendall(b"?Fwinner\n")
        self.assertEqual(self.drain_relay(), [],
                         "a second F before any 'n' must not double-post "
                         "the same round")

        a.sendall(b"?n\n")  # ready for next round -- resets the dedup guard
        self.assertEqual(self.drain_relay(), [], "'n' itself must not fire anything")

        fired = self._finish_round(a, b, "winner2")
        self.assertEqual(len(fired), 1, "a new round's F after 'n' must post again")
        self.assertEqual(fired[0].split("|", 12)[4], "winner2")

    def test_pipe_in_winner_payload_cannot_corrupt_the_datagram_fields(self):
        # winner is lifted straight from a client's 'F' payload with none of
        # is_nick_ok()'s charset restriction behind it -- unlike every other
        # field in this datagram. A literal '|' in it would otherwise shift
        # this and every field after it one column to the right.
        a, b = self._start_two_player_game("pipetest")
        fired = self._finish_round(a, b, "evil|injected")
        self.assertEqual(len(fired), 1)
        parts = fired[0].split("|", 12)
        self.assertEqual(parts[0], "RESULT")
        self.assertEqual(parts[4], "evil injected")
        self.assertEqual(set(parts[5].split(",")), {"pipetest", "guest1"})

    def test_game_id_is_stable_across_rounds_of_the_same_room(self):
        # The whole point of carrying game_id at all: the relay groups every
        # round from one room into a single Discord thread by this value
        # (see server/discord-relay/relay.py's _room_threads), so it must
        # not change just because a new round started.
        a, b = self._start_two_player_game("stableroom")

        first_game_id = self._finish_round(a, b, "winner")[0].split("|", 12)[1]

        a.sendall(b"?n\n")
        self.drain_relay()

        second_game_id = self._finish_round(a, b, "winner2")[0].split("|", 12)[1]

        self.assertEqual(first_game_id, second_game_id)

    def test_different_rooms_get_different_game_ids(self):
        a1, b1 = self._start_two_player_game("roomone", "guest1")
        a2, b2 = self._start_two_player_game("roomtwo", "guest2")

        game_id_1 = self._finish_round(a1, b1, "winner")[0].split("|", 12)[1]
        game_id_2 = self._finish_round(a2, b2, "winner")[0].split("|", 12)[1]

        self.assertNotEqual(game_id_1, game_id_2)

    def test_round_number_increments_each_round_and_never_resets(self):
        a, b = self._start_two_player_game("roundcnt")

        self.assertEqual(self._finish_round(a, b, "winner")[0].split("|", 12)[2], "1")

        a.sendall(b"?n\n")
        self.drain_relay()
        self.assertEqual(self._finish_round(a, b, "winner")[0].split("|", 12)[2], "2")

        a.sendall(b"?n\n")
        self.drain_relay()
        # a draw still counts as a round
        self.assertEqual(self._finish_round(a, b, "")[0].split("|", 12)[2], "3")

    def test_wins_csv_accumulates_across_rounds_index_aligned_with_roster(self):
        # "guest1" is a real seated player (see the docstring on
        # _start_two_player_game) -- report_round_result() only credits a
        # win to a name that resolves to an actual seat, so this is the one
        # nick in these tests whose repeated wins are meant to show up here.
        a, b = self._start_two_player_game("winstally")

        parts = self._finish_round(a, b, "guest1")[0].split("|", 12)
        roster, wins = parts[5].split(","), parts[6].split(",")
        self.assertEqual(dict(zip(roster, wins)), {"winstally": "0", "guest1": "1"})

        a.sendall(b"?n\n")
        self.drain_relay()
        parts = self._finish_round(a, b, "guest1")[0].split("|", 12)
        roster, wins = parts[5].split(","), parts[6].split(",")
        self.assertEqual(dict(zip(roster, wins)), {"winstally": "0", "guest1": "2"},
                         "a second win for the same player accumulates, not resets")

    def test_result_carries_the_rooms_victories_limit(self):
        # VICTORIESLIMIT flows into every RESULT for the room (not just the
        # MATCH alert that fires once it's reached) so the relay's win-count
        # chart can scale its bars against the real target -- see
        # discordalert_fire_result_event()'s own doc comment.
        a, b = self._start_two_player_game("vlimroom", victories_limit=7)
        parts = self._finish_round(a, b, "guest1")[0].split("|", 12)
        self.assertEqual(parts[7], "7")

    def test_no_victories_limit_reports_zero_in_result(self):
        a, b = self._start_two_player_game("novlim")  # victories_limit unset
        parts = self._finish_round(a, b, "guest1")[0].split("|", 12)
        self.assertEqual(parts[7], "0")

    def test_match_win_fires_when_victories_limit_reached(self):
        # "guest1" (the default guest_nick) is a real seated player -- only a
        # winner claim that resolves to an actual seat accrues a win count
        # (see report_round_result()'s find_player_slot_by_nick lookup), so
        # an unrecognized name like "winner" would never reach the limit.
        a, b = self._start_two_player_game("matchroom", victories_limit=2)

        fired = self._finish_round(a, b, "guest1")
        self.assertEqual(len(fired), 1, "below the limit: no MATCH alert yet")
        self.assertEqual(fired[0].split("|", 1)[0], "RESULT")

        a.sendall(b"?n\n")
        self.drain_relay()
        fired = self._finish_round(a, b, "guest1")
        self.assertEqual(len(fired), 2,
                         "the round reaching the limit fires RESULT then MATCH")
        self.assertEqual(fired[0].split("|", 1)[0], "RESULT")
        match_parts = fired[1].split("|", 4)
        self.assertEqual(match_parts[0], "MATCH")
        self.assertTrue(match_parts[1].isdigit(), "game_id must be a plain int")
        self.assertEqual(match_parts[2], "2", "champion's win count == the limit")
        self.assertEqual(match_parts[3], "0", "GAMEMODE defaults to Classic")
        champion, _, servername = match_parts[4].partition("|")
        self.assertEqual(champion, "guest1")
        self.assertTrue(servername, "servername field must not be empty")

    def test_match_event_shares_its_rounds_game_id(self):
        a, b = self._start_two_player_game("matchid", victories_limit=1)

        fired = self._finish_round(a, b, "guest1")
        self.assertEqual(len(fired), 2)
        result_game_id = fired[0].split("|", 12)[1]
        match_game_id = fired[1].split("|", 4)[1]
        self.assertEqual(result_game_id, match_game_id)

    def test_no_victories_limit_set_never_fires_a_match_event(self):
        a, b = self._start_two_player_game("nolimit")  # victories_limit unset

        for _ in range(3):
            fired = self._finish_round(a, b, "winner")
            self.assertEqual(len(fired), 1, "no VICTORIESLIMIT: only RESULT, never MATCH")
            self.assertEqual(fired[0].split("|", 1)[0], "RESULT")
            a.sendall(b"?n\n")
            self.drain_relay()

    def test_draw_never_fires_a_match_event(self):
        a, b = self._start_two_player_game("drawlim", victories_limit=1)
        fired = self._finish_round(a, b, "")  # bare F -- draw, no winner to credit
        self.assertEqual(len(fired), 1, "a draw can never reach a win-count limit")
        self.assertEqual(fired[0].split("|", 1)[0], "RESULT")

    def test_unrecognized_winner_claim_never_fires_a_match_event(self):
        a, b = self._start_two_player_game("ghostlim", victories_limit=1)
        fired = self._finish_round(a, b, "ghost")  # no seated player named "ghost"
        self.assertEqual(len(fired), 1,
                         "an unrecognized winner gets no win-count, so no MATCH either")
        self.assertEqual(fired[0].split("|", 1)[0], "RESULT")

    def test_deferred_post_waits_for_the_last_seats_stats_before_firing(self):
        # The core of this feature's timing change: the datagram must not
        # appear the instant 'F' arrives (the old, synchronous behavior),
        # and must appear as soon as the *last* outstanding seat reports,
        # without waiting out the full timeout.
        a, b = self._start_two_player_game("deferroom")
        a.sendall(b"?Fguest1\n")
        self.assertEqual(self.drain_relay(timeout=0.5), [],
                         "must not fire synchronously with 'F' any more")

        self._report_stats(a, popped=2)
        self.assertEqual(self.drain_relay(timeout=0.5), [],
                         "still waiting on guest1's own report")

        self._report_stats(b, popped=6)
        fired = self.drain_relay(timeout=0.5)
        self.assertEqual(len(fired), 1,
                         "must fire promptly once the last seat reports, well "
                         "before PENDING_STATS_TIMEOUT_SECS elapses")

    def test_deferred_post_fires_after_timeout_when_a_seat_never_reports(self):
        # guest1 never sends its own 'S' at all -- the round must still be
        # posted (with an empty popped field for that seat) once
        # PENDING_STATS_TIMEOUT_SECS passes, rather than waiting forever.
        a, b = self._start_two_player_game("tmoutroom")
        a.sendall(b"?Fguest1\n")
        self._report_stats(a, popped=4)
        self.assertEqual(self.drain_relay(timeout=0.5), [],
                         "must not fire before the deadline with a seat still missing")

        fired = self.drain_relay(timeout=5.0)
        self.assertEqual(len(fired), 1,
                         "must still fire once the timeout passes, generous "
                         "margin over the ~2s deadline for scheduling jitter")
        parts = fired[0].split("|", 12)
        roster = parts[5].split(",")
        popped = dict(zip(roster, parts[11].split(",")))
        self.assertEqual(popped["tmoutroom"], "4")
        self.assertEqual(popped["guest1"], "", "never reported -- empty, not zero")

    def test_implausible_popped_report_is_clamped_and_flagged(self):
        # guest1 fires no 'f' opcodes this round at all, so its plausibility
        # ceiling is the flat grace allowance alone (cap == 0 * MAX_POPS_PER_SHOT
        # + POP_CEILING_GRACE == 10, see game.c) -- 999 is far past it.
        a, b = self._start_two_player_game("clamproom")
        a.sendall(b"?Fguest1\n")
        self._report_stats(a, popped=5)
        b.sendall(b"?S0:999:0:0:0:0\n")
        fired = self.drain_relay()
        self.assertEqual(len(fired), 1)
        parts = fired[0].split("|", 12)
        roster = parts[5].split(",")
        popped = dict(zip(roster, parts[11].split(",")))
        self.assertEqual(popped["clamproom"], "5", "plausible report passes through untouched")
        self.assertEqual(popped["guest1"], "10!",
                         "clamped to the flat grace ceiling and flagged")

    def test_plausible_popped_report_is_not_flagged(self):
        a, b = self._start_two_player_game("plausroom")
        fired = self._finish_round(a, b, "guest1", popped_a=3, popped_b=4)
        parts = fired[0].split("|", 12)
        self.assertNotIn("!", parts[11], "neither report is anywhere near implausible")

    def test_popped_stats_shift_correctly_on_a_mid_round_departure(self):
        # Same shift-correctness concern player_wins[]/players_team[] already
        # have coverage for (see the departure-shift comment in
        # player_part_game_(), game.c) -- now extended to the 4 new per-seat
        # arrays this feature added. Middle seat (guest1) leaves mid-round,
        # before reporting its own stats; the remaining two seats' pending
        # reports must not be corrupted by the shift.
        # CREATE's own argument doubles as the room name AND the creator's
        # roster identity (g->players_nick[0]) -- not whatever NICK they
        # connected with (see _start_two_player_game's docstring) -- so the
        # creator connects under a throwaway nick and creates the room as
        # "host2" itself, matching the roster identity used in the
        # assertions below.
        a = self.connect()
        a.sendall(b"FB/1.3 NICK creator3\n")
        self.assertIn(b"NICK: OK", recv_until(a, b"NICK:"))
        b = self.connect()
        b.sendall(b"FB/1.3 NICK guest1\n")
        self.assertIn(b"NICK: OK", recv_until(b, b"NICK:"))
        c = self.connect()
        c.sendall(b"FB/1.3 NICK guest2\n")
        self.assertIn(b"NICK: OK", recv_until(c, b"NICK:"))
        self.drain_relay()

        a.sendall(b"FB/1.3 CREATE host2\n")
        self.assertIn(b"CREATE: OK", recv_until(a, b"CREATE:"))
        b.sendall(b"FB/1.3 JOIN host2 guest1\n")
        self.assertIn(b"JOIN: OK", recv_until(b, b"JOIN:"))
        c.sendall(b"FB/1.3 JOIN host2 guest2\n")
        self.assertIn(b"JOIN: OK", recv_until(c, b"JOIN:"))

        a.sendall(b"FB/1.3 START\n")
        self.assertIn(b"START: OK", recv_until(a, b"START:"))
        for conn in (a, b, c):
            conn.sendall(b"FB/1.3 OK_GAME_START\n")
            self.assertIn(b"OK_GAME_START: OK", recv_until(conn, b"OK_GAME_START:"))
        self.drain_relay()

        a.sendall(b"?Fhost2\n")
        self._report_stats(a, popped=5)
        self.assertEqual(self.drain_relay(timeout=0.5), [],
                         "still waiting on guest1 and guest2")

        # guest1 (the middle seat, index 1) departs before ever reporting.
        b.close()
        self.socks.remove(b)

        self._report_stats(c, popped=9)
        fired = self.drain_relay(timeout=5.0)
        self.assertEqual(len(fired), 1,
                         "the departed seat must not block the deferred post forever")
        parts = fired[0].split("|", 12)
        roster = parts[5].split(",")
        self.assertEqual(roster, ["host2", "guest2"],
                         "the departed seat is gone from the roster, not just "
                         "silently missing its stats")
        popped = dict(zip(roster, parts[11].split(",")))
        self.assertEqual(popped, {"host2": "5", "guest2": "9"},
                         "the remaining seats' own reports must survive the "
                         "shift uncorrupted")


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]] + sys.argv[2:])
