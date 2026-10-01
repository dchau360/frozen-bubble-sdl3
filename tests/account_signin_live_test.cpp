// End to end: the game's own NetworkClient signs in to an account against a
// real fb-server (protocol 1.6, server/account.h). Unit tests cover each half
// -- playeraccount_test.cpp the signing, server_account_test.py the checking
// -- this covers the wiring between them: AUTH going out on connect, the
// challenge being answered from HandleServerResponse, and the account id the
// server hands back landing in NetworkClient::accountId, all while NICK
// completes normally around it.
//
// Usage: account-signin-live-test <path-to-fb-server>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include <SDL3/SDL.h>

#include "../third_party/monocypher/monocypher.h"
#include "networkclient.h"
#include "playeraccount.h"
#include "test_ports.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <fb-server>\n", argv[0]);
        return 77;
    }
    const int port = FreeTcpPort();
    char weeklyFile[] = "/tmp/fb-account-live-XXXXXX";
    const int tmpfd = mkstemp(weeklyFile);
    if (tmpfd >= 0) close(tmpfd);
    unlink(weeklyFile);

    const pid_t server = fork();
    if (server == 0) {
        setenv("FB_SERVER_WEEKLY_FILE", weeklyFile, 1);
        setenv("FB_SERVER_STATS_FILE", (std::string(weeklyFile) + ".stats").c_str(), 1);
        const std::string portArg = std::to_string(port);
        execl(argv[1], argv[1], "-p", portArg.c_str(), "-q", "-z", "-d", (char*)nullptr);
        _exit(127);
    }
    SDL_Init(0);

    // A fixed code, not saved anywhere, so the run leaves no account behind.
    CHECK(playeraccount::UseCode("7K3M-9QX2-HD4R-B8TN", false));
    uint8_t pk[32], id[8];
    const std::string pkHex = playeraccount::PublicKeyHex();
    for (int i = 0; i < 32; ++i) pk[i] = (uint8_t)std::strtoul(pkHex.substr(i * 2, 2).c_str(), nullptr, 16);
    crypto_blake2b(id, sizeof(id), pk, sizeof(pk));
    char expected[17];
    for (int i = 0; i < 8; ++i) std::snprintf(expected + i * 2, 3, "%02x", id[i]);

    NetworkClient* nc = NetworkClient::Instance();
    bool connected = false;
    for (int attempt = 0; attempt < 50 && !connected; ++attempt) {
        if (nc->Connect("127.0.0.1", port)) {
            const Uint64 start = SDL_GetTicks();
            while (nc->IsConnecting() && SDL_GetTicks() - start < 2000) {
                nc->Update();
                SDL_Delay(5);
            }
            connected = nc->GetState() == CONNECTED;
        }
        if (!connected) {
            nc->Disconnect();
            SDL_Delay(100);  // server still starting
        }
    }
    CHECK(connected);

    CHECK(nc->SendNick("alice"));
    const Uint64 start = SDL_GetTicks();
    while ((nc->IsPendingNick() || nc->accountId.empty()) && SDL_GetTicks() - start < 5000) {
        nc->Update();
        SDL_Delay(10);
    }
    CHECK(!nc->IsPendingNick());
    CHECK(nc->GetPlayerNick() == "alice");
    CHECK(nc->accountId == expected);
    CHECK(nc->AccountTag() == std::string(expected, 4));
    if (nc->accountId != expected)
        std::fprintf(stderr, "  accountId='%s' expected='%s'\n", nc->accountId.c_str(), expected);

    // Per-connection: a disconnect forgets it, so a different server can't
    // be shown this one's id.
    nc->Disconnect();
    CHECK(nc->accountId.empty());

    kill(server, SIGKILL);
    waitpid(server, nullptr, 0);
    unlink(weeklyFile);
    unlink((std::string(weeklyFile) + ".stats").c_str());
    SDL_Quit();
    if (failures) return 1;
    std::printf("account sign-in: all checks passed\n");
    return 0;
}
