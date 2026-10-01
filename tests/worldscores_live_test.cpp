// End to end: the game's worldscores module (src/worldscores.cpp) against a
// real fb-server (protocol 1.7, server/hiscores.h) -- a recorded run goes out
// on its own once the game isn't mid-run, under the device's account, and
// the board comes back with it listed as nick#tag. server_hiscore_test.py
// covers the server's rules; this covers the client's session: banner, AUTH,
// AUTHSIG, HISCORE, HISCORES, parsing, and clearing what was sent.
//
// Usage: worldscores-live-test <path-to-fb-server>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include <SDL3/SDL.h>

#include "gamesettings.h"
#include "playeraccount.h"
#include "worldscores.h"
#include "test_ports.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static bool PumpUntil(bool (*done)(), Uint64 ms) {
    const Uint64 start = SDL_GetTicks();
    while (!done() && SDL_GetTicks() - start < ms) {
        worldscores::Pump(false);
        SDL_Delay(5);
    }
    return done();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <fb-server>\n", argv[0]);
        return 77;
    }
    const int port = FreeTcpPort();
    char dir[] = "/tmp/fb-worldscores-live-XXXXXX";
    if (!mkdtemp(dir)) return 1;
    const std::string base = dir;

    const pid_t server = fork();
    if (server == 0) {
        setenv("FB_SERVER_WEEKLY_FILE", (base + "/weekly.dat").c_str(), 1);
        setenv("FB_SERVER_STATS_FILE", (base + "/stats.dat").c_str(), 1);
        setenv("FB_SERVER_HISCORE_FILE", (base + "/hiscores.dat").c_str(), 1);
        const std::string portArg = std::to_string(port);
        execl(argv[1], argv[1], "-p", portArg.c_str(), "-q", "-z", "-d", (char*)nullptr);
        _exit(127);
    }
    SDL_Init(0);

    CHECK(playeraccount::UseCode("7K3M-9QX2-HD4R-B8TN", false));
    GameSettings* gs = GameSettings::Instance();
    std::snprintf(gs->savedNickname, sizeof(gs->savedNickname), "%s", "dee dee!");  // -> "deedee"
    CHECK(worldscores::SubmitNick() == "deedee");
    const std::string shown = "deedee#" + playeraccount::AccountIdHex().substr(0, 4);
    CHECK(worldscores::ShownName() == shown);

    worldscores::SetServerForTest("127.0.0.1", port);
    CHECK(worldscores::SendingEnabled());

    // Only the best pending run per track is kept.
    // Shots ride with the run they came with and never decide which is kept.
    worldscores::RecordRun(1, 12, 90000, 150);
    worldscores::RecordRun(1, 11, 10000, 20);
    worldscores::RecordRun(1, 12, 80000, 140);
    CHECK(worldscores::PendingCountForTest() == 1);
    // A life's points go to the most-points board for the same track; the
    // life only ever grows, and the biggest total is what's kept.
    worldscores::RecordLife(1, 3000, 5, 40000);
    worldscores::RecordLife(1, 9000, 12, 80000, 140);
    worldscores::RecordLife(1, 0, 1, 1000);  // a life that scored nothing is ignored
    CHECK(worldscores::PendingCountForTest() == 2);
    // Mid-run nothing is sent.
    for (int i = 0; i < 20; ++i) { worldscores::Pump(true); SDL_Delay(5); }
    CHECK(worldscores::PendingCountForTest() == 2);

    // The server may still be starting; a failed attempt waits a minute
    // before retrying on its own, so ask for the board until it answers.
    bool ok = false;
    for (int attempt = 0; attempt < 40 && !ok; ++attempt) {
        worldscores::RequestBoards();
        PumpUntil([] { return worldscores::BoardStatus() != worldscores::Status::Loading; }, 4000);
        ok = worldscores::BoardStatus() == worldscores::Status::Ready;
        if (!ok) SDL_Delay(100);
    }
    CHECK(ok);
    CHECK(worldscores::PendingCountForTest() == 0);
    const WorldBoard& mouse = worldscores::Board(1);
    CHECK(mouse.alltime.size() == 1);
    if (!mouse.alltime.empty()) {
        CHECK(mouse.alltime[0].name == shown);
        CHECK(mouse.alltime[0].level == 12 && mouse.alltime[0].timeMs == 80000);
        CHECK(mouse.alltime[0].shots == 140);
    }
    CHECK(mouse.hasMine && mouse.myAlltime.rank == 1 && mouse.myWeek.rank == 1);
    CHECK(worldscores::Board(0).alltime.empty());
    const WorldBoard& mousePoints = worldscores::Board(worldscores::BoardIndex(true, 1));
    CHECK(mousePoints.alltime.size() == 1);
    if (!mousePoints.alltime.empty())
        CHECK(mousePoints.alltime[0].points == 9000 && mousePoints.alltime[0].level == 12 &&
              mousePoints.alltime[0].shots == 140);
    CHECK(mousePoints.hasMine && mousePoints.myAlltime.points == 9000);
    CHECK(worldscores::Board(worldscores::BoardIndex(true, 0)).alltime.empty());

    // A run recorded later goes out from Pump() alone, outside a game.
    // One without shots (a caller that doesn't count them) still goes up.
    worldscores::RecordRun(0, 30, 400000);
    CHECK(PumpUntil([] { return worldscores::PendingCountForTest() == 0; }, 5000));
    worldscores::RequestBoards();
    PumpUntil([] { return worldscores::BoardStatus() != worldscores::Status::Loading; }, 5000);
    CHECK(worldscores::Board(0).alltime.size() == 1);
    if (!worldscores::Board(0).alltime.empty()) CHECK(worldscores::Board(0).alltime[0].shots == 0);

    // Delete account: the server drops the account's runs, then this device
    // forgets its unsent ones and moves to a new account. StartNewAccount()
    // saves the new code beside the settings, so point those at the temp
    // directory first -- never the developer's real account file.
    std::string prefDir = base + "/";
    gs->prefPath = prefDir.c_str();
    const std::string oldId = playeraccount::AccountIdHex();
    worldscores::RecordRun(0, 31, 1000);
    CHECK(worldscores::PendingCountForTest() == 1);
    worldscores::RequestDeleteAccount();
    CHECK(worldscores::DeleteAccountStatus() == worldscores::DeleteStatus::Working);
    PumpUntil([] { return worldscores::DeleteAccountStatus() != worldscores::DeleteStatus::Working; }, 5000);
    CHECK(worldscores::DeleteAccountStatus() == worldscores::DeleteStatus::Done);
    CHECK(worldscores::DeleteAccountError().empty());
    CHECK(playeraccount::AccountIdHex() != oldId);
    CHECK(worldscores::PendingCountForTest() == 0);
    worldscores::ClearDeleteAccountStatus();
    CHECK(worldscores::DeleteAccountStatus() == worldscores::DeleteStatus::Idle);
    worldscores::RequestBoards();
    PumpUntil([] { return worldscores::BoardStatus() != worldscores::Status::Loading; }, 5000);
    for (int b = 0; b < worldscores::kBoards; ++b) CHECK(worldscores::Board(b).alltime.empty());

    kill(server, SIGKILL);
    waitpid(server, nullptr, 0);
    for (const char* f : {"/weekly.dat", "/stats.dat", "/hiscores.dat", "/account.txt"})
        unlink((base + f).c_str());
    rmdir(dir);
    SDL_Quit();
    if (failures) return 1;
    std::printf("worldscores live: all checks passed\n");
    return 0;
}
