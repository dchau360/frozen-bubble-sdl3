#!/usr/bin/env python3
"""Headless bot clients for stressing large netplay rooms."""

from __future__ import annotations

import argparse
import math
import random
import signal
import socket
import sys
import threading
import time
from dataclasses import dataclass, field
from typing import Callable, Dict, Optional, Tuple


PROTO_MAJOR = 1
PROTO_MINOR = 3
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 1511
DEFAULT_FIRE_INTERVAL = 2.0
START_PUSH_PREFIX = b"PUSH: GAME_CAN_START: "
BUFFER_SIZE = 16384


def build_command(command: str) -> bytes:
    return f"FB/{PROTO_MAJOR}.{PROTO_MINOR} {command}\n".encode("ascii")


def encode_game_message(player_id: int, payload: str) -> bytes:
    return bytes((player_id,)) + payload.encode("ascii") + b"\n"


def parse_game_can_start(payload: bytes, my_nick: str) -> Tuple[Dict[int, str], int]:
    mappings: Dict[int, str] = {}
    my_player_id = 0
    index = 0
    while index < len(payload):
        player_id = payload[index]
        index += 1
        nick_bytes = bytearray()
        while index < len(payload) and payload[index] != ord(","):
            nick_bytes.append(payload[index])
            index += 1
        if index < len(payload) and payload[index] == ord(","):
            index += 1
        nick = nick_bytes.decode("latin1")
        if not nick:
            continue
        mappings[player_id] = nick
        if nick == my_nick:
            my_player_id = player_id
    return mappings, my_player_id


def build_next_colors_str(rng: random.Random, num_colors: int) -> str:
    return " ".join(str(rng.randrange(num_colors)) for _ in range(8))


# Rows 0..4 of a freshly generated board carry colours (RandomLevel fills up to
# `untilend = 13 / 2 - 1`), so the first bubble a player lands in a column comes
# to rest on row 5.
FIRST_FREE_ROW = 5
BOARD_COLUMNS = 7
LAST_ROW = 12


@dataclass
class BotRuntimeState:
    shot_count: int = 0
    round_index: int = 0
    last_fire_at: float = 0.0
    # Next free row per column, so a stuck bubble lands against the bubble
    # above it instead of somewhere arbitrary.
    column_rows: list = field(default_factory=lambda: [FIRST_FREE_ROW] * BOARD_COLUMNS)

    def reset_board(self) -> None:
        self.column_rows = [FIRST_FREE_ROW] * BOARD_COLUMNS


@dataclass
class FailureState:
    lock: threading.Lock = field(default_factory=threading.Lock)
    first_error: Optional[str] = None

    def record(self, nick: str, message: str) -> bool:
        with self.lock:
            if self.first_error is not None:
                return False
            self.first_error = f"{nick}: {message}"
            return True


# A mini board's own geometry, taken from the 5-player layout in
# bubblegame.cpp: every side board places its shooter at the same offset from
# its bubbleOffset, so one relative calculation serves all four slots.
MINI_SHOOTER = (64, 181)
MINI_BUBBLE = 16
MINI_ROW = 14
# The shooter cannot point at or below the horizontal.
MIN_ANGLE = 0.20
MAX_ANGLE = 2.94


def next_free_cell(state: BotRuntimeState) -> Tuple[int, int]:
    """The column and row this bot's next bubble will come to rest in.

    A real bubble stops where it collides, which is always against whatever is
    already above it -- so walk each column down from the first row the level
    leaves empty. Stamping a diagonal across the grid instead, as this used to,
    left a staircase of bubbles visibly floating in mid-air on every mini
    board, because each shot landed alone in a fresh column.
    """
    col = state.shot_count % BOARD_COLUMNS
    # Prefer a column that still has room; keep the chosen one when the whole
    # board is full, which the danger-zone check ends shortly anyway.
    for offset in range(BOARD_COLUMNS):
        candidate = (col + offset) % BOARD_COLUMNS
        if state.column_rows[candidate] <= LAST_ROW:
            col = candidate
            break
    return col, min(state.column_rows[col], LAST_ROW)


def fire_angle_for_cell(col: int, row: int) -> float:
    """The launch angle that actually points at the cell the bubble lands in.

    A receiving client animates a remote launch along this angle alone -- it
    does not simulate the collision, it just teleports the bubble into place
    when the stick message arrives. An angle unrelated to the landing cell
    therefore reads on screen as the bubble flying straight through the
    opponent's bubble mass before appearing somewhere else entirely.
    """
    inset = MINI_BUBBLE // 2 if row % 2 else 0
    target_x = MINI_BUBBLE * col + inset + MINI_BUBBLE // 2
    target_y = MINI_ROW * row + MINI_BUBBLE // 2
    # Screen y grows downward while the launch angle measures upward, hence
    # the flip on the vertical component.
    angle = math.atan2(MINI_SHOOTER[1] - target_y, target_x - MINI_SHOOTER[0])
    return min(max(angle, MIN_ANGLE), MAX_ANGLE)


def build_fire_message(angle: float, color: int) -> str:
    return f"f{angle:.3f}:{color}"


def build_stick_message(state: BotRuntimeState, rng: random.Random,
                        num_colors: int, cell: Optional[Tuple[int, int]] = None,
                        color: Optional[int] = None) -> str:
    col, row = cell if cell is not None else next_free_cell(state)
    if color is None:
        color = rng.randrange(num_colors)
    next_colors = build_next_colors_str(rng, num_colors)
    state.shot_count += 1
    state.column_rows[col] = row + 1
    return f"s{col}:{row}:{color}:{next_colors}"


def plan_shot(state: BotRuntimeState, rng: random.Random,
              num_colors: int) -> Tuple[str, str]:
    """The fire and stick pair for one shot, agreeing on both colour and
    destination -- a real client's do, and every other client draws this
    board from nothing else."""
    col, row = next_free_cell(state)
    color = rng.randrange(num_colors)
    fire = build_fire_message(fire_angle_for_cell(col, row), color)
    stick = build_stick_message(state, rng, num_colors, cell=(col, row),
                                color=color)
    return fire, stick


@dataclass
class BotClient:
    room_nick: str
    nick: str
    host: str
    port: int
    fire_interval: float
    stop_event: threading.Event
    log_lock: threading.Lock
    report_failure: Callable[[str, str], bool]
    num_colors: int = 8
    fire_jitter: float = 0.0
    socket_timeout: float = 0.2
    stick_delay: float = 0.4
    rng: random.Random = field(init=False)
    state: BotRuntimeState = field(default_factory=BotRuntimeState)
    sock: Optional[socket.socket] = field(default=None, init=False)
    lobby_buffer: bytearray = field(default_factory=bytearray, init=False)
    game_buffer: bytearray = field(default_factory=bytearray, init=False)
    my_player_id: int = field(default=0, init=False)
    in_game: bool = field(default=False, init=False)
    joined: bool = field(default=False, init=False)
    game_started: bool = field(default=False, init=False)
    pending_stick_payload: Optional[str] = field(default=None, init=False)
    stick_due_at: float = field(default=0.0, init=False)
    ready_acked: bool = field(default=False, init=False)

    def __post_init__(self) -> None:
        self.rng = random.Random(self.nick)
        # Persistent per-bot pace multiplier, drawn once (not resampled per shot):
        # a fleet firing on the same average cadence reaches its Nth shot within
        # a couple of seconds of each other regardless of per-shot jitter, since
        # independent per-shot noise averages out over ~12-13 shots (the number
        # needed to reach the danger zone). A fixed multiplier instead makes the
        # gap between bots grow with every shot, so deaths actually spread out.
        self.pace_multiplier = 1.0 + self.rng.uniform(-self.fire_jitter, self.fire_jitter)

    def log(self, message: str) -> None:
        timestamp = time.strftime("%H:%M:%S")
        with self.log_lock:
            print(f"[{timestamp}] [{self.nick}] {message}", flush=True)

    def fail(self, message: str) -> None:
        if self.report_failure(self.nick, message):
            self.log(f"error: {message}")
        self.stop_event.set()

    def connect(self) -> None:
        self.sock = socket.create_connection((self.host, self.port), timeout=5.0)
        self.sock.settimeout(self.socket_timeout)
        self.wait_for_server_ready()
        self.send_command(f"NICK {self.nick}")
        self.send_command(f"JOIN {self.room_nick} {self.nick}")
        self.joined = True
        self.log(f"joined room '{self.room_nick}'")

    def wait_for_server_ready(self) -> None:
        deadline = time.monotonic() + 5.0
        while time.monotonic() < deadline and not self.stop_event.is_set():
            for line in self.read_lobby_lines(deadline=time.monotonic() + self.socket_timeout):
                self.log(f"server {line.decode('latin1', errors='replace')}")
                if b"SERVER_READY" in line:
                    return
        raise RuntimeError("did not receive SERVER_READY greeting")

    def send_command(self, command: str) -> None:
        assert self.sock is not None
        self.sock.sendall(build_command(command))
        self.log(f"sent command: {command}")

    def send_game_payload(self, payload: str) -> None:
        assert self.sock is not None
        if self.my_player_id == 0:
            raise RuntimeError("cannot send in-game payload before GAME_CAN_START mapping")
        self.sock.sendall(encode_game_message(self.my_player_id, payload))
        self.log(f"sent game payload: {payload}")

    def read_from_socket(self) -> bytes:
        assert self.sock is not None
        try:
            chunk = self.sock.recv(BUFFER_SIZE)
        except socket.timeout:
            return b""
        if not chunk:
            raise ConnectionError("server closed the connection")
        return chunk

    def read_lobby_lines(self, deadline: Optional[float] = None) -> list[bytes]:
        lines: list[bytes] = []
        while True:
            newline_index = self.lobby_buffer.find(b"\n")
            if newline_index != -1:
                line = bytes(self.lobby_buffer[:newline_index])
                del self.lobby_buffer[: newline_index + 1]
                lines.append(line.rstrip(b"\r"))
                continue
            if deadline is not None and time.monotonic() >= deadline:
                break
            chunk = self.read_from_socket()
            if not chunk:
                break
            self.lobby_buffer.extend(chunk)
        return lines

    def drain_lobby_messages(self) -> None:
        lines = self.read_lobby_lines(deadline=time.monotonic() + self.socket_timeout)
        for line in lines:
            self.handle_lobby_line(line)
            if self.in_game:
                self.game_buffer.extend(self.lobby_buffer)
                self.lobby_buffer.clear()
                break

    def handle_lobby_line(self, line: bytes) -> None:
        text = line.decode("latin1", errors="replace")
        if text:
            self.log(f"recv lobby: {text}")

        if b"NO_SUCH_GAME" in line:
            raise RuntimeError(f"join failed, no such game '{self.room_nick}'")
        if b"GAME_FULL" in line:
            raise RuntimeError(f"join failed, room '{self.room_nick}' is full")
        if b"NICK_IN_USE" in line:
            raise RuntimeError(f"nickname '{self.nick}' already in use")
        if START_PUSH_PREFIX in line:
            payload = line.split(START_PUSH_PREFIX, 1)[1]
            mappings, my_player_id = parse_game_can_start(payload, self.nick)
            if my_player_id == 0:
                raise RuntimeError("GAME_CAN_START mapping did not include this bot")
            self.my_player_id = my_player_id
            self.log(f"game can start, player id={self.my_player_id}, players={mappings}")
            self.send_command("OK_GAME_START")
        elif b"OK_GAME_START: OK" in line:
            self.in_game = True
            self.game_started = True
            self.state = BotRuntimeState()
            self.pending_stick_payload = None
            self.log("entered in-game binary mode")
        elif b"PARTED:" in line and self.nick.encode("ascii") in line:
            raise RuntimeError("server reported this bot as parted")
        elif b"KICKED" in line:
            raise RuntimeError("server kicked this bot")

    def drain_game_messages(self) -> None:
        chunk = self.read_from_socket()
        if chunk:
            self.game_buffer.extend(chunk)

        while True:
            newline_index = self.game_buffer.find(b"\n")
            if newline_index == -1:
                break
            packet = bytes(self.game_buffer[:newline_index])
            del self.game_buffer[: newline_index + 1]
            if not packet:
                continue
            sender_id = packet[0]
            payload = packet[1:].decode("latin1", errors="replace")
            self.handle_game_message(sender_id, payload)

    def handle_game_message(self, sender_id: int, payload: str) -> None:
        if not payload:
            return
        msg_type = payload[0]
        if msg_type == "n":
            # The next round only starts once every player has sent its own 'n'
            # (ready) message, so ack the first peer 'n' of each transition
            # exactly once; the client guards against double-counting repeats.
            if not self.ready_acked:
                self.state.reset_board()
                self.state.shot_count = 0
                self.state.round_index += 1
                self.pending_stick_payload = None
                self.ready_acked = True
                self.send_game_payload("n")
                # Grace period so we don't fire into the stats screen before
                # the round actually restarts on the human client.
                self.state.last_fire_at = time.monotonic() + 1.0
                self.log(
                    f"round reset from player {sender_id}; acked ready; "
                    f"round={self.state.round_index + 1}"
                )
            return
        if msg_type in {"f", "s", "p"}:
            return
        if msg_type == "l":
            self.log(f"player-left message from {sender_id}: {payload}")

    def maybe_send_shot_pair(self) -> None:
        if not self.in_game or not self.game_started:
            return

        now = time.monotonic()
        if self.pending_stick_payload is not None and now >= self.stick_due_at:
            self.send_game_payload(self.pending_stick_payload)
            self.pending_stick_payload = None
            return

        if self.pending_stick_payload is not None:
            return

        effective_interval = self.fire_interval * self.pace_multiplier
        if self.state.last_fire_at and now - self.state.last_fire_at < effective_interval:
            return

        fire_payload, stick_payload = plan_shot(
            self.state, self.rng, self.num_colors)
        self.ready_acked = False  # firing means the new round is underway
        self.send_game_payload(fire_payload)
        self.pending_stick_payload = stick_payload
        self.stick_due_at = now + self.stick_delay
        self.state.last_fire_at = now

    def run(self) -> None:
        try:
            self.connect()
            while not self.stop_event.is_set():
                if self.in_game:
                    self.drain_game_messages()
                else:
                    self.drain_lobby_messages()
                self.maybe_send_shot_pair()
        except ConnectionError as exc:
            if not self.stop_event.is_set():
                self.fail(f"disconnect: {exc}")
        except Exception as exc:
            if not self.stop_event.is_set():
                self.fail(str(exc))
        finally:
            self.close()

    def close(self) -> None:
        if self.sock is not None:
            try:
                self.sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            try:
                self.sock.close()
            except OSError:
                pass
            self.sock = None
        self.log("stopped")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=DEFAULT_HOST, help=f"server host (default: {DEFAULT_HOST})")
    parser.add_argument("--port", default=DEFAULT_PORT, type=int, help=f"server port (default: {DEFAULT_PORT})")
    parser.add_argument("--join", required=True, help="room creator nickname to join")
    parser.add_argument("--count", required=True, type=int, help="number of bot clients to spawn")
    parser.add_argument(
        "--fire-interval",
        default=DEFAULT_FIRE_INTERVAL,
        type=float,
        help=f"seconds between fire messages per bot (default: {DEFAULT_FIRE_INTERVAL})",
    )
    parser.add_argument(
        "--fire-jitter",
        default=0.0,
        type=float,
        help="random fraction of --fire-interval applied as each bot's fixed pace "
        "(e.g. 0.4 = each bot fires steadily somewhere between 60%% and 140%% of "
        "the interval for its whole session); a per-shot jitter would average out "
        "over the ~12-13 shots it takes to reach the danger zone, so this is "
        "drawn once per bot to actually desynchronize deaths (default: 0.0)",
    )
    return parser.parse_args(argv)


def run_bots(args: argparse.Namespace) -> int:
    stop_event = threading.Event()
    log_lock = threading.Lock()
    failure_state = FailureState()
    threads: list[threading.Thread] = []

    def request_stop(signum: int, _frame: object) -> None:
        with log_lock:
            print(f"\nreceived signal {signum}, stopping bots...", flush=True)
        stop_event.set()

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)

    bots = [
        BotClient(
            room_nick=args.join,
            nick=f"bot{index:02d}",
            host=args.host,
            port=args.port,
            fire_interval=args.fire_interval,
            fire_jitter=getattr(args, "fire_jitter", 0.0),
            stop_event=stop_event,
            log_lock=log_lock,
            report_failure=failure_state.record,
        )
        for index in range(1, args.count + 1)
    ]

    def run_bot(bot: BotClient) -> None:
        try:
            bot.run()
        except Exception as exc:
            if failure_state.record(bot.nick, f"thread crashed: {exc}"):
                with log_lock:
                    print(
                        f"[{time.strftime('%H:%M:%S')}] [{bot.nick}] error: thread crashed: {exc}",
                        flush=True,
                    )
            stop_event.set()
            bot.close()

    for bot in bots:
        if stop_event.is_set():
            break
        thread = threading.Thread(target=run_bot, args=(bot,), name=bot.nick, daemon=True)
        threads.append(thread)
        thread.start()
        time.sleep(0.05)

    try:
        while any(thread.is_alive() for thread in threads):
            for thread in threads:
                thread.join(timeout=0.2)
            if stop_event.is_set():
                break
    except KeyboardInterrupt:
        stop_event.set()
    finally:
        stop_event.set()
        for thread in threads:
            thread.join(timeout=2.0)

    if failure_state.first_error is not None:
        with log_lock:
            print(f"bot failure: {failure_state.first_error}", flush=True)
        return 1
    return 0


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.count < 1:
        raise SystemExit("--count must be at least 1")
    if args.fire_interval <= 0:
        raise SystemExit("--fire-interval must be positive")
    if not 0.0 <= getattr(args, "fire_jitter", 0.0) < 1.0:
        raise SystemExit("--fire-jitter must be in [0, 1)")
    return run_bots(args)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
