// High Scores row details: a tap on an entry, or SPACE (a pad's Y) and the
// arrows, highlights it and draws its whole record in a strip under the
// list; SPACE / ENTER / ESC or a tap off the rows put it away, and only then
// does a stray tap or ESC leave the screen. A score just named is selected
// so its details show straight away. Covers MY SCORES' grid and the world
// view's two columns (boards injected, no server).
// Set FB_DUMP_DIR to a directory to also get each screen as a PNG.

#include "frozenbubble.h"
#include "gamesettings.h"
#include "highscoremanager.h"
#include "menulist.h"
#include "platform.h"
#include "worldboard.h"
#include "worldscores.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

struct HighscoreManagerTestAccess {
    static bool selecting(const HighscoreManager& m) { return m.rowSelect; }
    static int index(const HighscoreManager& m) { return m.selIndex; }
    static int column(const HighscoreManager& m) { return m.selColumn; }
    static void world(HighscoreManager& m, bool on, bool points) {
        m.viewWorld = on;
        m.viewPoints = points;
        m.rowSelect = false;
    }
};
using A = HighscoreManagerTestAccess;

static void Key(HighscoreManager* m, SDL_Keycode k) {
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = k;
    m->HandleInput(&e);
}

// A left click at canvas (x, y); the window is 640x480, so window = canvas.
static void Click(HighscoreManager* m, float x, float y) {
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = x;
    e.button.y = y;
    m->HandleInput(&e);
}

static void Dump(SDL_Renderer* rend, const char* name) {
    const char* dir = SDL_getenv("FB_DUMP_DIR");
    if (!dir) return;
    SDL_Surface* sfc = SDL_RenderReadPixels(rend, nullptr);
    if (!sfc) return;
    IMG_SavePNG(sfc, (std::string(dir) + "/" + name + ".png").c_str());
    SDL_DestroySurface(sfc);
}

static void Draw(SDL_Renderer* rend, HighscoreManager* m, const char* name) {
    SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
    SDL_RenderClear(rend);
    m->RenderScoreScreen();
    Dump(rend, name);
}

// Whether pixel (x, y) of the last frame is the details strip's gold edge.
static bool IsGold(SDL_Renderer* rend, int x, int y) {
    SDL_Rect r{x, y, 1, 1};
    SDL_Surface* s = SDL_RenderReadPixels(rend, &r);
    if (!s) return false;
    Uint8 cr = 0, cg = 0, cb = 0, ca = 0;
    SDL_ReadSurfacePixel(s, 0, 0, &cr, &cg, &cb, &ca);
    SDL_DestroySurface(s);
    return cr == menulist::kGold.r && cg == menulist::kGold.g && cb == menulist::kGold.b;
}

static bool OnHighscores() { return FrozenBubble::Instance()->currentState == Highscores; }

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_AUDIO_DRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow("highscore-details-test", 640, 480, SDL_WINDOW_HIDDEN);
    SDL_Renderer* rend = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!rend) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }

    // Never the developer's real settings.ini or tables (see CLAUDE.md).
    const auto dir = std::filesystem::temp_directory_path() /
        ("frozen-bubble-highscore-details-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    const std::string prefPath = dir.string() + "/";
    GameSettings* settings = GameSettings::Instance();
    settings->prefPath = prefPath.c_str();
    settings->ReadSettings();
    settings->setScoreTracks(3);

    // This renderer's, before FrozenBubble makes its own.
    HighscoreManager* m = HighscoreManager::Instance(rend);
    FrozenBubble::Instance();
    // Each named as it would be after its run, so only the last is pending.
    auto addNamed = [&](int level, float time, HighscoreManager::InputMethod method, int shots,
                        const char* name) {
        CHECK(m->CheckAndAddScore(level, time, method, shots));
        m->ShowNewScorePanel(0);
        SDL_Event text{};
        text.type = SDL_EVENT_TEXT_INPUT;
        text.text.text = name;
        m->HandleInput(&text);
        Key(m, SDLK_RETURN);
    };
    addNamed(30, 400.f, HighscoreManager::InputMethod::Keyboard, 512, "alice");
    addNamed(22, 300.f, HighscoreManager::InputMethod::Mouse, 340, "bob");
    addNamed(12, 90.f, HighscoreManager::InputMethod::Keyboard, 0, "averylongname");

    // -- MY SCORES ------------------------------------------------------
    m->ShowScoreScreen(0);
    Draw(rend, m, "details-local-none");
    CHECK(!A::selecting(*m));
    CHECK(!IsGold(rend, 40, 427));

    // SPACE picks the first entry; the arrows move along the grid.
    Key(m, SDLK_SPACE);
    CHECK(A::selecting(*m) && A::index(*m) == 0);
    Key(m, SDLK_RIGHT);
    Key(m, SDLK_RIGHT);
    CHECK(A::index(*m) == 2);
    Key(m, SDLK_RIGHT);  // only three entries
    CHECK(A::index(*m) == 2);
    Key(m, SDLK_DOWN);   // no second row
    CHECK(A::index(*m) == 2);
    CHECK(OnHighscores());
    Draw(rend, m, "details-local-selected");
    CHECK(IsGold(rend, 40, 427));  // the strip's edge
    // ESC puts the details away; it doesn't leave the screen.
    Key(m, SDLK_ESCAPE);
    CHECK(!A::selecting(*m) && OnHighscores());

    // A pad's Y arrives as F1 in menus and opens the details too.
    Key(m, SDLK_F1);
    CHECK(A::selecting(*m));
    Key(m, SDLK_F1);
    CHECK(!A::selecting(*m));

    // A click on the second thumbnail selects it.
    Click(m, 105 * 1 + 85 + 30, 120);
    CHECK(A::selecting(*m) && A::index(*m) == 1);
    // A click on empty space puts the details away; a second goes back.
    Click(m, 320, 300);
    CHECK(!A::selecting(*m) && OnHighscores());
    Click(m, 320, 300);
    CHECK(!OnHighscores());

    // With details showing, the Back button still goes back.
    m->ShowScoreScreen(0);
    Click(m, 105 * 0 + 85 + 30, 120);
    CHECK(A::selecting(*m));
    Click(m, menulist::kBackBtn.x + 10, menulist::kBackBtn.y + 10);
    CHECK(!OnHighscores());

    // Leaving and coming back starts with nothing selected.
    m->ShowScoreScreen(0);
    CHECK(!A::selecting(*m));

    // A score just named is selected once its panel closes.
    CHECK(m->CheckAndAddScore(25, 200.f, HighscoreManager::InputMethod::Keyboard, 99));
    m->ShowNewScorePanel(0);
    SDL_Event text{};
    text.type = SDL_EVENT_TEXT_INPUT;
    text.text.text = "newbie";
    m->HandleInput(&text);
    Draw(rend, m, "details-new-score-panel");
    Key(m, SDLK_RETURN);
    CHECK(A::selecting(*m));
    CHECK(A::index(*m) == 1);  // levels 30, 25, 22, 12
    Draw(rend, m, "details-local-new");

    // -- world view -----------------------------------------------------
    if (worldscores::Available()) {
        WorldBoard kb, ms;
        CHECK(kb.Parse("1790380800 alpha#1a2b=40/300000/0/DE,bravo#3c4d=20/100000/0 "
                       "bravo#3c4d=20/100000/0 - 20743,0 20743 412,0 88"));
        CHECK(ms.Parse("1790380800 charlie#5e6f=30/200000/0/JP - - 20740 - 260 -"));
        worldscores::SetBoardForTest(worldscores::BoardIndex(false, 0), kb);
        worldscores::SetBoardForTest(worldscores::BoardIndex(false, 1), ms);
        m->ShowScoreScreen(0);
        A::world(*m, true, false);
        Draw(rend, m, "details-world-none");

        Key(m, SDLK_SPACE);
        CHECK(A::selecting(*m) && A::column(*m) == 0 && A::index(*m) == 0);
        Key(m, SDLK_DOWN);
        Key(m, SDLK_DOWN);
        CHECK(A::index(*m) == 2);  // alpha, charlie, bravo
        Key(m, SDLK_DOWN);
        CHECK(A::index(*m) == 2);
        Key(m, SDLK_RIGHT);        // this week has one entry
        CHECK(A::column(*m) == 1 && A::index(*m) == 0);
        Key(m, SDLK_LEFT);
        CHECK(A::column(*m) == 0);
        Draw(rend, m, "details-world-selected");
        CHECK(IsGold(rend, 22, 339));
        Key(m, SDLK_RETURN);       // ENTER closes the details, not a button
        CHECK(!A::selecting(*m) && OnHighscores());

        // A click on the week column's first row.
        Click(m, 22 + 304 + 100, 102 + 5);
        CHECK(A::selecting(*m) && A::column(*m) == 1 && A::index(*m) == 0);
        // Switching board puts the details away.
        Click(m, 320, 50);  // the scope tabs' middle one, POINTS
        CHECK(!A::selecting(*m));
    }

    if (failures) {
        std::fprintf(stderr, "highscore details: %d check(s) failed\n", failures);
        return 1;
    }
    std::printf("highscore details: all checks passed\n");
    return 0;
}
