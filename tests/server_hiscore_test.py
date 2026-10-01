#!/usr/bin/env python3
"""The world board for single-player runs in fb-server (server/hiscores.c,
protocol 1.7): HISCORE needs a signed-in account and range-checks the run,
keeps each account's best per board (furthest level and most points, each
per input track) all-time and this week, and HISCORES lists both scopes plus
the asker's own line. The file survives a restart,
and a new week clears the week bests but not the all-time ones.

Usage: server_hiscore_test.py <path-to-fb-server> <path-to-account-sign-tool>
"""

import sys
import unittest
from pathlib import Path

from server_weekly_test import WeeklyTestBase, monday_of, recv_until, today


class HiscoreTest(WeeklyTestBase):
    @property
    def hiscore_file(self):
        return Path(self.tmpdir.name) / "hiscores.dat"

    def session(self, name=None):
        """A connection like the game's: signed in (unless name is None), no NICK."""
        import socket
        s = socket.create_connection(("127.0.0.1", self.PORT), timeout=3.0)
        self.socks.append(s)
        recv_until(s, b"SERVER_READY")
        if name:
            self.acct.sign_in(s, name)
        return s

    def ask(self, sock, line, token):
        sock.sendall((line + "\n").encode())
        got = recv_until(sock, token.encode(), timeout=3.0)
        got += recv_until(sock, b"\n", timeout=0.5)
        return got.decode().split(token, 1)[1].strip()

    def submit(self, sock, board, level, ms, nick, points=0):
        return self.ask(sock, f"FB/1.3 HISCORE {board} {level} {ms} {points} {nick}", "HISCORE: ")

    def board(self, sock, board):
        ws, alltime, week, me = self.ask(sock, f"FB/1.3 HISCORES {board}", "HISCORES: ").split(" ")
        split = lambda v: [] if v == "-" else v.split(",")
        return int(ws), split(alltime), split(week), me

    def test_needs_a_signed_in_account(self):
        self.start()
        s = self.session()
        self.assertEqual(self.submit(s, 0, 10, 60000, "alice"), "NOT_SIGNED_IN")

    def test_out_of_range_runs_are_refused(self):
        self.start()
        s = self.session("alice")
        for bad in ("4 10 1000 0 alice", "0 0 1000 0 alice", "0 102 1000 0 alice",
                    "0 10 0 0 alice", f"0 10 {7 * 24 * 3600 * 1000 + 1} 0 alice",
                    "0 10 1000 0 bad:name", "0 10 1000 0",
                    # points only on the most-points boards, and at least 1 there
                    "0 10 1000 500 alice", "2 10 1000 0 alice", "3 10 1000 -5 alice",
                    "2 10 1000 100000001 alice",
                    # the pre-points form, with no points field
                    "0 10 1000 alice"):
            self.assertEqual(self.ask(s, f"FB/1.3 HISCORE {bad}", "HISCORE: "), "INVALID", bad)
        self.assertEqual(self.board(s, 0)[1], [])

    def test_best_run_is_kept_and_ranked(self):
        self.start()
        a, b = self.session("alice"), self.session("bob")
        self.assertEqual(self.submit(a, 0, 20, 300000, "alice"), "OK 1 1")
        self.assertEqual(self.submit(b, 0, 25, 900000, "bob"), "OK 1 1")
        # A worse run doesn't replace alice's best, but is accepted.
        self.assertEqual(self.submit(a, 0, 20, 400000, "alice"), "OK 2 2")
        # Same level faster beats; higher level beats any time.
        self.assertEqual(self.submit(a, 0, 25, 800000, "alice"), "OK 1 1")
        ws, alltime, week, me = self.board(a, 0)
        self.assertEqual(ws, monday_of(today()) * 86400)
        self.assertEqual(alltime, [f"{self.acct.tagged('alice')}=25/800000/0",
                                   f"{self.acct.tagged('bob')}=25/900000/0"])
        self.assertEqual(week, alltime)
        self.assertEqual(me, "1,25,800000,0,1,25,800000,0")
        self.assertEqual(self.board(b, 0)[3], "2,25,900000,0,2,25,900000,0")

    def test_tracks_are_separate(self):
        self.start()
        a = self.session("alice")
        self.submit(a, 1, 50, 1000000, "alice")
        self.assertEqual(self.board(a, 0)[1], [])
        self.assertEqual(self.board(a, 0)[3], "-")
        self.assertEqual(self.board(a, 1)[1], [f"{self.acct.tagged('alice')}=50/1000000/0"])

    def test_anyone_can_read_the_board(self):
        self.start()
        self.submit(self.session("alice"), 0, 101, 5000000, "alice")
        ws, alltime, week, me = self.board(self.session(), 0)
        self.assertEqual(alltime, [f"{self.acct.tagged('alice')}=101/5000000/0"])
        self.assertEqual(me, "-")

    def test_no_join_alert_for_a_score_session(self):
        self.start()
        s = self.session("alice")
        self.submit(s, 0, 10, 60000, "alice")
        self.assertEqual([d for d in self.drain(0.5) if d.startswith("JOIN|")], [])

    def test_survives_a_restart(self):
        self.start()
        self.submit(self.session("alice"), 0, 30, 123456, "alice")
        self.server.kill()
        self.server.wait(timeout=5)
        self.start()
        self.assertEqual(self.board(self.session(), 0)[1],
                         [f"{self.acct.tagged('alice')}=30/123456/0"])

    def test_most_points_ranks_on_points(self):
        self.start()
        a, b = self.session("alice"), self.session("bob")
        self.assertEqual(self.submit(a, 2, 40, 900000, "alice", 20000), "OK 1 1")
        # Fewer levels but more points still wins this board.
        self.assertEqual(self.submit(b, 2, 12, 300000, "bob", 25000), "OK 1 1")
        # A lower-points life never replaces alice's best, however far it got.
        self.assertEqual(self.submit(a, 2, 90, 2000000, "alice", 19000), "OK 2 2")
        # Equal points: the further life wins.
        self.assertEqual(self.submit(a, 2, 50, 999999, "alice", 25000), "OK 1 1")
        ws, alltime, week, me = self.board(a, 2)
        self.assertEqual(alltime, [f"{self.acct.tagged('alice')}=50/999999/25000",
                                   f"{self.acct.tagged('bob')}=12/300000/25000"])
        self.assertEqual(me, "1,50,999999,25000,1,50,999999,25000")
        # Separate from the furthest-level boards and from the other track.
        self.assertEqual(self.board(a, 0)[1], [])
        self.assertEqual(self.board(a, 3)[1], [])

    def test_points_survive_a_restart(self):
        self.start()
        self.submit(self.session("alice"), 3, 7, 70000, "alice", 4321)
        self.server.kill()
        self.server.wait(timeout=5)
        self.start()
        self.assertEqual(self.board(self.session(), 3)[1],
                         [f"{self.acct.tagged('alice')}=7/70000/4321"])

    def test_v1_file_loads_into_the_level_boards(self):
        self.hiscore_file.write_text(
            f"v1 {monday_of(today())}\n{self.acct.id('alice')} alice 40 500000 12 34000 41 510000 0 0\n")
        self.start()
        s = self.session()
        self.assertEqual(self.board(s, 0)[1], [f"{self.acct.tagged('alice')}=40/500000/0"])
        self.assertEqual(self.board(s, 1)[1], [f"{self.acct.tagged('alice')}=12/34000/0"])
        self.assertEqual(self.board(s, 2)[1], [])

    def test_new_week_clears_week_bests_only(self):
        last_week = monday_of(today()) - 7
        self.hiscore_file.write_text(
            f"v1 {last_week}\n{self.acct.id('alice')} alice 40 500000 0 0 40 500000 0 0\n")
        self.start()
        ws, alltime, week, me = self.board(self.session("alice"), 0)
        self.assertEqual(alltime, [f"{self.acct.tagged('alice')}=40/500000/0"])
        self.assertEqual(week, [])
        self.assertEqual(me, "1,40,500000,0,0,0,0,0")


if __name__ == "__main__":
    unittest.main(argv=sys.argv[:1], verbosity=2)
