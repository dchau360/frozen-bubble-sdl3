#ifndef SDL3_COMPAT_H
#define SDL3_COMPAT_H

#include <SDL3/SDL.h>

// SDL3 render functions use SDL_FRect (float) instead of SDL_Rect (int).
// This helper avoids changing every stored rect type throughout the codebase.
inline SDL_FRect ToFRect(const SDL_Rect& r) {
    return SDL_FRect{(float)r.x, (float)r.y, (float)r.w, (float)r.h};
}

#endif // SDL3_COMPAT_H
