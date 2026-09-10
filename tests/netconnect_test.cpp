// NetworkClient's async connect, against a deliberately awkward peer.
//
// These cases cannot be produced against a real fb-server on localhost, which
// is why they were never covered: localhost always answers, instantly, in one
// segment. tests/fake_server.h scripts the awkward peers instead -- a banner
// that arrives late, a banner split across two TCP segments, a peer that
// accepts and then says nothing, a refused port, and a blackholed one.
//
// The assertion that matters most here is not "does it connect" but "how long
// did any single call take". Connect() used to run name lookup, the TCP
// handshake and the SERVER_READY exchange to completion before returning, so
// one ENTER on a dead server froze the render loop for up to 8 seconds plus an
// unbounded DNS lookup. Every case below therefore measures the worst single
// call and asserts the frame budget was never blown -- that is what
// docs/OPTIMIZATION_HANDOFF.md's "Async networking rearchitecture" section
// means by "demonstrate that input and rendering continue during waits".

#include "networkclient.h"
#include "platform.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <string>

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

// One simulated frame may not exceed this. A 60fps frame is 16ms and that is
// the number the handoff doc names, but a single call occasionally losing the
// CPU to the scheduler on a loaded machine is not a blocking bug and must not
// fail the build. 50ms still proves the point past any doubt: the behaviour
// this replaced blocked for 3000-8000ms in exactly these cases, two orders of
// magnitude above this bound.
static const Uint64 kFrameBudgetMs = 50;

struct PumpResult {
    bool connected = false;
    bool failed = false;      // settled as DISCONNECTED
    Uint64 elapsedMs = 0;
    Uint64 worstFrameMs = 0;  // longest single Update() call
    int frames = 0;
};

// Drives the client exactly the way MainMenu::PumpNetworkFrame() does -- one
// Update() per frame, ~60fps -- until the connection settles or the budget
// runs out, timing every individual call.
static PumpResult PumpUntilSettled(NetworkClient* nc, Uint64 budgetMs) {
    PumpResult r;
    const Uint64 start = SDL_GetTicks();
    while (SDL_GetTicks() - start < budgetMs) {
        const Uint64 frameStart = SDL_GetTicks();
        nc->Update();
        const Uint64 frameMs = SDL_GetTicks() - frameStart;
        if (frameMs > r.worstFrameMs) r.worstFrameMs = frameMs;
        ++r.frames;

        if (!nc->IsConnecting()) break;  // settled, one way or the other
        SDL_Delay(16);                   // the rest of a frame's work
    }
    r.elapsedMs = SDL_GetTicks() - start;
    r.connected = nc->IsConnected();
    r.failed = (nc->GetState() == DISCONNECTED);
    return r;
}

// As above, but settles on an arbitrary condition rather than on the connect
// state machine -- used by the game-start cases, which run entirely after the
// connection is up.
static PumpResult PumpUntil(NetworkClient* nc, Uint64 budgetMs, bool (*done)(NetworkClient*)) {
    PumpResult r;
    const Uint64 start = SDL_GetTicks();
    while (SDL_GetTicks() - start < budgetMs) {
        const Uint64 frameStart = SDL_GetTicks();
        nc->Update();
        const Uint64 frameMs = SDL_GetTicks() - frameStart;
        if (frameMs > r.worstFrameMs) r.worstFrameMs = frameMs;
        ++r.frames;

        if (done(nc)) break;
        SDL_Delay(16);
    }
    r.elapsedMs = SDL_GetTicks() - start;
    r.connected = nc->IsConnected();
    r.failed = (nc->GetState() == DISCONNECTED);
    return r;
}

static size_t CountOccurrences(const std::string& haystack, const std::string& needle) {
    size_t count = 0;
    for (size_t at = haystack.find(needle); at != std::string::npos;
         at = haystack.find(needle, at + needle.size())) {
        ++count;
    }
    return count;
}

// Printed on success as well as failure, unlike the other tests here. The
// whole claim of this file is about how long things took, and a timing result
// that is only visible when it fails cannot be sanity-checked by a human
// reading CI output -- "total 5019ms, worst frame 1ms" is the entire point,
// and it is worth six lines to show it.
static void ReportFrames(const char* label, const PumpResult& r) {
    std::fprintf(stderr, "  [%s] worst frame %llums, total %llums, %d frames%s\n", label,
                 (unsigned long long)r.worstFrameMs, (unsigned long long)r.elapsedMs, r.frames,
                 r.worstFrameMs > kFrameBudgetMs ? "  <-- OVER FRAME BUDGET" : "");
}

// Connect() refuses to start from any state but DISCONNECTED, and the client
// is a process-wide singleton, so every case has to hand the next one a clean
// slate whether it connected or not.
static void ResetClient() {
    NetworkClient::Instance()->Disconnect();
}

struct NetworkClientTestAccess {
    static void BeginPendingCreate(NetworkClient& client, const char* nick, int maxPlayers) {
        client.pendingCreate = true;
        client.pendingCreateOrigNick = nick;
        client.pendingCreateNick = nick;
        client.pendingCreateSuffix = 2;
        client.pendingCreateMaxPlayers = maxPlayers;
    }
    static void HandleResponse(NetworkClient& client, const char* response) {
        client.HandleServerResponse(response);
    }
};

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
        // Connect() now means "the attempt started", not "we are connected".
        CHECK(nc->Connect("127.0.0.1", server.Port()));
        const PumpResult r = PumpUntilSettled(nc, 5000);
        CHECK(r.connected);
        CHECK(nc->GetState() == CONNECTED);
        CHECK(server.AcceptedCount() == 1);
        CHECK(r.worstFrameMs <= kFrameBudgetMs);
        ReportFrames("baseline", r);
        ResetClient();
    }

    // --- A banner that arrives late, but inside the handshake deadline. The
    // client must keep being pumped throughout and must not block on any one
    // frame while waiting: this is the case that most directly demonstrates
    // the UI staying alive during a slow server's greeting.
    {
        fbtest::FakeServerOptions opts;
        opts.banner = fbtest::Banner::Delayed;
        opts.delayMs = 600;
        fbtest::FakeServer server(opts);
        CHECK(server.Started());

        NetworkClient* nc = NetworkClient::Instance();
        CHECK(nc->Connect("127.0.0.1", server.Port()));
        const PumpResult r = PumpUntilSettled(nc, 5000);
        CHECK(r.connected);
        CHECK(r.worstFrameMs <= kFrameBudgetMs);
        // It really did have to wait -- otherwise this proves nothing about
        // waiting without blocking.
        CHECK(r.frames > 5);
        ReportFrames("delayed banner", r);
        ResetClient();
    }

    // --- A banner split across two TCP segments.
    //
    // The banner is one short line, so on loopback it almost always arrives in
    // a single read and the handshake's line handling is never exercised.
    // Split it -- which a real network, proxy, or WebSocket bridge can do at
    // any time -- and a reader that treats each recv() as a self-contained
    // line sees "FB/1.3 PUSH: SERVER_R" then "EADY fake en\n", finds
    // SERVER_READY in neither, and reports a working server as unreachable.
    // That was a real, shipping bug; this is its regression test.
    //
    // The split point is inside the SERVER_READY token itself, which is the
    // worst case and the one a token-boundary split would miss.
    {
        fbtest::FakeServerOptions opts;
        opts.banner = fbtest::Banner::Split;
        opts.delayMs = 120;   // long enough that the two arrive as separate reads
        opts.splitAt = 22;    // lands inside "SERVER_READY"
        fbtest::FakeServer server(opts);
        CHECK(server.Started());

        NetworkClient* nc = NetworkClient::Instance();
        CHECK(nc->Connect("127.0.0.1", server.Port()));
        const PumpResult r = PumpUntilSettled(nc, 5000);
        CHECK(r.connected);
        CHECK(nc->GetState() == CONNECTED);
        if (!r.connected)
            std::fprintf(stderr,
                         "  (split banner: never connected -- the handshake is "
                         "reading raw recv() chunks as whole lines)\n");
        CHECK(r.worstFrameMs <= kFrameBudgetMs);
        ReportFrames("split banner", r);
        ResetClient();
    }

    // --- A peer that accepts and then never says anything. The client has to
    // give up on its own deadline rather than waiting forever, and has to stay
    // responsive for the entire wait.
    {
        fbtest::FakeServerOptions opts;
        opts.banner = fbtest::Banner::Never;
        fbtest::FakeServer server(opts);
        CHECK(server.Started());

        NetworkClient* nc = NetworkClient::Instance();
        CHECK(nc->Connect("127.0.0.1", server.Port()));
        const PumpResult r = PumpUntilSettled(nc, 10000);
        CHECK(!r.connected);
        CHECK(r.failed);
        CHECK(server.AcceptedCount() == 1);  // it really did reach the peer
        CHECK(r.worstFrameMs <= kFrameBudgetMs);
        ReportFrames("silent peer", r);
        ResetClient();
    }

    // --- A refused connect: nothing is bound to the port, so the RST comes
    // straight back. Exercises the branch where the socket becomes writable
    // but SO_ERROR is non-zero -- writability alone means the attempt
    // finished, not that it succeeded, and treating the two as the same thing
    // is what used to list dead servers as online.
    //
    // A refusal is an answer, not a timeout, so it must also be quick. This is
    // the one negative case here that can assert a real upper bound on total
    // time as well as on frame time.
    {
        fbtest::FakeServerOptions opts;
        opts.reachability = fbtest::Reachability::Refused;
        fbtest::FakeServer server(opts);
        CHECK(server.Started());

        NetworkClient* nc = NetworkClient::Instance();
        CHECK(nc->Connect("127.0.0.1", server.Port()));
        const PumpResult r = PumpUntilSettled(nc, 10000);
        CHECK(!r.connected);
        CHECK(r.failed);
        CHECK(server.AcceptedCount() == 0);
        CHECK(r.elapsedMs < 2000);
        CHECK(r.worstFrameMs <= kFrameBudgetMs);
        ReportFrames("refused connect", r);
        ResetClient();
    }

    // --- A blackholed port: bound but never listening, so on this platform
    // the SYN is dropped and the connect neither completes nor fails on its
    // own. The unreachable-host case. Only two things are worth asserting: the
    // client imposed some bound of its own instead of waiting forever, and it
    // stayed responsive while doing so. The duration is the OS's business, and
    // on a platform that RSTs this instead (Linux typically does) it simply
    // fails sooner. Upper bounds only, per the handoff doc.
    {
        fbtest::FakeServerOptions opts;
        opts.reachability = fbtest::Reachability::Blackholed;
        fbtest::FakeServer server(opts);
        CHECK(server.Started());

        NetworkClient* nc = NetworkClient::Instance();
        CHECK(nc->Connect("127.0.0.1", server.Port()));
        const PumpResult r = PumpUntilSettled(nc, 15000);
        CHECK(!r.connected);
        CHECK(r.failed);
        CHECK(server.AcceptedCount() == 0);
        CHECK(r.worstFrameMs <= kFrameBudgetMs);
        ReportFrames("blackholed connect", r);
        ResetClient();
    }

    // --- Cancelling a connection that is still in flight. This is what ESC on
    // the connecting screen does, and the case the detached-resolver design
    // exists for: the worker may still be inside getaddrinfo() when the client
    // stops caring. It must not touch the client afterwards -- under ASan this
    // case is what would catch it if it did.
    {
        fbtest::FakeServerOptions opts;
        opts.reachability = fbtest::Reachability::Blackholed;
        fbtest::FakeServer server(opts);
        CHECK(server.Started());

        NetworkClient* nc = NetworkClient::Instance();
        CHECK(nc->Connect("127.0.0.1", server.Port()));
        nc->Update();
        CHECK(nc->IsConnecting());   // genuinely mid-flight before we cancel

        const Uint64 cancelStart = SDL_GetTicks();
        nc->Disconnect();
        const Uint64 cancelMs = SDL_GetTicks() - cancelStart;
        CHECK(nc->GetState() == DISCONNECTED);
        CHECK(!nc->IsConnecting());
        CHECK(!nc->IsConnected());
        // Cancelling must be instant: it may not wait on the resolver, which
        // is the whole reason that worker is detached and co-owns its result.
        CHECK(cancelMs <= kFrameBudgetMs);

        // And the client must be reusable straight afterwards -- a cancel that
        // left stale state behind would show up as the next Connect() being
        // refused for not being DISCONNECTED.
        fbtest::FakeServer good;
        CHECK(good.Started());
        CHECK(nc->Connect("127.0.0.1", good.Port()));
        const PumpResult r = PumpUntilSettled(nc, 5000);
        CHECK(r.connected);
        ResetClient();
    }

    // --- The leader's game-start poll (stage 3a).
    //
    // After START, the leader must poll LEADER_CHECK_GAME_START until every
    // joiner has acknowledged and only then send its own OK_GAME_START, so
    // that everyone is in prio mode before it starts broadcasting level sync.
    // It used to do that in a blocking loop *inside a push-message handler*:
    // 50 attempts of up to 200ms select() plus a 100ms sleep each, so a joiner
    // that was merely slow froze the leader's render loop for up to 15s (the
    // loop's own comment claimed 5s). Worse, it was reached from the per-frame
    // pump, so anything else the client spoke for -- a hosted bot on its own
    // socket -- stopped being serviced for the duration and could miss the
    // very acknowledgement being waited for.
    //
    // Both cases below reach the poll through the real path: NICK, CREATE,
    // START, and a GAME_CAN_START push in the server's own wire format.
    {
        // "the server says no a few times, then yes" -- the ordinary case, and
        // the one a real fb-server on localhost answers too fast to exercise.
        fbtest::FakeServerOptions opts;
        // Rule order matters and is load-bearing: "LEADER_CHECK_GAME_START"
        // contains "START", and the first matching rule wins.
        opts.rules = {
            {"LEADER_CHECK_GAME_START", {"FB/1.3 LEADER_CHECK_GAME_START: OTHERS_NOT_READY\n",
                                         "FB/1.3 LEADER_CHECK_GAME_START: OTHERS_NOT_READY\n",
                                         "FB/1.3 LEADER_CHECK_GAME_START: OTHERS_NOT_READY\n",
                                         "FB/1.3 LEADER_CHECK_GAME_START: OK\n"}},
            {"NICK", {"FB/1.3 NICK: OK\n"}},
            {"CREATE", {"FB/1.3 CREATE: OK\n"}},
            // The player-id byte is binary, so it cannot go in a string
            // literal next to "leader" without the hex escape swallowing the
            // following letters.
            {"START", {std::string("FB/1.3 PUSH: GAME_CAN_START: \x01") + "leader\n"}},
        };
        fbtest::FakeServer server(opts);
        CHECK(server.Started());

        NetworkClient* nc = NetworkClient::Instance();
        CHECK(nc->Connect("127.0.0.1", server.Port()));
        CHECK(PumpUntilSettled(nc, 5000).connected);

        CHECK(nc->SendNick("leader"));
        PumpUntil(nc, 3000, [](NetworkClient* c) { return !c->IsPendingNick(); });
        CHECK(!nc->IsPendingNick());

        CHECK(nc->CreateGame(2));
        PumpUntil(nc, 3000, [](NetworkClient* c) { return c->GetState() == IN_LOBBY; });
        CHECK(nc->GetState() == IN_LOBBY);
        CHECK(nc->IsLeader());

        CHECK(nc->StartGame());
        const PumpResult r =
            PumpUntil(nc, 8000, [](NetworkClient* c) { return c->GetState() == IN_GAME; });
        CHECK(nc->GetState() == IN_GAME);
        // The acknowledgement is only correct if it comes *after* the server
        // said everyone was ready -- sending it early is the desync this whole
        // handshake exists to prevent.
        CHECK(server.WaitForReceived("OK_GAME_START", 1000));
        // It polled more than once (so it really did wait) but nothing like
        // the old loop's 50, and it finished on the answer rather than on the
        // deadline.
        const size_t polls = CountOccurrences(server.Received(), "LEADER_CHECK_GAME_START");
        CHECK(polls >= 4);
        CHECK(polls < 20);
        CHECK(r.elapsedMs < 3000);
        CHECK(r.worstFrameMs <= kFrameBudgetMs);
        ReportFrames("game start (server says ready)", r);
        std::fprintf(stderr, "  [game start] %d polls sent\n", (int)polls);
        ResetClient();
    }

    {
        // A joiner that never acknowledges. The leader has to start anyway on
        // its own deadline: refusing to start would strand every other player
        // in the room with no way forward, which is strictly worse than one
        // client playing a desynced board. What must not happen is the render
        // loop being held hostage for the whole wait.
        fbtest::FakeServerOptions opts;
        opts.rules = {
            {"LEADER_CHECK_GAME_START", {"FB/1.3 LEADER_CHECK_GAME_START: OTHERS_NOT_READY\n"}},
            {"NICK", {"FB/1.3 NICK: OK\n"}},
            {"CREATE", {"FB/1.3 CREATE: OK\n"}},
            {"START", {std::string("FB/1.3 PUSH: GAME_CAN_START: \x01") + "leader\n"}},
        };
        fbtest::FakeServer server(opts);
        CHECK(server.Started());

        NetworkClient* nc = NetworkClient::Instance();
        CHECK(nc->Connect("127.0.0.1", server.Port()));
        CHECK(PumpUntilSettled(nc, 5000).connected);
        CHECK(nc->SendNick("leader"));
        PumpUntil(nc, 3000, [](NetworkClient* c) { return !c->IsPendingNick(); });
        CHECK(nc->CreateGame(2));
        PumpUntil(nc, 3000, [](NetworkClient* c) { return c->GetState() == IN_LOBBY; });
        CHECK(nc->IsLeader());

        CHECK(nc->StartGame());
        const PumpResult r =
            PumpUntil(nc, 15000, [](NetworkClient* c) { return c->GetState() == IN_GAME; });
        CHECK(nc->GetState() == IN_GAME);
        CHECK(server.WaitForReceived("OK_GAME_START", 1000));
        // Bounded by the client's own deadline, not by the old loop's 15s.
        CHECK(r.elapsedMs < 8000);
        CHECK(r.worstFrameMs <= kFrameBudgetMs);
        // And it kept turning frames throughout rather than sleeping through
        // the wait -- roughly one per 16ms of it.
        CHECK(r.frames > 100);
        ReportFrames("game start (nobody answers)", r);
        ResetClient();
    }

    // --- The level-sync wait rule (stage 3c). This runs only on the WASM
    // joiner path, but the rule itself is a pure function compiled everywhere
    // precisely so it can be checked here: it was wrong for two releases and
    // nobody noticed, because it lived inline inside an #ifdef __WASM_PORT__
    // block that no test could reach.
    {
        // Round 1: nothing is draining the main queue yet, so the messages
        // pile up there. Keep waiting until all 40 have landed.
        CHECK(ShouldKeepWaitingForLevelSync(0, 0, 10, 5000));
        CHECK(ShouldKeepWaitingForLevelSync(39, 0, 10, 5000));
        CHECK(!ShouldKeepWaitingForLevelSync(40, 0, 10, 5000));

        // Round 2+: ProcessNetworkMessages() has been draining the main queue
        // into the sync queue all along, so the messages are split across the
        // two. This is the case the old rule got wrong -- it counted only the
        // first number, so it never reached 40 and every round after the first
        // burned the entire timeout before starting.
        CHECK(!ShouldKeepWaitingForLevelSync(0, 40, 10, 5000));
        CHECK(!ShouldKeepWaitingForLevelSync(18, 22, 10, 5000));
        CHECK(ShouldKeepWaitingForLevelSync(18, 21, 10, 5000));  // 39: one short

        // The timeout still wins over an incomplete sync: a joiner whose
        // leader went quiet must start eventually rather than being stranded.
        CHECK(!ShouldKeepWaitingForLevelSync(0, 0, 5001, 5000));
        CHECK(ShouldKeepWaitingForLevelSync(0, 0, 4999, 5000));
    }

    // --- CREATE confirmation must be scoped to CREATE's own reply. The
    // server echoes the command in every response, so an unrelated successful
    // command cannot be allowed to create a phantom room while CREATE is still
    // pending. This parser is shared by native and WASM even though the bug was
    // first identified in the WebSocket flow.
    {
        NetworkClient* nc = NetworkClient::Instance();
        nc->SetConnected();
        NetworkClientTestAccess::BeginPendingCreate(*nc, "creator", 8);

        NetworkClientTestAccess::HandleResponse(*nc, "FB/1.3 TALK: OK");
        CHECK(nc->IsPendingCreate());
        CHECK(nc->GetState() == CONNECTED);
        CHECK(nc->GetCurrentGame() == nullptr);

        NetworkClientTestAccess::HandleResponse(*nc, "FB/1.3 CREATE: OK");
        CHECK(!nc->IsPendingCreate());
        CHECK(nc->GetState() == IN_LOBBY);
        CHECK(nc->GetCurrentGame() != nullptr);
        CHECK(nc->GetCurrentGame()->creator == "creator");
        CHECK(nc->GetCurrentGame()->maxPlayers == 8);
        ResetClient();
    }

    NetworkClient::Dispose();
    SDL_Quit();

    if (failures == 0) std::fprintf(stderr, "netconnect-test: all checks passed\n");
    return failures == 0 ? 0 : 1;
}
#endif
