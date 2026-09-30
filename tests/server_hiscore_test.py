#!/usr/bin/env python3
"""The world board for single-player runs in fb-server (server/hiscores.c,
protocol 1.7): HISCORE needs a signed-in account and range-checks the run,
keeps each account's best per track all-time and this week, and HISCORES
lists both boards plus the asker's own line. The file survives a restart,
and a new week clears the week bests but not the all-time ones.

Usage: server_hiscore_test.py <path-to-fb-server> <path-to-account-sign-tool>
"""

import sys
import unittest
from pathlib import Path

from server_weekly_test import WeeklyTestBase, monday_of, recv_until, today


class HiscoreTest(WeeklyTestBase):
    PORT = 15533

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

    def submit(self, sock, track, level, ms, nick):
        return self.ask(sock, f"FB/1.3 HISCORE {track} {level} {ms} {nick}", "HISCORE: ")

    def board(self, sock, track):
        ws, alltime, week, me = self.ask(sock, f"FB/1.3 HISCORES {track}", "HISCORES: ").split(" ")
        split = lambda v: [] if v == "-" else v.split(",")
        return int(ws), split(alltime), split(week), me

    def test_needs_a_signed_in_account(self):
        self.start()
        s = self.session()
        self.assertEqual(self.submit(s, 0, 10, 60000, "alice"), "NOT_SIGNED_IN")

    def test_out_of_range_runs_are_refused(self):
        self.start()
        s = self.session("alice")
        for bad in ("2 10 1000 alice", "0 0 1000 alice", "0 102 1000 alice",
                    "0 10 0 alice", f"0 10 {7 * 24 * 3600 * 1000 + 1} alice",
                    "0 10 1000 bad:name", "0 10 1000"):
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
        self.assertEqual(alltime, [f"{self.acct.tagged('alice')}=25/800000",
                                   f"{self.acct.tagged('bob')}=25/900000"])
        self.assertEqual(week, alltime)
        self.assertEqual(me, "1,25,800000,1,25,800000")
        self.assertEqual(self.board(b, 0)[3], "2,25,900000,2,25,900000")

    def test_tracks_are_separate(self):
        self.start()
        a = self.session("alice")
        self.submit(a, 1, 50, 1000000, "alice")
        self.assertEqual(self.board(a, 0)[1], [])
        self.assertEqual(self.board(a, 0)[3], "-")
        self.assertEqual(self.board(a, 1)[1], [f"{self.acct.tagged('alice')}=50/1000000"])

    def test_anyone_can_read_the_board(self):
        self.start()
        self.submit(self.session("alice"), 0, 101, 5000000, "alice")
        ws, alltime, week, me = self.board(self.session(), 0)
        self.assertEqual(alltime, [f"{self.acct.tagged('alice')}=101/5000000"])
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
                         [f"{self.acct.tagged('alice')}=30/123456"])

    def test_new_week_clears_week_bests_only(self):
        last_week = monday_of(today()) - 7
        self.hiscore_file.write_text(
            f"v1 {last_week}\n{self.acct.id('alice')} alice 40 500000 0 0 40 500000 0 0\n")
        self.start()
        ws, alltime, week, me = self.board(self.session("alice"), 0)
        self.assertEqual(alltime, [f"{self.acct.tagged('alice')}=40/500000"])
        self.assertEqual(week, [])
        self.assertEqual(me, "1,40,500000,0,0,0")


if __name__ == "__main__":
    unittest.main(argv=sys.argv[:1], verbosity=2)
