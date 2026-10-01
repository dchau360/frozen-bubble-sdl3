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

    def submit(self, sock, board, level, ms, nick, points=0, shots=None):
        """shots None: the pre-shots form, with no shots field at all."""
        tail = "" if shots is None else f" {shots}"
        return self.ask(sock, f"FB/1.3 HISCORE {board} {level} {ms} {points} {nick}{tail}",
                        "HISCORE: ")

    def board(self, sock, board):
        ws, alltime, week, me = self.board_fields(sock, board)[:4]
        split = lambda v: [] if v == "-" else v.split(",")
        return int(ws), split(alltime), split(week), me

    def board_fields(self, sock, board):
        return self.ask(sock, f"FB/1.3 HISCORES {board}", "HISCORES: ").split(" ")

    def days(self, sock, board):
        """The (alltime, week) day lists: the UTC day each listed run was set."""
        f = self.board_fields(sock, board)
        self.assertEqual(len(f), 8)
        split = lambda v: [] if v == "-" else [int(d) for d in v.split(",")]
        return split(f[4]), split(f[5])

    def shots(self, sock, board):
        """The (alltime, week) shot lists: the shots each listed run took."""
        f = self.board_fields(sock, board)
        self.assertEqual(len(f), 8)
        split = lambda v: [] if v == "-" else [int(d) for d in v.split(",")]
        return split(f[6]), split(f[7])

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

    def test_country_rides_on_the_entry(self):
        self.start()
        a, b = self.session("alice"), self.session("bob")
        self.assertEqual(self.ask(a, "FB/1.3 COUNTRY FR", "COUNTRY: "), "OK")
        self.submit(a, 0, 30, 100000, "alice")
        self.submit(b, 0, 20, 100000, "bob")  # never sent one: no field at all
        self.assertEqual(self.board(a, 0)[1], [f"{self.acct.tagged('alice')}=30/100000/0/FR",
                                               f"{self.acct.tagged('bob')}=20/100000/0"])
        # It's the account's, so it shows on every board it has a run on...
        self.submit(a, 2, 30, 100000, "alice", 5000)
        self.assertEqual(self.board(a, 2)[1], [f"{self.acct.tagged('alice')}=30/100000/5000/FR"])
        # ...a later session without COUNTRY keeps it, and a new one replaces it.
        c = self.session("alice")
        self.submit(c, 0, 31, 100000, "alice")
        self.assertEqual(self.board(c, 0)[1][0], f"{self.acct.tagged('alice')}=31/100000/0/FR")
        self.ask(c, "FB/1.3 COUNTRY DE", "COUNTRY: ")
        self.submit(c, 0, 31, 200000, "alice")
        self.assertEqual(self.board(c, 0)[1][0], f"{self.acct.tagged('alice')}=31/100000/0/DE")
        # And it survives a restart.
        self.server.kill()
        self.server.wait(timeout=5)
        self.start()
        self.assertEqual(self.board(self.session(), 2)[1],
                         [f"{self.acct.tagged('alice')}=30/100000/5000/DE"])

    def test_delete_account_drops_its_runs_and_weekly_line(self):
        wk = monday_of(today())
        self.start(seed=f"v2 {wk} {today()}\n" + self.line("alice", 3, 1, 50) + self.line("bob", 5, 0, 20))
        a, b = self.session("alice"), self.session("bob")
        self.submit(a, 0, 30, 100000, "alice")
        self.submit(a, 2, 30, 100000, "alice", 900)
        self.submit(b, 0, 20, 100000, "bob")
        # Signed out: refused, nothing dropped.
        anon = self.session()
        self.assertEqual(self.ask(anon, "FB/1.3 DELETEACCOUNT", "DELETEACCOUNT: "), "NOT_SIGNED_IN")
        self.assertEqual(len(self.board(anon, 0)[1]), 2)

        self.assertEqual(self.ask(a, "FB/1.3 DELETEACCOUNT", "DELETEACCOUNT: "), "OK")
        self.assertEqual(self.board(anon, 0)[1], [f"{self.acct.tagged('bob')}=20/100000/0"])
        self.assertEqual(self.board(anon, 2)[1], [])
        wins = self.weekly(anon)[1]
        self.assertNotIn(self.acct.tagged("alice"), wins)
        self.assertIn(self.acct.tagged("bob"), wins)
        # Nothing left to drop is still OK, and it stays gone after a restart.
        self.assertEqual(self.ask(a, "FB/1.3 DELETEACCOUNT", "DELETEACCOUNT: "), "OK")
        self.server.kill()
        self.server.wait(timeout=5)
        self.start()
        s = self.session()
        self.assertEqual(self.board(s, 0)[1], [f"{self.acct.tagged('bob')}=20/100000/0"])
        self.assertNotIn(self.acct.tagged("alice"), self.weekly(s)[1])

    def test_v2_file_loads_without_countries(self):
        self.hiscore_file.write_text(
            f"v2 {monday_of(today())}\n{self.acct.id('alice')} alice "
            + " ".join(["40 500000 0", "0 0 0", "0 0 0", "0 0 0"] * 2) + "\n")
        self.start()
        self.assertEqual(self.board(self.session(), 0)[1], [f"{self.acct.tagged('alice')}=40/500000/0"])

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

    def test_new_week_snapshots_both_files_and_keeps_the_newest(self):
        import datetime
        last_week = monday_of(today()) - 7
        date = lambda day: datetime.datetime.fromtimestamp(
            day * 86400, datetime.timezone.utc).strftime("%Y-%m-%d")
        hiscores = f"v3 {last_week}\n{self.acct.id('alice')} alice DE " + " ".join(
            ["40 500000 0", "0 0 0", "0 0 0", "0 0 0"] * 2) + "\n"
        self.hiscore_file.write_text(hiscores)
        d = self.hiscore_file.parent
        # Three older snapshots, and a file that only looks like one.
        for weeks in (2, 3, 4):
            (d / f"hiscores.dat.{date(last_week - 7 * (weeks - 1))}").write_text("old")
        (d / "hiscores.dat.bak").write_text("mine")
        self.start(seed=f"v2 {last_week} {today() - 1}\n" + self.line("alice", 3, 1, 50),
                   extra_env={"FB_SERVER_SNAPSHOTS": "3"})
        self.board(self.session(), 0)   # touching the board rolls it over

        snap = d / f"hiscores.dat.{date(last_week)}"
        self.assertEqual(snap.read_text(), hiscores, "the finished week, as it ended")
        kept = sorted(p.name for p in d.glob("hiscores.dat.????-??-??"))
        self.assertEqual(kept, sorted([snap.name] + [
            f"hiscores.dat.{date(last_week - 7 * w)}" for w in (1, 2)]))
        self.assertEqual((d / "hiscores.dat.bak").read_text(), "mine")
        weekly_snap = d / f"weekly.dat.{date(last_week)}"
        self.assertIn(self.acct.id("alice"), weekly_snap.read_text())

    def test_snapshots_can_be_turned_off(self):
        last_week = monday_of(today()) - 7
        self.hiscore_file.write_text(
            f"v1 {last_week}\n{self.acct.id('alice')} alice 40 500000 0 0 40 500000 0 0\n")
        self.start(seed=f"v2 {last_week} {today() - 1}\n" + self.line("alice", 3, 1, 50),
                   extra_env={"FB_SERVER_SNAPSHOTS": "0"})
        self.board(self.session(), 0)
        self.assertEqual(list(self.hiscore_file.parent.glob("*.dat.????-??-??")), [])
    def test_impossible_runs_are_refused(self):
        self.start()
        a = self.session("alice")
        # 30 levels in 29s (under 1s a level), and 100 levels in 99s.
        self.assertEqual(self.submit(a, 0, 30, 29999, "alice"), "IMPLAUSIBLE")
        self.assertEqual(self.submit(a, 1, 101, 99999, "alice"), "IMPLAUSIBLE")
        # A life that reached level 3 scoring more than 20,000 a level.
        self.assertEqual(self.submit(a, 2, 3, 60000, "alice", 60001), "IMPLAUSIBLE")
        self.assertEqual(self.board(a, 0)[1], [])
        self.assertEqual(self.board(a, 2)[1], [])
        # Right at the limits is fine: a life still on level 1 cleared nothing.
        self.assertTrue(self.submit(a, 0, 30, 30000, "alice").startswith("OK"))
        self.assertTrue(self.submit(a, 1, 101, 100000, "alice").startswith("OK"))
        self.assertTrue(self.submit(a, 2, 1, 50, "alice", 20000).startswith("OK"))
        self.assertTrue(self.submit(a, 3, 3, 60000, "alice", 60000).startswith("OK"))

    def test_banned_account_is_hidden_and_refused_until_unbanned(self):
        self.start()
        a, b = self.session("alice"), self.session("bob")
        self.submit(a, 0, 40, 400000, "alice")
        self.submit(b, 0, 20, 400000, "bob")
        bans = self.hiscore_file.parent / "banned.txt"
        # A comment, a junk line and a whole pasted hiscores.dat line.
        bans.write_text(f"# cheaters\nnot-an-id\n{self.acct.id('alice')} alice DE 40 400000 0\n")
        anon = self.session()
        self.assertEqual(self.board(anon, 0)[1], [f"{self.acct.tagged('bob')}=20/400000/0"])
        ws, alltime, week, me = self.board(a, 0)
        self.assertEqual(me, "-")
        self.assertTrue(self.board(b, 0)[3].startswith("1,"), "bob ranks first with alice hidden")
        self.assertEqual(self.submit(a, 0, 50, 500000, "alice"), "BANNED")
        # Unbanned: the old line comes back; the refused run never landed.
        bans.write_text("")
        import os, time
        os.utime(bans, (time.time() + 5, time.time() + 5))  # a new mtime even within the second
        self.assertEqual(self.board(anon, 0)[1][0], f"{self.acct.tagged('alice')}=40/400000/0")

    def test_each_run_carries_the_day_it_was_set(self):
        old = monday_of(today()) - 7
        # A v3 file: no days yet, so its runs come back as 0 (unknown).
        self.hiscore_file.write_text(
            f"v3 {monday_of(today())}\n{self.acct.id('bob')} bob - "
            + " ".join(["50 500000 0", "0 0 0", "0 0 0", "0 0 0"] * 2) + "\n")
        self.start()
        a = self.session("alice")
        self.submit(a, 0, 30, 100000, "alice")
        self.assertEqual(self.days(a, 0), ([0, today()], [0, today()]))
        # Matching your own best doesn't move its date; nor does a worse run.
        self.submit(a, 0, 30, 100000, "alice")
        self.submit(a, 0, 20, 100000, "alice")
        self.assertEqual(self.days(a, 0)[0], [0, today()])
        self.assertEqual(self.days(a, 2), ([], []))
        # Saved as v5 and read back.
        self.server.kill()
        self.server.wait(timeout=5)
        self.assertTrue(self.hiscore_file.read_text().startswith("v5 "))
        self.start()
        self.assertEqual(self.days(self.session(), 0), ([0, today()], [0, today()]))


    def test_each_run_carries_its_shots(self):
        # A v4 file: no shots yet, so its runs come back as 0 (not counted).
        self.hiscore_file.write_text(
            f"v4 {monday_of(today())}\n{self.acct.id('bob')} bob - "
            + " ".join([f"50 500000 0 {today()}", "0 0 0 0", "0 0 0 0", "0 0 0 0"] * 2) + "\n")
        self.start()
        a, c = self.session("alice"), self.session("carol")
        self.assertTrue(self.submit(a, 0, 30, 100000, "alice", shots=240).startswith("OK"))
        # A game from before shots were counted sends none: listed as 0.
        self.assertTrue(self.submit(c, 0, 20, 100000, "carol").startswith("OK"))
        self.assertEqual(self.shots(a, 0), ([0, 240, 0], [0, 240, 0]))
        # The shots ride with the run they came with: a worse run with fewer
        # shots changes nothing, a better one replaces both.
        self.submit(a, 0, 25, 100000, "alice", shots=100)
        self.assertEqual(self.shots(a, 0)[0], [0, 240, 0])
        self.submit(a, 0, 31, 100000, "alice", shots=300)
        self.assertEqual(self.shots(a, 0)[0], [0, 300, 0])
        # Older parsers read four fields and still see the same lists.
        self.assertEqual(self.board(a, 0)[1][1], f"{self.acct.tagged('alice')}=31/100000/0")
        # Most-points boards carry them too, though only the web page and
        # the game's level boards show them.
        self.submit(a, 2, 5, 60000, "alice", 7000, shots=90)
        self.assertEqual(self.shots(a, 2), ([90], [90]))
        # Saved as v5 and read back.
        self.server.kill()
        self.server.wait(timeout=5)
        self.assertTrue(self.hiscore_file.read_text().startswith("v5 "))
        self.start()
        self.assertEqual(self.shots(self.session(), 0), ([0, 300, 0], [0, 300, 0]))

    def test_fewer_shots_than_levels_is_refused(self):
        self.start()
        a = self.session("alice")
        # Every cleared level takes at least one shot.
        self.assertEqual(self.submit(a, 0, 30, 100000, "alice", shots=29), "IMPLAUSIBLE")
        self.assertEqual(self.submit(a, 1, 101, 1000000, "alice", shots=99), "IMPLAUSIBLE")
        # A life on level 5 has cleared 4.
        self.assertEqual(self.submit(a, 2, 5, 60000, "alice", 5000, shots=3), "IMPLAUSIBLE")
        self.assertEqual(self.board(a, 0)[1], [])
        self.assertTrue(self.submit(a, 0, 30, 100000, "alice", shots=30).startswith("OK"))
        self.assertTrue(self.submit(a, 1, 101, 1000000, "alice", shots=100).startswith("OK"))
        self.assertTrue(self.submit(a, 2, 5, 60000, "alice", 5000, shots=4).startswith("OK"))
        # Out of range is refused outright.
        for bad in ("-1", f"{1000000 + 1}"):
            self.assertEqual(self.ask(a, f"FB/1.3 HISCORE 0 10 100000 0 alice {bad}", "HISCORE: "),
                             "INVALID", bad)


if __name__ == "__main__":
    unittest.main(argv=sys.argv[:1], verbosity=2)
