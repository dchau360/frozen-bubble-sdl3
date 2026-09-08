// NetworkClient::Connect() against a deliberately awkward peer.
//
// These cases cannot be produced against a real fb-server on localhost, which
// is why they were never covered: localhost always answers, instantly, in one
// segment. tests/fake_server.h scripts the awkward peers instead -- a banner
// that arrives late, a banner split across two TCP segments, a peer that
// accepts and then says nothing, and a port that refuses outright.
//
// Part of docs/ASYNC_NETWORKING_HANDOFF.md's verification section: the fixture
// is what makes item A's "demonstrate that input and rendering continue during
// waits" matrix deterministic rather than dependent on timing luck.

#include "networkclient.h"
#include "platform.h"

#include <SDL3/SDL.h>

#include <cstdio>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

#if defined(__ANDROID__) || defined(__WASM_PORT__) || defined(_WIN32) || defined(__IOS_PORT__)
int main() {
    std::fprintf(stderr, "netconnect-test: POSIX sockets only; skipping\n");
    return 77;  // SKIP_RETURN_CODE -- reported as skipped, not as a pass
}
#else

#include "fake_server.h"

// Connect() refuses to start from any state but DISCONNECTED, and the client
// is a process-wide singleton, so every case has to hand the next one a clean
// slate whether it connected or not.
static void ResetClient() {
    NetworkClient* nc = NetworkClient::Instance();
    nc->Disconnect();
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    InitDataDir();

    // --- Baseline: an ordinary, well-behaved peer. Establishes that the
    // fixture speaks the real banner format (server/net.c's
    // "FB/1.3 PUSH: SERVER_READY <name> <lang>") closely enough for the real
    // client to accept it -- without this, every failure below would be
    // ambiguous between "the client is wrong" and "the fixture is wrong".
    {
        fbtest::FakeServer server;
        CHECK(server.Started());

        NetworkClient* nc = NetworkClient::Instance();
        CHECK(nc->Connect("127.0.0.1", server.Port()));
        CHECK(nc->GetState() == CONNECTED);
        CHECK(nc->IsConnected());
        CHECK(server.AcceptedCount() == 1);
        ResetClient();
    }

    // --- A banner that arrives late, but inside the handshake deadline.
    // 600ms is comfortably under the 3s the drain loop allows and comfortably
    // over the single 100ms poll slice it uses, so this pins that the deadline
    // is judged over the whole wait and not per-slice -- the bug the item A
    // slice fixed, kept honest here.
    {
        fbtest::FakeServerOptions opts;
        opts.banner = fbtest::Banner::Delayed;
        opts.delayMs = 600;
        fbtest::FakeServer server(opts);
        CHECK(server.Started());

        NetworkClient* nc = NetworkClient::Instance();
        const Uint64 start = SDL_GetTicks();
        const bool connected = nc->Connect("127.0.0.1", server.Port());
        const Uint64 elapsed = SDL_GetTicks() - start;
        CHECK(connected);
        CHECK(nc->GetState() == CONNECTED);
        // Upper bound only -- the point is that it did not sit out the full
        // deadline once the banner had actually arrived.
        CHECK(elapsed < 2000);
        if (elapsed >= 2000)
            std::fprintf(stderr, "  (delayed banner: connect took %llums)\n",
                         (unsigned long long)elapsed);
        ResetClient();
    }

    // --- A banner split across two TCP segments.
    //
    // This is the case the whole fixture exists for. The banner is one short
    // line, so on loopback it almost always arrives in a single read and the
    // handshake's line handling is never actually exercised. Split it -- which
    // a real network, a proxy, or a WebSocket bridge can do at any time -- and
    // a reader that treats each recv() as a self-contained line sees
    // "FB/1.3 PUSH: SERVER_R" and then "EADY fake en\n", finds SERVER_READY in
    // neither, and concludes the server never greeted it.
    //
    // The split point is inside the SERVER_READY token itself, which is the
    // worst case and the one a token-boundary-only split would miss.
    {
        fbtest::FakeServerOptions opts;
        opts.banner = fbtest::Banner::Split;
        opts.delayMs = 120;   // long enough that the two arrive as separate reads
        opts.splitAt = 22;    // lands inside "SERVER_READY"
        fbtest::FakeServer server(opts);
        CHECK(server.Started());

        NetworkClient* nc = NetworkClient::Instance();
        const bool connected = nc->Connect("127.0.0.1", server.Port());
        CHECK(connected);
        CHECK(nc->GetState() == CONNECTED);
        if (!connected)
            std::fprintf(stderr,
                         "  (split banner: Connect() failed -- the handshake is "
                         "reading raw recv() chunks as whole lines)\n");
        ResetClient();
    }

    // --- A peer that accepts and then never says anything. Connect() has to
    // give up on its own deadline rather than hanging forever. Asserted as an
    // upper bound (and a sanity lower bound that it did not give up instantly
    // for some unrelated reason), never as a specific duration.
    {
        fbtest::FakeServerOptions opts;
        opts.banner = fbtest::Banner::Never;
        fbtest::FakeServer server(opts);
        CHECK(server.Started());

        NetworkClient* nc = NetworkClient::Instance();
        const Uint64 start = SDL_GetTicks();
        const bool connected = nc->Connect("127.0.0.1", server.Port());
        const Uint64 elapsed = SDL_GetTicks() - start;
        CHECK(!connected);
        CHECK(nc->GetState() == DISCONNECTED);
        CHECK(server.AcceptedCount() == 1);  // it really did reach the peer
        CHECK(elapsed >= 1000);              // did not fail for an unrelated reason
        CHECK(elapsed < 8000);               // and did not hang
        if (elapsed >= 8000)
            std::fprintf(stderr, "  (silent peer: Connect() took %llums\n",
                         (unsigned long long)elapsed);
        ResetClient();
    }

    // --- A refused connect: nothing is bound to the port, so the RST comes
    // straight back. Exercises the branch where select() reports the socket
    // writable but SO_ERROR is non-zero -- writability alone means the attempt
    // finished, not that it succeeded, and treating the two as the same thing
    // is what used to list dead servers as online.
    //
    // A refusal is an answer, not a timeout, so it must be fast. This is the
    // one negative case here that can assert a real upper bound.
    {
        fbtest::FakeServerOptions opts;
        opts.reachability = fbtest::Reachability::Refused;
        fbtest::FakeServer server(opts);
        CHECK(server.Started());

        NetworkClient* nc = NetworkClient::Instance();
        const Uint64 start = SDL_GetTicks();
        const bool connected = nc->Connect("127.0.0.1", server.Port());
        const Uint64 elapsed = SDL_GetTicks() - start;
        CHECK(!connected);
        CHECK(nc->GetState() == DISCONNECTED);
        CHECK(server.AcceptedCount() == 0);
        CHECK(elapsed < 2000);
        if (elapsed >= 2000)
            std::fprintf(stderr, "  (refused connect: took %llums)\n",
                         (unsigned long long)elapsed);
        ResetClient();
    }

    // --- A blackholed port: bound but never listening, so on this platform
    // the SYN is dropped and the connect never completes or fails on its own.
    // This is the unreachable-host case, and the only thing worth asserting is
    // that the client imposed *some* bound instead of hanging forever -- the
    // duration is the OS's business, and on a platform that RSTs this instead
    // (Linux typically does) it will simply fail faster. Upper bound only, per
    // the handoff doc.
    {
        fbtest::FakeServerOptions opts;
        opts.reachability = fbtest::Reachability::Blackholed;
        fbtest::FakeServer server(opts);
        CHECK(server.Started());

        NetworkClient* nc = NetworkClient::Instance();
        const Uint64 start = SDL_GetTicks();
        const bool connected = nc->Connect("127.0.0.1", server.Port());
        const Uint64 elapsed = SDL_GetTicks() - start;
        CHECK(!connected);
        CHECK(nc->GetState() == DISCONNECTED);
        CHECK(server.AcceptedCount() == 0);
        CHECK(elapsed < 8000);
        if (elapsed >= 8000)
            std::fprintf(stderr, "  (blackholed connect: took %llums)\n",
                         (unsigned long long)elapsed);
        ResetClient();
    }

    NetworkClient::Dispose();
    SDL_Quit();

    if (failures == 0) std::fprintf(stderr, "netconnect-test: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
#endif
