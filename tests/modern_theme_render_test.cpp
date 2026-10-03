// The modern in-game theme (GameSettings::modernTheme(), BubbleGame::
// UsesModernHud()) draws a 1-player game's screens headlessly without
// tripping over anything, and its lost card hands real button rects to the
// tap hit-test (HandleFinishedTap) the way RenderContinuePrompt() does in the
// original theme -- a card drawn without them would leave CONTINUE / START
// OVER keyboard-only.
//
// Set FB_DUMP_DIR to a directory to also get each screen as a PNG.

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "bubblegame.h"
#include "gamesettings.h"
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
    static BubbleArray& player(BubbleGame& game) { return game.bubbleArrays[0]; }
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
        for (int i = 0; i < 40; ++i) BubbleGameTestAccess::draw(game);  // let the score count up
        Dump(renderer, "1-playing");

        BubbleGameTestAccess::paused(game);
        Dump(renderer, "1b-paused");
        BubbleGameTestAccess::midDrop(game);
        BubbleGameTestAccess::draw(game);
        Dump(renderer, "1c-dropping");
        BubbleGameTestAccess::ready(game);

        BubbleGameTestAccess::finish(game, true, false);
        BubbleGameTestAccess::draw(game);
        Dump(renderer, "2-cleared");

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

    // Local multiplayer keeps the original screens: only the bubbles change.
    {
        BubbleGame game(renderer);
        SetupSettings setup;
        setup.playerCount = 2;
        setup.localMultiplayer = true;
        setup.randomLevels = true;
        game.NewGame(setup);
        CHECK(!BubbleGameTestAccess::modern(game));
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
