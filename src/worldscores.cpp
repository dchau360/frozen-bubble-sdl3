#include "worldscores.h"

#include "gamesettings.h"
#include "platform.h"
#include "playeraccount.h"

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>

#ifdef __WASM_PORT__
#include <emscripten.h>
#include <emscripten/websocket.h>
#else
#include "socket_compat.h"
#include <cerrno>
#include <thread>
#ifndef _WIN32
#include <netdb.h>
#endif
#endif

namespace worldscores {
namespace {

constexpr int kNativePort = 1511;
constexpr Uint64 kSessionTimeoutMs = 12000;
// After a failed send, wait this long before trying again on our own.
constexpr Uint64 kRetryAfterMs = 60000;
// The server's own protocol minor that has HISCORE/HISCORES.
constexpr int kMinProtoMinor = 7;

struct Run {
    int level = 0;
    int timeMs = 0;
    int points = 0;  // most-points boards only
    int shots = 0;   // shots the run had taken (0 = not counted)
};

// The server's order (hiscores.c better()): points first on a most-points
// board, then level, then time.
bool Better(int board, const Run& a, const Run& b) {
    if (board >= kTracks && a.points != b.points) return a.points > b.points;
    if (a.level != b.level) return a.level > b.level;
    return a.timeMs < b.timeMs;
}

std::string testHost;
int testPort = 0;
bool testMode = false;

Run pending[kBoards];
bool pendingLoaded = false;

WorldBoard boards[kBoards];
Status status = Status::Idle;
DeleteStatus deleteStatus = DeleteStatus::Idle;
std::string deleteError;
bool deleteWanted = false;  // asked while another session was in flight
std::string lastError;
bool boardsWanted = false;
Uint64 retryAt = 0;

#ifndef FROZEN_BUBBLE_TEST_ACCESS
// Test builds never touch the real one (same rule as playeraccount.cpp).
std::string PendingPath() {
    const char* pref = GameSettings::Instance()->prefPath;
    return std::string(pref ? pref : "") + "worldscores_pending";
}
#endif

void LoadPending() {
    if (pendingLoaded) return;
    pendingLoaded = true;
#ifndef FROZEN_BUBBLE_TEST_ACCESS
    if (testMode) return;
    // "<board> <level> <timeMs> <points> <shots>" per line; a line without
    // points (written before the most-points boards) is a furthest-level
    // run, one without shots is from before they were counted.
    std::ifstream in(PendingPath());
    std::string line;
    while (std::getline(in, line)) {
        int board, level, timeMs, points = 0, shots = 0;
        if (std::sscanf(line.c_str(), "%d %d %d %d %d", &board, &level, &timeMs, &points, &shots) < 3) continue;
        if (board >= 0 && board < kBoards && level > 0 && timeMs > 0 &&
            (board >= kTracks ? points > 0 : points == 0))
            pending[board] = {level, timeMs, points, shots > 0 ? shots : 0};
    }
#endif
}

void SavePending() {
#ifndef FROZEN_BUBBLE_TEST_ACCESS
    if (testMode) return;
    std::ofstream out(PendingPath(), std::ios::trunc);
    for (int b = 0; b < kBoards; ++b)
        if (pending[b].level > 0)
            out << b << ' ' << pending[b].level << ' ' << pending[b].timeMs << ' ' << pending[b].points
                << ' ' << pending[b].shots << '\n';
#endif
}

bool HasPending() {
    for (int b = 0; b < kBoards; ++b)
        if (pending[b].level > 0) return true;
    return false;
}

std::string Host() {
    if (testMode) return testHost;
    return kWorldScoresHost ? kWorldScoresHost : "";
}

// ---------------------------------------------------------------------------
// Transport: one line-based connection, polled from the main thread.
// ---------------------------------------------------------------------------

class Transport {
public:
    virtual ~Transport() = default;
    // Queue a line (without "\n") to send once connected.
    virtual void Send(const std::string& line) = 0;
    // Advances the connection and appends every complete line that arrived.
    // False once it has failed or closed.
    virtual bool Poll(std::vector<std::string>& lines) = 0;
};

#ifdef __WASM_PORT__

// The browser build reaches fb-server the way NetworkClient does: wss:// on
// 443 through the server's nginx when the page is HTTPS, fb-server's own
// ws:// on 1511 otherwise.
class WebSocketTransport : public Transport {
public:
    explicit WebSocketTransport(const std::string& host, int port) {
        const bool https = EM_ASM_INT({ return location.protocol === 'https:' ? 1 : 0; });
        std::string url = std::string(https ? "wss://" : "ws://") + host + ":" +
                          std::to_string(port > 0 ? port : (https ? 443 : kNativePort));
        EmscriptenWebSocketCreateAttributes attrs;
        emscripten_websocket_init_create_attributes(&attrs);
        attrs.url = url.c_str();
        attrs.protocols = nullptr;
        attrs.createOnMainThread = EM_TRUE;
        ws = emscripten_websocket_new(&attrs);
        if (ws <= 0) { failed = true; return; }
        emscripten_websocket_set_onopen_callback(ws, this, OnOpen);
        emscripten_websocket_set_onclose_callback(ws, this, OnClose);
        emscripten_websocket_set_onerror_callback(ws, this, OnError);
        emscripten_websocket_set_onmessage_callback(ws, this, OnMessage);
    }
    ~WebSocketTransport() override {
        if (ws > 0) {
            emscripten_websocket_close(ws, 1000, "");
            emscripten_websocket_delete(ws);
        }
    }
    void Send(const std::string& line) override {
        const std::string msg = line + "\n";
        if (open) emscripten_websocket_send_utf8_text(ws, msg.c_str());
        else queued.push_back(msg);
    }
    bool Poll(std::vector<std::string>& lines) override {
        size_t nl;
        while ((nl = buffer.find('\n')) != std::string::npos) {
            lines.push_back(buffer.substr(0, nl));
            buffer.erase(0, nl + 1);
        }
        return !failed;
    }

private:
    static EM_BOOL OnOpen(int, const EmscriptenWebSocketOpenEvent*, void* user) {
        auto* self = static_cast<WebSocketTransport*>(user);
        self->open = true;
        for (const auto& m : self->queued) emscripten_websocket_send_utf8_text(self->ws, m.c_str());
        self->queued.clear();
        return EM_TRUE;
    }
    static EM_BOOL OnClose(int, const EmscriptenWebSocketCloseEvent*, void* user) {
        static_cast<WebSocketTransport*>(user)->failed = true;
        return EM_TRUE;
    }
    static EM_BOOL OnError(int, const EmscriptenWebSocketErrorEvent*, void* user) {
        static_cast<WebSocketTransport*>(user)->failed = true;
        return EM_TRUE;
    }
    static EM_BOOL OnMessage(int, const EmscriptenWebSocketMessageEvent* e, void* user) {
        if (e->data && e->numBytes)
            static_cast<WebSocketTransport*>(user)->buffer.append((const char*)e->data, e->numBytes);
        return EM_TRUE;
    }

    EMSCRIPTEN_WEBSOCKET_T ws = 0;
    bool open = false, failed = false;
    std::vector<std::string> queued;
    std::string buffer;
};

#else

// Native: the lookup on a detached worker that owns its result block jointly
// with us (NetworkClient::Connect has the full reasoning), then a
// non-blocking connect polled each frame.
class SocketTransport : public Transport {
public:
    SocketTransport(const std::string& host, int port) {
        socket_init();
        auto r = std::make_shared<Resolve>();
        resolve = r;
        std::thread([r, host, port]() {
            struct addrinfo hints;
            struct addrinfo* res = nullptr;
            std::memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            const std::string portStr = std::to_string(port);
            bool ok = false;
            if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) == 0 && res) {
                std::memcpy(&r->addr, res->ai_addr, sizeof(r->addr));
                ok = true;
            }
            if (res) freeaddrinfo(res);
            r->ok.store(ok);
            r->done.store(true);
        }).detach();
    }
    ~SocketTransport() override {
        if (fd >= 0) SOCKET_CLOSE(fd);
    }
    void Send(const std::string& line) override { out += line + "\n"; }
    bool Poll(std::vector<std::string>& lines) override {
        if (failed) return false;
        if (phase == 0) {
            if (!resolve->done.load()) return true;
            if (!resolve->ok.load()) return Fail();
            fd = (int)socket(AF_INET, SOCK_STREAM, 0);
            if (fd == (int)INVALID_SOCKET) { fd = -1; return Fail(); }
#ifdef _WIN32
            u_long nb = 1;
            ioctlsocket(fd, FIONBIO, &nb);
#else
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
#endif
            struct sockaddr_in addr = resolve->addr;
            const int rc = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
#ifdef _WIN32
            const bool inFlight = rc < 0 && SOCK_ERRNO == WSAEWOULDBLOCK;
#else
            const bool inFlight = rc < 0 && (errno == EINPROGRESS || errno == EWOULDBLOCK);
#endif
            if (rc < 0 && !inFlight) return Fail();
            phase = rc == 0 ? 2 : 1;
            return true;
        }
        if (phase == 1) {
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(fd, &wfds);
            struct timeval noWait{0, 0};
            if (select(fd + 1, nullptr, &wfds, nullptr, &noWait) <= 0) return true;
            int soErr = 0;
            socklen_t len = sizeof(soErr);
#ifdef _WIN32
            char* p = reinterpret_cast<char*>(&soErr);
#else
            void* p = &soErr;
#endif
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, p, &len) != 0 || soErr != 0) return Fail();
            phase = 2;
        }
        while (!out.empty()) {
            const ssize_t n = send(fd, out.data(), out.size(), MSG_NOSIGNAL);
            if (n > 0) { out.erase(0, (size_t)n); continue; }
            if (n < 0 && SOCK_WOULD_BLOCK(SOCK_ERRNO)) break;
            return Fail();
        }
        char buf[2048];
        for (;;) {
            const ssize_t n = recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
            if (n > 0) { in.append(buf, (size_t)n); continue; }
            if (n < 0 && SOCK_WOULD_BLOCK(SOCK_ERRNO)) break;
            failed = true;  // closed or broken; still hand over what arrived
            break;
        }
        size_t nl;
        while ((nl = in.find('\n')) != std::string::npos) {
            std::string line = in.substr(0, nl);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
            in.erase(0, nl + 1);
        }
        return !failed;
    }

private:
    struct Resolve {
        std::atomic<bool> done{false}, ok{false};
        struct sockaddr_in addr{};
    };
    bool Fail() { failed = true; return false; }

    std::shared_ptr<Resolve> resolve;
    int fd = -1;
    int phase = 0;  // 0 resolving, 1 connecting, 2 connected
    bool failed = false;
    std::string out, in;
};

#endif

// ---------------------------------------------------------------------------
// Session: banner -> AUTH -> AUTHSIG -> HISCORE/HISCORES -> close.
// ---------------------------------------------------------------------------

struct Session {
    std::unique_ptr<Transport> transport;
    Uint64 deadline = 0;
    bool signIn = false;       // send AUTH (and any pending runs)
    bool fetchBoards = false;
    bool deleteAccount = false;  // DELETEACCOUNT instead of HISCORE/HISCORES
    bool deleted = false;        // ...and the server said OK
    Run submitting[kBoards];
    int awaiting = 0;          // replies still owed
    bool sentRequests = false;
    bool failed = false;
    std::string error;
    WorldBoard gotBoards[kBoards];
    bool gotBoard[kBoards] = {};
    int boardOrder = 0;        // HISCORES replies arrive in the order asked
};

std::unique_ptr<Session> session;

// "FB/1.7 AUTH: CHALLENGE abc" -> payload after "AUTH: ", or "" if the line
// isn't that reply. Server lines are "FB/1.<m> <CMD>: <payload>".
bool ReplyTo(const std::string& line, const char* cmd, std::string& payload) {
    const std::string key = std::string(" ") + cmd + ": ";
    if (line.compare(0, 5, "FB/1.") != 0) return false;
    const size_t at = line.find(key);
    if (at == std::string::npos || line.find(' ') != at) return false;
    payload = line.substr(at + key.size());
    return true;
}

void SendRequests(Session& s) {
    s.sentRequests = true;
    if (s.deleteAccount) {
        s.transport->Send("FB/1.3 DELETEACCOUNT");
        ++s.awaiting;
        return;
    }
    if (s.signIn) {
        const std::string nick = SubmitNick();
        // Shown beside this account's runs; the server keeps the last one it
        // was given, so a session without it changes nothing. Its "COUNTRY:"
        // reply is just not read.
        const std::string& country = GameSettings::Instance()->lastCountry();
        if (!country.empty()) s.transport->Send("FB/1.3 COUNTRY " + country);
        for (int b = 0; b < kBoards; ++b) {
            const Run& r = s.submitting[b];
            if (r.level <= 0) continue;
            s.transport->Send("FB/1.3 HISCORE " + std::to_string(b) + " " + std::to_string(r.level) +
                              " " + std::to_string(r.timeMs) + " " + std::to_string(r.points) +
                              " " + nick + " " + std::to_string(r.shots));
            ++s.awaiting;
        }
    }
    if (s.fetchBoards) {
        for (int b = 0; b < kBoards; ++b) {
            s.transport->Send("FB/1.3 HISCORES " + std::to_string(b));
            ++s.awaiting;
        }
    }
}

void HandleLine(Session& s, const std::string& line) {
    std::string payload;
    if (line.find("SERVER_READY") != std::string::npos && line.compare(0, 5, "FB/1.") == 0) {
        const int minor = std::atoi(line.c_str() + 5);
        if (minor < kMinProtoMinor) {
            s.failed = true;
            s.error = "The online board server needs an update.";
            return;
        }
        if (s.signIn) s.transport->Send("FB/1.3 AUTH " + playeraccount::PublicKeyHex());
        else SendRequests(s);
    } else if (ReplyTo(line, "AUTH", payload)) {
        if (payload.compare(0, 10, "CHALLENGE ") == 0) {
            const std::string sig = playeraccount::SignChallenge(payload.substr(10));
            if (sig.empty()) { s.failed = true; s.error = "Couldn't sign in."; return; }
            s.transport->Send("FB/1.3 AUTHSIG " + sig);
        } else {
            s.failed = true;
            s.error = "Couldn't sign in.";
        }
    } else if (ReplyTo(line, "AUTHSIG", payload)) {
        if (payload.compare(0, 3, "OK ") != 0) {
            s.failed = true;
            s.error = "Couldn't sign in.";
            return;
        }
        SendRequests(s);
    } else if (ReplyTo(line, "HISCORE", payload)) {
        // OK, or INVALID (which a retry would only repeat): either way the
        // server has answered for the oldest submission still owed.
        for (int b = 0; b < kBoards; ++b) {
            if (s.submitting[b].level <= 0) continue;
            if (!Better(b, pending[b], s.submitting[b])) pending[b] = Run{};
            s.submitting[b] = Run{};
            break;
        }
        SavePending();
        --s.awaiting;
    } else if (ReplyTo(line, "DELETEACCOUNT", payload)) {
        --s.awaiting;
        if (payload.compare(0, 2, "OK") == 0) {
            s.deleted = true;
        } else {
            s.failed = true;
            s.error = payload.compare(0, 15, "UNKNOWN_COMMAND") == 0
                ? "The server can't delete accounts yet. Try again after it is updated."
                : "The server couldn't delete the account.";
        }
    } else if (ReplyTo(line, "HISCORES", payload)) {
        const int b = s.boardOrder++;
        if (b < kBoards && s.gotBoards[b].Parse(payload)) s.gotBoard[b] = true;
        --s.awaiting;
    }
}

void Start(bool withBoards, bool deleteAccount = false) {
    const std::string host = Host();
    if (host.empty()) return;
    auto s = std::make_unique<Session>();
    s->deleteAccount = deleteAccount;
    // Deleting needs the account's own signature whatever the setting says,
    // and sends nothing else: unsent runs belong to the account being deleted.
    s->signIn = deleteAccount || SendingEnabled();
    s->fetchBoards = withBoards && !deleteAccount;
    if (s->signIn && !deleteAccount)
        for (int b = 0; b < kBoards; ++b) s->submitting[b] = pending[b];
    const int port = testMode ? testPort : 0;
#ifdef __WASM_PORT__
    s->transport = std::make_unique<WebSocketTransport>(host, port);
#else
    s->transport = std::make_unique<SocketTransport>(host, port > 0 ? port : kNativePort);
#endif
    s->deadline = SDL_GetTicks() + kSessionTimeoutMs;
    if (withBoards) {
        status = Status::Loading;
        lastError.clear();
    }
    session = std::move(s);
}

void Finish() {
    Session& s = *session;
    const bool complete = s.sentRequests && s.awaiting <= 0 && !s.failed;
    if (s.deleteAccount) {
        if (complete && s.deleted) {
            for (auto& p : pending) p = Run{};
            SavePending();
            playeraccount::StartNewAccount();
            deleteStatus = DeleteStatus::Done;
            deleteError.clear();
            // The boards on screen still list the deleted runs.
            status = Status::Idle;
        } else {
            deleteStatus = DeleteStatus::Failed;
            deleteError = !s.error.empty() ? s.error : "Couldn't reach the server to delete the account.";
        }
        session.reset();
        return;
    }
    if (s.fetchBoards) {
        for (int b = 0; b < kBoards; ++b)
            if (s.gotBoard[b]) boards[b] = s.gotBoards[b];
        if (complete) {
            status = Status::Ready;
        } else {
            status = Status::Failed;
            lastError = !s.error.empty() ? s.error : "Couldn't reach the online board.";
        }
    }
    if (!complete) retryAt = SDL_GetTicks() + kRetryAfterMs;
    session.reset();
}

}  // namespace

bool Available() {
    return !Host().empty();
}

bool SendingEnabled() {
    return Available() && GameSettings::Instance()->worldHighscoresEnabled();
}

void RecordRun(int track, int level, int timeMs, int shots) {
    if (track < 0 || track >= kTracks || level <= 0 || timeMs <= 0) return;
    if (!SendingEnabled()) return;
    LoadPending();
    const Run run{level, timeMs, 0, shots > 0 ? shots : 0};
    if (pending[track].level <= 0 || Better(track, run, pending[track])) {
        pending[track] = run;
        SavePending();
    }
}

void RecordLife(int track, int points, int level, int timeMs, int shots) {
    if (track < 0 || track >= kTracks || points <= 0 || level <= 0 || timeMs <= 0) return;
    if (!SendingEnabled()) return;
    LoadPending();
    const int b = BoardIndex(true, track);
    const Run run{level, timeMs, points, shots > 0 ? shots : 0};
    if (pending[b].level <= 0 || Better(b, run, pending[b])) {
        pending[b] = run;
        SavePending();
    }
}

void RequestBoards() {
    if (!Available()) return;
    LoadPending();
    if (session) {
        boardsWanted = true;  // after the one in flight
        status = Status::Loading;
        return;
    }
    Start(true);
}

void RequestDeleteAccount() {
    if (deleteStatus == DeleteStatus::Working) return;
    if (!Available()) {
        deleteStatus = DeleteStatus::Failed;
        deleteError = "This build has no account server.";
        return;
    }
    LoadPending();
    deleteStatus = DeleteStatus::Working;
    deleteError.clear();
    if (session) deleteWanted = true;  // after the one in flight
    else Start(false, true);
}

DeleteStatus DeleteAccountStatus() { return deleteStatus; }
const std::string& DeleteAccountError() { return deleteError; }
void ClearDeleteAccountStatus() {
    if (deleteStatus != DeleteStatus::Working) deleteStatus = DeleteStatus::Idle;
}

Status BoardStatus() { return status; }
const WorldBoard& Board(int board) { return boards[board < 0 || board >= kBoards ? 0 : board]; }
const std::string& LastError() { return lastError; }

std::string SubmitNick() {
    // What fb-server's is_nick_ok() accepts: 1-10 of [A-Za-z0-9_-].
    std::string nick;
    for (const char* c = GameSettings::Instance()->savedNickname; *c && nick.size() < 10; ++c)
        if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') ||
            *c == '_' || *c == '-')
            nick += *c;
    return nick.empty() ? "unnamed" : nick;
}

std::string ShownName() {
    // Sending off means no account is needed, and working out the tag would
    // create one (playeraccount::Code() makes one on first use).
    if (!SendingEnabled()) return SubmitNick();
    const std::string id = playeraccount::AccountIdHex();
    return id.size() >= 4 ? SubmitNick() + "#" + id.substr(0, 4) : SubmitNick();
}

const char* WebUrl() {
    if (testMode || !kWorldScoresUrl || !kWorldScoresUrl[0]) return "";
    return kWorldScoresUrl;
}

void Pump(bool inGame) {
    if (session) {
        std::vector<std::string> lines;
        const bool alive = session->transport->Poll(lines);
        for (const auto& line : lines) {
            HandleLine(*session, line);
            if (session->failed) break;
        }
        const bool done = session->sentRequests && session->awaiting <= 0;
        if (done || session->failed || !alive || SDL_GetTicks() > session->deadline) {
            Finish();
            if (deleteWanted) {
                deleteWanted = false;
                Start(false, true);
            } else if (boardsWanted) {
                boardsWanted = false;
                Start(true);
            }
        }
        return;
    }
    if (inGame || !SendingEnabled()) return;
    LoadPending();
    if (HasPending() && SDL_GetTicks() >= retryAt) Start(false);
}

void SetServerForTest(const std::string& host, int port) {
    testMode = true;
    testHost = host;
    testPort = port;
    pendingLoaded = true;
    for (auto& p : pending) p = Run{};
    retryAt = 0;
    session.reset();
    status = Status::Idle;
    deleteStatus = DeleteStatus::Idle;
    deleteError.clear();
    deleteWanted = false;
}

int PendingCountForTest() {
    int n = 0;
    for (const auto& p : pending) n += p.level > 0;
    return n;
}

}  // namespace worldscores
