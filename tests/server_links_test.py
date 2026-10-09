#!/usr/bin/env python3
"""Account PINs and links in fb-server (server/links.c, protocol 1.8):
SETPIN gives a signed-in account a name and PIN, LINKPIN on another device's
account folds that account's runs and weekly line into the PIN's and makes
the device sign in as it from then on. Wrong guesses are limited per name,
and DELETEACCOUNT drops the PIN and every link.

Usage: server_links_test.py <path-to-fb-server> <path-to-account-sign-tool>
"""

import sys
import unittest

from server_hiscore_test import HiscoreTest
from server_weekly_test import monday_of, re_challenge, recv_until, today


class LinksTest(HiscoreTest):
    """HiscoreTest for its helpers; only the test_link* cases run here (see
    the bottom), the inherited ones run in server_hiscore_test.py."""

    def sign_in_as(self, name):
        """Sign in with `name`'s key and return the id the server answers."""
        import socket
        s = socket.create_connection(("127.0.0.1", self.PORT), timeout=3.0)
        self.socks.append(s)
        recv_until(s, b"SERVER_READY")
        s.sendall(f"FB/1.3 AUTH {self.acct.pubkey(name)}\n".encode())
        got = recv_until(s, b"\n", timeout=3.0).decode()
        m = re_challenge.search(got)
        self.assertTrue(m, got)
        s.sendall(f"FB/1.3 AUTHSIG {self.acct.sign(name, m.group(1))}\n".encode())
        got = recv_until(s, b"AUTHSIG: ", timeout=3.0)
        got += recv_until(s, b"\n", timeout=1.0)
        reply = got.decode().split("AUTHSIG: ", 1)[1].strip()
        self.assertTrue(reply.startswith("OK "), reply)
        return s, reply[3:]

    def setpin(self, sock, pin, name):
        return self.ask(sock, f"FB/1.3 SETPIN {pin} {name}", "SETPIN: ")

    def linkpin(self, sock, name, pin):
        return self.ask(sock, f"FB/1.3 LINKPIN {name} {pin}", "LINKPIN: ")

    def test_link_needs_a_signed_in_account(self):
        self.start()
        s = self.session()
        self.assertEqual(self.setpin(s, "1234", "alice"), "NOT_SIGNED_IN")
        self.assertEqual(self.linkpin(s, "alice", "1234"), "NOT_SIGNED_IN")

    def test_link_pin_shape_is_checked(self):
        self.start()
        s = self.session("alice")
        for bad in ("123 alice", "123456789 alice", "12a4 alice", "1234 bad:name", "1234"):
            self.assertEqual(self.ask(s, f"FB/1.3 SETPIN {bad}", "SETPIN: "), "INVALID", bad)

    def test_link_folds_the_device_into_the_pin_account(self):
        wk = monday_of(today())
        self.start(seed=f"v2 {wk} {today()}\n" + self.line("phone", 3, 1, 50)
                   + self.line("browser", 2, 2, 30))
        phone = self.session("phone")
        self.submit(phone, 0, 20, 300000, "dchau")
        self.assertEqual(self.setpin(phone, "4321", "dchau"), "OK")

        browser = self.session("browser")
        self.submit(browser, 0, 25, 900000, "dchau")
        self.submit(browser, 2, 10, 100000, "dchau", 900)
        # A wrong PIN changes nothing.
        self.assertEqual(self.linkpin(browser, "dchau", "0000"), "WRONG_PIN")
        self.assertEqual(len(self.board(browser, 0)[1]), 2)
        # The name is matched without case, like the board's names.
        self.assertEqual(self.linkpin(browser, "DChau", "4321"), f"OK {self.acct.id('phone')}")

        # One line now, the phone's account, with the best of both.
        anon = self.session()
        self.assertEqual(self.board(anon, 0)[1], [f"{self.acct.tagged('phone', 'dchau')}=25/900000/0"])
        self.assertEqual(self.board(anon, 2)[1], [f"{self.acct.tagged('phone', 'dchau')}=10/100000/900"])
        wins = self.weekly(anon)[1]
        self.assertIn(f"{self.acct.tagged('phone')}=5", wins)
        self.assertNotIn(self.acct.tagged("browser"), wins)
        # The browser's connection carries on as the phone's account...
        self.assertEqual(self.submit(browser, 0, 30, 100000, "dchau"), "OK 1 1")
        self.assertEqual(self.board(anon, 0)[1], [f"{self.acct.tagged('phone', 'dchau')}=30/100000/0"])
        # ...and signs in as it from now on, across a restart.
        self.server.kill()
        self.server.wait(timeout=5)
        self.start()
        _, got = self.sign_in_as("browser")
        self.assertEqual(got, self.acct.id("phone"))

    def test_link_name_taken_by_another_account(self):
        self.start()
        self.assertEqual(self.setpin(self.session("alice"), "1111", "alice"), "OK")
        self.assertEqual(self.setpin(self.session("mallory"), "2222", "Alice"), "NAME_TAKEN")
        # The owner may change it, and the old PIN stops working.
        a = self.session("alice")
        self.assertEqual(self.setpin(a, "3333", "alice"), "OK")
        other = self.session("bob")
        self.assertEqual(self.linkpin(other, "alice", "1111"), "WRONG_PIN")
        self.assertEqual(self.linkpin(other, "alice", "3333"), f"OK {self.acct.id('alice')}")

    def test_link_wrong_guesses_are_limited_per_name(self):
        self.start()
        self.assertEqual(self.setpin(self.session("alice"), "9876", "alice"), "OK")
        g = self.session("mallory")
        for pin in ("0001", "0002", "0003", "0004", "0005"):
            self.assertEqual(self.linkpin(g, "alice", pin), "WRONG_PIN")
        # Even the right PIN is refused until the hour is up.
        self.assertEqual(self.linkpin(self.session("bob"), "alice", "9876"), "TOO_MANY_TRIES")
        # An unknown name answers like a wrong PIN.
        self.assertEqual(self.linkpin(g, "nobody", "1234"), "WRONG_PIN")

    def test_link_to_itself_changes_nothing(self):
        self.start()
        a = self.session("alice")
        self.assertEqual(self.setpin(a, "1234", "alice"), "OK")
        self.assertEqual(self.linkpin(a, "alice", "1234"), f"OK {self.acct.id('alice')}")

    def test_link_delete_account_drops_pin_and_links(self):
        self.start()
        a = self.session("alice")
        self.assertEqual(self.setpin(a, "1234", "alice"), "OK")
        b = self.session("bob")
        self.assertEqual(self.linkpin(b, "alice", "1234"), f"OK {self.acct.id('alice')}")
        self.assertEqual(self.ask(b, "FB/1.3 DELETEACCOUNT", "DELETEACCOUNT: "), "OK")
        # Bob's device is itself again and the PIN is gone.
        _, got = self.sign_in_as("bob")
        self.assertEqual(got, self.acct.id("bob"))
        self.assertEqual(self.linkpin(self.session("carol"), "alice", "1234"), "WRONG_PIN")


if __name__ == "__main__":
    names = sorted(n for n in dir(LinksTest) if n.startswith("test_link"))
    result = unittest.TextTestRunner(verbosity=2).run(
        unittest.TestSuite(LinksTest(n) for n in names))
    sys.exit(0 if result.wasSuccessful() else 1)
