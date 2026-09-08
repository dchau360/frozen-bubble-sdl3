// A scriptable stand-in for fb-server, for tests that need a peer which
// misbehaves in a specific, repeatable way.
//
// Why this exists: the async networking work (docs/ASYNC_NETWORKING_HANDOFF.md)
// is almost entirely about what happens when a server is slow, silent, or
// rude, and none of those cases can be produced by pointing a test at a real
// fb-server on localhost -- localhost always answers, and it answers instantly.
// Tests written against a real server can only ever confirm the happy path,
// which is the path least likely to break. This listener lets a test say
// "answer after 800ms", "split the banner across two writes", or "accept the
// connection and then never say anything" and get exactly that, every run,
// with no dependency on network reachability or timing luck.
//
// On unreachable and refused peers -- these are two different things, and
// which one you get is decided by the kernel, not by this fixture. Measured on
// this platform (macOS 25.6, non-blocking connect to 127.0.0.1) rather than
// assumed:
//
//   * nothing bound to the port at all -> RST comes straight back. select()
//     reports the socket writable within a millisecond and SO_ERROR is
//     ECONNREFUSED. This is Reachability::Refused below.
//   * port bound but never listen()ed -> the SYN is silently dropped. The
//     connect never completes and never fails; it runs to whatever deadline
//     the caller imposed. This is Reachability::Blackholed below, and it is
//     how this fixture simulates an unreachable host without needing a real
//     unroutable address.
//
// The second one is BSD/macOS behaviour and is not portable: Linux normally
// RSTs a bound-but-unlistening socket, which would turn Blackholed into
// Refused there. So any test using Blackholed must assert an upper bound on
// how long the client waited -- never a lower bound, and never a specific
// duration. That is the same rule the handoff doc already states for
// genuinely-unreachable hosts, and for the same reason.

#ifndef FB_TESTS_FAKE_SERVER_H
#define FB_TESTS_FAKE_SERVER_H

#if defined(__ANDROID__) || defined(__WASM_PORT__) || defined(_WIN32) || defined(__IOS_PORT__)
#error "fake_server.h is POSIX-socket only; guard your include the way the tests do"
#endif

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fbtest {

// The exact line a real fb-server opens with. Assembled the same way it is on
// the server side (server/net.c: send_line's "FB/%d.%d %s: %s\n" wrapper,
// current_command forced to PUSH by send_line_log_push, greets_msg_base
// "SERVER_READY %s %s"), so a client that parses this fixture's banner is
// parsing the real format and not a convenient approximation of it.
inline std::string RealBannerLine(const std::string& serverName = "fake",
                                  const std::string& language = "en") {
    return "FB/1.3 PUSH: SERVER_READY " + serverName + " " + language + "\n";
}

enum class Banner {
    Immediate,  // write the whole banner as soon as the peer is accepted
    Delayed,    // wait delayMs, then write the whole banner in one call
    Split,      // write the banner in two pieces separated by delayMs
    Never,      // accept the peer and then never write anything at all
};

// How the port answers a connect at all. See the long note at the top of this
// file -- these are measured behaviours, and Blackholed is platform-dependent.
enum class Reachability {
    Listening,   // normal: accept peers and run the Banner script above
    Refused,     // nothing bound; connect gets RST -> ECONNREFUSED immediately
    Blackholed,  // bound but never listening; on BSD/macOS the SYN is dropped
                 // and the connect runs to the caller's own deadline
};

// A canned answer to a command the client sends.
//
// This is deliberately the smallest thing that can express "the server says no
// three times and then says yes": match a substring of an incoming line, and
// reply with the next entry in `replies`, the last of which repeats forever.
// It is not a protocol implementation and must not grow into one -- a test that
// needs real server semantics should drive a real fb-server. What it is for is
// the handful of exchanges whose *timing* is the thing under test, where a real
// server would answer instantly and prove nothing.
struct Rule {
    std::string trigger;               // substring; matched against one line
    std::vector<std::string> replies;  // consumed in order; the last one repeats
};

struct FakeServerOptions {
    Banner banner = Banner::Immediate;

    // Used by Delayed (how long before the banner) and Split (the gap between
    // the two fragments).
    int delayMs = 0;

    // Where to cut the banner for Banner::Split. The default lands inside
    // "SERVER_READY" itself, so a reader that treats each recv() as a whole
    // line -- rather than buffering until a newline -- sees a first chunk that
    // contains neither a complete token nor a terminator. That is the exact
    // shape of the bug this fixture was built to catch.
    size_t splitAt = 22;

    // false: never recv() from the peer. The peer's own writes then fill the
    // socket buffers and eventually block or return EWOULDBLOCK, which is what
    // a client's send path has to survive.
    bool readPeer = true;

    // Whether the port answers connects at all, and how it declines to.
    Reachability reachability = Reachability::Listening;

    std::string bannerText = RealBannerLine();

    // Canned answers, applied to each newline-terminated line the client
    // sends. Empty by default: with no rules the fixture only ever sends the
    // banner, which is all most cases here want. Requires readPeer.
    std::vector<Rule> rules;
};

// One listener on 127.0.0.1, on a kernel-assigned port. Starts serving in a
// background thread from the constructor and stops in the destructor; every
// accessor is safe to call from the test thread while it runs.
class FakeServer {
public:
    explicit FakeServer(FakeServerOptions options = FakeServerOptions())
        : opts_(std::move(options)) {
        listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd_ < 0) return;

        int reuse = 1;
        ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = 0;  // let the kernel pick, so parallel tests never collide
        ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            ::close(listenFd_);
            listenFd_ = -1;
            return;
        }

        sockaddr_in bound{};
        socklen_t boundLen = sizeof(bound);
        if (::getsockname(listenFd_, reinterpret_cast<sockaddr*>(&bound), &boundLen) != 0) {
            ::close(listenFd_);
            listenFd_ = -1;
            return;
        }
        port_ = ntohs(bound.sin_port);

        switch (opts_.reachability) {
            case Reachability::Listening:
                if (::listen(listenFd_, 4) != 0) {
                    ::close(listenFd_);
                    listenFd_ = -1;
                    return;
                }
                worker_ = std::thread([this] { Serve(); });
                break;

            case Reachability::Blackholed:
                // Stay bound, never listen. Holding the socket open is the
                // point: it reserves the port so nothing else can take it,
                // while the kernel drops incoming SYNs. No worker thread --
                // there is nothing to accept.
                break;

            case Reachability::Refused:
                // Release the port entirely. What we keep is the *number*,
                // which the kernel just told us was free -- a connect to it
                // now gets an immediate RST. Releasing rather than holding is
                // what makes this a refusal instead of a blackhole (see the
                // note at the top of this file).
                //
                // There is an unavoidable race here: another process could
                // claim the port between this close and the test's connect.
                // Nothing portable avoids it, and the window is microseconds
                // on an ephemeral port the kernel has just handed out, so the
                // fixture accepts it rather than pretending otherwise.
                ::close(listenFd_);
                listenFd_ = -1;
                break;
        }
        started_ = true;
    }

    ~FakeServer() { Stop(); }

    FakeServer(const FakeServer&) = delete;
    FakeServer& operator=(const FakeServer&) = delete;

    bool Started() const { return started_; }
    int Port() const { return port_; }

    // How many peers have been accepted so far. Zero for a Refuse-style
    // fixture, and the way a test tells "the client never got that far" apart
    // from "the client connected and then gave up".
    int AcceptedCount() const { return accepted_.load(); }

    // Everything read off accepted peers so far, concatenated. Empty when
    // readPeer is false -- the fixture is not reading, by construction.
    std::string Received() {
        std::lock_guard<std::mutex> lock(mutex_);
        return received_;
    }

    // Blocks until the fixture has read at least `text` from some peer, or
    // until timeoutMs elapses. Returns whether it arrived. Saves every caller
    // from writing the same poll loop.
    bool WaitForReceived(const std::string& text, int timeoutMs) {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            if (Received().find(text) != std::string::npos) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return Received().find(text) != std::string::npos;
    }

    void Stop() {
        if (stopping_.exchange(true)) return;
        // Closing the listener is what breaks the worker out of its select();
        // it polls with a short timeout too, so this never hangs even if the
        // close races the poll.
        if (listenFd_ >= 0) {
            ::shutdown(listenFd_, SHUT_RDWR);
            ::close(listenFd_);
            listenFd_ = -1;
        }
        if (worker_.joinable()) worker_.join();
        // Join the peer threads rather than closing their fds out from under
        // them. Each one polls stopping_ on a 10-20ms slice and closes its own
        // fd on the way out, so this returns promptly -- and nothing ever
        // touches a descriptor another thread has already closed and the
        // kernel may have handed to something else.
        std::vector<std::thread> peerThreads;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            peerThreads.swap(peerThreads_);
        }
        for (std::thread& t : peerThreads) {
            if (t.joinable()) t.join();
        }
    }

private:
    void Serve() {
        while (!stopping_.load()) {
            const int fd = listenFd_;
            if (fd < 0) return;

            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);
            // Short slices rather than a blocking accept(), so Stop() is
            // observed promptly even if no peer ever arrives.
            timeval tv{0, 20 * 1000};
            const int ready = ::select(fd + 1, &readfds, nullptr, nullptr, &tv);
            if (ready <= 0) continue;

            const int peer = ::accept(fd, nullptr, nullptr);
            if (peer < 0) continue;

            accepted_.fetch_add(1);

            // One thread per peer, kept joinable (see Stop()). Tests here
            // connect once or twice, so the cost is irrelevant, and it keeps a
            // slow banner on one peer from delaying the next peer's accept.
            std::lock_guard<std::mutex> lock(mutex_);
            peerThreads_.emplace_back([this, peer] { ServePeer(peer); });
        }
    }

    // Owns `peer` for its whole lifetime and closes it on every exit path, so
    // no other thread ever has to reason about that descriptor.
    void ServePeer(int peer) {
        struct FdCloser {
            int fd;
            ~FdCloser() { if (fd >= 0) ::close(fd); }
        } closer{peer};

        // Nagle would coalesce a Split banner's two fragments back into one
        // segment on loopback, quietly turning the fragmentation test into a
        // duplicate of the Immediate one.
        int nodelay = 1;
        ::setsockopt(peer, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

        switch (opts_.banner) {
            case Banner::Immediate:
                SendAll(peer, opts_.bannerText);
                break;
            case Banner::Delayed:
                if (!SleepUnlessStopping(opts_.delayMs)) return;
                SendAll(peer, opts_.bannerText);
                break;
            case Banner::Split: {
                const size_t cut = opts_.splitAt < opts_.bannerText.size()
                                       ? opts_.splitAt
                                       : opts_.bannerText.size() / 2;
                SendAll(peer, opts_.bannerText.substr(0, cut));
                if (!SleepUnlessStopping(opts_.delayMs)) return;
                SendAll(peer, opts_.bannerText.substr(cut));
                break;
            }
            case Banner::Never:
                break;
        }

        if (!opts_.readPeer) {
            // Hold the connection open without ever reading it. Returning here
            // would close the peer fd and hand the client an EOF, which is a
            // different scenario entirely (a server that hangs up, not one
            // that stops listening).
            while (!stopping_.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return;
        }

        // Per-peer, so each connection starts its rules from the top -- a test
        // that reconnects gets the same script again rather than the tail of
        // the previous run's.
        std::vector<size_t> ruleUses(opts_.rules.size(), 0);
        std::string lineBuffer;

        char buffer[4096];
        while (!stopping_.load()) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(peer, &readfds);
            timeval tv{0, 20 * 1000};
            if (::select(peer + 1, &readfds, nullptr, nullptr, &tv) <= 0) continue;

            const ssize_t got = ::recv(peer, buffer, sizeof(buffer), 0);
            if (got <= 0) return;  // peer closed, or errored
            {
                std::lock_guard<std::mutex> lock(mutex_);
                received_.append(buffer, static_cast<size_t>(got));
            }
            if (opts_.rules.empty()) continue;

            // Buffer to newlines before matching. A command can arrive split
            // across reads, and matching raw recv() chunks would make the
            // fixture guilty of exactly the bug it was built to catch.
            lineBuffer.append(buffer, static_cast<size_t>(got));
            size_t nl;
            while ((nl = lineBuffer.find('\n')) != std::string::npos) {
                const std::string line = lineBuffer.substr(0, nl);
                lineBuffer.erase(0, nl + 1);
                for (size_t i = 0; i < opts_.rules.size(); ++i) {
                    const Rule& rule = opts_.rules[i];
                    if (rule.replies.empty()) continue;
                    if (line.find(rule.trigger) == std::string::npos) continue;
                    const size_t which = ruleUses[i] < rule.replies.size()
                                             ? ruleUses[i]
                                             : rule.replies.size() - 1;
                    ++ruleUses[i];
                    SendAll(peer, rule.replies[which]);
                    break;  // first matching rule wins
                }
            }
        }
    }

    // Returns false if the fixture was stopped mid-wait, so the caller drops
    // the rest of its script instead of writing to a socket being torn down.
    bool SleepUnlessStopping(int totalMs) {
        int slept = 0;
        while (slept < totalMs) {
            if (stopping_.load()) return false;
            const int slice = totalMs - slept < 10 ? totalMs - slept : 10;
            std::this_thread::sleep_for(std::chrono::milliseconds(slice));
            slept += slice;
        }
        return !stopping_.load();
    }

    static void SendAll(int fd, const std::string& data) {
        size_t sent = 0;
        while (sent < data.size()) {
            const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, 0);
            if (n <= 0) return;
            sent += static_cast<size_t>(n);
        }
    }

    FakeServerOptions opts_;
    int listenFd_ = -1;
    int port_ = 0;
    bool started_ = false;
    std::atomic<bool> stopping_{false};
    std::atomic<int> accepted_{0};
    std::thread worker_;
    std::mutex mutex_;          // guards received_ and peerThreads_
    std::string received_;
    std::vector<std::thread> peerThreads_;
};

}  // namespace fbtest

#endif  // FB_TESTS_FAKE_SERVER_H
