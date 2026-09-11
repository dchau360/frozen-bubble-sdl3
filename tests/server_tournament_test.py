#!/usr/bin/env python3
"""Exercise tournament control over real TCP, including legacy priority mode."""
import select
import socket
import subprocess
import sys
import time
import unittest
from pathlib import Path


class Peer:
    def __init__(self, port):
        self.sock = socket.create_connection(('127.0.0.1', port), timeout=3)
        self.buffer = b''
        self.lines = []
        self.wait('SERVER_READY')

    def read(self, timeout=.05):
        if select.select([self.sock], [], [], timeout)[0]:
            data = self.sock.recv(65536)
            if data:
                self.buffer += data
                while b'\n' in self.buffer:
                    line, self.buffer = self.buffer.split(b'\n', 1)
                    self.lines.append(line.decode(errors='replace'))

    def wait(self, token, timeout=4):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            for i, line in enumerate(self.lines):
                if token in line:
                    return self.lines.pop(i)
            self.read()
        raise AssertionError(f'missing {token}: {self.lines[-8:]}')

    def command(self, text, expected='OK'):
        self.sock.sendall(('FB/1.3 ' + text + '\n').encode())
        reply = self.wait(text.split()[0] + ': ')
        assert reply.endswith(': ' + expected), reply
        return reply

    def state(self, tid=1):
        self.lines = [line for line in self.lines if 'TOUR_STATE:' not in line]
        self.command(f'TOUR STATE {tid}')
        # Replies follow snapshots, but older asynchronous states may precede it.
        states = [line for line in self.lines if 'TOUR_STATE:' in line]
        self.lines = [line for line in self.lines if 'TOUR_STATE:' not in line]
        fields = max(states, key=lambda line: int(line.split('TOUR_STATE: ')[1].split()[1])).split('TOUR_STATE: ')[1].split()
        entrants = [] if fields[7] == '-' else [e.split(',') for e in fields[7].split(';')]
        matches = [] if fields[8] == '-' else [m.split(',') for m in fields[8].split(';')]
        return dict(tid=int(fields[0]), revision=int(fields[1]), status=fields[2], owner=int(fields[3]), self=int(fields[4]), stage=int(fields[5]), champion=int(fields[6]), entrants=entrants, matches=matches)


class TournamentTest(unittest.TestCase):
    def setUp(self):
        with socket.socket() as s:
            s.bind(('127.0.0.1', 0))
            self.port = s.getsockname()[1]
        self.server = subprocess.Popen([str(Path(sys.argv[1]).resolve()), '-p', str(self.port), '-q', '-z', '-d'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.peers = []
        for _ in range(100):
            try:
                self.viewer = self.peer('viewer')
                break
            except OSError:
                time.sleep(.03)
        else:
            self.fail('server failed to start')

    def tearDown(self):
        for p in self.peers:
            p.sock.close()
        self.server.terminate()
        # 5s was tight enough to flake under the ASan/UBSan CI job specifically
        # on this file's heaviest test (test_eight_entrants_complete_best_of_
        # three_stage_barriers -- a full 8-entrant bracket is the most
        # struct game/struct tournament churn any test in the suite produces,
        # and ASan's own instrumentation plus LeakSanitizer's scan at exit
        # both add real overhead SIGTERM-to-exit that a debug/no-sanitizer
        # build never pays -- CI log showed a bare subprocess.TimeoutExpired
        # in this wait(), no sanitizer report of any kind, so this was pure
        # shutdown-time margin, not a caught bug. 20s comfortably covers that
        # overhead without meaningfully slowing a normal run, where the
        # server exits almost immediately and wait() returns as soon as it
        # does.
        self.server.wait(timeout=20)

    def peer(self, name):
        p = Peer(self.port)
        self.peers.append(p)
        p.command('NICK ' + name)
        return p

    def register(self, count=8, options=''):
        players = [self.peer(f'p{i+1}') for i in range(count)]
        players[0].command('TOUR CREATE' + (' ' + options if options else ''))
        for p in players[1:]:
            p.command('TOUR JOIN 1')
        self.viewer.command('TOUR WATCH 1')
        return players

    def start(self, players):
        # No registration-phase READY here on purpose: the organizer alone
        # decides when to start (dropped 2026-09-11, user request) -- START
        # used to require every entrant to have sent TOUR READY <tid> 0 0
        # first, and no longer does. See test_organizer_starts_without_ready
        # for the regression coverage.
        players[0].command('TOUR START 1')

    def ready_matches(self, players, matches):
        for m in matches:
            for pid in map(int, m[2:4]):
                players[pid-1].command(f'TOUR READY 1 {m[0]} {m[7]}')
        for m in matches:
            for pid in map(int, m[2:4]):
                p = players[pid-1]
                p.wait(f'TOUR_ASSIGN: 1 {m[0]} {m[7]} ', timeout=8)
                p.wait('GAME_CAN_START:')
                p.command('OK_GAME_START')

    def report(self, players, m, winner=None):
        winner = m[2] if winner is None else str(winner)
        for pid in map(int, m[2:4]):
            players[pid-1].command(f'TOUR REPORT 1 {m[0]} {m[7]} {winner}')
        for pid in map(int, m[2:4]):
            players[pid-1].wait(f'TOUR_RETURN: 1 {m[0]} {m[7]}')

    def test_capability_registration_and_restrictions(self):
        self.viewer.command('TOUR CAPS')
        self.viewer.wait('TOUR_CAPS: 1')
        players = self.register(3)
        players[0].command('TOUR START 1', 'MIN_PLAYERS')
        players[0].command('CREATE other', 'TOURNAMENT_ACTIVE')
        players[0].command('NICK renamed', 'TOURNAMENT_ACTIVE')
        players[0].command('TOUR JOIN 1', 'ALREADY_ENTERED')
        players[0].command('TOUR LEAVE 1')
        self.assertEqual(players[1].state()['owner'], 2)
        self.assertEqual(players[1].state()['self'], 2)
        self.assertEqual(self.viewer.state()['self'], 0)
        players[1].command('TOUR CANCEL 1')
        self.assertEqual(self.viewer.state()['status'], 'cancelled')

    def test_organizer_starts_without_ready(self):
        """The registration-phase READY gate on START was dropped 2026-09-11
        (user request: the organizer decides when to start, not a unanimous
        ready-up) -- START must now succeed with zero READY commands sent by
        anyone, and must still be owner-only."""
        players = self.register(4)
        players[1].command('TOUR START 1', 'DENIED')
        players[0].command('TOUR START 1')
        self.assertEqual(self.viewer.state()['status'], 'running')

    def test_organizer_chosen_ruleset_applies_to_every_match(self):
        """TOUR CREATE's optional ruleset blob (the same comma-separated
        KEY:value shape a room's own SETOPTIONS carries) is stored on the
        tournament and pushed to both seats of every match exactly like a
        live room's own SETOPTIONS would -- the organizer picks it once and
        it's locked in for the whole bracket (2026-09-11, user request:
        tournaments "should have most of the game options")."""
        options = 'CHAINREACTION:1,NUMCOLORS_P1:5,NUMCOLORS_P2:5,DISABLEMALUS:1'
        players = self.register(4, options=options)
        players[0].command('TOUR START 1')
        semis = [m for m in self.viewer.state()['matches'] if int(m[1]) == 0]
        self.ready_matches(players, semis)
        for p in players:
            opts_lines = [l for l in p.lines if 'OPTIONS: ' in l]
            self.assertTrue(opts_lines, f'no OPTIONS push seen for {p.lines}')
            self.assertIn('CHAINREACTION:1', opts_lines[0])
            self.assertIn('NUMCOLORS_P1:5', opts_lines[0])
            self.assertIn('NUMCOLORS_P2:5', opts_lines[0])
            self.assertIn('DISABLEMALUS:1', opts_lines[0])

    def test_bare_create_still_produces_default_ruleset(self):
        """No options given ("TOUR CREATE", no blob) must keep the
        pre-existing behavior exactly: no OPTIONS push at all, and the
        match's room simply keeps the server's zero-initialized defaults."""
        players = self.register(4)
        players[0].command('TOUR START 1')
        semis = [m for m in self.viewer.state()['matches'] if int(m[1]) == 0]
        self.ready_matches(players, semis)
        for p in players:
            self.assertFalse([l for l in p.lines if 'OPTIONS: ' in l])

    def test_eight_entrants_complete_best_of_three_stage_barriers(self):
        players = self.register()
        self.start(players)
        self.assertEqual(len(self.viewer.state()['matches']), 7)
        for stage in range(3):
            for series_round in (1, 2):
                state = self.viewer.state()
                matches = [m for m in state['matches'] if int(m[1]) == stage]
                self.assertTrue(all(m[6] == 'ready' and int(m[7]) == series_round for m in matches))
                self.ready_matches(players, matches)
                # Ordinary F/n are never tournament score/control inputs.
                first = matches[0]
                players[int(first[2])-1].sock.sendall(b'?Fp1\n?n\nFB/1.3 TOUR STATE 1\nFB/1.3 PING\n')
                players[int(first[2])-1].wait('PING: PONG')
                self.assertEqual(self.viewer.state()['matches'][int(first[0])-1][4:6], [str(series_round-1), '0'])
                for m in matches:
                    self.report(players, m)
                    if series_round == 2 and m != matches[-1]:
                        self.assertEqual(self.viewer.state()['stage'], stage)
        final = self.viewer.state()
        self.assertEqual(final['status'], 'complete')
        self.assertGreater(final['champion'], 0)
        self.assertTrue(all(m[4:6] == ['2', '0'] and m[7] == '2' for m in final['matches']))

    def test_four_entrants_minimum_bracket(self):
        """The lowered minimum (was 8) must produce a real 4-slot bracket --
        two semifinals plus a final, no byes -- not just satisfy the START
        guard. Exercises tournament.c's t->slots <= 4 branch end to end,
        including the client-visible two-stage layout (mainmenu_tournament.cpp
        special-cases a 3-match bracket rather than assuming the 7-match/
        3-stage 8-slot shape)."""
        players = self.register(4)
        self.start(players)
        self.assertEqual(len(self.viewer.state()['matches']), 3)
        self.assertEqual(sum(m[6] == 'bye' for m in self.viewer.state()['matches']), 0)
        for stage in range(2):
            for series_round in (1, 2):
                matches = [m for m in self.viewer.state()['matches'] if int(m[1]) == stage]
                self.ready_matches(players, matches)
                for m in matches:
                    self.report(players, m)
        final = self.viewer.state()
        self.assertEqual(final['status'], 'complete')
        self.assertGreater(final['champion'], 0)
        self.assertTrue(all(m[4:6] == ['2', '0'] and m[7] == '2' for m in final['matches']))

    def test_conflict_immutable_reports_resolution_disconnect_and_byes(self):
        players = self.register(9)
        self.start(players)
        state = self.viewer.state()
        self.assertEqual(sum(m[6] == 'bye' for m in state['matches']), 7)
        m = next(m for m in state['matches'] if m[6] == 'ready')
        self.ready_matches(players, [m])
        a, b = (players[int(pid)-1] for pid in m[2:4])
        self.viewer.command(f'TOUR REPORT 1 {m[0]} 1 {m[2]}', 'NOT_ASSIGNED')
        a.command(f'TOUR REPORT 1 {m[0]} 1 {m[2]}')
        a.command(f'TOUR REPORT 1 {m[0]} 1 {m[2]}')
        a.command(f'TOUR REPORT 1 {m[0]} 1 {m[3]}', 'REPORT_IMMUTABLE')
        b.command(f'TOUR REPORT 1 {m[0]} 1 {m[3]}')
        self.assertEqual(self.viewer.state()['matches'][int(m[0])-1][6], 'disputed')
        self.viewer.command(f'TOUR RESOLVE 1 {m[0]} 0')
        replay = self.viewer.state()['matches'][int(m[0])-1]
        self.assertEqual(replay[7], '2')
        a.command(f'TOUR REPORT 1 {m[0]} 1 {m[2]}', 'STALE_ROUND')
        a.sock.close()
        end = time.monotonic()+3
        while time.monotonic() < end:
            state = self.viewer.state()
            if state['stage'] == 1:
                break
            time.sleep(.05)
        self.assertEqual(state['stage'], 1)
        self.assertEqual(state['matches'][int(m[0])-1][6], 'forfeit')

    def test_participant_at_connection_list_head_survives_promotion(self):
        """Regression for add_prio(): promoting the server's *first*
        accepted connection into prio (in-game) mode must not corrupt the
        connection list. Before the fix, add_prio() unconditionally
        mutated `new_conns`, which is only a fresh iteration copy of
        `conns` while the lobby is actively being scanned -- at any other
        time (e.g. idle) it's a stale alias, and removing the list head
        from it leaves `conns` itself pointing at freed storage. setUp()
        always connects a viewer first for server-readiness polling, so
        drop it and let a tournament participant become the new head
        before anyone else connects."""
        self.viewer.sock.close()
        self.peers.remove(self.viewer)
        time.sleep(.3)  # let the server notice and drop the closed connection
        players = [self.peer(f'h{i+1}') for i in range(8)]  # h1 is now the connection-list head
        self.viewer = self.peer('viewer2')
        players[0].command('TOUR CREATE')
        for p in players[1:]:
            p.command('TOUR JOIN 1')
        self.viewer.command('TOUR WATCH 1')
        self.start(players)
        matches = [m for m in self.viewer.state()['matches'] if int(m[1]) == 0]
        self.ready_matches(players, matches)  # add_prio() promotes h1, the list head
        for m in matches:
            self.report(players, m)  # remove_prio() demotes h1 back to the lobby
        # The lobby list must still be intact and serviceable for everyone else.
        outsider = self.peer('outsider')
        outsider.sock.sendall(b'FB/1.3 LIST\n')
        outsider.wait('LIST: ')
        self.assertEqual(self.viewer.state()['stage'], 0)


if __name__ == '__main__':
    unittest.main(argv=[sys.argv[0]] + sys.argv[2:])
