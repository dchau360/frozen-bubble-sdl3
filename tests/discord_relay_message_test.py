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
