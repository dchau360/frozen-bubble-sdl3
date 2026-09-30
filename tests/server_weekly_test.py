#!/usr/bin/env python3
"""Weekly rankings in fb-server (server/weeklystats.c): what a finished round
records, the WEEKLY lobby command, the player's weekly line on the Discord
JOIN datagram, and the daily / Monday-rollover LEADERBOARD datagram.

Runs a real fb-server with a UDP socket standing in for discord-relay, and a
weekly-stats file in a temp directory -- seeded before startup where a test
needs a particular week or last-posted day, since the server's clock can't be
moved from outside.

Players sign in to an account (AUTH/AUTHSIG, server/account.h) the way the
game does, since only signed-in connections are counted. Each nick gets a
deterministic key, so a seeded weekly.dat can name the same account.

Usage: server_weekly_test.py <path-to-fb-server> <path-to-account-sign-tool>
"""

import hashlib
import os
import socket
import subprocess
import sys
import tempfile
import re
import time
import unittest
from pathlib import Path

re_challenge = re.compile(r"AUTH: CHALLENGE ([0-9a-f]{64})")


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


def today():
    return int(time.time() // 86400)


def monday_of(day):
    # 1970-01-01 (day 0) was a Thursday -- same arithmetic as weeklystats.c.
    return day - ((day + 3) % 7)


class Accounts:
    """Deterministic test accounts: the key for a name is derived from the
    name, so a test can seed weekly.dat with an account and then connect as
    it. sign_tool is tests/account_sign_tool.c."""

    def __init__(self, sign_tool):
        self.sign_tool = sign_tool
        self.cache = {}

    def seed(self, name):
        return hashlib.sha256(("fb-test-account:" + name).encode()).hexdigest()

    def pubkey(self, name):
        if name not in self.cache:
            self.cache[name] = subprocess.check_output(
                [self.sign_tool, self.seed(name)], text=True).strip()
        return self.cache[name]

    def id(self, name):
        # server/account.c: the first 8 bytes of BLAKE2b(public key).
        return hashlib.blake2b(bytes.fromhex(self.pubkey(name)), digest_size=8).hexdigest()

    def tagged(self, name, nick=None):
        """How the weekly lists show this account: nick#<first 4 hex>."""
        return f"{nick or name}#{self.id(name)[:4]}"

    def sign(self, name, nonce):
        return subprocess.check_output(
            [self.sign_tool, self.seed(name), nonce], text=True).strip()

    def sign_in(self, sock, name):
        sock.sendall(f"FB/1.3 AUTH {self.pubkey(name)}\n".encode())
        got = recv_until(sock, b"\n", timeout=3.0).decode()
        m = re_challenge.search(got)
        assert m, got
        sock.sendall(f"FB/1.3 AUTHSIG {self.sign(name, m.group(1))}\n".encode())
        got = recv_until(sock, b"AUTHSIG: ", timeout=3.0)
        got += recv_until(sock, b"\n", timeout=1.0)
        assert f"AUTHSIG: OK {self.id(name)}".encode() in got, got


class WeeklyTestBase(unittest.TestCase):
    PORT = 15521

    def setUp(self):
        if len(sys.argv) < 2:
            self.skipTest("fb-server binary path not passed as argv[1]")
        self.server_path = Path(sys.argv[1])
        if not self.server_path.exists():
            self.skipTest(f"fb-server binary not found at {self.server_path}")
        if len(sys.argv) < 3 or not Path(sys.argv[2]).exists():
            self.skipTest("account-sign-tool path not passed as argv[2]")
        self.acct = Accounts(sys.argv[2])
        self.tmpdir = tempfile.TemporaryDirectory()
        self.weekly_file = Path(self.tmpdir.name) / "weekly.dat"
        self.relay = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.relay.bind(("127.0.0.1", 0))
        self.relay.settimeout(0.2)
        self.socks = []
        self.server = None

    def start(self, seed=None):
        """Boot the server, optionally after writing `seed` as weekly.dat."""
        if seed is not None:
            self.weekly_file.write_text(seed)
        env = dict(os.environ)
        env["FB_SERVER_DISCORD_RELAY"] = f"127.0.0.1:{self.relay.getsockname()[1]}"
        env["FB_SERVER_STATS_FILE"] = str(Path(self.tmpdir.name) / "stats.dat")
        env["FB_SERVER_WEEKLY_FILE"] = str(self.weekly_file)
        self.server = subprocess.Popen(
            [str(self.server_path), "-p", str(self.PORT), "-q", "-z", "-d"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            try:
                socket.create_connection(("127.0.0.1", self.PORT), timeout=0.2).close()
                return
            except OSError:
                time.sleep(0.05)
        self.fail("server never started listening")

    def tearDown(self):
        for s in self.socks:
            s.close()
        self.relay.close()
        if self.server:
            self.server.kill()
            self.server.wait(timeout=5)
        self.tmpdir.cleanup()

    def line(self, name, w, l, p, nick=None):
        """One v2 weekly.dat line for the test account `name`."""
        return f"{self.acct.id(name)} {nick or name} {w} {l} {p}\n"

    def connect(self, nick, bot=False, account=True):
        """account: True signs in as the account named after nick, a string
        signs in as that account instead, False stays signed out."""
        s = socket.create_connection(("127.0.0.1", self.PORT), timeout=3.0)
        self.socks.append(s)
        recv_until(s, b"SERVER_READY")
        if account and not bot:
            self.acct.sign_in(s, account if isinstance(account, str) else nick)
        s.sendall(f"FB/1.3 NICK {nick}\n".encode())
        self.assertIn(b"NICK: OK", recv_until(s, b"NICK:"))
        if bot:
            s.sendall(b"FB/1.3 BOT\n")
            self.assertIn(b"BOT: OK", recv_until(s, b"BOT:"))
        return s

    def drain(self, timeout=0.8):
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

    def weekly(self, sock):
        """The WEEKLY reply's first five fields: week_start, wins, losses,
        popped, me. The sixth (lobby ranks) is in self.last_lobby_ranks."""
        # A player still seated in a room also has that room's relayed game
        # messages queued ahead of the reply; skip past them.
        sock.sendall(b"FB/1.3 WEEKLY\n")
        got = recv_until(sock, b"WEEKLY: ").decode()
        rest = got.split("WEEKLY: ", 1)[1] if "WEEKLY: " in got else ""
        if "\n" not in rest:
            rest += recv_until(sock, b"\n").decode()
        self.assertIn("\n", rest, got + rest)
        fields = rest.split("\n", 1)[0].strip().split(" ")
        self.assertEqual(len(fields), 6, rest)
        self.last_lobby_ranks = fields[5]
        return fields[:5]

    def play_round(self, seats, winner, popped, accounts=None):
        """seats: [(nick, is_bot)], first is the room's creator. Starts a
        room, sends the round's 'F' (winner "" = draw) and every seat's 'S'.
        accounts: per seat, as connect()'s account argument (default: each
        seat signs in as the account named after its nick)."""
        accounts = accounts or [True] * len(seats)
        socks = [self.connect(n, b, a) for (n, b), a in zip(seats, accounts)]
        host = socks[0]
        host.sendall(f"FB/1.3 CREATE {seats[0][0]} 5\n".encode())
        self.assertIn(b"CREATE: OK", recv_until(host, b"CREATE:"))
        for (n, _), s in zip(seats[1:], socks[1:]):
            s.sendall(f"FB/1.3 JOIN {seats[0][0]} {n}\n".encode())
            self.assertIn(b"JOIN: OK", recv_until(s, b"JOIN:"))
        host.sendall(b"FB/1.3 START\n")
        self.assertIn(b"START: OK", recv_until(host, b"START:"))
        for s in socks:
            s.sendall(b"FB/1.3 OK_GAME_START\n")
            self.assertIn(b"OK_GAME_START: OK", recv_until(s, b"OK_GAME_START:"))
        self.drain()
        host.sendall(f"?F{winner}\n".encode())
        for s, p in zip(socks, popped):
            s.sendall(f"?S0:{p}:0:0:0:0\n".encode())
        self.round_datagrams = self.drain()
        return socks


class RoundRecordingTest(WeeklyTestBase):
    def test_round_records_wins_losses_popped_and_skips_bots(self):
        self.start()
        socks = self.play_round([("host", False), ("guest", False), ("robo", True)],
                                winner="host", popped=[5, 3, 7])
        spectator = self.connect("watcher")
        week, wins, losses, popped, me = self.weekly(spectator)
        self.assertEqual(int(week) % 86400, 0)
        self.assertEqual(int(week) // 86400, monday_of(today()))
        self.assertEqual(wins, f"{self.acct.tagged('host')}=1")
        self.assertEqual(losses, f"{self.acct.tagged('guest')}=1", "the bot lost too, but bots are never recorded")
        self.assertEqual(popped, f"{self.acct.tagged('host')}=5,{self.acct.tagged('guest')}=3")
        self.assertEqual(me, "-", "watcher has played nothing this week")

        # The asking player's own line, ranks included.
        _, _, _, _, me = self.weekly(socks[1])
        self.assertEqual(me, "0,1,3,0,1,2")

    def test_draw_records_popped_but_no_wins_or_losses(self):
        self.start()
        socks = self.play_round([("host", False), ("guest", False)], winner="", popped=[4, 6])
        _, wins, losses, popped, _ = self.weekly(socks[0])
        self.assertEqual((wins, losses), ("-", "-"))
        self.assertEqual(popped, f"{self.acct.tagged('guest')}=6,{self.acct.tagged('host')}=4")

    def test_stats_survive_a_restart(self):
        self.start()
        self.play_round([("host", False), ("guest", False)], winner="guest", popped=[1, 2])
        for s in self.socks:
            s.close()
        self.socks = []
        self.server.kill()
        self.server.wait(timeout=5)
        self.start()
        s = self.connect("host")
        _, wins, losses, _, me = self.weekly(s)
        self.assertEqual((wins, losses), (f"{self.acct.tagged('guest')}=1", f"{self.acct.tagged('host')}=1"))
        self.assertEqual(me, "0,1,1,0,1,2")


class DefaultNickTest(WeeklyTestBase):
    """Names the game fills in for a player who never chose one are shared by
    strangers, so they never get a weekly line."""

    def test_default_nicks_and_their_retries_are_not_recorded(self):
        self.start()
        socks = self.play_round([("unnamed", False), ("android_us", False),
                                 ("web_user2", False), ("realname", False)],
                                winner="unnamed", popped=[5, 3, 7, 2])
        _, wins, losses, popped, me = self.weekly(socks[3])
        self.assertEqual(wins, "-", "the winner was a default name")
        self.assertEqual(losses, f"{self.acct.tagged('realname')}=1")
        self.assertEqual(popped, f"{self.acct.tagged('realname')}=2")
        _, _, _, _, me = self.weekly(socks[0])
        self.assertEqual(me, "-")

    def test_look_alike_chosen_names_are_still_recorded(self):
        self.start()
        self.play_round([("unnamedx", False), ("web_users", False)],
                        winner="unnamedx", popped=[1, 1])
        _, wins, losses, _, _ = self.weekly(self.connect("watcher"))
        self.assertEqual((wins, losses), (f"{self.acct.tagged('unnamedx')}=1", f"{self.acct.tagged('web_users')}=1"))

    def test_default_nicks_already_on_file_are_dropped_on_load(self):
        day = today()
        self.start(seed=f"v2 {monday_of(day)} {day}\n" + self.line("unnamed", 9, 0, 50)
                   + self.line("android_u3", 4, 1, 20) + self.line("alice", 2, 1, 10))
        _, wins, _, popped, _ = self.weekly(self.connect("watcher"))
        self.assertEqual(wins, f"{self.acct.tagged('alice')}=2")
        self.assertEqual(popped, f"{self.acct.tagged('alice')}=10")


class AccountKeyingTest(WeeklyTestBase):
    """Rankings belong to accounts, not names."""

    def test_same_nick_on_two_accounts_gets_two_lines(self):
        self.start()
        # Two different people both called "bob", one after the other.
        self.play_round([("bob", False), ("guest", False)], winner="bob", popped=[1, 1],
                        accounts=["bob-one", "guest"])
        for s in self.socks:
            s.close()
        self.socks = []
        self.play_round([("bob", False), ("guest", False)], winner="bob", popped=[1, 1],
                        accounts=["bob-two", "guest"])
        _, wins, losses, _, _ = self.weekly(self.connect("watcher"))
        self.assertEqual(sorted(wins.split(",")),
                         sorted([f"{self.acct.tagged('bob-one', 'bob')}=1",
                                 f"{self.acct.tagged('bob-two', 'bob')}=1"]))
        self.assertEqual(losses, f"{self.acct.tagged('guest')}=2")

    def test_rename_keeps_the_accounts_line(self):
        self.start()
        self.play_round([("alice", False), ("guest", False)], winner="alice", popped=[1, 1])
        for s in self.socks:
            s.close()
        self.socks = []
        self.play_round([("alice2", False), ("guest", False)], winner="alice2", popped=[1, 1],
                        accounts=["alice", "guest"])
        _, wins, _, _, _ = self.weekly(self.connect("watcher"))
        self.assertEqual(wins, f"{self.acct.tagged('alice', 'alice2')}=2",
                         "one line, shown under the name last played")

    def test_signed_out_players_are_not_recorded(self):
        self.start()
        socks = self.play_round([("host", False), ("guest", False)], winner="host", popped=[5, 3],
                                accounts=[False, "guest"])
        _, wins, losses, popped, _ = self.weekly(socks[1])
        self.assertEqual(wins, "-", "the winner never signed in")
        self.assertEqual(losses, f"{self.acct.tagged('guest')}=1")
        self.assertEqual(popped, f"{self.acct.tagged('guest')}=3")

    def test_pre_accounts_v1_file_keeps_its_week_but_not_its_lines(self):
        wk = monday_of(today())
        self.start(seed=f"v1 {wk} {today()}\nalice 3 1 50\n")
        week, wins, _, _, _ = self.weekly(self.connect("watcher"))
        self.assertEqual(int(week) // 86400, wk)
        self.assertEqual(wins, "-")
        self.assertEqual([d for d in self.drain(1.0) if d.startswith("LEADERBOARD|")], [],
                         "a same-day v1 file has already posted today")


class LobbyRanksTest(WeeklyTestBase):
    def test_weekly_reply_carries_lobby_players_wins_ranks(self):
        wk = monday_of(today())
        self.start(seed=f"v2 {wk} {today()}\n" + self.line("alice", 3, 1, 50) + self.line("bob", 5, 0, 20)
                   + self.line("carol", 0, 2, 9))
        alice = self.connect("alice")
        self.connect("bob")
        self.connect("carol")   # in the lobby, but no wins: no rank
        self.connect("dave")    # in the lobby, no line at all
        self.weekly(alice)
        ranks = dict(item.split("=") for item in self.last_lobby_ranks.split(","))
        self.assertEqual(ranks, {"alice": "2", "bob": "1"})

    def test_players_seated_in_a_room_are_not_in_the_lobby_list(self):
        wk = monday_of(today())
        self.start(seed=f"v2 {wk} {today()}\n" + self.line("alice", 3, 1, 50))
        alice = self.connect("alice")
        alice.sendall(b"FB/1.3 CREATE alice 5\n")
        self.assertIn(b"CREATE: OK", recv_until(alice, b"CREATE:"))
        watcher = self.connect("watcher")
        self.weekly(watcher)
        self.assertEqual(self.last_lobby_ranks, "-")


class JoinAlertTest(WeeklyTestBase):
    def test_join_datagram_carries_the_players_weekly_line(self):
        wk = monday_of(today())
        self.start(seed=f"v2 {wk} {today()}\n" + self.line("alice", 3, 1, 50) + self.line("bob", 5, 0, 20))
        self.connect("alice")
        joins = [d for d in self.drain() if d.startswith("JOIN|")]
        self.assertEqual(len(joins), 1)
        parts = joins[0].split("|")
        # JOIN|nick|ip|geoloc|platform|country|weekly|servername
        self.assertEqual(len(parts), 8, joins[0])
        self.assertEqual(parts[1], self.acct.tagged("alice"))
        self.assertEqual(parts[6], "3,1,50,2,1,1")

    def test_new_player_has_an_empty_weekly_field(self):
        self.start()
        self.connect("newbie")
        joins = [d for d in self.drain() if d.startswith("JOIN|")]
        self.assertEqual(joins[0].split("|")[6], "")


class LeaderboardPostTest(WeeklyTestBase):
    def leaderboards(self, timeout=2.0):
        return [d for d in self.drain(timeout) if d.startswith("LEADERBOARD|")]

    def test_fresh_file_posts_nothing(self):
        self.start()
        self.assertEqual(self.leaderboards(), [])

    def test_same_day_restart_posts_nothing(self):
        wk = monday_of(today())
        self.start(seed=f"v2 {wk} {today()}\n" + self.line("alice", 3, 1, 50))
        self.assertEqual(self.leaderboards(), [])

    def test_new_day_posts_current_standings_once(self):
        wk = monday_of(today())
        self.start(seed=f"v2 {wk} {today() - 1}\n" + self.line("alice", 3, 1, 50) + self.line("bob", 5, 0, 20))
        posts = self.leaderboards()
        self.assertEqual(len(posts), 1)
        self.assertEqual(posts[0].split("|")[:6],
                         ["LEADERBOARD", "0", str(wk * 86400),
                          f"{self.acct.tagged('bob')}=5,{self.acct.tagged('alice')}=3", f"{self.acct.tagged('alice')}=1",
                          f"{self.acct.tagged('alice')}=50,{self.acct.tagged('bob')}=20"])
        self.assertEqual(self.leaderboards(timeout=1.0), [], "one post per day, not per tick")

    def test_monday_rollover_posts_final_standings_and_starts_empty(self):
        old = monday_of(today()) - 7
        self.start(seed=f"v2 {old} {today() - 1}\n" + self.line("alice", 3, 1, 50))
        posts = self.leaderboards()
        self.assertEqual(len(posts), 1)
        parts = posts[0].split("|")
        self.assertEqual(parts[1:3], ["1", str(old * 86400)],
                         "final standings, labelled with the finished week")
        self.assertEqual(parts[3], f"{self.acct.tagged('alice')}=3")
        s = self.connect("alice")
        week, wins, losses, popped, me = self.weekly(s)
        self.assertEqual(int(week) // 86400, monday_of(today()))
        self.assertEqual((wins, losses, popped, me), ("-", "-", "-", "-"))

    def test_empty_week_is_never_posted(self):
        wk = monday_of(today())
        self.start(seed=f"v2 {wk} {today() - 1}\n")
        self.assertEqual(self.leaderboards(), [])


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]] + sys.argv[3:])
