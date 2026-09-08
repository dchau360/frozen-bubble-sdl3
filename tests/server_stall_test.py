#!/usr/bin/env python3
"""Regression test for audit finding BUG-007: a peer that stops reading must
not stall the single-threaded server for everyone else.

Before this fix, every write to a client socket (send_line()'s plain-TCP
branch, and ws_send()'s WebSocket frame loop) was a blocking send() on a
blocking-mode fd. connections_manager() is a single-threaded select() loop,
so one peer whose TCP receive window filled up -- because it stopped
reading, deliberately or not -- could freeze that send() call, and with it
service to every other connected player, for as long as the peer's window
stayed full.

This test builds exactly that scenario with real sockets against a real
fb-server: a flooder and a victim start a game together, the victim stops
calling recv() entirely (without closing its socket), and the flooder blasts
enough real GAMEMSG traffic to fill the victim's kernel receive buffer and
then the server's own per-fd output queue. Throughout, an unrelated control
connection keeps issuing LIST and asserts every reply arrives promptly --
that promptness is the actual claim: the server must not go quiet while one
peer backs up. Finally the victim's stalled backlog must trip the queue's
byte/age cap and get dropped, rather than being buffered without bound.
"""

import os
import socket
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path


PROTO = "FB/1.3"
BUFSZ = 65536
HOST = "127.0.0.1"

# Comfortably past OUTQUEUE_MAX_BYTES (256 KiB, see server/net.c) plus a
# generous allowance for the victim's own kernel receive buffer filling up
# first -- both have to be exceeded before the server's queue starts
# growing at all. Small GAMEMSG fire messages, blasted as fast as the
# loopback socket accepts them, get here in well under a second.
FLOOD_TARGET_BYTES = 3 * 1024 * 1024
# server/net.c: OUTQUEUE_MAX_AGE_SECONDS. The victim must be dropped within
# this long of its backlog going stale, plus slack for scheduling.
OUTQUEUE_MAX_AGE_SECONDS = 30


class Peer:
    """A minimal real client: enough protocol to reach in-game state."""

    def __init__(self, name, host, port):
        self.name, self.host, self.port = name, host, port
        self.sock = None
        self.buf = bytearray()
        self.seat = 0
        self.lobby_lines = []
        self.closed_by_server = False

    def connect(self, timeout=5.0):
        self.sock = socket.create_connection((self.host, self.port), timeout=timeout)
        self.sock.settimeout(0.05)
        return self.wait_line("SERVER_READY", 5.0)

    def send_cmd(self, cmd):
        self.sock.sendall(f"{PROTO} {cmd}\n".encode("latin1"))

    def send_game(self, payload):
        self.sock.sendall(bytes((self.seat,)) + payload.encode("latin1") + b"\n")

    def _recv(self):
        try:
            chunk = self.sock.recv(BUFSZ)
        except socket.timeout:
            return
        except OSError:
            self.closed_by_server = True
            return
        if not chunk:
            self.closed_by_server = True
            return
        self.buf.extend(chunk)

    def pump(self, duration=0.15):
        end = time.monotonic() + duration
        while time.monotonic() < end:
            if self.sock is None:
                return
            self._recv()
            if self.closed_by_server:
                return
            while True:
                newline = self.buf.find(b"\n")
                if newline < 0:
                    break
                raw = bytes(self.buf[:newline])
                del self.buf[:newline + 1]
                text = raw.rstrip(b"\r").decode("latin1", "replace")
                if text:
                    self.lobby_lines.append(text)

    def wait_line(self, needle, timeout=3.0):
        end = time.monotonic() + timeout
        index = 0
        while time.monotonic() < end:
            while index < len(self.lobby_lines):
                if needle in self.lobby_lines[index]:
                    return self.lobby_lines[index]
                index += 1
            self.pump(0.05)
            if self.closed_by_server:
                break
        return None

    def close(self):
        if self.sock is not None:
            try:
                self.sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            self.sock.close()
            self.sock = None


class ServerStallTest(unittest.TestCase):
    def setUp(self):
        if len(sys.argv) < 2:
            self.skipTest("fb-server binary path not passed as argv[1]")
        self.server_path = Path(sys.argv[1])
        if not self.server_path.exists():
            self.skipTest(f"fb-server binary not found at {self.server_path}")

        self.peers = []
        self.server = None
        self.port = None
        self.log_stream = None
        self.temp_home = tempfile.TemporaryDirectory(prefix="fb-server-stall-")
        self.log_path = Path(self.temp_home.name) / "fb-server.log"

        try:
            self.port = self._find_free_port()
            child_env = os.environ.copy()
            child_env["HOME"] = self.temp_home.name
            child_env["PATH"] = os.environ.get("PATH", os.defpath)
            # Halt-on-error left at its default (usually on): if this scenario
            # trips anything AddressSanitizer/UBSan cares about in the new
            # queue code, the test should see the server die, not limp on.
            child_env.setdefault("ASAN_OPTIONS", "detect_leaks=0")

            self.log_stream = self.log_path.open("wb", buffering=0)
            self.server = subprocess.Popen(
                [
                    str(self.server_path),
                    # -d: stay in the foreground rather than daemonizing, so
                    # this Popen handle tracks the actual server process.
                    "-d", "-q", "-z", "-p", str(self.port),
                    # -g: a generous in-game gracetime, well past the queue's
                    # own OUTQUEUE_MAX_AGE_SECONDS below. The default 5s
                    # gracetime kicks any connection that has SENT nothing
                    # in 5s -- unrelated to BUG-007, which is about the
                    # server's *output* to a peer that stops reading. Left
                    # at the default, gracetime fires first every time
                    # (independently confirmed against the pre-BUG-007
                    # server too) and the test would never actually reach
                    # the output queue's own cap. Raising it here isolates
                    # what this test means to exercise.
                    "-g", str(int(OUTQUEUE_MAX_AGE_SECONDS) + 60),
                ],
                stdout=self.log_stream,
                stderr=subprocess.STDOUT,
                env=child_env,
            )
            self._wait_until_listening()
        except Exception:
            self._cleanup()
            raise

    def tearDown(self):
        self._cleanup()

    @staticmethod
    def _find_free_port():
        for _ in range(100):
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
                probe.bind((HOST, 0))
                port = probe.getsockname()[1]
            if (
                port != 1511
                and not 15000 <= port <= 15999
                and not 25600 <= port <= 25700
            ):
                return port
        raise RuntimeError("could not allocate a free non-reserved test port")

    def _wait_until_listening(self):
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            if self.server.poll() is not None:
                self.fail(
                    "server exited before accepting connections:\n"
                    + self._read_log()[-4000:]
                )
            try:
                with socket.create_connection((HOST, self.port), timeout=0.2):
                    return
            except OSError:
                time.sleep(0.05)
        self.fail("server never started listening:\n" + self._read_log()[-4000:])

    def _cleanup(self):
        for peer in self.peers:
            peer.close()
        self.peers.clear()

        if self.server is not None:
            if self.server.poll() is None:
                self.server.kill()
            try:
                self.server.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                self.server.kill()
                self.server.wait(timeout=5.0)
            self.server = None

        if self.log_stream is not None:
            self.log_stream.close()
            self.log_stream = None

        self.port = None

        if self.temp_home is not None:
            self.temp_home.cleanup()
            self.temp_home = None

    def _read_log(self):
        try:
            return self.log_path.read_text(encoding="utf-8", errors="replace")
        except OSError as error:
            return f"<unable to read server log: {error}>"

    def _assert_reply(self, peer, needle, timeout=3.0):
        line = peer.wait_line(needle, timeout)
        self.assertIsNotNone(
            line,
            f"{peer.name} did not receive {needle!r}; "
            f"lobby lines={peer.lobby_lines!r}\n"
            f"server log:\n{self._read_log()[-4000:]}",
        )

    def _pump_peers(self, peers, duration):
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            for peer in peers:
                peer.pump(0.02)

    def _start_two_player_game(self, leader, joiner):
        for peer in (leader, joiner):
            self.peers.append(peer)
            ready = peer.connect()
            self.assertIsNotNone(ready, f"{peer.name} did not receive SERVER_READY")
            peer.send_cmd(f"NICK {peer.name}")
            self._assert_reply(peer, "NICK: OK")

        leader.send_cmd(f"CREATE {leader.name} 2")
        self._assert_reply(leader, "CREATE: OK")
        joiner.send_cmd(f"JOIN {leader.name} {joiner.name}")
        self._assert_reply(joiner, "JOIN: OK")
        self._pump_peers([leader, joiner], 0.15)

        leader.send_cmd("START")
        deadline = time.monotonic() + 6.0
        seats = {}
        while time.monotonic() < deadline:
            for peer in (leader, joiner):
                peer.pump(0.02)
                for line in peer.lobby_lines:
                    if "PUSH: GAME_CAN_START:" in line and peer.name not in seats:
                        seats[peer.name] = True
                        peer.seat = 1 if peer is leader else 2
            if len(seats) == 2:
                break
        self.assertEqual(
            len(seats), 2,
            "not both peers saw GAME_CAN_START: "
            + repr({p.name: p.lobby_lines for p in (leader, joiner)}),
        )

        for peer in (leader, joiner):
            peer.send_cmd("OK_GAME_START")
        self._pump_peers([leader, joiner], 0.5)

    def test_stalled_peer_does_not_block_other_traffic(self):
        flooder = Peer("flooder", HOST, self.port)
        victim = Peer("victim", HOST, self.port)
        self._start_two_player_game(flooder, victim)

        # An independent connection with nothing to do with the game or the
        # stalled peer -- this is the actual assertion. If the server's
        # single event-loop thread is blocked inside a send() to victim,
        # control's unrelated LIST requests stop being answered too.
        control = Peer("control", HOST, self.port)
        self.peers.append(control)
        ready = control.connect()
        self.assertIsNotNone(ready, "control did not receive SERVER_READY")

        # The victim now behaves like a peer that has stopped reading: the
        # socket stays open (no FIN, no RST) but nothing ever calls recv()
        # on it again. Its kernel receive buffer fills from the flood below,
        # then the server's own per-fd queue takes over -- exactly the
        # scenario BUG-007 is about. (No separate reader thread is needed --
        # simply never calling recv() on victim.sock again *is* "stopped
        # reading".)

        # Flood real GAMEMSG traffic through the running game -- the same
        # relay path (process_msg_prio_) every fire/stick message takes.
        # Fire is unrelated to the two-fire-round-trip protocol elsewhere in
        # the suite; only its volume matters here.
        #
        # Sent in small batches (a few thousand bytes, not tens of
        # thousands) with a short pause between them. This isn't about
        # throughput -- it's so the *flooder's own inbound* framing never
        # accumulates past INCOMING_DATA_BUFSIZE (net.c) without a '\n' in
        # it, which trips an unrelated "too much data without LF" guard and
        # kills the flooder itself (independently confirmed against the
        # pre-BUG-007 server too: a single huge sendall() reliably tripped
        # it well before the output queue this test targets ever came into
        # play). A real client never emits gameplay traffic anywhere near
        # this bunched; the pacing here just avoids that unrelated cap so
        # the test can actually reach the one it means to exercise.
        one_message = flooder.seat.to_bytes(1, "big") + b"f0.500:0\n"
        messages_per_batch = 50
        batch = one_message * messages_per_batch
        flooder.sock.settimeout(5.0)

        flood_bytes_sent = 0
        worst_reply_s = 0.0
        replies_seen = 0
        control_check_every = 20  # roughly every 20 * 500B = 10KB sent
        rounds_deadline = time.monotonic() + 60.0
        victim_drop_marker = "Player victim left during game"

        # While the flood runs, repeatedly prove the server is still live:
        # control's LIST must keep getting answered promptly. A stalled
        # single-threaded server would simply stop replying here for as
        # long as the flood is running. Also watch the server's own log for
        # victim's eviction (the byte/age cap, checked below) -- once that
        # has happened there is nothing left to prove by continuing to
        # flood a connection the server has already closed.
        batch_count = 0
        while flood_bytes_sent < FLOOD_TARGET_BYTES and time.monotonic() < rounds_deadline:
            flooder.sock.sendall(batch)
            flood_bytes_sent += len(batch)
            time.sleep(0.002)
            batch_count += 1

            if batch_count % control_check_every != 0:
                continue

            start_index = len(control.lobby_lines)
            request_start = time.monotonic()
            control.send_cmd("LIST")
            line = control.wait_line("LIST:", timeout=2.0)
            elapsed = time.monotonic() - request_start
            self.assertIsNotNone(
                line,
                f"control's LIST got no reply within 2s while flooding victim "
                f"({flood_bytes_sent} bytes sent so far); "
                f"server log:\n{self._read_log()[-4000:]}",
            )
            worst_reply_s = max(worst_reply_s, elapsed)
            replies_seen += 1
            del control.lobby_lines[start_index:]

            if victim_drop_marker in self._read_log():
                break

        victim_already_dropped = victim_drop_marker in self._read_log()
        self.assertTrue(
            victim_already_dropped or flood_bytes_sent >= FLOOD_TARGET_BYTES,
            f"flood did not reach its target within 60s and victim was not "
            f"dropped either ({flood_bytes_sent} of {FLOOD_TARGET_BYTES} "
            f"bytes sent); server log:\n{self._read_log()[-4000:]}",
        )

        with self.subTest("control kept getting prompt replies while victim stalled"):
            self.assertGreater(
                replies_seen, 0,
                "no LIST round completed during the flood -- the timing loop "
                "itself is broken, not just slow",
            )
            # 2s is the per-request timeout above; comfortably below the
            # blocking behaviour this replaces (which had no ceiling at all
            # and would simply have hung for as long as the flood ran).
            self.assertLess(
                worst_reply_s, 2.0,
                f"control's worst LIST reply took {worst_reply_s:.3f}s while "
                f"victim was stalled ({replies_seen} rounds completed); "
                f"server log:\n{self._read_log()[-4000:]}",
            )

        # The victim's backlog must not be kept forever: the byte/age cap
        # (server/net.c, OUTQUEUE_MAX_BYTES / OUTQUEUE_MAX_AGE_SECONDS) has
        # to evict it. Confirm the server actually closes that connection
        # rather than silently holding an ever-growing queue for it.
        #
        # This checks the server's own log rather than probing victim.sock:
        # victim never drains its huge backlog, so a socket-level probe
        # (even MSG_PEEK) just keeps seeing that buffered application data
        # forever and never observes the EOF/RST the closed connection
        # would otherwise show -- the peer being closed and the peer's
        # *unread backlog* being gone are different things, and only the
        # log distinguishes them without first draining megabytes.
        with self.subTest("stalled peer is eventually dropped, not held forever"):
            deadline = time.monotonic() + OUTQUEUE_MAX_AGE_SECONDS + 10.0
            dropped = victim_drop_marker in self._read_log()
            while not dropped and time.monotonic() < deadline:
                time.sleep(0.5)
                dropped = victim_drop_marker in self._read_log()
            self.assertTrue(
                dropped,
                f"server never dropped the stalled victim within "
                f"{OUTQUEUE_MAX_AGE_SECONDS + 10:.0f}s; "
                f"server log:\n{self._read_log()[-4000:]}",
            )

        # And the server process itself must still be alive and clean
        # throughout -- this is also the sanitizer's chance to catch
        # anything wrong in the new queue code (GByteArray lifetime, the
        # accept-time / conn_terminated() reset pairing, etc.) under real
        # backpressure, not just the happy path every other server test
        # exercises.
        with self.subTest("server survives the whole scenario"):
            status = self.server.poll()
            log = self._read_log()
            bad_markers = [
                marker for marker in ("ERROR: AddressSanitizer", "runtime error:")
                if marker in log
            ]
            self.assertIsNone(
                status,
                f"fb-server exited with status {status} during the stall test; "
                f"server log:\n{log[-8000:]}",
            )
            self.assertEqual(
                bad_markers, [],
                f"sanitizer diagnostic(s) {bad_markers!r} found; "
                f"server log:\n{log[-8000:]}",
            )


CTEST_SKIP_RETURN_CODE = 77


def main():
    suite = unittest.TestLoader().loadTestsFromTestCase(ServerStallTest)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.failures or result.errors:
        return 1
    if result.skipped and len(result.skipped) == result.testsRun:
        return CTEST_SKIP_RETURN_CODE
    return 0


if __name__ == "__main__":
    sys.exit(main())
