// The modern in-game theme (GameSettings::modernTheme(), BubbleGame::
// UsesModernHud()) draws a 1-player game's screens headlessly without
// tripping over anything, and its lost card hands real button rects to the
// tap hit-test (HandleFinishedTap) the way RenderContinuePrompt() does in the
// original theme -- a card drawn without them would leave CONTINUE / START
// OVER keyboard-only.
//
// The 1-player pause button (ShowsPauseButton) is checked here too.
//
// Set FB_DUMP_DIR to a directory to also get each screen as a PNG.

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "bubblegame.h"
#include "bubblegame_internal.h"
#include "gamesettings.h"
#include "mainmenu.h"
#include "platform.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

static int failures = 0;
#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                     __FILE__, __LINE__, #expression); \
        ++failures; \
    } \
} while (false)

struct BubbleGameTestAccess {
    static bool skipHintPending(const BubbleGame& g) { return g.skipShotHintPending; }
    // Shown one second ago: past the fade-in, so the dump shows it at full strength.
    static void skipHintShownFor(BubbleGame& g, Uint64 ms) { g.skipShotHintStartMs = SDL_GetTicks() - ms; }
    static void ready(BubbleGame& game) {
        // Skip the level transition (a blocking effect) and the drop-in, so
        // the frame shows the whole board.
        game.firstRenderDone = true;
        game.levelIntroStartMs = 1;
    }
    static void draw(BubbleGame& game) { game.Draw(); }
    static void paused(BubbleGame& game) { game.RenderPaused(); game.modernPauseStartMs = 1; game.RenderPaused(); }
    static void midDrop(BubbleGame& game) { game.levelIntroStartMs = SDL_GetTicks() - 200; }
    static bool modern(const BubbleGame& game) { return game.UsesModernHud(); }
    static bool pauseHit(const BubbleGame& game, float x, float y) { return game.PauseButtonHit(x, y); }
    static BubbleArray& player(BubbleGame& game) { return game.bubbleArrays[0]; }
    // A swap press's slides, frozen `ms` into their run.
    static void pocketSlides(BubbleGame& g, bool fire, Uint64 ms) {
        BubbleArray& b = g.bubbleArrays[0];
        g.pocketSlides.clear();
        const SDL_Rect pocket = BubbleGame::PocketRect(b);
        if (fire) {
            g.StartPocketSlide(0, b.curLaunch, pocket, b.curLaunchRct, BubbleGame::PocketSlide::kNone, true);
            g.StartPocketSlide(0, b.pocketColor, b.curLaunchRct, pocket, BubbleGame::PocketSlide::kPocket);
        } else {
            g.StartPocketSlide(0, b.pocketColor, b.curLaunchRct, pocket, BubbleGame::PocketSlide::kPocket);
            g.StartPocketSlide(0, b.curLaunch, b.nextBubbleRct, b.curLaunchRct, BubbleGame::PocketSlide::kLauncher);
        }
        for (auto& s : g.pocketSlides) s.startMs = SDL_GetTicks() - ms;
    }
    static size_t slideCount(const BubbleGame& g) { return g.pocketSlides.size(); }
    // The swap shot indicator, `ms` after the shot left the launcher.
    static void swapTag(BubbleGame& g, Uint64 ms) {
        g.swapTagFrom[0] = g.bubbleArrays[0].curLaunchRct;
        g.swapTagStartMs[0] = SDL_GetTicks() - ms;
    }
    static bool swapTagShowing(const BubbleGame& g) { return g.swapTagStartMs[0] != 0; }
    static bool showsPocket(const BubbleGame& g, int i) { return g.ShowsPocket(g.bubbleArrays[i]); }
    static BubbleArray& board(BubbleGame& g, int i) { return g.bubbleArrays[i]; }
    static void swapTagOn(BubbleGame& g, int i, Uint64 ms) {
        g.swapTagFrom[i] = g.bubbleArrays[i].curLaunchRct;
        g.swapTagStartMs[i] = SDL_GetTicks() - ms;
    }
    static void finish(BubbleGame& game, bool won, bool prompt) {
        game.gameFinish = true;
        game.gameWon = won;
        game.gameLost = !won;
        game.continuePrompt = prompt;
        game.modernCardStartMs = 1;  // fully in
    }
    static void unfinish(BubbleGame& game) {
        game.gameFinish = game.gameWon = game.gameLost = game.continuePrompt = false;
    }
    static void setRun(BubbleGame& game, int shots) { game.runShots = shots; }
    static void popups(BubbleGame& game) {
        game.scorePopups.push_back({300, 200, 120, 10});
        game.scorePopups.push_back({318, 236, 0, 8, 5});
    }
    static SDL_Rect continueBtn(const BubbleGame& game) { return game.continueBtnRect; }
    static SDL_Rect startOverBtn(const BubbleGame& game) { return game.startOverBtnRect; }
    static void clearButtons(BubbleGame& game) { game.continueBtnRect = game.startOverBtnRect = {}; }
};

// The 1-player menu, with its In-game theme and Bubbles rows.
struct MainMenuTestAccess {
    static void DrawSP(const SDL_Renderer* renderer, int row) {
        MainMenu menu(renderer, MainMenu::HeadlessTestTag{});
        menu.showingSPPanel = true;
        menu.activeSPIdx = row;
        menu.SPPanelRender();
    }
};

static void Dump(SDL_Renderer* rend, const char* name) {
    const char* dir = SDL_getenv("FB_DUMP_DIR");
    if (!dir) return;
    SDL_Surface* sfc = SDL_RenderReadPixels(rend, nullptr);
    if (!sfc) return;
    IMG_SavePNG(sfc, (std::string(dir) + "/" + name + ".png").c_str());
    SDL_DestroySurface(sfc);
}

static bool OnScreen(const SDL_Rect& r) {
    return r.w > 0 && r.h > 0 && r.x >= 0 && r.y >= 0 && r.x + r.w <= 640 && r.y + r.h <= 480;
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow("modern-theme-render-test", 640, 480, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }

    // Never the developer's real settings.ini (see CLAUDE.md).
    const auto dir = std::filesystem::temp_directory_path() /
        ("frozen-bubble-modern-theme-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    const std::string prefPath = dir.string() + "/";
    GameSettings* settings = GameSettings::Instance();
    settings->prefPath = prefPath.c_str();
    settings->ReadSettings();
    CHECK(settings->modernTheme());  // on by default

    {
        BubbleGame game(renderer);
        SetupSettings setup;
        setup.playerCount = 1;
        setup.startLevel = 1;
        game.NewGame(setup);
        CHECK(BubbleGameTestAccess::modern(game));
        BubbleGameTestAccess::ready(game);

        BubbleGameTestAccess::player(game).score = 1234;
        BubbleGameTestAccess::setRun(game, 17);
        BubbleGameTestAccess::popups(game);
        // The Swap hint comes with every run.
        CHECK(BubbleGameTestAccess::skipHintPending(game));
        BubbleGameTestAccess::draw(game);
        BubbleGameTestAccess::skipHintShownFor(game, 1000);
        BubbleGameTestAccess::draw(game);
        Dump(renderer, "1d-skip-hint");
        CHECK(game.SkipShotHintLine().find("Right Shift") != std::string::npos);
        CHECK(game.SkipShotHintLine().find("Swap") == 0);
        BubbleGameTestAccess::skipHintShownFor(game, BubbleGame::kSkipShotHintMs);
        for (int i = 0; i < 40; ++i) BubbleGameTestAccess::draw(game);  // let the score count up
        CHECK(!BubbleGameTestAccess::skipHintPending(game));   // timed out
        Dump(renderer, "1-playing");
        // A bubble in the swap pocket, beside the next one.
        BubbleGameTestAccess::player(game).pocketColor = 3;
        BubbleGameTestAccess::draw(game);
        Dump(renderer, "1e-pocket");
        // Pocketing, part way: the loaded bubble on its way down-left into the
        // pocket, the next one on its way up into the launcher.
        for (Uint64 ms : {40, 90}) {
            BubbleGameTestAccess::pocketSlides(game, false, ms);
            BubbleGameTestAccess::draw(game);
            Dump(renderer, ("1f-pocketing-" + std::to_string(ms)).c_str());
        }
        // Firing the pocket: the pocketed bubble darts up into the shot.
        for (Uint64 ms : {40, 90}) {
            BubbleGameTestAccess::pocketSlides(game, true, ms);
            BubbleGameTestAccess::draw(game);
            Dump(renderer, ("1g-pocket-fire-" + std::to_string(ms)).c_str());
        }
        BubbleGameTestAccess::pocketSlides(game, false, BubbleGame::kPocketSlideMs);
        BubbleGameTestAccess::draw(game);
        CHECK(BubbleGameTestAccess::slideCount(game) == 0);   // over, and dropped
        // A swap shot in flight: the bubble from the pocket ringed, "SWAP!"
        // rising off the launcher.
        {
            BubbleArray& b = BubbleGameTestAccess::player(game);
            SingleBubble shot{};
            shot.assignedArray = 0;
            shot.bubbleId = 3;
            shot.pos = {b.curLaunchRct.x - 90, b.curLaunchRct.y - 90};
            shot.bubbleSize = 32;
            shot.launching = true;
            shot.swapShot = true;
            singleBubbles.push_back(shot);
            BubbleGameTestAccess::swapTag(game, 250);
            BubbleGameTestAccess::draw(game);
            Dump(renderer, "1h-swap-shot");
            singleBubbles.clear();
            BubbleGameTestAccess::swapTag(game, BubbleGame::kSwapTagMs);
            BubbleGameTestAccess::draw(game);
            CHECK(!BubbleGameTestAccess::swapTagShowing(game));   // over
        }
        BubbleGameTestAccess::player(game).pocketColor = -1;

        // The next run shows it again (user decision: no "learned" switch).
        {
            BubbleGame again(renderer);
            again.NewGame(setup);
            CHECK(BubbleGameTestAccess::skipHintPending(again));
        }

        // The pause button, top right; the board itself is not part of it.
        CHECK(BubbleGameTestAccess::pauseHit(game, 616, 24));
        CHECK(!BubbleGameTestAccess::pauseHit(game, 318, 200));

        BubbleGameTestAccess::paused(game);
        Dump(renderer, "1b-paused");
        BubbleGameTestAccess::midDrop(game);
        BubbleGameTestAccess::draw(game);
        Dump(renderer, "1c-dropping");
        BubbleGameTestAccess::ready(game);

        BubbleGameTestAccess::finish(game, true, false);
        BubbleGameTestAccess::draw(game);
        Dump(renderer, "2-cleared");
        // Gone once the level is over: a tap there answers the card instead.
        CHECK(!BubbleGameTestAccess::pauseHit(game, 616, 24));

        // Lost, before the prompt: no buttons, so no tap targets.
        BubbleGameTestAccess::unfinish(game);
        BubbleGameTestAccess::clearButtons(game);
        BubbleGameTestAccess::finish(game, false, false);
        BubbleGameTestAccess::draw(game);
        CHECK(BubbleGameTestAccess::continueBtn(game).w == 0);
        Dump(renderer, "3-lost");

        // The CONTINUE? prompt: the card's two buttons become the tap targets.
        BubbleGameTestAccess::finish(game, false, true);
        BubbleGameTestAccess::draw(game);
        const SDL_Rect cont = BubbleGameTestAccess::continueBtn(game);
        const SDL_Rect over = BubbleGameTestAccess::startOverBtn(game);
        CHECK(OnScreen(cont));
        CHECK(OnScreen(over));
        CHECK(cont.x + cont.w <= over.x);  // side by side, CONTINUE on the left
        Dump(renderer, "4-continue");

        // Modern screens with the classic bubbles.
        settings->SetValue("GFX:ModernBubbles", "");
        CHECK(!settings->modernBubbles());
        CHECK(BubbleGameTestAccess::modern(game));
        BubbleGameTestAccess::unfinish(game);
        BubbleGameTestAccess::draw(game);
        Dump(renderer, "6-modern-classic-bubbles");
        settings->SetValue("GFX:ModernBubbles", "");
        BubbleGameTestAccess::finish(game, false, true);
        BubbleGameTestAccess::draw(game);

        // Original theme: the same prompt still sets its own rects.
        settings->SetValue("GFX:ModernTheme", "");
        CHECK(!settings->modernTheme());
        CHECK(!BubbleGameTestAccess::modern(game));
        BubbleGameTestAccess::clearButtons(game);
        BubbleGameTestAccess::draw(game);
        CHECK(OnScreen(BubbleGameTestAccess::continueBtn(game)));
        Dump(renderer, "5-original-continue");
        settings->SetValue("GFX:ModernTheme", "");
        CHECK(settings->modernTheme());
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    MainMenuTestAccess::DrawSP(renderer, kSPRowBubbles);
    Dump(renderer, "7-sp-menu");

    // Local multiplayer keeps the original screens: only the bubbles change.
    {
        BubbleGame game(renderer);
        SetupSettings setup;
        setup.playerCount = 2;
        setup.localMultiplayer = true;
        setup.randomLevels = true;
        game.NewGame(setup);
        CHECK(!BubbleGameTestAccess::modern(game));
        CHECK(!BubbleGameTestAccess::pauseHit(game, 616, 24));  // no pause button either
    }

    // Local multiplayer: every human board has a swap pocket, the small side
    // boards too (without the SWAP tag); a bot's board has none.
    {
        BubbleGame game(renderer);
        SetupSettings setup;
        setup.playerCount = 4;
        setup.localMultiplayer = true;
        setup.randomLevels = true;
        setup.playerIsBot[3] = true;
        game.NewGame(setup);
        BubbleGameTestAccess::ready(game);
        for (int i = 0; i < 3; ++i) CHECK(BubbleGameTestAccess::showsPocket(game, i));
        CHECK(!BubbleGameTestAccess::showsPocket(game, 3));
        BubbleGameTestAccess::board(game, 0).pocketColor = 3;
        BubbleGameTestAccess::board(game, 1).pocketColor = 5;
        BubbleGameTestAccess::swapTagOn(game, 2, 250);
        BubbleGameTestAccess::draw(game);
        Dump(renderer, "8-local-pockets");
    }
    {
        BubbleGame game(renderer);
        SetupSettings setup;
        setup.playerCount = 2;
        setup.localMultiplayer = true;
        setup.randomLevels = true;
        game.NewGame(setup);
        BubbleGameTestAccess::ready(game);
        CHECK(BubbleGameTestAccess::showsPocket(game, 0) && BubbleGameTestAccess::showsPocket(game, 1));
        BubbleGameTestAccess::board(game, 1).pocketColor = 2;
        BubbleGameTestAccess::draw(game);
        Dump(renderer, "8b-local-2p-pockets");
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    if (failures) std::fprintf(stderr, "%d check(s) failed\n", failures);
    return failures ? 1 : 0;
}
