/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 Huy Chau
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

#ifndef MAINMENU_INTERNAL_H
#define MAINMENU_INTERNAL_H

// Internal shared declarations for the mainmenu_*.cpp translation units.
// Not part of the public MainMenu interface (see mainmenu.h).

#include "mainmenu.h"

#include <cstdlib>

inline int ranrange(int a, int b) { return a + rand() % ((b - a ) + 1); }

// Texture dimensions helper (mainmenu.cpp).
SDL_Point GetSize(SDL_Texture *texture);

// Returns true if something is already listening on localhost:port
// (mainmenu_server.cpp).
bool portInUse(int port);

#endif // MAINMENU_INTERNAL_H
