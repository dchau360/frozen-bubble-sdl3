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

#include "controlhints.h"

#include "gamesettings.h"
#include "platform.h"

// Defined with the settings panel (mainmenu_panels.cpp): "Right Shift", or
// "Ctrl1:X" for a pad button.
std::string ControllerScancodeName(SDL_Scancode sc);

namespace controlhints {

bool TouchDevice() {
#if defined(__IOS_PORT__)
    return true;
#elif defined(__ANDROID__)
    return DeviceHasTouchscreen();
#elif defined(__WASM_PORT__)
    return WasmHasTouch();
#else
    return false;
#endif
}

std::string KeyName(SDL_Scancode sc) {
    if (sc == SDL_SCANCODE_UNKNOWN) return "";
    std::string key = ControllerScancodeName(sc);
    const size_t colon = key.find(':');   // a pad: just the button
    if (IsVirtualScancode(sc) && colon != std::string::npos) key = key.substr(colon + 1);
    return key;
}

std::string FireControl(bool mouseAim) {
    if (mouseAim && TouchDevice()) return "tap the board";
    const std::string key = KeyName(GameSettings::Instance()->player1Keys.fire);
    if (!mouseAim) return key;
    return key.empty() ? "click the board" : key + " or click the board";
}

std::string SwapControl(bool mouseAim) {
    if (mouseAim && TouchDevice()) return "tap below the board";
    const std::string key = KeyName(GameSettings::Instance()->player1Keys.fireNext);
    if (!mouseAim) return key;
    return key.empty() ? "right click" : key + " or right click";
}

}  // namespace controlhints
