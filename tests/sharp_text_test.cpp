// Font text drawn at the screen's resolution (SetTextRenderScale, ttftext.h)
// lands in the same 640x480 rect it did when it was stretched: callers place
// text by TTFText::Coords() and RenderRingedText's outSize, so those must stay
// in canvas pixels while the texture behind them grows. A shared font must
// also come back at its own size, since the menu theme and other TTFTexts
// render from the same handle.
//
// Set FB_DUMP_DIR to a directory to also get a 1-player screen on a 3x
// (1920x1440) output, stretched and sharp, as PNGs.

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "bubblegame.h"
#include "gamesettings.h"
#include "platform.h"
#include "ttftext.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
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
        game.firstRenderDone = true;
        game.levelIntroStartMs = 1;
    }
    static void draw(BubbleGame& game) { game.Draw(); }
    static BubbleArray& player(BubbleGame& game) { return game.bubbleArrays[0]; }
    static void popups(BubbleGame& game) {
        game.scorePopups.push_back({300, 200, 120, 10});
        game.scorePopups.push_back({318, 236, 0, 8, 5});
    }
};

// Close enough: a font hinted at 3x its size is not exactly 3x as wide.
static bool Near(int a, int b, int slack) { return std::abs(a - b) <= slack; }

static SDL_Point TextureSize(SDL_Texture* tex) {
    float w = 0, h = 0;
    if (tex) SDL_GetTextureSize(tex, &w, &h);
    return {(int)w, (int)h};
}

static void Dump(SDL_Renderer* rend, const char* name) {
    const char* dir = SDL_getenv("FB_DUMP_DIR");
    if (!dir) return;
    SDL_Surface* sfc = SDL_RenderReadPixels(rend, nullptr);
    if (!sfc) return;
    IMG_SavePNG(sfc, (std::string(dir) + "/" + name + ".png").c_str());
    SDL_DestroySurface(sfc);
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow("sharp-text-test", 1920, 1440, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) {
        std::fprintf(stderr, "headless renderer setup failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    const std::string display = ASSET("/gfx/Baloo2-ExtraBold.ttf");
    const std::string body = ASSET("/gfx/DroidSans.ttf");

    CHECK(TextRenderScale() == 1.f);  // nothing set: the old stretched text
    SetTextRenderScale(0.5f);
    CHECK(TextRenderScale() == 1.f);  // never smaller than the canvas

    // A TTFText keeps its canvas size and gets a bigger texture.
    {
        TTFText t;
        t.LoadFont(display.c_str(), 22);
        t.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 110});
        SetTextRenderScale(1.f);
        t.UpdateText(renderer, "48,210", 0);
        const SDL_Rect at1 = *t.Coords();
        CHECK(TextureSize(t.Texture()).x == at1.w);

        SetTextRenderScale(3.f);
        t.UpdateText(renderer, "48,210", 0);  // same text: redrawn for the new scale
        const SDL_Rect at3 = *t.Coords();
        CHECK(Near(at3.w, at1.w, 2));
        CHECK(Near(at3.h, at1.h, 2));
        CHECK(Near(TextureSize(t.Texture()).x, at3.w * 3, 3));
        CHECK(Near(TextureSize(t.Texture()).y, at3.h * 3, 3));
    }

    // Wrapped text breaks in the same places: as tall at 3x as at 1x.
    {
        const char* para = "Score goes back to 0; the clock keeps running. Start over to begin a new run.";
        TTFText t;
        t.LoadFont(body.c_str(), 12);
        t.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 110});
        SetTextRenderScale(1.f);
        t.UpdateText(renderer, para, 200);
        const SDL_Rect at1 = *t.Coords();
        SetTextRenderScale(3.f);
        t.UpdateText(renderer, para, 200);
        CHECK(Near(t.Coords()->h, at1.h, 3));
        CHECK(t.Coords()->w <= 202);
    }

    // A shared font is back at its own size after a scaled render, and a
    // ringed label keeps its canvas size.
    {
        TTF_Font* shared = TTF_OpenFont(display.c_str(), 20);
        CHECK(shared != nullptr);
        TTFText a;
        a.LoadFont(shared);
        a.UpdateRing({6, 20, 40, 230}, 2);
        a.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 0});
        SetTextRenderScale(1.f);
        a.UpdateText(renderer, "+120", 0);
        const SDL_Rect at1 = *a.Coords();
        SDL_Point ring1{};
        SDL_Texture* label1 = RenderRingedText(renderer, shared, "START", {255, 255, 255, 255},
                                               {41, 22, 8, 235}, 2, &ring1);

        SetTextRenderScale(4.5f);
        a.UpdateText(renderer, "+120", 0);
        CHECK(TTF_GetFontSize(shared) == 20.f);
        CHECK(Near(a.Coords()->w, at1.w, 2));
        CHECK(Near(a.Coords()->h, at1.h, 2));
        SDL_Point ring4{};
        SDL_Texture* label4 = RenderRingedText(renderer, shared, "START", {255, 255, 255, 255},
                                               {41, 22, 8, 235}, 2, &ring4);
        CHECK(TTF_GetFontSize(shared) == 20.f);
        CHECK(Near(ring4.x, ring1.x, 2));
        CHECK(Near(ring4.y, ring1.y, 2));
        CHECK(Near(TextureSize(label4).x, (int)(ring4.x * 4.5f), 5));
        SDL_DestroyTexture(label1);
        SDL_DestroyTexture(label4);
        TTF_CloseFont(shared);
    }

    // A real 1-player screen, stretched and sharp, for looking at.
    if (SDL_getenv("FB_DUMP_DIR")) {
        const auto dir = std::filesystem::temp_directory_path() /
            ("frozen-bubble-sharp-text-test-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(dir);
        const std::string prefPath = dir.string() + "/";
        GameSettings* settings = GameSettings::Instance();
        settings->prefPath = prefPath.c_str();  // never the developer's settings.ini
        settings->ReadSettings();
        for (float scale : {1.f, 3.f}) {
            SetTextRenderScale(scale);
            BubbleGame game(renderer);
            SetupSettings setup;
            setup.playerCount = 1;
            setup.startLevel = 1;
            game.NewGame(setup);
            BubbleGameTestAccess::ready(game);
            BubbleGameTestAccess::player(game).score = 48210;
            BubbleGameTestAccess::popups(game);
            for (int i = 0; i < 40; ++i) BubbleGameTestAccess::draw(game);
            Dump(renderer, scale == 1.f ? "text-stretched" : "text-sharp");
        }
        std::filesystem::remove_all(dir);
    }

    SetTextRenderScale(1.f);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    if (failures) std::fprintf(stderr, "%d check(s) failed\n", failures);
    return failures ? 1 : 0;
}
