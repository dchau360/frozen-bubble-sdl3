#pragma once

// Whether a pixel read back from a render is the modern palette's accent,
// modernui::kIce, allowing for anti-aliasing at an edge. The menu tests use it
// to see which row is focused. Going through the palette rather than testing
// for blue keeps them right in every brand (see brand.h): Frozen Bubble's
// accent is ice blue, Boba Buster's caramel.

#include <SDL3/SDL.h>
#include <cstdlib>

#include "modernui.h"

inline bool IsAccent(Uint8 r, Uint8 g, Uint8 b) {
    const SDL_Color k = modernui::kIce;
    return std::abs(r - k.r) <= 55 && std::abs(g - k.g) <= 55 && std::abs(b - k.b) <= 55;
}
