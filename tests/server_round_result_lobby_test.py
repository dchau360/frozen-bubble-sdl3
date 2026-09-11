#!/usr/bin/env python3
"""Round-result lobby broadcast: the same 'F' sniff in process_msg_prio_
(game.c) that fires the Discord RESULT alert also now announces the round
to whoever is sitting in the lobby right now -- win-count, top scorers, and
(on a team win) the team and its roster.

"Sitting in the lobby" is deliberately narrower than every connected
player: it means open_players, the same list talk()'s own server-wide
branch uses when its sender isn't seated in a game -- a player off playing
in some *other* room never sees another room's results, only someone
idling in the lobby (or, as it happens, anyone still sitting in a room
that has not yet reached GAME_STATUS_PLAYING) does.

This drives the real binary over the real protocol, same harness as
server_discordalert_test.py, because the point is the wire text an idler's
socket actually receives, not a formatting function in isolation.
"""

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


class ServerRoundResultLobbyTest(unittest.TestCase):
    def setUp(self):
        if len(sys.argv) < 2:
            self.skipTest("fb-server binary path not passed as argv[1]")
        self.server_path = Path(sys.argv[1])
        if not self.server_path.exists():
            self.skipTest(f"fb-server binary not found at {self.server_path}")

        self.tmpdir = tempfile.TemporaryDirectory()
        self.port = 15519

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
        self.tmpdir.cleanup()

    def connect(self):
        s = socket.create_connection(("127.0.0.1", self.port), timeout=3.0)
        self.socks.append(s)
        recv_until(s, b"SERVER_READY")
        return s

    def nick(self, sock, name):
        sock.sendall(f"FB/1.3 NICK {name}\n".encode())
        self.assertIn(b"NICK: OK", recv_until(sock, b"NICK:"))

    def talk_lines(self, sock, timeout=0.8):
        """Every 'TALK: ...' line queued for this socket right now, in order,
        with the "FB/1.3 PUSH: " framing every pushed line carries (net.c)
        stripped off so callers can match on the chat text itself."""
        deadline = time.monotonic() + timeout
        buf = b""
        sock.setblocking(False)
        while time.monotonic() < deadline:
            try:
                d = sock.recv(4096)
                if d:
                    buf += d
            except (BlockingIOError, socket.error):
                time.sleep(0.02)
        out = []
        for line in buf.decode(errors="replace").split("\n"):
            idx = line.find("TALK:")
            if idx != -1:
                out.append(line[idx:])
        return out

    def _start_two_player_game(self, room, guest_nick="guest1"):
        """Same shape as server_discordalert_test.py's helper: gets a 2-player
        room to GAME_STATUS_PLAYING with the creator (player A) in prio mode."""
        a = self.connect()
        self.nick(a, "creator")

        b = self.connect()
        self.nick(b, guest_nick)

        a.sendall(f"FB/1.3 CREATE {room}\n".encode())
        self.assertIn(b"CREATE: OK", recv_until(a, b"CREATE:"))

        b.sendall(f"FB/1.3 JOIN {room} {guest_nick}\n".encode())
        self.assertIn(b"JOIN: OK", recv_until(b, b"JOIN:"))

        return a, b

    def _set_teams(self, a, team_count, teams):
        """teams: list of up to 5 ints, team number for room slots 0..4 in
        join order (slot 0 is the creator). Missing slots default to 0
        (kNoTeam), same as SETOPTIONS's own PLAYERTEAM_Pn fields do when a
        client simply never mentions a slot past its own player count."""
        padded = (teams + [0, 0, 0, 0, 0])[:5]
        opts = f"TEAMCOUNT:{team_count}," + ",".join(
            f"PLAYERTEAM_P{i+1}:{t}" for i, t in enumerate(padded)
        )
        a.sendall(f"FB/1.3 SETOPTIONS {opts}\n".encode())
        self.assertIn(b"SETOPTIONS: OK", recv_until(a, b"SETOPTIONS:"))

    def _start(self, a):
        a.sendall(b"FB/1.3 START\n")
        self.assertIn(b"START: OK", recv_until(a, b"START:"))
        a.sendall(b"FB/1.3 OK_GAME_START\n")
        self.assertIn(b"OK_GAME_START: OK", recv_until(a, b"OK_GAME_START:"))

    def test_solo_win_announced_to_lobby_idler_not_to_another_room(self):
        idler = self.connect()
        self.nick(idler, "idler1")
        self.talk_lines(idler, timeout=0.2)  # drain anything incidental

        a, b = self._start_two_player_game("winroom", "guest1")
        self._start(a)
        self.talk_lines(idler, timeout=0.2)  # drain CREATE/JOIN/START chatter, if any

        a.sendall(b"?Fguest1\n")

        idler_lines = self.talk_lines(idler)
        self.assertEqual(len(idler_lines), 2, f"expected headline + top-scorers, got {idler_lines!r}")
        self.assertIn("Server: Round over: guest1 wins! (win #1 this match)", idler_lines[0])
        self.assertIn("Server: Top scorers: guest1 (1)", idler_lines[1])

        # The room's own players are not "in the lobby" -- they already saw
        # this round end via the ordinary 'F' relay and the on-screen stats
        # table, so this broadcast must not double up on their own socket.
        room_lines = self.talk_lines(b, timeout=0.2)
        self.assertEqual(room_lines, [])

    def test_draw_has_no_top_scorers_line_when_nobody_has_won_yet(self):
        idler = self.connect()
        self.nick(idler, "idler2")
        self.talk_lines(idler, timeout=0.2)

        a, b = self._start_two_player_game("drawroom", "guest1")
        self._start(a)
        self.talk_lines(idler, timeout=0.2)

        a.sendall(b"?F\n")  # bare F -- fb-server's own draw signal

        idler_lines = self.talk_lines(idler)
        self.assertEqual(len(idler_lines), 1, f"a draw with no wins yet needs no scorers line, got {idler_lines!r}")
        self.assertIn("Server: Round over: a draw.", idler_lines[0])

    def test_win_count_increments_across_rounds(self):
        idler = self.connect()
        self.nick(idler, "idler3")
        self.talk_lines(idler, timeout=0.2)

        a, b = self._start_two_player_game("streakroom", "guest1")
        self._start(a)
        self.talk_lines(idler, timeout=0.2)

        a.sendall(b"?Fguest1\n")
        self.talk_lines(idler)  # drain round 1

        a.sendall(b"?n\n")      # ready for next round -- resets the dedup guard
        self.talk_lines(idler)

        a.sendall(b"?Fguest1\n")
        idler_lines = self.talk_lines(idler)
        self.assertIn("Server: Round over: guest1 wins! (win #2 this match)", idler_lines[0])
        self.assertIn("Server: Top scorers: guest1 (2)", idler_lines[1])

    def test_team_win_names_the_team_and_its_players(self):
        idler = self.connect()
        self.nick(idler, "idler4")
        self.talk_lines(idler, timeout=0.2)

        a, b = self._start_two_player_game("teamroom", "guest1")
        # Both players on team 1 -- a 2v0 room is a degenerate case, but it
        # exercises the roster-building loop the same as a real 2v2 would.
        self._set_teams(a, team_count=2, teams=[1, 1])
        self._start(a)
        self.talk_lines(idler, timeout=0.2)

        a.sendall(b"?Fguest1\n")

        idler_lines = self.talk_lines(idler)
        self.assertEqual(len(idler_lines), 2)
        self.assertIn("Server: Round over: Team 1 wins", idler_lines[0])
        # The creator's roster identity is CREATE's own argument ("teamroom"),
        # not the "creator" nick they connected with -- see
        # _start_two_player_game's docstring.
        self.assertIn("teamroom", idler_lines[0])
        self.assertIn("guest1", idler_lines[0])
        # A team win's headline names the team, not an individual win-count --
        # the win still lands on guest1's own tally underneath, surfaced via
        # the very next line instead.
        self.assertIn("Server: Top scorers: guest1 (1)", idler_lines[1])

    def test_no_team_number_is_not_a_team_win_even_with_teamcount_set(self):
        # TEAMCOUNT alone does not put anyone on a team -- kNoTeam (0) means
        # "not on a side," same as the client-side convention (src/netteams.h).
        idler = self.connect()
        self.nick(idler, "idler5")
        self.talk_lines(idler, timeout=0.2)

        a, b = self._start_two_player_game("noteamroom", "guest1")
        self._set_teams(a, team_count=2, teams=[0, 0])
        self._start(a)
        self.talk_lines(idler, timeout=0.2)

        a.sendall(b"?Fguest1\n")

        idler_lines = self.talk_lines(idler)
        self.assertIn("Server: Round over: guest1 wins! (win #1 this match)", idler_lines[0])

    def test_top_five_scorers_sorted_by_wins_descending(self):
        idler = self.connect()
        self.nick(idler, "idler6")
        self.talk_lines(idler, timeout=0.2)

        a, b = self._start_two_player_game("scoreroom", "guest1")
        self._start(a)
        self.talk_lines(idler, timeout=0.2)

        # guest1 takes two rounds, the creator (roster identity "scoreroom" --
        # see _start_two_player_game's docstring) takes one -- guest1 must lead.
        a.sendall(b"?Fguest1\n")
        self.talk_lines(idler)
        a.sendall(b"?n\n")
        self.talk_lines(idler)
        a.sendall(b"?Fguest1\n")
        self.talk_lines(idler)
        a.sendall(b"?n\n")
        self.talk_lines(idler)
        a.sendall(b"?Fscoreroom\n")

        idler_lines = self.talk_lines(idler)
        self.assertIn("Server: Top scorers: guest1 (2), scoreroom (1)", idler_lines[1])

    def test_unrecognized_winner_claim_gets_no_win_count(self):
        # winner is lifted straight from the 'F' payload with none of
        # is_nick_ok's validation behind it (same trust gap the Discord
        # alert already documents) -- a name matching no seated player must
        # not be able to invent a win count out of thin air.
        idler = self.connect()
        self.nick(idler, "idler7")
        self.talk_lines(idler, timeout=0.2)

        a, b = self._start_two_player_game("ghostroom", "guest1")
        self._start(a)
        self.talk_lines(idler, timeout=0.2)

        a.sendall(b"?Fnobodyhere\n")

        idler_lines = self.talk_lines(idler)
        self.assertEqual(len(idler_lines), 1, f"no real scorer exists yet, got {idler_lines!r}")
        self.assertIn("Server: Round over: nobodyhere wins!", idler_lines[0])
        self.assertNotIn("win #", idler_lines[0])


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]] + sys.argv[2:])
