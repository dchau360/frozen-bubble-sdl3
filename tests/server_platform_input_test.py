#!/usr/bin/env python3
"""Per-player platform and input-device tags: the PLATFORM command, the 'i'
opcode, and the two places fb-server surfaces them.

Both are self-declared, one-char, and purely cosmetic -- the same trust
posture BOT already has, and for the same reason: nothing on the wire can
prove what OS a client runs on or what its player's hands are touching. What
this file pins is not the honesty of the tags but the plumbing around them:
that a tag reaches LIST without breaking the format older clients parse, that
it reaches the relay datagram in the right field, and that a client which
never sends one is indistinguishable from how things worked before the
feature existed.

The last of those is most of the file. A room can mix this build with the
previous release, and a server can outlive several client versions, so
"nobody reported anything" and "only some players reported" both have to come
out looking deliberate rather than half-broken.

Drives the real binary over the real protocol, same as
server_discordalert_test.py, whose relay-stand-in harness this reuses.
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


class ServerPlatformInputTest(unittest.TestCase):
    def setUp(self):
        if len(sys.argv) < 2:
            self.skipTest("fb-server binary path not passed as argv[1]")
        self.server_path = Path(sys.argv[1])
        if not self.server_path.exists():
            self.skipTest(f"fb-server binary not found at {self.server_path}")

        self.relay = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.relay.bind(("127.0.0.1", 0))
        self.relay.settimeout(0.2)
        self.relay_port = self.relay.getsockname()[1]

        self.tmpdir = tempfile.TemporaryDirectory()
        env = dict(os.environ)
        env["FB_SERVER_DISCORD_RELAY"] = f"127.0.0.1:{self.relay_port}"
        env["FB_SERVER_STATS_FILE"] = str(Path(self.tmpdir.name) / "stats.dat")

        self.port = 15521
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

    # -- helpers ----------------------------------------------------------

    def connect(self):
        s = socket.create_connection(("127.0.0.1", self.port), timeout=3.0)
        self.socks.append(s)
        recv_until(s, b"SERVER_READY")
        return s

    def cmd(self, sock, line, expect):
        sock.sendall(f"FB/1.3 {line}\n".encode())
        return recv_until(sock, expect.encode())

    def nick(self, sock, name):
        self.assertIn(b"NICK: OK", self.cmd(sock, f"NICK {name}", "NICK:"))

    def list_line(self, sock):
        got = self.cmd(sock, "LIST", "LIST:").decode(errors="replace")
        for line in got.split("\n"):
            idx = line.find("LIST: ")
            if idx != -1:
                return line[idx + len("LIST: "):]
        self.fail(f"no LIST response in {got!r}")

    def entry_for(self, sock, nick):
        """That player's own comma-separated LIST entry, badge included."""
        line = self.list_line(sock)
        open_players = line.split(" ", 1)[0]
        for entry in open_players.split(","):
            if entry.split(":")[0] == nick:
                return entry
        self.fail(f"{nick} not found in LIST open players {open_players!r}")

    def drain_relay(self):
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

    def start_two_player_game(self, room, platforms=(None, None)):
        """A room in GAME_STATUS_PLAYING, creator first. platforms is the tag
        each client sends before its NICK, or None for a client that never
        sends one at all (the pre-1.4 case)."""
        socks = []
        for name, tag in zip(("creator", "guest1"), platforms):
            s = self.connect()
            if tag:
                self.assertIn(b"PLATFORM: OK", self.cmd(s, f"PLATFORM {tag}", "PLATFORM:"))
            self.nick(s, name)
            socks.append(s)
        a, b = socks
        self.assertIn(b"CREATE: OK", self.cmd(a, f"CREATE {room}", "CREATE:"))
        self.assertIn(b"JOIN: OK", self.cmd(b, f"JOIN {room} guest1", "JOIN:"))
        self.assertIn(b"START: OK", self.cmd(a, "START", "START:"))
        # Every player acknowledges, not just the creator: OK_GAME_START is
        # what puts that one connection into prio mode (ok_start_game in
        # game.c), and a socket still in lobby mode has its raw "?i..." line
        # rejected as a protocol violation rather than read as an opcode.
        for s in (a, b):
            self.assertIn(b"OK_GAME_START: OK",
                          self.cmd(s, "OK_GAME_START", "OK_GAME_START:"))
        self.drain_relay()  # the two arrival alerts
        return a, b

    def result_datagram(self):
        fired = [d for d in self.drain_relay() if d.startswith("RESULT|")]
        self.assertEqual(len(fired), 1, f"expected one RESULT, got {fired!r}")
        # Field order is documented in discordalert.c; servername is the
        # unbounded remainder, so it stays last and is split off by count.
        parts = fired[0].split("|", 11)
        self.assertEqual(len(parts), 12, f"unexpected RESULT shape: {fired[0]!r}")
        self.assertTrue(parts[11], "servername must remain the trailing field")
        return {
            "winner": parts[4],
            # players_nick[0] is CREATE's argument -- the room name, not the
            # creator's lobby nick. See build_roster_csv()'s note in game.c.
            "roster": parts[5],
            "wins": parts[6],
            "platforms": parts[8],
            "inputs": parts[9],
            "countries": parts[10],
        }

    # -- PLATFORM in LIST -------------------------------------------------

    def test_platform_appears_after_an_empty_geoloc_field(self):
        # The common shape: PLATFORM lands with NICK, GEOLOC's ~16s lookup has
        # not finished, so the middle field is empty. This is exactly the entry
        # both existing parsers have to survive -- see append_player_list_tags.
        a = self.connect()
        self.cmd(a, "PLATFORM W", "PLATFORM:")
        self.nick(a, "winuser")
        self.assertEqual(self.entry_for(a, "winuser"), "winuser::W")

    def test_platform_follows_a_geoloc_without_disturbing_it(self):
        a = self.connect()
        self.cmd(a, "PLATFORM L", "PLATFORM:")
        self.nick(a, "linuser")
        self.assertIn(b"GEOLOC: OK", self.cmd(a, "GEOLOC 37.7:-122.4", "GEOLOC:"))
        entry = self.entry_for(a, "linuser")
        self.assertEqual(entry, "linuser:37.7:-122.4:L")
        # The first three colon-separated fields are byte-for-byte what a
        # pre-1.4 server sent, which is what makes the old parsers work: they
        # read nick/lat/lon and never look past them.
        self.assertEqual(entry.split(":")[:3], ["linuser", "37.7", "-122.4"])

    def test_no_platform_leaves_the_entry_exactly_as_before(self):
        a = self.connect()
        self.nick(a, "olduser")
        self.assertEqual(self.entry_for(a, "olduser"), "olduser")

    def test_no_platform_with_a_geoloc_leaves_the_entry_exactly_as_before(self):
        a = self.connect()
        self.nick(a, "oldgeo")
        self.assertIn(b"GEOLOC: OK", self.cmd(a, "GEOLOC 51.5:-0.1", "GEOLOC:"))
        self.assertEqual(self.entry_for(a, "oldgeo"), "oldgeo:51.5:-0.1")

    def test_every_documented_tag_is_accepted(self):
        for tag in "WMLAIB":
            s = self.connect()
            self.assertIn(b"PLATFORM: OK", self.cmd(s, f"PLATFORM {tag}", "PLATFORM:"),
                          f"tag {tag} must be accepted")

    def test_a_tag_outside_the_set_is_rejected_and_stored_nowhere(self):
        # Rejected rather than truncated or silently kept: the closed set is
        # what guarantees no ':' or '|' can reach the LIST line or a relay
        # datagram, so a tag that is not in it must leave no trace at all.
        a = self.connect()
        self.nick(a, "badtag")
        for bad in ("X", "WW", "w", ":", "|", "W:L"):
            self.assertIn(b"PLATFORM: INVALID_PLATFORM",
                          self.cmd(a, f"PLATFORM {bad}", "PLATFORM:"),
                          f"{bad!r} must be rejected")
        self.assertEqual(self.entry_for(a, "badtag"), "badtag")

    def test_platform_with_no_argument_is_rejected(self):
        a = self.connect()
        self.assertIn(b"PLATFORM: MISSING_ARGUMENTS", self.cmd(a, "PLATFORM", "PLATFORM:"))

    def test_a_later_platform_replaces_the_earlier_one(self):
        # No reason a client should do this, but the handler overwrites rather
        # than appending, and a second tag must not end up stacked in LIST.
        a = self.connect()
        self.cmd(a, "PLATFORM W", "PLATFORM:")
        self.nick(a, "switcher")
        self.cmd(a, "PLATFORM B", "PLATFORM:")
        self.assertEqual(self.entry_for(a, "switcher"), "switcher::B")

    # -- PLATFORM in the join alert ---------------------------------------

    def test_join_alert_carries_a_platform_sent_before_nick(self):
        # Why the client sends PLATFORM first: the alert fires from the NICK
        # handler and reads the tag while doing so.
        a = self.connect()
        self.cmd(a, "PLATFORM M", "PLATFORM:")
        self.nick(a, "macuser")
        fired = self.drain_relay()
        self.assertEqual(len(fired), 1, f"expected one JOIN, got {fired!r}")
        parts = fired[0].split("|", 6)
        self.assertEqual(parts[0], "JOIN")
        self.assertEqual(parts[1], "macuser")
        self.assertEqual(parts[3], "", "GEOLOC still arrives after NICK")
        self.assertEqual(parts[4], "M")
        self.assertEqual(parts[5], "", "COUNTRY rides GEOLOC's ~16s lookup, so it is not here yet")
        self.assertTrue(parts[6], "servername must remain the trailing field")

    def test_join_alert_platform_is_empty_for_a_client_that_never_sends_one(self):
        a = self.connect()
        self.nick(a, "silent")
        fired = self.drain_relay()
        self.assertEqual(len(fired), 1)
        self.assertEqual(fired[0].split("|", 6)[4], "")

    def test_platform_after_nick_misses_its_own_join_alert(self):
        # Documents the ordering cost rather than asserting it is fine: a
        # client that sends PLATFORM second still gets its badge from the next
        # LIST, it just is not in the alert that already fired.
        a = self.connect()
        self.nick(a, "lateuser")
        fired = self.drain_relay()
        self.assertEqual(fired[0].split("|", 6)[4], "")
        self.cmd(a, "PLATFORM A", "PLATFORM:")
        self.assertEqual(self.entry_for(a, "lateuser"), "lateuser::A")

    # -- COUNTRY ----------------------------------------------------------

    def test_country_reaches_the_result_datagram_index_aligned(self):
        room = "ctryroom"
        a, b = self.start_two_player_game(room, platforms=("W", "L"))
        self.assertIn(b"COUNTRY: OK", self.cmd(a, "COUNTRY US", "COUNTRY:"))
        self.assertIn(b"COUNTRY: OK", self.cmd(b, "COUNTRY JP", "COUNTRY:"))
        a.sendall(f"?F{room}\n".encode())
        self.assertEqual(self.result_datagram()["countries"], "US,JP")

    def test_country_is_deliberately_absent_from_list(self):
        # The platform tag is in LIST because the game draws a badge from it.
        # The country is not: it exists for the operator's Discord channel
        # only, and putting it in LIST would hand every client in the lobby a
        # roster of where everyone is from -- which is not what was asked for
        # and not what the in-game UI reads.
        a = self.connect()
        self.cmd(a, "PLATFORM W", "PLATFORM:")
        self.nick(a, "ctryuser")
        self.cmd(a, "COUNTRY US", "COUNTRY:")
        entry = self.entry_for(a, "ctryuser")
        self.assertEqual(entry, "ctryuser::W")
        self.assertNotIn("US", entry)

    def test_a_malformed_country_is_rejected_and_stored_nowhere(self):
        room = "badctry"
        a, b = self.start_two_player_game(room)
        for bad in ("U", "USA", "us", "U1", "1:2", "3.4"):
            self.assertIn(b"COUNTRY: INVALID_COUNTRY",
                          self.cmd(a, f"COUNTRY {bad}", "COUNTRY:"),
                          f"{bad!r} must be rejected")
        a.sendall(f"?F{room}\n".encode())
        self.assertEqual(self.result_datagram()["countries"], ",")

    def test_country_with_no_argument_is_rejected(self):
        a = self.connect()
        self.assertIn(b"COUNTRY: MISSING_ARGUMENTS", self.cmd(a, "COUNTRY", "COUNTRY:"))

    def test_a_room_where_only_one_player_reported_a_country(self):
        room = "onectry"
        a, b = self.start_two_player_game(room)
        self.cmd(b, "COUNTRY CA", "COUNTRY:")
        a.sendall(f"?F{room}\n".encode())
        self.assertEqual(self.result_datagram()["countries"], ",CA")

    # -- the 'i' opcode and the round-result datagram ---------------------

    def test_input_tags_reach_the_result_datagram_index_aligned(self):
        room = "inproom"
        a, b = self.start_two_player_game(room, platforms=("W", "L"))
        a.sendall(b"?iM\n")   # creator on a mouse
        b.sendall(b"?iG\n")   # guest on a gamepad
        time.sleep(0.2)
        a.sendall(b"?Fcreator\n")
        fields = self.result_datagram()
        self.assertEqual(fields["roster"], f"{room},guest1")
        self.assertEqual(fields["platforms"], "W,L")
        self.assertEqual(fields["inputs"], "M,G")

    def test_a_later_input_tag_replaces_the_earlier_one(self):
        # The badge answers "what are they playing with now", so a mid-round
        # switch has to win rather than the round's first report sticking.
        a, b = self.start_two_player_game("swroom", platforms=("W", "W"))
        a.sendall(b"?iK\n")
        time.sleep(0.1)
        a.sendall(b"?iT\n")
        time.sleep(0.2)
        a.sendall(b"?Fcreator\n")
        self.assertEqual(self.result_datagram()["inputs"], "T,")

    def test_an_unknown_input_tag_is_ignored_rather_than_stored(self):
        a, b = self.start_two_player_game("unkroom", platforms=("W", "W"))
        a.sendall(b"?iK\n")
        time.sleep(0.1)
        a.sendall(b"?iZ\n")   # not in the closed set
        time.sleep(0.2)
        a.sendall(b"?Fcreator\n")
        self.assertEqual(self.result_datagram()["inputs"], "K,",
                         "an unrecognized tag must not overwrite a good one")

    def test_a_room_where_nobody_reported_sends_empty_tag_fields(self):
        # The pre-1.4 room, end to end: the fields exist but say nothing, and
        # the roster and win counts beside them are untouched.
        room = "legroom"
        a, b = self.start_two_player_game(room)
        # Claimed under the roster's own slot-0 name (the room name -- see
        # result_datagram) so the win actually lands and the win-count column
        # beside the tag fields is the ordinary populated one, not the "winner
        # matched nobody" degenerate case.
        a.sendall(f"?F{room}\n".encode())
        fields = self.result_datagram()
        self.assertEqual(fields["roster"], f"{room},guest1")
        self.assertEqual(fields["wins"], "1,0")
        self.assertEqual(fields["platforms"], ",")
        self.assertEqual(fields["inputs"], ",")
        self.assertEqual(fields["countries"], ",")

    def test_a_mixed_room_reports_only_the_player_who_sent_a_tag(self):
        a, b = self.start_two_player_game("mixroom", platforms=("B", None))
        b.sendall(b"?iT\n")
        time.sleep(0.2)
        a.sendall(b"?Fguest1\n")
        fields = self.result_datagram()
        self.assertEqual(fields["platforms"], "B,")
        self.assertEqual(fields["inputs"], ",T")

    def test_the_i_opcode_is_still_relayed_to_the_other_player(self):
        # Sniffing must stay read-only with respect to the relay loop: peers
        # draw their badge straight from this message, so an 'i' that the
        # server swallowed would leave every board but the sender's blank.
        a, b = self.start_two_player_game("relroom", platforms=("W", "W"))
        a.sendall(b"?iM\n")
        got = recv_until(b, b"i", timeout=2.0)
        self.assertIn(b"iM", got, f"guest never received the sender's 'i': {got!r}")


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]] + sys.argv[2:], verbosity=2)
