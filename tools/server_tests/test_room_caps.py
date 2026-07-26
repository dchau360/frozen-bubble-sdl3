#!/usr/bin/env python3
"""
Protocol-level test for per-room max_players support (Task 5).

Verifies:
  - Legacy CREATE (no arg) defaults the room cap to 5; a 6th join is
    rejected with GAME_FULL.
  - New CREATE with an explicit max_players arg (FB/1.3) honors that cap
    (tested with 20 and with a small cap of 3).
  - A pre-1.3 client (FB/1.2) is rejected from joining a room whose cap
    is above 5 (legacy-client guard), but can still join a <=5-cap room.

Starts its own fb-server instance on a non-default port, raw-TCP only
(no client engine involved), and tears the server down in a finally
block so a failing assertion never leaves it running.
"""

import os
import socket
import subprocess
import sys
import time

SERVER_BIN = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..", "build", "server", "fb-server"
)
PORT = 15113  # non-default port
HOST = "127.0.0.1"
SOCK_TIMEOUT = 0.2
SLEEP = 0.05

failures = []


def check(label, cond):
    if cond:
        print(f"PASS: {label}")
    else:
        print(f"FAIL: {label}")
        failures.append(label)


class Client:
    def __init__(self, proto_minor="1.2"):
        self.proto = f"FB/{proto_minor}"
        self.sock = socket.create_connection((HOST, PORT), timeout=SOCK_TIMEOUT)
        self.sock.settimeout(SOCK_TIMEOUT)
        self.buf = b""
        # drain the greeting line (SERVER_READY ...)
        self._recv_line()

    def _recv_line(self, timeout=SOCK_TIMEOUT):
        self.sock.settimeout(timeout)
        # already have a full line buffered?
        if b"\n" in self.buf:
            line, self.buf = self.buf.split(b"\n", 1)
            return line.decode(errors="replace")
        try:
            while b"\n" not in self.buf:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                self.buf += chunk
        except socket.timeout:
            pass
        if b"\n" in self.buf:
            line, self.buf = self.buf.split(b"\n", 1)
            return line.decode(errors="replace")
        line, self.buf = self.buf, b""
        return line.decode(errors="replace")

    def send(self, line):
        self.sock.sendall((line + "\n").encode())
        time.sleep(SLEEP)

    def cmd(self, text):
        """Send a full protocol line (without the FB/x.y prefix) and read one reply line."""
        self.send(f"{self.proto} {text}")
        return self._recv_line()

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


def wait_for_server_ready(timeout=5.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            s = socket.create_connection((HOST, PORT), timeout=SOCK_TIMEOUT)
            s.settimeout(SOCK_TIMEOUT)
            data = b""
            try:
                data = s.recv(4096)
            except socket.timeout:
                pass
            s.close()
            if b"SERVER_READY" in data:
                return True
        except OSError:
            pass
        time.sleep(SLEEP)
    return False


def main():
    if not os.path.exists(SERVER_BIN):
        print(f"FAIL: server binary not found at {SERVER_BIN}")
        sys.exit(1)

    server_proc = subprocess.Popen(
        [SERVER_BIN, "-p", str(PORT), "-q", "-l", "-z"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    try:
        if not wait_for_server_ready():
            print("FAIL: server did not become ready in time")
            failures.append("server startup")
        else:
            run_tests()
    finally:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server_proc.kill()
            server_proc.wait(timeout=5)

    if failures:
        print(f"\n{len(failures)} FAILURE(S): {failures}")
        sys.exit(1)
    else:
        print("\nALL PASS")
        sys.exit(0)


def run_tests():
    # --- Case 1: legacy CREATE (no arg) caps at 5 ---------------------------
    creator = Client(proto_minor="1.2")
    resp = creator.cmd("CREATE alice")
    check("legacy CREATE alice succeeds", resp.endswith("OK"))

    joiners = []
    # 4 more joins should succeed to fill the room to 5 (alice + 4 = 5)
    for i in range(4):
        c = Client(proto_minor="1.2")
        resp = c.cmd(f"JOIN alice j5_{i}")
        check(f"legacy room join {i+1}/4 succeeds (room now has {i+2}/5)", resp.endswith("OK"))
        joiners.append(c)

    # 6th player (room already has 5) must be rejected
    overflow = Client(proto_minor="1.2")
    resp = overflow.cmd("JOIN alice overflow5")
    check("legacy room 6th join rejected (GAME_FULL)", "GAME_FULL" in resp)
    overflow.close()
    for c in joiners:
        c.close()
    creator.close()

    # --- Case 2: new CREATE with max_players=20 ------------------------------
    bob = Client(proto_minor="1.3")
    resp = bob.cmd("CREATE bob 20")
    check("FB/1.3 CREATE bob 20 succeeds", resp.endswith("OK"))

    bob_joiners = []
    ok_count = 0
    for i in range(19):
        c = Client(proto_minor="1.3")
        resp = c.cmd(f"JOIN bob p20_{i}")
        ok = resp.endswith("OK")
        if ok:
            ok_count += 1
        bob_joiners.append(c)
    check(f"20-room: 19 joins succeed (bob is player 1, {ok_count}/19 joined -> room full at 20)", ok_count == 19)

    # 21st player (room now has 20) must be rejected
    overflow20 = Client(proto_minor="1.3")
    resp = overflow20.cmd("JOIN bob overflow20")
    check("20-room 21st join rejected (GAME_FULL)", "GAME_FULL" in resp)

    # --- Case 4 (uses bob's 20-room): legacy FB/1.2 client rejected ----------
    legacy_into_big_room = Client(proto_minor="1.2")
    resp = legacy_into_big_room.cmd("JOIN bob legacyjoin")
    check("FB/1.2 client rejected from >5-cap room (legacy-client guard)", "GAME_FULL" in resp)
    legacy_into_big_room.close()

    overflow20.close()
    for c in bob_joiners:
        c.close()
    bob.close()

    # --- Case 3: new CREATE with small max_players=3 -------------------------
    # carl2 (creator) is player 1/3; two joins fill the room to 3/3; a third
    # join attempt is the one that must be rejected.
    carl2 = Client(proto_minor="1.3")
    resp = carl2.cmd("CREATE carl2 3")
    check("FB/1.3 CREATE carl2 3 succeeds", resp.endswith("OK"))

    fill1 = Client(proto_minor="1.3")
    resp = fill1.cmd("JOIN carl2 f1")
    check("3-room(carl2): join #1 succeeds (room now 2/3)", resp.endswith("OK"))

    fill2 = Client(proto_minor="1.3")
    resp = fill2.cmd("JOIN carl2 f2")
    check("3-room(carl2): join #2 succeeds (room now 3/3, full)", resp.endswith("OK"))

    fill3 = Client(proto_minor="1.3")
    resp = fill3.cmd("JOIN carl2 f3")
    check("3-room(carl2): join #3 rejected (GAME_FULL, cap=3 already met)", "GAME_FULL" in resp)

    fill3.close()
    fill2.close()
    fill1.close()
    carl2.close()

    # --- Legacy FB/1.2 client still able to join a <=5-cap room --------------
    small_creator = Client(proto_minor="1.3")
    resp = small_creator.cmd("CREATE dana 5")
    check("FB/1.3 CREATE dana 5 (explicit, still <=5) succeeds", resp.endswith("OK"))

    legacy_small = Client(proto_minor="1.2")
    resp = legacy_small.cmd("JOIN dana legacyok")
    check("FB/1.2 client CAN join a <=5-cap room", resp.endswith("OK"))

    legacy_small.close()
    small_creator.close()


if __name__ == "__main__":
    main()
