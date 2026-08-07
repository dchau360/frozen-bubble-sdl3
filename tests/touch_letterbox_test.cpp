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

// Touch coordinates are normalized against the *window*, but the game hit-tests
// against a 640x480 logical canvas that the renderer letterboxes into that
// window. Scaling a normalized coordinate straight onto the canvas therefore
// only works when the window is exactly 4:3; on any other aspect the letterbox
// bars are counted as playfield and every tap is displaced.
//
// FrozenBubble::TouchToLogical does the conversion the mouse path gets for free
// from window coordinates: normalized -> window -> SDL_RenderCoordinatesFromWindow.
// This test pins that conversion at the aspects real devices actually use, and
// pins the magnitude of the error the naive form would reintroduce.

#include <SDL3/SDL.h>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

// The conversion under test, in the same form as FrozenBubble::TouchToLogical.
void TouchToLogical(SDL_Window *window, SDL_Renderer *renderer,
                    float nx, float ny, float *lx, float *ly)
{
    int ww = 0, wh = 0;
    if (window) SDL_GetWindowSize(window, &ww, &wh);
    if (ww <= 0 || wh <= 0 || !renderer) {
        *lx = nx * 640.f;
        *ly = ny * 480.f;
        return;
    }
    SDL_RenderCoordinatesFromWindow(renderer, nx * (float)ww, ny * (float)wh, lx, ly);
}

bool near(float a, float b, float tol = 1.0f) { return fabsf(a - b) <= tol; }

// Where the canvas actually lands inside a window of this size, under
// LETTERBOX: uniform scale to fit, centred, with bars on the long axis.
struct Box { float scale, offX, offY; };
Box LetterboxOf(int ww, int wh)
{
    float scale = fminf((float)ww / 640.f, (float)wh / 480.f);
    return { scale, ((float)ww - 640.f * scale) * 0.5f,
                    ((float)wh - 480.f * scale) * 0.5f };
}

// Drives one window size: builds a renderer, then checks that a tap on a known
// point of the canvas maps back to that point.
void CheckAspect(const char *label, int ww, int wh, bool expectNaiveAgrees)
{
    SDL_Window *window = SDL_CreateWindow(label, ww, wh, 0);
    assert(window && "dummy video driver should create a window");
    SDL_Renderer *renderer = SDL_CreateRenderer(window, "software");
    assert(renderer && "software renderer should be available");
    assert(SDL_SetRenderLogicalPresentation(renderer, 640, 480,
                                            SDL_LOGICAL_PRESENTATION_LETTERBOX));

    // The dummy driver can hand back a different size than requested; measure
    // what we actually got rather than assuming, or the expectations below
    // would be checked against the wrong geometry.
    int aw = 0, ah = 0;
    SDL_GetWindowSize(window, &aw, &ah);
    assert(aw == ww && ah == wh);

    const Box box = LetterboxOf(ww, wh);

    // Canvas corners and centre, expressed as window-normalized taps.
    struct Probe { float cx, cy; } probes[] = {
        {   0.f,   0.f },     // top-left of the canvas
        { 640.f,   0.f },     // top-right
        {   0.f, 480.f },     // bottom-left
        { 640.f, 480.f },     // bottom-right
        { 320.f, 240.f },     // centre
        {  89.f,  14.f },     // "Start 1P game" button origin
    };

    for (const Probe &p : probes) {
        // Window pixel that the canvas point occupies, then normalized the way
        // SDL reports a finger event.
        float wx = box.offX + p.cx * box.scale;
        float wy = box.offY + p.cy * box.scale;
        float nx = wx / (float)ww, ny = wy / (float)wh;

        float lx = 0.f, ly = 0.f;
        TouchToLogical(window, renderer, nx, ny, &lx, &ly);
        assert(near(lx, p.cx) && near(ly, p.cy));

        // The naive mapping, for contrast. On a 4:3 window there is no
        // letterbox and it coincides; on anything else it must not, or this
        // test would pass just as happily against the bug.
        float naiveX = nx * 640.f, naiveY = ny * 480.f;
        bool naiveAgrees = near(naiveX, p.cx, 2.0f) && near(naiveY, p.cy, 2.0f);
        if (expectNaiveAgrees) {
            assert(naiveAgrees);
        } else if (p.cx != 320.f || p.cy != 240.f) {
            // The exact centre is the one point both forms agree on at every
            // aspect: it is the fixed point of a centred uniform scale.
            assert(!naiveAgrees);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    printf("  %-28s %4dx%-4d ok\n", label, ww, wh);
}

} // namespace

int main()
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL_Init(video) failed: %s\n", SDL_GetError());
        return 77; // treated as skipped: no usable video backend here
    }

    printf("touch letterbox mapping:\n");

    // Exactly 4:3 -- no bars, so the naive mapping is correct here. This is the
    // case that let the bug survive: every desktop window defaults to it.
    CheckAspect("4:3 desktop", 640, 480, true);

    // iPhone 17 in each orientation. Portrait is the worse of the two: the
    // canvas occupies a band about a third of the screen's height, so the naive
    // mapping is off by more than the canvas is tall.
    CheckAspect("iPhone landscape", 874, 402, false);
    CheckAspect("iPhone portrait", 402, 874, false);

    // A 4:3 iPad needs no correction either -- same fixed point as the desktop
    // window above. Kept so a future change to the conversion has to stay
    // correct on the aspect where the naive form happens to be right.
    CheckAspect("4:3 iPad", 1080, 810, true);

    // Modern iPads are ~1.43, not 4:3, so they do need it.
    CheckAspect("iPad Pro landscape", 1194, 834, false);
    CheckAspect("iPad Pro portrait", 834, 1194, false);

    // The Android TV build renders to a 16:9 panel: the same displacement has
    // been there all along for anyone playing with a touchscreen or a mouse.
    CheckAspect("16:9 TV", 1920, 1080, false);

    // A tap outside the canvas, in a letterbox bar, must land outside 0..640 /
    // 0..480 rather than being clamped onto an edge row -- the menu hit-tests
    // reject it that way instead of firing the nearest button.
    {
        SDL_Window *w = SDL_CreateWindow("bar", 402, 874, 0);
        SDL_Renderer *r = SDL_CreateRenderer(w, "software");
        SDL_SetRenderLogicalPresentation(r, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        float lx = 0.f, ly = 0.f;
        TouchToLogical(w, r, 0.5f, 0.02f, &lx, &ly);   // high in the top bar
        assert(ly < 0.f);
        TouchToLogical(w, r, 0.5f, 0.98f, &lx, &ly);   // low in the bottom bar
        assert(ly > 480.f);
        SDL_DestroyRenderer(r);
        SDL_DestroyWindow(w);
        printf("  %-28s ok\n", "taps in letterbox bars");
    }

    SDL_Quit();
    printf("touch-letterbox-test: all assertions passed\n");
    return 0;
}
