#!/usr/bin/env python3
"""Regression test: simultaneous departures must not free an active game frame.

When several in-game clients disconnected together, player_part_game_ relayed
each departure through process_msg_prio_().  A failed send to an already-closed
peer queued re-entrant connection cleanup, which could remove and free the same
game while the outer player_part_game_ frame still held its pointer.  The outer
frame then read players_number through that dangling pointer.  Commit b63d5437
fixed the lifetime bug by checking that the game is still live after the relay.
This test drives the real protocol and verifies that a sanitizer-built server
survives the mass departure without an AddressSanitizer or UBSan diagnostic.
"""

import os
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path


PROTO = "FB/1.3"
BUFSZ = 65536
HOST = "127.0.0.1"
BAD_DIAGNOSTICS = ("ERROR: AddressSanitizer", "runtime error:")


def parse_can_start(payload, my_nick):
    """Return the seat map and my seat from <seat-byte><nick>, records."""
    seats, mine, i = {}, 0, 0
    while i < len(payload):
        sid = payload[i]
        i += 1
        nick_bytes = bytearray()
        while i < len(payload) and payload[i] != ord(","):
            nick_bytes.append(payload[i])
            i += 1
        if i < len(payload) and payload[i] == ord(","):
            i += 1
        nick = nick_bytes.decode("latin1")
        if not nick:
            continue
        seats[sid] = nick
        if nick == my_nick:
            mine = sid
    return seats, mine


class Peer:
    def __init__(self, name, host, port):
        self.name, self.host, self.port = name, host, port
        self.sock = None
        self.buf = bytearray()
        self.in_game = False
        self.seat = 0
        self.seats = {}
        self.lobby_lines = []
        self.game_msgs = []
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
                self._dispatch(raw.rstrip(b"\r"))

    def _dispatch(self, raw):
        if not raw:
            return
        if self.in_game:
            self.game_msgs.append((raw[0], raw[1:].decode("latin1", "replace")))
            return
        text = raw.decode("latin1", "replace")
        self.lobby_lines.append(text)
        marker = b"PUSH: GAME_CAN_START: "
        if marker in raw:
            self.seats, self.seat = parse_can_start(
                raw.split(marker, 1)[1], self.name
            )

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


def _is_sanitizer_build(binary):
    """Was this binary linked against AddressSanitizer?

    ASan interceptors leave __asan_* symbols in the binary's symbol table.
    Absence of `nm` is treated as "not sanitized" so the test skips rather
    than claiming coverage it cannot deliver.
    """
    try:
        out = subprocess.run(
            ["nm", "-u", str(binary)],
            capture_output=True,
            text=True,
            timeout=30,
        )
    except (OSError, subprocess.SubprocessError):
        return False
    return "__asan_" in (out.stdout or "")


class ServerMassLeaveTest(unittest.TestCase):
    def setUp(self):
        if len(sys.argv) < 2:
            self.skipTest("fb-server binary path not passed as argv[1]")
        self.server_path = Path(sys.argv[1])
        if not self.server_path.exists():
            self.skipTest(f"fb-server binary not found at {self.server_path}")
        if not _is_sanitizer_build(self.server_path):
            # BUG-049's corruption is silent without AddressSanitizer: the freed
            # bytes just decide a branch. Passing here would make this a false
            # guard, so skip loudly instead and say how to get real coverage.
            self.skipTest(
                f"{self.server_path} is not an AddressSanitizer build, and this "
                "regression cannot be detected without one. Configure with "
                "-DCMAKE_C_FLAGS='-fsanitize=address,undefined' "
                "-DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' "
                "to exercise it."
            )

        self.peers = []
        self.server = None
        self.port = None
        self.log_stream = None
        self.temp_home = tempfile.TemporaryDirectory(prefix="fb-server-massleave-")
        self.log_path = Path(self.temp_home.name) / "fb-server.log"

        try:
            self.port = self._find_free_port()
            child_env = os.environ.copy()
            child_env["HOME"] = self.temp_home.name
            child_env["PATH"] = os.environ.get("PATH", os.defpath)
            child_env["ASAN_OPTIONS"] = (
                "detect_leaks=0:halt_on_error=1:abort_on_error=1"
            )
            child_env["UBSAN_OPTIONS"] = "print_stacktrace=1:halt_on_error=1"

            self.log_stream = self.log_path.open("wb", buffering=0)
            self.server = subprocess.Popen(
                [
                    str(self.server_path),
                    "-d",
                    "-q",
                    "-z",
                    "-p",
                    str(self.port),
                    "-o",
                    "DEBUG",
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
                    + self._log_excerpt(self._read_log())
                )
            try:
                with socket.create_connection((HOST, self.port), timeout=0.2):
                    return
            except OSError:
                time.sleep(0.05)
        self.fail(
            "server never started listening:\n"
            + self._log_excerpt(self._read_log())
        )

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

        released = self.port is None or self._wait_for_port_release()
        self.port = None

        if self.temp_home is not None:
            self.temp_home.cleanup()
            self.temp_home = None

        if not released:
            self.fail("fb-server test port was not released after shutdown")

    def _wait_for_port_release(self):
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            try:
                with socket.create_connection((HOST, self.port), timeout=0.2):
                    pass
            except ConnectionRefusedError:
                return True
            except OSError:
                pass
            time.sleep(0.05)
        return False

    def _read_log(self):
        try:
            return self.log_path.read_text(encoding="utf-8", errors="replace")
        except OSError as error:
            return f"<unable to read server log: {error}>"

    @staticmethod
    def _log_excerpt(log):
        diagnostic_offsets = [
            log.find(marker) for marker in BAD_DIAGNOSTICS if marker in log
        ]
        if diagnostic_offsets:
            start = max(0, min(diagnostic_offsets) - 1000)
            return log[start:start + 16000]
        return log[-16000:]

    def _assert_reply(self, peer, needle, timeout=3.0):
        line = peer.wait_line(needle, timeout)
        self.assertIsNotNone(
            line,
            f"{peer.name} did not receive {needle!r}; "
            f"lobby lines={peer.lobby_lines!r}\n"
            f"server log:\n{self._log_excerpt(self._read_log())}",
        )

    def _pump_peers(self, duration):
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            for peer in self.peers:
                peer.pump(0.02)

    def _start_three_player_game(self):
        count = 3
        for index in range(count):
            peer = Peer(f"ml{index:02d}", HOST, self.port)
            self.peers.append(peer)
            ready = peer.connect()
            self.assertIsNotNone(ready, f"{peer.name} did not receive SERVER_READY")
            peer.send_cmd(f"NICK {peer.name}")
            self._assert_reply(peer, "NICK: OK")

        room = self.peers[0].name
        self.peers[0].send_cmd(f"CREATE {room} {count}")
        self._assert_reply(self.peers[0], "CREATE: OK")
        for peer in self.peers[1:]:
            peer.send_cmd(f"JOIN {room} {peer.name}")
            self._assert_reply(peer, "JOIN: OK")
        self._pump_peers(0.15)

        self.peers[0].send_cmd("START")
        deadline = time.monotonic() + 6.0
        while time.monotonic() < deadline:
            for peer in self.peers:
                peer.pump(0.02)
            if all(peer.seat for peer in self.peers):
                break
        self.assertTrue(
            all(peer.seat for peer in self.peers),
            "not every peer learned its seat before game start: "
            + repr(
                [
                    (peer.name, peer.seat, peer.lobby_lines)
                    for peer in self.peers
                ]
            ),
        )
        self.assertEqual(
            len({peer.seat for peer in self.peers}),
            count,
            "server assigned duplicate seats",
        )

        reply_starts = {}
        for peer in self.peers:
            reply_starts[peer.name] = len(peer.lobby_lines)
            peer.send_cmd("OK_GAME_START")

        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline:
            for peer in self.peers:
                peer.pump(0.02)
                if not peer.in_game and any(
                    line.endswith("OK_GAME_START: OK")
                    for line in peer.lobby_lines[reply_starts[peer.name]:]
                ):
                    peer.in_game = True
            if all(peer.in_game for peer in self.peers):
                break
        self.assertTrue(
            all(peer.in_game for peer in self.peers),
            "not every peer reached in-game state: "
            + repr(
                [
                    (peer.name, peer.in_game, peer.lobby_lines)
                    for peer in self.peers
                ]
            ),
        )

    def _play_one_round_of_shots(self):
        for index, peer in enumerate(self.peers):
            peer.send_game(f"f{0.5 + 0.1 * index:.3f}:{index % 8}")
        self._pump_peers(0.15)

        for index, peer in enumerate(self.peers):
            colors = " ".join(str((index + offset) % 8) for offset in range(8))
            peer.send_game(f"s{index % 8}:0:{index % 8}:{colors}")
        self._pump_peers(0.15)

    def _close_all_peers_together(self):
        barrier = threading.Barrier(len(self.peers) + 1)
        errors = []
        errors_lock = threading.Lock()

        def close_peer(peer):
            try:
                barrier.wait(timeout=2.0)
                peer.close()
            except Exception as error:
                with errors_lock:
                    errors.append(f"{peer.name}: {error!r}")

        threads = [
            threading.Thread(target=close_peer, args=(peer,))
            for peer in self.peers
        ]
        for thread in threads:
            thread.start()
        barrier.wait(timeout=2.0)
        for thread in threads:
            thread.join(timeout=2.0)

        self.assertFalse(
            any(thread.is_alive() for thread in threads),
            "a peer close thread did not finish",
        )
        self.assertEqual(errors, [], f"peer close failed: {errors!r}")

    def _poll_after_disconnect(self):
        deadline = time.monotonic() + 1.5
        activity_at = None
        diagnostic_at = None
        while time.monotonic() < deadline:
            now = time.monotonic()
            log = self._read_log()
            if self.server.poll() is not None:
                return
            if diagnostic_at is None and any(
                marker in log for marker in BAD_DIAGNOSTICS
            ):
                diagnostic_at = now
            if activity_at is None and (
                "left during game" in log or "Game ended:" in log
            ):
                activity_at = now
            if diagnostic_at is not None and now - diagnostic_at >= 0.25:
                return
            if activity_at is not None and now - activity_at >= 0.20:
                return
            time.sleep(0.02)

    def test_simultaneous_disconnect_does_not_use_freed_game(self):
        self._start_three_player_game()
        self._play_one_round_of_shots()
        self._close_all_peers_together()
        self._poll_after_disconnect()

        status = self.server.poll()
        log = self._read_log()
        excerpt = self._log_excerpt(log)
        bad_markers = [marker for marker in BAD_DIAGNOSTICS if marker in log]

        with self.subTest("server survives simultaneous disconnects"):
            self.assertIsNone(
                status,
                f"fb-server exited with status {status} after mass leave; "
                f"server log:\n{excerpt}",
            )
        with self.subTest("sanitizer log remains clean"):
            self.assertEqual(
                bad_markers,
                [],
                f"sanitizer diagnostic(s) {bad_markers!r} found after mass leave; "
                f"server log:\n{excerpt}",
            )


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]] + sys.argv[2:])
