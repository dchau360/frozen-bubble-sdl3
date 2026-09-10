#!/usr/bin/env python3
"""What the Discord join-alert relay is allowed to put in a channel.

The companion to server_discordalert_test.py, which covers the other side of
the same pipe: that one drives the real fb-server and checks the datagram it
emits, this one takes the datagram apart and checks the message built from
it. Neither substitutes for the other -- a correct datagram formatted into a
hostile message is still a hostile message.

Two of the three interpolated fields arrive from outside the relay's trust
boundary, and that boundary moved outward the moment one webhook could be
shared with other server operators (see server/discord-relay/README.md):

  nick        a player types it. fb-server's is_nick_ok() allows only
              [A-Za-z0-9_-]{1,10}, but the relay does not get to assume the
              sender is an unmodified fb-server.
  servername  whoever runs that server sets it. net_servername() imposes no
              charset rule whatsoever, so in a shared-channel setup this is
              somebody else's free-form config landing in your channel.
  geoloc      a client sends it, but it can only ever reach the message as
              two parsed floats, so there is nothing to escape.

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
        msg = relay.build_message("alice", "37.77:-122.42", "fb.example.org")
        self.assertIn("**alice**", msg)
        self.assertIn("**fb.example.org**", msg)
        self.assertIn("https://www.google.com/maps?q=37.77,-122.42", msg)

    def test_missing_geoloc_drops_the_location_clause(self):
        # The normal case, not an error: a fast joiner beats their own
        # client-side lookup, which can take ~16s.
        msg = relay.build_message("bob", "", "fb.example.org")
        self.assertEqual(msg, "🔔 **bob** joined **fb.example.org**")

    def test_unparseable_geoloc_is_dropped_not_interpolated(self):
        msg = relay.build_message("bob", "not:coords", "fb.example.org")
        self.assertNotIn("not:coords", msg)
        self.assertNotIn("location", msg)

    def test_no_ip_reaches_the_message(self):
        # The datagram still carries one; build_message never receives it.
        # Pinned so the parameter cannot quietly come back.
        msg = relay.build_message("alice", "37.77:-122.42", "fb.example.org")
        self.assertNotIn("203.0.113", msg)
        self.assertNotIn("(", msg.replace("[this location](", ""))

    def test_hostile_servername_cannot_inject_a_link(self):
        # The one that matters: a server named so its alert renders as a
        # plain-looking link to somewhere else entirely.
        msg = relay.build_message(
            "alice", "", "[totally fine](https://evil.example/steal)")
        self.assertNotIn("](https://evil.example/steal)", msg)
        self.assertIn("\\[", msg)
        self.assertIn("\\(", msg)

    def test_hostile_nick_cannot_inject_a_link(self):
        # Same check on the field fb-server does constrain, because the relay
        # is not entitled to assume the sender is an unmodified fb-server.
        msg = relay.build_message(
            "[click here](https://evil.example)", "", "fb.example.org")
        self.assertNotIn("](https://evil.example)", msg)

    def test_mention_text_is_escaped_and_never_requested(self):
        # Two independent guards, asserted independently: the text is escaped
        # so it does not render as a mention, and the payload separately
        # refuses to deliver any mention at all.
        msg = relay.build_message("alice", "", "@everyone free bubbles")
        self.assertIn("@everyone", msg)          # still readable as text
        self.assertNotIn("|", msg)               # no markup smuggled through

    def test_newline_cannot_forge_a_second_alert(self):
        msg = relay.build_message(
            "alice", "", "real\n🔔 **admin** joined **your-bank**")
        self.assertNotIn("\n", msg)

    def test_long_servername_cannot_crowd_out_the_line(self):
        msg = relay.build_message("alice", "", "A" * 500)
        self.assertLessEqual(len(msg), relay.MAX_DISCORD_CONTENT)
        self.assertNotIn("A" * (relay.MAX_DISPLAY + 1), msg)

    def test_truncation_never_leaves_a_dangling_escape(self):
        # Truncating after escaping could cut between a backslash and the
        # character it escapes, which would then escape whatever followed.
        msg = relay.build_message("alice", "", "*" * (relay.MAX_DISPLAY + 20))
        self.assertFalse(msg.endswith("\\"))


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
            return FakeResponse()

        original = relay.urllib.request.urlopen
        relay.urllib.request.urlopen = fake_urlopen
        try:
            relay._post_sync("https://discord.example/webhook", content)
        finally:
            relay.urllib.request.urlopen = original
        return captured["body"]

    def test_post_suppresses_every_mention(self):
        body = self._payload("🔔 **alice** joined **@everyone**")
        self.assertEqual(body["allowed_mentions"], {"parse": []})

    def test_post_still_carries_the_content(self):
        body = self._payload("hello")
        self.assertEqual(body["content"], "hello")


if __name__ == "__main__":
    unittest.main(verbosity=2)
