#!/usr/bin/env python3
"""Regression tests for the headless netplay bot harness."""

import random
import sys
import unittest
from unittest import mock
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))

import net_bots  # noqa: E402


class ProtocolHelpersTest(unittest.TestCase):
    def test_build_command_uses_fb_1_3_line_framing(self):
        self.assertEqual(
            net_bots.build_command("JOIN room42 bot07"),
            b"FB/1.3 JOIN room42 bot07\n",
        )

    def test_encode_game_message_uses_player_id_prefix_and_newline(self):
        self.assertEqual(
            net_bots.encode_game_message(7, "f1.234:5"),
            b"\x07f1.234:5\n",
        )

    def test_parse_game_can_start_extracts_binary_mappings_and_self_id(self):
        payload = b"\x01host,\x0ebot14,\x14bot20,"
        self.assertEqual(
            net_bots.parse_game_can_start(payload, "bot14"),
            ({1: "host", 14: "bot14", 20: "bot20"}, 14),
        )

    def test_build_next_colors_str_matches_eight_space_separated_values(self):
        rng = random.Random(1234)
        next_colors = net_bots.build_next_colors_str(rng, 8)
        self.assertEqual(next_colors, "7 1 0 1 0 1 1 5")

    def test_build_stick_message_uses_monotonic_rows_and_rolls_columns(self):
        state = net_bots.BotRuntimeState(row=11, shot_count=7)
        rng = random.Random(99)
        stick = net_bots.build_stick_message(state, rng, num_colors=8)
        self.assertEqual(stick, "s7:11:6:6 3 2 3 3 2 1 4")
        self.assertEqual(state.row, 12)
        self.assertEqual(state.shot_count, 8)


class RunBotsFailurePropagationTest(unittest.TestCase):
    def test_run_bots_returns_nonzero_when_a_bot_fails(self):
        args = net_bots.argparse.Namespace(
            host="127.0.0.1",
            port=1511,
            join="hostnick",
            count=3,
            fire_interval=1.0,
        )
        start_event = net_bots.threading.Event()
        ready_event = net_bots.threading.Event()
        finish_event = net_bots.threading.Event()
        outcome_by_nick = {
            "bot01": RuntimeError("join failed, no such game 'hostnick'"),
            "bot02": "wait",
            "bot03": "wait",
        }
        started: list[str] = []
        stopped: list[str] = []

        # run_bots() starts bot threads strictly sequentially (a `for` loop
        # that only stops launching further bots once stop_event is set),
        # so nothing here races *that* part. The race is inside this fake:
        # with time.sleep mocked to a no-op, bot01's thread can run to
        # completion (record its failure and flip stop_event) before
        # bot02/bot03 ever reach their own ready state, or even before
        # their threads are scheduled at all. That produced two different
        # flaky failures in CI: `stopped` in the wrong order, and
        # `ready_event` never getting set at all.
        #
        # To make "peers are up and running, then one bot fails, then
        # run_bots stops the peers" the *only* possible interleaving, block
        # bot01's failure behind a rendezvous on every healthy bot reaching
        # ready first, instead of hoping thread scheduling cooperates.
        healthy_total = sum(
            1 for outcome in outcome_by_nick.values() if not isinstance(outcome, Exception)
        )
        ready_lock = net_bots.threading.Lock()
        ready_count = {"n": 0}

        class FakeBotClient:
            def __init__(
                self,
                room_nick: str,
                nick: str,
                host: str,
                port: int,
                fire_interval: float,
                stop_event: net_bots.threading.Event,
                log_lock: net_bots.threading.Lock,
                report_failure,
            ) -> None:
                self.nick = nick
                self.stop_event = stop_event
                self.outcome = outcome_by_nick[nick]

            def run(self) -> None:
                started.append(self.nick)
                start_event.set()
                if isinstance(self.outcome, Exception):
                    # Don't fail until every healthy bot has confirmed ready,
                    # so this always exercises "fail after peers are up"
                    # rather than racing bot02/bot03's own thread startup.
                    ready_in_time = ready_event.wait(timeout=5.0)
                    assert ready_in_time, "healthy bots never became ready"
                    raise self.outcome
                with ready_lock:
                    ready_count["n"] += 1
                    if ready_count["n"] == healthy_total:
                        ready_event.set()
                while not self.stop_event.is_set():
                    finish_event.wait(0.01)
                stopped.append(self.nick)

            def close(self) -> None:
                return None

        with mock.patch.object(net_bots, "BotClient", FakeBotClient):
            with mock.patch.object(net_bots.time, "sleep", lambda _seconds: None):
                exit_code = net_bots.run_bots(args)

        self.assertTrue(start_event.is_set())
        self.assertTrue(ready_event.is_set())
        self.assertEqual(exit_code, 1)
        self.assertCountEqual(stopped, [nick for nick in started if nick != "bot01"])


if __name__ == "__main__":
    unittest.main()
