#!/usr/bin/env python3
"""Account sign-in in fb-server (server/account.c, protocol 1.6): the
AUTH/AUTHSIG challenge-response, what it refuses (a malformed key, a replayed
or forged signature, a signature with no challenge), and the Discord join
alert waiting for AUTHSIG when the client pipelines AUTH, PLATFORM and NICK
the way the game does.

Usage: server_account_test.py <path-to-fb-server> <path-to-account-sign-tool>
"""

import sys
import time
import unittest

from server_weekly_test import WeeklyTestBase, monday_of, recv_until, re_challenge, today


class AccountTest(WeeklyTestBase):
    def raw(self):
        import socket
        s = socket.create_connection(("127.0.0.1", self.PORT), timeout=3.0)
        self.socks.append(s)
        recv_until(s, b"SERVER_READY")
        return s

    def ask(self, sock, line, token):
        sock.sendall((line + "\n").encode())
        got = recv_until(sock, token.encode(), timeout=3.0)
        got += recv_until(sock, b"\n", timeout=0.5)
        return got.decode()

    def challenge(self, sock, name):
        got = self.ask(sock, f"FB/1.3 AUTH {self.acct.pubkey(name)}", "AUTH: ")
        m = re_challenge.search(got)
        self.assertTrue(m, got)
        return m.group(1)

    def test_valid_signature_signs_in(self):
        self.start()
        s = self.raw()
        nonce = self.challenge(s, "alice")
        got = self.ask(s, f"FB/1.3 AUTHSIG {self.acct.sign('alice', nonce)}", "AUTHSIG: ")
        self.assertIn(f"FB/1.7 AUTHSIG: OK {self.acct.id('alice')}", got)
        got = self.ask(s, f"FB/1.3 AUTH {self.acct.pubkey('alice')}", "AUTH: ")
        self.assertIn("AUTH: ALREADY_AUTHENTICATED", got)

    def test_every_challenge_is_fresh(self):
        self.start()
        a, b = self.raw(), self.raw()
        self.assertNotEqual(self.challenge(a, "alice"), self.challenge(b, "alice"))

    def test_replayed_signature_is_refused(self):
        self.start()
        first = self.raw()
        old = self.acct.sign("alice", self.challenge(first, "alice"))
        self.assertIn("AUTHSIG: OK", self.ask(first, f"FB/1.3 AUTHSIG {old}", "AUTHSIG: "))
        # Someone who saw that exchange tries it on a new connection.
        second = self.raw()
        self.challenge(second, "alice")
        self.assertIn("AUTHSIG: INVALID_SIGNATURE",
                      self.ask(second, f"FB/1.3 AUTHSIG {old}", "AUTHSIG: "))

    def test_signature_from_another_key_is_refused(self):
        self.start()
        s = self.raw()
        nonce = self.challenge(s, "alice")
        got = self.ask(s, f"FB/1.3 AUTHSIG {self.acct.sign('mallory', nonce)}", "AUTHSIG: ")
        self.assertIn("AUTHSIG: INVALID_SIGNATURE", got)

    def test_a_challenge_is_good_for_one_attempt(self):
        self.start()
        s = self.raw()
        nonce = self.challenge(s, "alice")
        self.ask(s, f"FB/1.3 AUTHSIG {'00' * 64}", "AUTHSIG: ")
        got = self.ask(s, f"FB/1.3 AUTHSIG {self.acct.sign('alice', nonce)}", "AUTHSIG: ")
        self.assertIn("AUTHSIG: INVALID_SIGNATURE", got)

    def test_signature_without_a_challenge_is_refused(self):
        self.start()
        s = self.raw()
        self.assertIn("AUTHSIG: INVALID_SIGNATURE",
                      self.ask(s, f"FB/1.3 AUTHSIG {'00' * 64}", "AUTHSIG: "))

    def test_malformed_key_is_refused(self):
        self.start()
        s = self.raw()
        self.assertIn("AUTH: INVALID_KEY", self.ask(s, "FB/1.3 AUTH nothex", "AUTH: "))
        self.assertIn("AUTH: INVALID_KEY", self.ask(s, "FB/1.3 AUTH abcd", "AUTH: "))

    def joins(self, timeout=0.8):
        return [d for d in self.drain(timeout) if d.startswith("JOIN|")]

    def test_join_alert_waits_for_authsig_and_carries_the_weekly_line(self):
        wk = monday_of(today())
        self.start(seed=f"v2 {wk} {today()}\n" + self.line("alice", 3, 1, 50))
        s = self.raw()
        # The game's order: AUTH, PLATFORM, NICK back to back.
        s.sendall((f"FB/1.3 AUTH {self.acct.pubkey('alice')}\n"
                   "FB/1.3 PLATFORM M\nFB/1.3 NICK alice\n").encode())
        got = recv_until(s, b"NICK: ", timeout=3.0).decode()
        self.assertIn("NICK: OK", got + recv_until(s, b"\n", 0.5).decode())
        nonce = re_challenge.search(got).group(1)
        self.assertEqual(self.joins(), [], "held until AUTHSIG")
        s.sendall(f"FB/1.3 AUTHSIG {self.acct.sign('alice', nonce)}\n".encode())
        joins = self.joins()
        self.assertEqual(len(joins), 1)
        parts = joins[0].split("|")
        self.assertEqual(parts[1], self.acct.tagged("alice"), "Discord shows the account tag")
        self.assertEqual(parts[4], "M")
        self.assertEqual(parts[6], "3,1,50,1,1,1")

    def test_failed_authsig_still_announces_the_player(self):
        wk = monday_of(today())
        self.start(seed=f"v2 {wk} {today()}\n" + self.line("alice", 3, 1, 50))
        s = self.raw()
        s.sendall((f"FB/1.3 AUTH {self.acct.pubkey('alice')}\nFB/1.3 NICK alice\n").encode())
        recv_until(s, b"NICK: ", timeout=3.0)
        s.sendall(f"FB/1.3 AUTHSIG {'00' * 64}\n".encode())
        joins = self.joins()
        self.assertEqual(len(joins), 1)
        self.assertEqual(joins[0].split("|")[6], "", "no account, no weekly line")
        self.assertEqual(joins[0].split("|")[1], "alice", "no account, no tag")

    def test_round_result_tags_signed_in_players_and_the_winner(self):
        self.start()
        self.play_round([("host", False), ("guest", False)], winner="host", popped=[1, 1],
                        accounts=[True, False])
        results = [d for d in self.round_datagrams if d.startswith("RESULT|")]
        self.assertEqual(len(results), 1)
        parts = results[0].split("|")
        # RESULT|game_id|round|mode|winner|roster|...
        self.assertEqual(parts[4], self.acct.tagged("host"))
        self.assertEqual(parts[5], f"{self.acct.tagged('host')},guest")

    def test_signed_out_client_is_announced_at_once(self):
        self.start()
        self.connect("oldclient", account=False)
        self.assertEqual(len(self.joins()), 1)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]] + sys.argv[3:])
