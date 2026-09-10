#!/usr/bin/env python3
"""What the Discord join-alert relay is allowed to put in a channel.

The companion to server_discordalert_test.py, which covers the other side of
the same pipe: that one drives the real fb-server and checks the datagram it
emits, this one takes the datagram apart and checks the message built from
it. Neither substitutes for the other -- a correct datagram formatted into a
hostile message is still a hostile message.

Two fields reach the message, and both arrive from outside the relay's trust
boundary. That boundary moved outward twice: first when one webhook could be
shared with other server operators (see server/discord-relay/README.md), and
again when the game itself started advertising the channel to players, which
is what took the location out of the message entirely.

  nick        a player types it. fb-server's is_nick_ok() allows only
              [A-Za-z0-9_-]{1,10}, but the relay does not get to assume the
              sender is an unmodified fb-server.
  servername  whoever runs that server sets it. net_servername() imposes no
              charset rule whatsoever, so in a shared-channel setup this is
              somebody else's free-form config landing in your channel.

The case that actually matters below is link injection: unescaped "[" and
"(" let a display string render as [innocent text](somewhere-else), which is
the difference between a defaced alert and a working phishing lure.
"""

import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent /
                       "server" / "discord-relay"))

import relay  # noqa: E402


class BuildMessageTest(unittest.TestCase):
    def test_ordinary_join_reads_naturally(self):
        msg = relay.build_message("alice", "fb.example.org")
        self.assertEqual(msg, "🔔 **alice** joined **fb.example.org**")

    def test_hostile_servername_cannot_inject_a_link(self):
        # The one that matters: a server named so its alert renders as a
        # plain-looking link to somewhere else entirely.
        msg = relay.build_message(
            "alice", "[totally fine](https://evil.example/steal)")
        self.assertNotIn("](https://evil.example/steal)", msg)
        self.assertIn("\\[", msg)
        self.assertIn("\\(", msg)

    def test_hostile_nick_cannot_inject_a_link(self):
        # Same check on the field fb-server does constrain, because the relay
        # is not entitled to assume the sender is an unmodified fb-server.
        msg = relay.build_message(
            "[click here](https://evil.example)", "fb.example.org")
        self.assertNotIn("](https://evil.example)", msg)

    def test_mention_text_is_escaped_and_never_requested(self):
        # Two independent guards, asserted independently: the text is escaped
        # so it does not render as a mention, and the payload separately
        # refuses to deliver any mention at all (see PayloadTest).
        msg = relay.build_message("alice", "@everyone free bubbles")
        self.assertIn("@everyone", msg)          # still readable as text
        self.assertNotIn("|", msg)               # no markup smuggled through

    def test_newline_cannot_forge_a_second_alert(self):
        msg = relay.build_message(
            "alice", "real\n🔔 **admin** joined **your-bank**")
        self.assertNotIn("\n", msg)

    def test_long_servername_cannot_crowd_out_the_line(self):
        msg = relay.build_message("alice", "A" * 500)
        self.assertLessEqual(len(msg), relay.MAX_DISCORD_CONTENT)
        self.assertNotIn("A" * (relay.MAX_DISPLAY + 1), msg)

    def test_truncation_never_leaves_a_dangling_escape(self):
        # Truncating after escaping could cut between a backslash and the
        # character it escapes, which would then escape whatever followed.
        msg = relay.build_message("alice", "*" * (relay.MAX_DISPLAY + 20))
        self.assertFalse(msg.endswith("\\"))


class BuildResultMessageTest(unittest.TestCase):
    """Round-end alerts. Same trust boundary as BuildMessageTest, plus one
    more: winner is not validated against is_nick_ok() on the server side at
    all (unlike nick, which at least started out constrained), since it's
    lifted straight from a client's 'F' payload -- see game.c's comment on
    process_msg_prio_'s sniff. roster, by contrast, is fb-server's own
    bookkeeping and could not have been forged the same way."""

    def test_win_reads_naturally(self):
        msg = relay.build_result_message(0, "alice", "alice,bob", "fb.example.org")
        self.assertEqual(
            msg, "🏆 **alice** won (Classic) on **fb.example.org** — alice, bob")

    def test_draw_reads_naturally(self):
        msg = relay.build_result_message(3, "", "alice,bob", "fb.example.org")
        self.assertEqual(
            msg, "🤝 Draw (Timed) on **fb.example.org** — alice, bob")

    def test_every_mode_number_maps_to_its_name(self):
        names = {0: "Classic", 1: "Clear", 2: "Race", 3: "Timed"}
        for mode, name in names.items():
            msg = relay.build_result_message(mode, "alice", "alice", "s")
            self.assertIn(f"({name})", msg)

    def test_unrecognized_mode_number_labels_nothing(self):
        # A value this relay predates or simply garbage (int parse of a
        # malformed field falls back to -1) must not crash or guess.
        msg = relay.build_result_message(-1, "alice", "alice", "s")
        self.assertNotIn("(", msg)
        msg = relay.build_result_message(99, "alice", "alice", "s")
        self.assertNotIn("(", msg)

    def test_hostile_winner_cannot_inject_a_link(self):
        # The field with no is_nick_ok() backing it at all on the server
        # side -- exactly the one a modified client would target.
        msg = relay.build_result_message(
            0, "[click here](https://evil.example)", "alice", "s")
        self.assertNotIn("](https://evil.example)", msg)

    def test_hostile_roster_name_cannot_inject_a_link(self):
        msg = relay.build_result_message(
            0, "alice", "alice,[click here](https://evil.example)", "s")
        self.assertNotIn("](https://evil.example)", msg)

    def test_hostile_servername_cannot_inject_a_link(self):
        msg = relay.build_result_message(
            0, "alice", "alice,bob",
            "[totally fine](https://evil.example/steal)")
        self.assertNotIn("](https://evil.example/steal)", msg)

    def test_newline_cannot_forge_a_second_alert(self):
        msg = relay.build_result_message(
            0, "real\n🏆 **admin** won on **your-bank**", "alice", "s")
        self.assertNotIn("\n", msg)

    def test_long_roster_cannot_blow_past_discords_own_limit(self):
        huge_roster = ",".join(f"p{i}" for i in range(500))
        msg = relay.build_result_message(0, "alice", huge_roster, "s")
        self.assertLessEqual(len(msg), relay.MAX_DISCORD_CONTENT)


class ResultDatagramDispatchTest(unittest.TestCase):
    """handle_datagram() routes by the leading type name -- these pin that
    routing end to end, the way NoLocationLeakTest's own datagram test does
    for JOIN, rather than only exercising build_result_message() directly."""

    def _captured_line(self, datagram):
        import asyncio
        captured = []
        original = relay.log.info
        relay.log.info = lambda fmt, *a: captured.append(fmt % a)
        try:
            asyncio.run(relay.handle_datagram(datagram, ""))
        finally:
            relay.log.info = original
        return captured

    def test_result_datagram_posts_the_result_message(self):
        captured = self._captured_line(
            b"RESULT|42|0|alice|alice,bob|fb.example.org")
        self.assertEqual(len(captured), 1)
        self.assertIn("**alice**", captured[0])
        self.assertIn("(Classic)", captured[0])
        self.assertIn("fb.example.org", captured[0])

    def test_draw_datagram_posts_without_a_winner_field(self):
        captured = self._captured_line(
            b"RESULT|42|1|" + b"|alice,bob|fb.example.org")
        self.assertEqual(len(captured), 1)
        self.assertIn("Draw", captured[0])

    def test_non_integer_game_id_still_posts_flat(self):
        # game_id only matters for thread grouping (see
        # ResultThreadingTest) -- a malformed one must not sink the alert
        # itself, since that would silence a real round-end over a
        # threading key alone.
        captured = self._captured_line(
            b"RESULT|not-a-number|0|alice|alice,bob|fb.example.org")
        self.assertEqual(len(captured), 1)
        self.assertIn("**alice**", captured[0])

    def test_old_five_field_format_is_now_malformed(self):
        # Pins the wire-format change itself: a RESULT datagram from a
        # fb-server built before game_id existed must not be silently
        # misparsed (e.g. reading roster as game_id) -- it should be
        # dropped and logged, same as any other malformed datagram.
        import asyncio
        warned = []
        original = relay.log.warning
        relay.log.warning = lambda fmt, *a: warned.append(fmt % a)
        try:
            asyncio.run(relay.handle_datagram(
                b"RESULT|0|alice|alice,bob|fb.example.org", ""))
        finally:
            relay.log.warning = original
        self.assertEqual(len(warned), 1)

    def test_unknown_type_name_is_dropped_not_guessed_at(self):
        import asyncio
        warned = []
        original = relay.log.warning
        relay.log.warning = lambda fmt, *a: warned.append(fmt % a)
        try:
            asyncio.run(relay.handle_datagram(b"DEPARTED|alice|s", ""))
        finally:
            relay.log.warning = original
        self.assertEqual(len(warned), 1)


class NoLocationLeakTest(unittest.TestCase):
    """The message is nick and servername, and nothing else.

    The game now advertises this channel to players from the NET GAME list
    and the lobby ("Join our Discord"), so its audience is no longer the
    operator and a couple of moderators -- it is anyone who took up the
    offer. A joining player's IP was never posted; their approximate location
    used to be, and stopped when that changed. These pin both, because the
    datagram still carries both and re-interpolating one is a two-word edit.
    """

    def test_build_message_takes_no_location_argument(self):
        # The signature itself is the guard: geoloc cannot be posted by
        # accident if build_message is never handed it.
        with self.assertRaises(TypeError):
            relay.build_message("alice", "fb.example.org", "37.77:-122.42")

    def test_no_ip_reaches_the_message(self):
        msg = relay.build_message("alice", "fb.example.org")
        self.assertNotIn("203.0.113", msg)

    def test_no_url_of_any_kind_reaches_the_message(self):
        # Broader than "no maps link": with the location gone there is no
        # legitimate URL left in an alert at all, so any "://" is a leak.
        msg = relay.build_message("alice", "fb.example.org")
        self.assertNotIn("://", msg)
        self.assertNotIn("maps", msg)

    def test_datagram_with_a_location_still_posts_without_it(self):
        # End to end from the wire format, since build_message's signature
        # only proves the caller could not pass it -- this proves the caller
        # does not.
        import asyncio
        captured = []

        original = relay.log.info
        relay.log.info = lambda fmt, *a: captured.append(fmt % a)
        try:
            asyncio.run(relay.handle_datagram(
                b"JOIN|alice|203.0.113.7|37.77:-122.42|fb.example.org", ""))
        finally:
            relay.log.info = original

        self.assertEqual(len(captured), 1)
        line = captured[0]
        self.assertIn("**alice**", line)
        self.assertIn("**fb.example.org**", line)
        self.assertNotIn("37.77", line)
        self.assertNotIn("203.0.113.7", line)
        self.assertNotIn("://", line)


class ServerNameOverrideTest(unittest.TestCase):
    """DISCORD_SERVER_NAME lets an operator show a full name in Discord that
    fb-server's own -n could never carry (12 chars, [a-zA-Z0-9.-] only)."""

    def _posted_line(self, datagram):
        import asyncio
        captured = []
        original = relay.log.info
        relay.log.info = lambda fmt, *a: captured.append(fmt % a)
        try:
            asyncio.run(relay.handle_datagram(datagram, ""))
        finally:
            relay.log.info = original
        self.assertEqual(len(captured), 1)
        return captured[0]

    def test_unset_uses_whatever_fb_server_sent(self):
        self.assertEqual(relay.DISCORD_SERVER_NAME, "")
        line = self._posted_line(b"JOIN|alice|203.0.113.7||servequake")
        self.assertIn("**servequake**", line)

    def test_set_overrides_the_datagram_servername(self):
        original = relay.DISCORD_SERVER_NAME
        relay.DISCORD_SERVER_NAME = "fb.servequake.com"
        try:
            line = self._posted_line(b"JOIN|alice|203.0.113.7||servequake")
        finally:
            relay.DISCORD_SERVER_NAME = original
        self.assertIn("**fb.servequake.com**", line)
        self.assertNotIn("**servequake**", line)

    def test_applies_to_result_alerts_too(self):
        # Not just JOIN -- the override is a display setting for everything
        # this relay posts, not something to redo per message type.
        original = relay.DISCORD_SERVER_NAME
        relay.DISCORD_SERVER_NAME = "fb.servequake.com"
        try:
            line = self._posted_line(b"RESULT|42|0|alice|alice,bob|servequake")
        finally:
            relay.DISCORD_SERVER_NAME = original
        self.assertIn("**fb.servequake.com**", line)


class ResultThreadingTest(unittest.TestCase):
    """Round-result alerts thread per room when DISCORD_BOT_TOKEN and
    DISCORD_CHANNEL_ID are both set (see "Round-result threading" in the
    module docstring) -- otherwise they post flat via the webhook, exactly
    as every alert did before this feature existed. Mocks
    relay._bot_request_sync itself rather than urlopen: PayloadTest already
    covers the shared HTTPS-request plumbing (headers, JSON body), so this
    only needs to pin which channel/thread each call targets and in what
    order, not how the request gets built."""

    def setUp(self):
        self._orig_token = relay.DISCORD_BOT_TOKEN
        self._orig_channel = relay.DISCORD_CHANNEL_ID
        self._orig_threads = dict(relay._room_threads)
        relay._room_threads.clear()

    def tearDown(self):
        relay.DISCORD_BOT_TOKEN = self._orig_token
        relay.DISCORD_CHANNEL_ID = self._orig_channel
        relay._room_threads.clear()
        relay._room_threads.update(self._orig_threads)

    def _enable_bot_mode(self):
        relay.DISCORD_BOT_TOKEN = "test-token"
        relay.DISCORD_CHANNEL_ID = "999"

    def _fake_bot_request(self, calls, responses):
        def fake(method, path, body):
            calls.append((method, path, body))
            return responses.pop(0)
        return fake

    def _captured_info(self, datagram):
        import asyncio
        captured = []
        original = relay.log.info
        relay.log.info = lambda fmt, *a: captured.append(fmt % a)
        try:
            asyncio.run(relay.handle_datagram(datagram, ""))
        finally:
            relay.log.info = original
        return captured

    def test_without_bot_config_posts_flat_via_webhook(self):
        # Bot vars both unset (the default): unchanged from before this
        # feature existed. Stub mode here, but the same branch applies to a
        # real DISCORD_WEBHOOK_URL.
        captured = self._captured_info(b"RESULT|1|0|alice|alice,bob|s")
        self.assertEqual(len(captured), 1)
        self.assertIn("[stub] would post", captured[0])
        self.assertNotIn("threaded", captured[0])

    def test_only_one_bot_variable_set_falls_back_to_flat(self):
        relay.DISCORD_BOT_TOKEN = "test-token"
        relay.DISCORD_CHANNEL_ID = ""
        captured = self._captured_info(b"RESULT|1|0|alice|alice,bob|s")
        self.assertIn("[stub] would post", captured[0])

    def test_first_result_for_a_room_creates_a_thread(self):
        self._enable_bot_mode()
        calls = []
        original = relay._bot_request_sync
        relay._bot_request_sync = self._fake_bot_request(
            calls, [{"id": "111"}, {"id": "222"}])
        try:
            captured = self._captured_info(b"RESULT|7|0|alice|alice,bob|s")
        finally:
            relay._bot_request_sync = original

        self.assertIn("threaded", captured[0])
        self.assertEqual(len(calls), 2)
        method, path, body = calls[0]
        self.assertEqual(method, "POST")
        self.assertEqual(path, "/channels/999/messages")
        self.assertIn("alice", body["content"])
        method, path, body = calls[1]
        self.assertEqual(method, "POST")
        self.assertEqual(path, "/channels/999/messages/111/threads")
        self.assertEqual(body["name"], "alice")  # roster's first entry
        self.assertEqual(relay._room_threads[7], "222")

    def test_second_result_for_the_same_room_posts_into_the_cached_thread(self):
        self._enable_bot_mode()
        relay._room_threads[7] = "222"
        calls = []
        original = relay._bot_request_sync
        relay._bot_request_sync = self._fake_bot_request(calls, [{}])
        try:
            self._captured_info(b"RESULT|7|0|bob|alice,bob|s")
        finally:
            relay._bot_request_sync = original

        self.assertEqual(len(calls), 1, "second round must not open a second thread")
        method, path, _body = calls[0]
        self.assertEqual(path, "/channels/222/messages")

    def test_different_rooms_get_different_threads(self):
        self._enable_bot_mode()
        relay._room_threads[7] = "222"
        original = relay._bot_request_sync
        relay._bot_request_sync = self._fake_bot_request(
            [], [{"id": "333"}, {"id": "444"}])
        try:
            self._captured_info(b"RESULT|9|0|carol|carol,dave|s")
        finally:
            relay._bot_request_sync = original

        self.assertEqual(relay._room_threads[7], "222", "unrelated room's thread must be untouched")
        self.assertEqual(relay._room_threads[9], "444")

    def test_a_dead_cached_thread_is_dropped_and_retried_next_time(self):
        self._enable_bot_mode()
        relay._room_threads[7] = "222"

        def always_fails(method, path, body):
            raise RuntimeError("channel gone")

        original = relay._bot_request_sync
        relay._bot_request_sync = always_fails
        try:
            self._captured_info(b"RESULT|7|0|alice|alice,bob|s")
        finally:
            relay._bot_request_sync = original

        self.assertNotIn(7, relay._room_threads,
                         "a dead thread must not wedge this room forever")


class PayloadTest(unittest.TestCase):
    """The JSON actually handed to Discord, as opposed to the text in it."""

    def _payload(self, content):
        captured = {}

        class FakeResponse:
            def read(self_inner):
                return b""

            def __enter__(self_inner):
                return self_inner

            def __exit__(self_inner, *exc):
                return False

        def fake_urlopen(req, timeout=None):
            captured["body"] = json.loads(req.data.decode("utf-8"))
            captured["user_agent"] = req.get_header("User-agent")
            return FakeResponse()

        original = relay.urllib.request.urlopen
        relay.urllib.request.urlopen = fake_urlopen
        try:
            relay._post_sync("https://discord.example/webhook", content)
        finally:
            relay.urllib.request.urlopen = original
        return captured

    def test_post_suppresses_every_mention(self):
        body = self._payload("🔔 **alice** joined **@everyone**")["body"]
        self.assertEqual(body["allowed_mentions"], {"parse": []})

    def test_post_still_carries_the_content(self):
        body = self._payload("hello")["body"]
        self.assertEqual(body["content"], "hello")

    def test_post_overrides_the_default_user_agent(self):
        # Discord's edge WAF 403s urllib's default User-Agent
        # ("Python-urllib/3.x") outright -- confirmed against the live
        # endpoint, GET included, so it is not a POST-specific quirk. A
        # request left at urllib's default fails on the very first join a
        # stock deploy tries to post, with nothing in the response body to
        # explain why.
        ua = self._payload("hello")["user_agent"]
        self.assertIsNotNone(ua)
        self.assertNotIn("urllib", ua.lower())


if __name__ == "__main__":
    unittest.main(verbosity=2)
