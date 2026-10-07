/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 dchau360
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "menutheme.h"
#include "gamesettings.h"
#include "modernui.h"
#include "platform.h"
#include "sdl3_compat.h"
#include "ttftext.h"

#include <SDL3_image/SDL_image.h>
#include <string>

namespace {

constexpr SDL_Color C(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) { return SDL_Color{r, g, b, a}; }

// Colors are carried over from the approved mockup. The two plate-art themes
// leave every plate* field at zero: nothing reads them.
const MenuThemeStyle kStyles[MENU_THEME_COUNT] = {
    // CLASSIC -- baked plates and baked labels. The font is still set because
    // the MENU STYLE row has no baked label to use (see menutheme.h).
    {
        "/gfx/DroidSans.ttf", 14, 9, 1, true,
        // Idle type is light on the plum plate, not dark: the plate is dark
        // enough that the mockup's dark ink disappeared into it entirely.
        // Both states are ringed rather than drop-shadowed, the way the baked
        // artwork is -- the selected plate runs near-white at the top and
        // magenta at the bottom, so no single flat ink reads across it.
        C(238, 206, 228), C(255, 255, 255),       // text idle / active
        C(46, 17, 38, 220), C(58, 20, 46, 235),   // outline idle / active
        {}, {}, {}, {}, {}, {}, {}, {}
    },
    // CLEAR -- the Classic plate, honest type.
    {
        "/gfx/Fredoka-Medium.ttf", 15, 9, 1, true,
        C(240, 212, 232), C(255, 255, 255),
        C(46, 17, 38, 220), C(58, 20, 46, 235),
        {}, {}, {}, {}, {}, {}, {}, {}
    },
    // SLATE -- translucent plum, the language the in-game panels already speak.
    {
        "/gfx/Fredoka-Medium.ttf", 15, 10, 0, false,
        C(240, 226, 236), C(255, 255, 255),
        C(0, 0, 0, 110), C(0, 0, 0, 130),
        C(61, 36, 56, 230), C(43, 23, 40, 240), C(255, 255, 255, 38),
        C(92, 43, 75, 230), C(67, 32, 58, 240), C(230, 148, 189, 128),
        C(255, 255, 255, 64),
        C(240, 124, 180, 255)
    },
    // ICE -- pulled from the background's ice blocks; selection fills solid.
    {
        "/gfx/Fredoka-Medium.ttf", 15, 10, 0, false,
        C(51, 32, 60), C(255, 255, 255),
        C(255, 255, 255, 90), C(0, 0, 0, 71),
        C(238, 246, 255, 255), C(207, 224, 242, 255), C(127, 151, 173, 255),
        C(213, 96, 159, 255), C(184, 77, 133, 255), C(109, 43, 80, 255),
        C(255, 255, 255, 204),
        {}
    },
    // POP -- the cartoon energy of the original, letters that actually resolve.
    {
        "/gfx/Baloo2-ExtraBold.ttf", 16, 10, 2, false,
        C(255, 255, 255), C(255, 255, 255),
        C(75, 31, 61, 255), C(109, 43, 86, 255),
        C(231, 179, 210, 255), C(197, 110, 166, 255), C(89, 38, 74, 255),
        // The selected plate has to carry the whole highlight here: Pop's type
        // is white with a dark ring in both states, so it cannot signal
        // selection itself. A plate only a shade lighter than idle -- what the
        // mockup had -- left the selected row all but invisible.
        C(255, 141, 200, 255), C(224, 66, 148, 255), C(89, 38, 74, 255),
        C(255, 255, 255, 133),
        {}
    },
    // MODERN -- the in-game theme's cards (modernui.h): dark fill, ice edge,
    // Baloo type. The selected card gets a full ice edge and a candy bubble,
    // which the label steps right to make room for.
    {
        "/gfx/Baloo2-ExtraBold.ttf", 15, 14, 0, false,
        modernui::kFrost, C(255, 255, 255),
        C(0, 0, 0, 110), C(0, 0, 0, 130),
        modernui::Alpha(modernui::kCardFill, 235), modernui::Alpha(modernui::kCardFill, 235),
        modernui::Alpha(modernui::kIce, 72),
        modernui::kCardRaised, modernui::kCardRaised, modernui::kIce,
        {}, {},
        true, 20
    },
};

static_assert(GameSettings::kMenuThemeCount == MENU_THEME_COUNT &&
              GameSettings::kMenuThemeDefault == MENU_THEME_MODERN,
              "GameSettings' copy of the menu theme ids is out of date");

const char *const kNames[MENU_THEME_COUNT] = {"CLASSIC", "CLEAR", "SLATE", "ICE", "POP", "MODERN"};

// Shared across every row: eight buttons opening their own copy of the same
// font would be eight FreeType faces for one typeface.
TTF_Font *g_fonts[MENU_THEME_COUNT] = {nullptr};
int g_fontSizes[MENU_THEME_COUNT] = {0};

// The label-free Classic plate, used by both Classic's MENU STYLE row and
// every row of Clear.
SDL_Texture *g_plateIdle = nullptr;
SDL_Texture *g_plateActive = nullptr;
bool g_plateTried = false;

// Modern's selection marker: a candy bubble, a different colour per row.
// Index 0 is grey and 1 white, which read as disabled, so they are skipped.
constexpr int kMarkerColours[] = {7, 3, 4, 5, 6, 8};
constexpr int kMarkerCount = (int)(sizeof(kMarkerColours) / sizeof(kMarkerColours[0]));
SDL_Texture *g_markers[kMarkerCount] = {nullptr};
bool g_markersTried = false;

int Clamp(int t) { return (t < 0 || t >= MENU_THEME_COUNT) ? MENU_THEME_CLASSIC : t; }

TTF_Font *FontFor(int theme)
{
    theme = Clamp(theme);
    const MenuThemeStyle &s = kStyles[theme];
    if (!s.fontPath) return nullptr;
    // Reopen when the size changed; TTF_SetFontSize on a shared face would
    // resize it for every theme pointing at the same file.
    if (g_fonts[theme] && g_fontSizes[theme] == s.fontSize) return g_fonts[theme];
    if (g_fonts[theme]) { TTF_CloseFont(g_fonts[theme]); g_fonts[theme] = nullptr; }
    g_fonts[theme] = TTF_OpenFont(ASSET(s.fontPath).c_str(), (float)s.fontSize);
    if (!g_fonts[theme]) {
        SDL_Log("MenuTheme: could not open %s (%s)", s.fontPath, SDL_GetError());
        return nullptr;
    }
    g_fontSizes[theme] = s.fontSize;
    return g_fonts[theme];
}

void EnsurePlates(const SDL_Renderer *rend)
{
    if (g_plateTried) return;
    g_plateTried = true;
    SDL_Renderer *r = const_cast<SDL_Renderer *>(rend);
    g_plateIdle = IMG_LoadTexture(r, ASSET("/gfx/menu/txt_menustyle_off.png").c_str());
    g_plateActive = IMG_LoadTexture(r, ASSET("/gfx/menu/txt_menustyle_over.png").c_str());
    if (!g_plateIdle || !g_plateActive)
        SDL_Log("MenuTheme: menustyle plate art missing (%s)", SDL_GetError());
}

void EnsureMarkers(const SDL_Renderer *rend)
{
    if (g_markersTried) return;
    g_markersTried = true;
    SDL_Renderer *r = const_cast<SDL_Renderer *>(rend);
    for (int i = 0; i < kMarkerCount; i++) {
        const std::string path = ASSET("/gfx/balls/modern/bubble-") + std::to_string(kMarkerColours[i]) + ".png";
        g_markers[i] = IMG_LoadTexture(r, path.c_str());
        if (g_markers[i]) SDL_SetTextureScaleMode(g_markers[i], SDL_SCALEMODE_LINEAR);
    }
}

// The rounded card, and on the selected row its candy bubble.
void DrawCard(const SDL_Renderer *rend, const SDL_Rect &rect, const MenuThemeStyle &s, bool active)
{
    SDL_Renderer *r = const_cast<SDL_Renderer *>(rend);
    const SDL_FRect card = {(float)rect.x + 2, (float)rect.y, (float)rect.w - 4, (float)rect.h};
    constexpr float kRadius = 14;
    modernui::FillRoundRect(r, {card.x, card.y + 3, card.w, card.h}, kRadius, {0, 0, 0, 70});
    modernui::FillRoundRect(r, card, kRadius, active ? s.plateTopActive : s.plateTopIdle);
    modernui::StrokeRoundRect(r, card, kRadius, active ? 2.f : 1.5f,
                              active ? s.plateBorderActive : s.plateBorderIdle);
    if (!active) return;
    EnsureMarkers(rend);
    // Rows sit on a 56px pitch from y=14 (MainMenu's constructor).
    SDL_Texture *marker = g_markers[((rect.y - 14) / 56 % kMarkerCount + kMarkerCount) % kMarkerCount];
    if (marker) {
        const SDL_FRect dst = {card.x + 8, card.y + card.h / 2 - 9, 18, 18};
        SDL_RenderTexture(r, marker, nullptr, &dst);
    }
}

// Straight-line interpolation between two colors, alpha included.
SDL_Color Lerp(const SDL_Color &a, const SDL_Color &b, float t)
{
    return SDL_Color{
        (Uint8)(a.r + (b.r - a.r) * t),
        (Uint8)(a.g + (b.g - a.g) * t),
        (Uint8)(a.b + (b.b - a.b) * t),
        (Uint8)(a.a + (b.a - a.a) * t),
    };
}

void SetColor(SDL_Renderer *r, const SDL_Color &c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

} // namespace

const char *MenuThemeName(int theme) { return kNames[Clamp(theme)]; }

const MenuThemeStyle &MenuStyleFor(int theme) { return kStyles[Clamp(theme)]; }

void MenuThemeDrawPlate(const SDL_Renderer *rend, const SDL_Rect &rect, int theme, bool active)
{
    theme = Clamp(theme);
    const MenuThemeStyle &s = kStyles[theme];
    SDL_Renderer *r = const_cast<SDL_Renderer *>(rend);

    if (s.roundCard) { DrawCard(rend, rect, s, active); return; }

    if (s.usesPlateArt) {
        EnsurePlates(rend);
        SDL_Texture *t = active ? g_plateActive : g_plateIdle;
        if (t) { SDL_FRect fr = ToFRect(rect); SDL_RenderTexture(r, t, nullptr, &fr); }
        return;
    }

    SDL_BlendMode prev = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(r, &prev);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    const SDL_Color top = active ? s.plateTopActive : s.plateTopIdle;
    const SDL_Color bot = active ? s.plateBotActive : s.plateBotIdle;
    const SDL_Color border = active ? s.plateBorderActive : s.plateBorderIdle;

    // Vertical gradient, one line per row. At 46 rows across eight buttons
    // that is under 400 lines a frame -- cheaper than keeping a texture per
    // theme per state in sync with the theme the player just cycled to.
    for (int i = 0; i < rect.h; i++) {
        float t = rect.h > 1 ? (float)i / (float)(rect.h - 1) : 0.0f;
        SetColor(r, Lerp(top, bot, t));
        SDL_RenderLine(r, (float)rect.x, (float)(rect.y + i),
                       (float)(rect.x + rect.w - 1), (float)(rect.y + i));
    }

    if (s.topHighlight.a) {
        SetColor(r, s.topHighlight);
        SDL_RenderLine(r, (float)(rect.x + 1), (float)(rect.y + 1),
                       (float)(rect.x + rect.w - 2), (float)(rect.y + 1));
    }

    SetColor(r, border);
    { SDL_FRect fr = ToFRect(rect); SDL_RenderRect(r, &fr); }

    if (active && s.railActive.a) {
        SetColor(r, s.railActive);
        // Full height, inside the border: an inset stub read as a rendering
        // glitch rather than as a selection marker.
        SDL_Rect rail = {rect.x + 1, rect.y + 1, 4, rect.h - 2};
        SDL_FRect fr = ToFRect(rail);
        SDL_RenderFillRect(r, &fr);
    }

    SDL_SetRenderDrawBlendMode(r, prev);
}

void MenuThemeDrawBackdrop(const SDL_Renderer *rend, int theme)
{
    if (Clamp(theme) != MENU_THEME_MODERN) return;
    // Dark on the left, fading out past the column, so the cards read while
    // the logo and the penguins on the right stay bright.
    const SDL_FColor dark = {5 / 255.f, 10 / 255.f, 22 / 255.f, 0.6f};
    const SDL_FColor light = {5 / 255.f, 10 / 255.f, 22 / 255.f, 0.15f};
    const float w = 340, h = 480;
    const SDL_Vertex v[4] = {
        {{0, 0}, dark, {0, 0}}, {{w, 0}, light, {0, 0}},
        {{w, h}, light, {0, 0}}, {{0, h}, dark, {0, 0}},
    };
    const int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_Renderer *r = const_cast<SDL_Renderer *>(rend);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(r, nullptr, v, 4, idx, 6);
}

SDL_Texture *MenuThemeRenderLabel(const SDL_Renderer *rend, int theme,
                                  const char *text, bool active, SDL_Point *outSize)
{
    theme = Clamp(theme);
    const MenuThemeStyle &s = kStyles[theme];
    TTF_Font *font = FontFor(theme);
    const SDL_Color fg = active ? s.textActive : s.textIdle;
    const SDL_Color sh = active ? s.shadowActive : s.shadowIdle;
    return RenderRingedText(rend, font, text, fg, sh, s.outline, outSize);
}

void MenuThemeShutdown()
{
    for (int i = 0; i < MENU_THEME_COUNT; i++) {
        if (g_fonts[i]) { TTF_CloseFont(g_fonts[i]); g_fonts[i] = nullptr; }
        g_fontSizes[i] = 0;
    }
    if (g_plateIdle) { SDL_DestroyTexture(g_plateIdle); g_plateIdle = nullptr; }
    if (g_plateActive) { SDL_DestroyTexture(g_plateActive); g_plateActive = nullptr; }
    g_plateTried = false;
    for (SDL_Texture *&t : g_markers) if (t) { SDL_DestroyTexture(t); t = nullptr; }
    g_markersTried = false;
}
