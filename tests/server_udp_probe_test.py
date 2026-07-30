#!/usr/bin/env python3
"""Regression test: a full-length LAN discovery datagram must stay in bounds.

handle_udp_request() zeroed msg[128] and then handed the whole sizeof(msg) to
recvfrom(), so a datagram that filled the buffer overwrote every zero the memset
had placed.  The validation that follows mixes a bounded check with an unbounded
one: the strncmp against the "FB/<major>." prefix is length-limited, but the
strstr for " SERVER PROBE" scans for a terminator that is no longer present and
runs off the end of the stack buffer.  Commit dd34182f reserved the final byte
and terminates the datagram from the returned length.

This test sends a maximum-length datagram carrying the prefix needed to reach
the strstr, and verifies that a sanitizer-built server survives it with no
diagnostic.  It also sends a well-formed probe as a positive control, so a
server that simply stopped answering could not pass.

Note the UDP listener always binds DEFAULT_PORT (1511) -- create_udp_server()
ignores -p -- so this port cannot be randomised, and the test skips when it is
already in use.
"""

import os
import socket
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path


HOST = "127.0.0.1"
UDP_DISCOVERY_PORT = 1511  # DEFAULT_PORT in server/net.c; not configurable
PROBE_TAIL = b" SERVER PROBE"
BAD_DIAGNOSTICS = ("ERROR: AddressSanitizer", "runtime error:")


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


def _udp_port_in_use(port):
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.bind(("", port))
    except OSError:
        return True
    finally:
        probe.close()
    return False


class ServerUdpProbeTest(unittest.TestCase):
    def setUp(self):
        if len(sys.argv) < 2:
            self.skipTest("fb-server binary path not passed as argv[1]")
        self.server_path = Path(sys.argv[1])
        if not self.server_path.exists():
            self.skipTest(f"fb-server binary not found at {self.server_path}")
        if not _is_sanitizer_build(self.server_path):
            # Without AddressSanitizer the over-read is silent -- strstr simply
            # walks into whatever follows on the stack.  Passing here would make
            # this a false guard, so skip loudly and say how to get real coverage.
            self.skipTest(
                f"{self.server_path} is not an AddressSanitizer build, and this "
                "regression cannot be detected without one. Configure with "
                "-DCMAKE_C_FLAGS='-fsanitize=address,undefined' "
                "-DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' "
                "to exercise it."
            )
        if _udp_port_in_use(UDP_DISCOVERY_PORT):
            self.skipTest(
                f"UDP {UDP_DISCOVERY_PORT} is already bound (another fb-server "
                "is probably running). The discovery listener hardcodes this "
                "port, so this test cannot pick another one."
            )

        self.server = None
        self.log_stream = None
        self.temp_home = tempfile.TemporaryDirectory(prefix="fb-server-udpprobe-")
        self.log_path = Path(self.temp_home.name) / "fb-server.log"

        try:
            self.tcp_port = self._find_free_tcp_port()
            child_env = os.environ.copy()
            # fb-server derives its stats path from $HOME unconditionally and
            # would otherwise touch the developer's real ~/.fb-server/stats.dat.
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
                    "-d",  # do not daemonize: without this the parent exits
                    "-l",  # create the UDP discovery listener
                    "-z",
                    "-p",
                    str(self.tcp_port),
                    "-o",
                    "DEBUG",
                ],
                stdout=self.log_stream,
                stderr=subprocess.STDOUT,
                env=child_env,
            )
            self._wait_until_udp_bound()
        except Exception:
            self._cleanup()
            raise

    def tearDown(self):
        self._cleanup()

    @staticmethod
    def _find_free_tcp_port():
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
            probe.bind((HOST, 0))
            return probe.getsockname()[1]

    def _wait_until_udp_bound(self, timeout=10.0):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.server.poll() is not None:
                self.fail(
                    "fb-server exited during startup:\n" + self._log_excerpt()
                )
            if _udp_port_in_use(UDP_DISCOVERY_PORT):
                return
            time.sleep(0.05)
        self.fail(f"fb-server did not bind UDP {UDP_DISCOVERY_PORT} in time")

    def _send_datagram(self, payload, timeout=2.0):
        """Send one datagram; return the reply, or None if none arrived."""
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(timeout)
            sock.sendto(payload, (HOST, UDP_DISCOVERY_PORT))
            try:
                return sock.recvfrom(512)[0]
            except socket.timeout:
                return None

    def _log_excerpt(self, limit=2500):
        try:
            text = self.log_path.read_text(errors="replace")
        except OSError:
            return "<log unavailable>"
        return text[-limit:]

    def _cleanup(self):
        if self.server is not None and self.server.poll() is None:
            self.server.terminate()
            try:
                self.server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.server.kill()
                self.server.wait(timeout=5)
        self.server = None
        if self.log_stream is not None:
            self.log_stream.close()
            self.log_stream = None
        if getattr(self, "temp_home", None) is not None:
            self.temp_home.cleanup()
            self.temp_home = None

    def test_full_length_datagram_stays_in_bounds(self):
        # Exactly the buffer size, with no NUL anywhere, so nothing terminates
        # it.  The "FB/1." prefix matters: it makes the bounded strncmp succeed
        # so that control actually reaches the unbounded strstr.
        payload = b"FB/1." + b"A" * (128 - 5)
        self.assertEqual(len(payload), 128)
        self.assertNotIn(0, payload)
        self._send_datagram(payload)

        # A well-formed probe must still be answered.  Without this control, a
        # server that had stopped replying entirely would pass the test above.
        reply = self._send_datagram(b"FB/1.3" + PROBE_TAIL)

        with self.subTest("server survives the full-length datagram"):
            self.assertIsNone(
                self.server.poll(),
                "fb-server exited while handling the datagrams:\n"
                + self._log_excerpt(),
            )
        with self.subTest("discovery still answers a well-formed probe"):
            self.assertIsNotNone(reply, "no reply to a well-formed probe")
            self.assertIn(b"SERVER HERE AT PORT", reply)
        with self.subTest("sanitizer log stays clean"):
            log = self._log_excerpt(limit=20000)
            for marker in BAD_DIAGNOSTICS:
                self.assertNotIn(
                    marker, log, f"{marker} reported:\n{log}"
                )


CTEST_SKIP_RETURN_CODE = 77


def main():
    """Run the suite, reporting a wholly-skipped run to ctest as a skip.

    unittest exits 0 for a skipped test, which ctest would display as a pass --
    reading as coverage that never actually ran. Exit 77 instead; CMake maps it
    to SKIPPED via SKIP_RETURN_CODE.
    """
    suite = unittest.TestLoader().loadTestsFromTestCase(ServerUdpProbeTest)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if result.failures or result.errors:
        return 1
    if result.skipped and len(result.skipped) == result.testsRun:
        return CTEST_SKIP_RETURN_CODE
    return 0


if __name__ == "__main__":
    sys.exit(main())
