/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2026 dchau360
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

// The client half of the protocol-1.4 platform tag: reading it back out of a
// LIST entry without breaking the geolocation that shares the same colons.
//
// fb-server's own side of this is covered end to end by
// tests/server_platform_input_test.py, which drives the real binary. What that
// cannot reach is this parser's discrimination rule, which is the part with a
// real failure mode: the tag is recognised by *shape* (a trailing ":<c>" whose
// c is one of a closed set) rather than by counting colons, because an entry
// can carry zero, one or two optional trailing pieces and only some
// combinations are distinguishable by count. The cases below are the ones
// where a naive split would put a platform tag in the geolocation field or a
// longitude in the platform one.

#include "networkclient.h"

#include <cstdio>
#include <string>
#include <vector>

static int failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                    \
        }                                                                  \
    } while (0)

// ParseListResponse is private; this friend (declared in networkclient.h) is
// the test-only way to drive it without a socket.
struct NetworkClientTestAccess {
    static void ParseList(NetworkClient& nc, const char* data) {
        nc.ParseListResponse(data);
    }
    static char PlatformFor(const NetworkClient& nc, const std::string& nick) {
        return nc.GetPlatformForNick(nick);
    }
};

namespace {

// A LIST line is "<open players> <rooms> free:.. games:.. playing:.. at:..";
// only the first field matters here, but the trailing shape has to be real or
// the parser stops before reaching the players at all.
std::string ListLine(const std::string& openPlayers) {
    return openPlayers + ", free:1 games:0 playing:0 at:";
}

// Borrows from `players` -- callers must bind GetOpenPlayers()'s by-value
// result to a named local first, or the returned pointer dangles.
const NetworkPlayer* Find(const std::vector<NetworkPlayer>& players,
                          const std::string& nick) {
    for (const auto& p : players) {
        if (p.nick == nick) return &p;
    }
    return nullptr;
}

}  // namespace

int main() {
    NetworkClient nc;

    // The common live shape: PLATFORM lands with NICK, GEOLOC's ~16s lookup
    // has not come back, so the middle field is empty.
    {
        NetworkClientTestAccess::ParseList(nc, ListLine("alice::W").c_str());
        const auto players = nc.GetOpenPlayers();
        const NetworkPlayer* alice = Find(players, "alice");
        CHECK(alice != nullptr);
        if (alice) {
            CHECK(alice->platform == 'W');
            CHECK(alice->geoloc.empty());
        }
    }

    // Both present. The geolocation must come out byte-identical to what a
    // pre-1.4 server would have sent, since the world-map dot parses it with
    // sscanf("%f:%f") and a stray ":W" on the end changes nothing only as
    // long as it is not left inside the field.
    {
        NetworkClientTestAccess::ParseList(nc, ListLine("bob:37.7:-122.4:M").c_str());
        const auto players = nc.GetOpenPlayers();
        const NetworkPlayer* bob = Find(players, "bob");
        CHECK(bob != nullptr);
        if (bob) {
            CHECK(bob->platform == 'M');
            CHECK(bob->geoloc == "37.7:-122.4");
        }
    }

    // A pre-1.4 server, or a client that never sent PLATFORM: unchanged in
    // both directions. The negative longitude case is the one that would
    // break a parser matching "trailing single char" too loosely.
    {
        NetworkClientTestAccess::ParseList(nc, ListLine("carol:51.5:-0.1").c_str());
        const auto players = nc.GetOpenPlayers();
        const NetworkPlayer* carol = Find(players, "carol");
        CHECK(carol != nullptr);
        if (carol) {
            CHECK(carol->platform == 0);
            CHECK(carol->geoloc == "51.5:-0.1");
        }
    }
    {
        NetworkClientTestAccess::ParseList(nc, ListLine("dave").c_str());
        const auto players = nc.GetOpenPlayers();
        const NetworkPlayer* dave = Find(players, "dave");
        CHECK(dave != nullptr);
        if (dave) {
            CHECK(dave->platform == 0);
            CHECK(dave->geoloc.empty());
        }
    }

    // A tag outside the set is not a tag. fb-server rejects these outright so
    // this should be unreachable from an honest server, but the rule that
    // makes the shape test safe is that it only ever matches the closed set --
    // if it matched any single char, "erin:1:2:5" would lose its longitude.
    {
        NetworkClientTestAccess::ParseList(nc, ListLine("erin::Z").c_str());
        const auto players = nc.GetOpenPlayers();
        const NetworkPlayer* erin = Find(players, "erin");
        CHECK(erin != nullptr);
        if (erin) {
            CHECK(erin->platform == 0);
            CHECK(erin->geoloc == ":Z");
        }
    }

    // A mixed lobby in one response, which is what an upgrade week looks like.
    {
        NetworkClientTestAccess::ParseList(
            nc, ListLine("frank::L,grace,heidi:1.0:2.0:B").c_str());
        const auto players = nc.GetOpenPlayers();
        CHECK(players.size() == 3);
        const NetworkPlayer* frank = Find(players, "frank");
        const NetworkPlayer* grace = Find(players, "grace");
        const NetworkPlayer* heidi = Find(players, "heidi");
        CHECK(frank && frank->platform == 'L');
        CHECK(grace && grace->platform == 0);
        CHECK(heidi && heidi->platform == 'B');
        CHECK(heidi && heidi->geoloc == "1.0:2.0");
    }

    // Players inside a room carry the tag too, and it survives into the
    // nick-keyed cache the in-game boards read -- LIST stops arriving once a
    // room starts, so that cache is the only thing they can draw from.
    {
        NetworkClientTestAccess::ParseList(
            nc, "ivan::W, [judy::A,ken:3.0:4.0:I]:5 free:1 games:1 playing:0 at:");
        CHECK(NetworkClientTestAccess::PlatformFor(nc, "judy") == 'A');
        CHECK(NetworkClientTestAccess::PlatformFor(nc, "ken") == 'I');
        CHECK(NetworkClientTestAccess::PlatformFor(nc, "ivan") == 'W');
        CHECK(NetworkClientTestAccess::PlatformFor(nc, "nobody") == 0);
    }

    // The cache keeps what it was told rather than being rebuilt per response:
    // a later LIST that happens not to mention someone (they moved into a
    // running room) must not blank their badge mid-match.
    {
        NetworkClientTestAccess::ParseList(nc, ListLine("someoneelse").c_str());
        CHECK(NetworkClientTestAccess::PlatformFor(nc, "judy") == 'A');
    }

    if (failures == 0) std::printf("list-platform-parse-test: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
