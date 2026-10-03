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

#include "modernui.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "platform.h"

namespace modernui {

namespace {

constexpr int kCornerSteps = 8;
constexpr float kPi = 3.14159265f;  // M_PI is not in MSVC's <cmath> by default
constexpr SDL_Color kTextShadow = {0, 0, 0, 110};

SDL_FColor ToF(SDL_Color c) { return {c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f}; }

// The outline of a rounded rect, clockwise from the top-left corner's start.
std::vector<SDL_FPoint> Outline(SDL_FRect r, float radius) {
    radius = std::max(0.f, std::min(radius, std::min(r.w, r.h) / 2));
    const float cx[4] = {r.x + radius, r.x + r.w - radius, r.x + r.w - radius, r.x + radius};
    const float cy[4] = {r.y + radius, r.y + radius, r.y + r.h - radius, r.y + r.h - radius};
    std::vector<SDL_FPoint> pts;
    pts.reserve(4 * (kCornerSteps + 1));
    for (int corner = 0; corner < 4; ++corner) {
        const float a0 = kPi + corner * kPi / 2;
        for (int s = 0; s <= kCornerSteps; ++s) {
            const float a = a0 + kPi / 2 * s / kCornerSteps;
            pts.push_back({cx[corner] + std::cos(a) * radius, cy[corner] + std::sin(a) * radius});
        }
    }
    return pts;
}

void Blit(SDL_Renderer* rend, TTFText& t, int x, int y, Uint8 alpha) {
    SDL_Texture* tex = t.Texture();
    if (!tex) return;
    t.UpdatePosition({x, y});
    SDL_SetTextureAlphaMod(tex, alpha);
    SDL_FRect fr = {(float)x, (float)y, (float)t.Coords()->w, (float)t.Coords()->h};
    SDL_RenderTexture(rend, tex, nullptr, &fr);
    SDL_SetTextureAlphaMod(tex, 255);
}

void SetText(SDL_Renderer* rend, TTFText& t, const std::string& s, SDL_Color color) {
    t.UpdateColor(color, kTextShadow);
    t.UpdateText(rend, s.c_str(), 0);
}

SDL_Color WithAlpha(SDL_Color c, float k) {
    c.a = (Uint8)std::clamp(c.a * k, 0.f, 255.f);
    return c;
}

}  // namespace

void FillRoundRect(SDL_Renderer* rend, SDL_FRect r, float radius, SDL_Color c) {
    const std::vector<SDL_FPoint> pts = Outline(r, radius);
    const SDL_FColor fc = ToF(c);
    std::vector<SDL_Vertex> v;
    v.reserve(pts.size() + 1);
    v.push_back({{r.x + r.w / 2, r.y + r.h / 2}, fc, {0, 0}});
    for (const SDL_FPoint& p : pts) v.push_back({p, fc, {0, 0}});
    std::vector<int> idx;
    const int n = (int)pts.size();
    for (int i = 0; i < n; ++i) {
        idx.push_back(0);
        idx.push_back(1 + i);
        idx.push_back(1 + (i + 1) % n);
    }
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(rend, nullptr, v.data(), (int)v.size(), idx.data(), (int)idx.size());
}

void StrokeRoundRect(SDL_Renderer* rend, SDL_FRect r, float radius, float width, SDL_Color c) {
    const std::vector<SDL_FPoint> outer = Outline(r, radius);
    const std::vector<SDL_FPoint> inner =
        Outline({r.x + width, r.y + width, r.w - 2 * width, r.h - 2 * width}, std::max(0.f, radius - width));
    const SDL_FColor fc = ToF(c);
    std::vector<SDL_Vertex> v;
    const int n = (int)outer.size();
    v.reserve(2 * n);
    for (int i = 0; i < n; ++i) {
        v.push_back({outer[i], fc, {0, 0}});
        v.push_back({inner[i], fc, {0, 0}});
    }
    std::vector<int> idx;
    for (int i = 0; i < n; ++i) {
        const int a = 2 * i, b = 2 * i + 1, c2 = 2 * ((i + 1) % n), d = c2 + 1;
        idx.insert(idx.end(), {a, b, c2, b, d, c2});
    }
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(rend, nullptr, v.data(), (int)v.size(), idx.data(), (int)idx.size());
}

int DrawTextLine(SDL_Renderer* rend, TTFText& t, const char* s, SDL_Color fg, SDL_Color shadow,
                 int x, int cy, int align) {
    t.UpdateColor(fg, shadow);
    t.UpdateText(rend, s, 0);
    const SDL_Rect* c = t.Coords();
    const int left = align == 0 ? x : align == 1 ? x - c->w / 2 : x - c->w;
    t.UpdatePosition({left, cy - c->h / 2});
    if (t.Texture()) {
        SDL_FRect fr = {(float)t.Coords()->x, (float)t.Coords()->y, (float)c->w, (float)c->h};
        SDL_RenderTexture(rend, t.Texture(), nullptr, &fr);
    }
    return c->w;
}

void DrawSwitch(SDL_Renderer* rend, int x, int cy, bool on) {
    FillRoundRect(rend, {(float)x, (float)cy - 8, 30, 16}, 8,
                  on ? SDL_Color{95, 224, 160, 255} : SDL_Color{58, 75, 108, 255});
    FillRoundRect(rend, {(float)(on ? x + 16 : x + 2), (float)cy - 6, 12, 12}, 6, kValue);
}

int DrawChip(SDL_Renderer* rend, TTFText& t, const char* s, int x, int cy, SDL_Color c, bool filled) {
    t.UpdateColor(filled ? kInk : c, kNoShadow);
    t.UpdateText(rend, s, 0);
    const int w = t.Coords()->w + 10;
    FillRoundRect(rend, {(float)x, (float)cy - 8, (float)w, 16}, 8,
                  filled ? c : SDL_Color{c.r, c.g, c.b, 46});
    DrawTextLine(rend, t, s, filled ? kInk : c, kNoShadow, x + 5, cy);
    return w;
}

void DrawPill(SDL_Renderer* rend, TTFText& t, const char* s, SDL_Rect r, bool primary, bool focused) {
    const SDL_FRect f = {(float)r.x, (float)r.y, (float)r.w, (float)r.h};
    const float rad = f.h / 2;
    FillRoundRect(rend, f, rad, primary ? kIce : SDL_Color{127, 214, 255, 26});
    if (!primary) StrokeRoundRect(rend, f, rad, 1, SDL_Color{127, 214, 255, 72});
    if (focused) StrokeRoundRect(rend, {f.x - 3, f.y - 3, f.w + 6, f.h + 6}, rad + 3, 2, kValue);
    DrawTextLine(rend, t, s, primary ? kInk : kFrost, kNoShadow, r.x + r.w / 2, r.y + r.h / 2, 1);
}

void DrawCard(SDL_Renderer* rend, SDL_Rect r, float radius) {
    const SDL_FRect f = {(float)r.x, (float)r.y, (float)r.w, (float)r.h};
    FillRoundRect(rend, {f.x, f.y + 4, f.w, f.h}, radius, {0, 0, 0, 77});
    FillRoundRect(rend, f, radius, {12, 24, 48, 235});
    StrokeRoundRect(rend, f, radius, 1.5f, {127, 214, 255, 72});
}

std::string FormatTime(Uint64 ms) {
    const Uint64 s = ms / 1000;
    char buf[32];
    SDL_snprintf(buf, sizeof(buf), "%u'%02u\"", (unsigned)(s / 60), (unsigned)(s % 60));
    return buf;
}

std::string FormatNumber(int n) {
    const std::string digits = std::to_string(std::max(0, n));
    std::string out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i && (digits.size() - i) % 3 == 0) out += ',';
        out += digits[i];
    }
    return out;
}

void Fonts::Load() {
    const std::string display = ASSET("/gfx/Baloo2-ExtraBold.ttf");
    const std::string body = ASSET("/gfx/DroidSans.ttf");
    for (TTFText& t : hudLabel) { t.LoadFont(body.c_str(), 10); t.UpdateStyle(10, TTF_STYLE_BOLD); }
    for (TTFText& t : hudValue) t.LoadFont(display.c_str(), 22);
    title.LoadFont(display.c_str(), 28);
    for (TTFText& t : statLabel) { t.LoadFont(body.c_str(), 10); t.UpdateStyle(10, TTF_STYLE_BOLD); }
    for (TTFText& t : statValue) t.LoadFont(display.c_str(), 22);
    for (TTFText& t : button) t.LoadFont(display.c_str(), 16);
    note.LoadFont(body.c_str(), 12);
    popup.LoadFont(display.c_str(), 20);
    popup.UpdateRing({6, 20, 40, 230}, 2);
    dropped.LoadFont(display.c_str(), 30);
    dropped.UpdateRing({6, 20, 40, 230}, 2);
}

void DrawHud(SDL_Renderer* rend, Fonts& f, const std::string& level, int score, int shots, Uint64 timeMs) {
    // Runs off the left edge, so only its right corners show round. Tall and
    // wide enough to cover the wooden sign the original level name sits on.
    const SDL_FRect box = {-16, 14, 182, 196};
    FillRoundRect(rend, {box.x, box.y + 4, box.w, box.h}, 16, {0, 0, 0, 70});  // soft drop shadow
    FillRoundRect(rend, box, 16, kCardFill);
    StrokeRoundRect(rend, box, 16, 2, kEdge);

    const std::string labels[4] = {"LEVEL", "SCORE", "SHOTS", "TIME"};
    const std::string values[4] = {level, FormatNumber(score), FormatNumber(shots), FormatTime(timeMs)};
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 4; ++i) {
        const int y = (int)box.y + 12 + i * 45;
        if (i) {
            SDL_SetRenderDrawColor(rend, kIce.r, kIce.g, kIce.b, 40);
            SDL_FRect line = {14, (float)y - 5, box.w - 46, 1};
            SDL_RenderFillRect(rend, &line);
        }
        SetText(rend, f.hudLabel[i], labels[i], kLabel);
        Blit(rend, f.hudLabel[i], 14, y, 255);
        SetText(rend, f.hudValue[i], values[i], kValue);
        Blit(rend, f.hudValue[i], 13, y + 9, 255);
    }
}

void DrawCard(SDL_Renderer* rend, Fonts& f, const Card& card, float appear, SDL_Rect btnOut[2]) {
    appear = std::clamp(appear, 0.f, 1.f);
    const float ease = 1 - (1 - appear) * (1 - appear) * (1 - appear);
    const Uint8 alpha = (Uint8)(255 * std::min(1.f, appear * 1.6f));

    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rend, 3, 8, 20, (Uint8)(110 * ease));
    SDL_FRect dim = {0, 0, 640, 480};
    SDL_RenderFillRect(rend, &dim);

    const float artH = card.art ? 80.f : 0.f;
    const float w = 316, h = 150 + artH + (card.buttonCount ? 48.f : 0.f) + (card.note.empty() ? 0.f : 18.f);
    const float x = 320 - w / 2, y = 238 - h / 2 + (1 - ease) * 18;
    const float k = alpha / 255.f;
    FillRoundRect(rend, {x, y + 5, w, h}, 18, WithAlpha({0, 0, 0, 90}, k));
    FillRoundRect(rend, {x, y, w, h}, 18, WithAlpha(kCardFill, k));
    StrokeRoundRect(rend, {x, y, w, h}, 18, 2, WithAlpha(kEdge, k));

    SetText(rend, f.title, card.title, card.titleColor);
    Blit(rend, f.title, (int)(320 - f.title.Coords()->w / 2), (int)y + 12, alpha);

    float cursor = y + 56;
    if (card.art) {
        SDL_SetTextureAlphaMod(card.art, alpha);
        SDL_FRect fr = {320 - 53, cursor - 4, 106, 80};
        SDL_RenderTexture(rend, card.art, nullptr, &fr);
        SDL_SetTextureAlphaMod(card.art, 255);
        cursor += artH;
    }

    const float colW = card.statCount ? w / card.statCount : w;
    for (int i = 0; i < card.statCount; ++i) {
        const int cx = (int)(x + colW * (i + 0.5f));
        SetText(rend, f.statLabel[i], card.stats[i].label, kLabel);
        Blit(rend, f.statLabel[i], cx - f.statLabel[i].Coords()->w / 2, (int)cursor + 6, alpha);
        SetText(rend, f.statValue[i], card.stats[i].value, kValue);
        Blit(rend, f.statValue[i], cx - f.statValue[i].Coords()->w / 2, (int)cursor + 18, alpha);
    }
    cursor += 70;

    if (!card.note.empty()) {
        SetText(rend, f.note, card.note, kLabel);
        Blit(rend, f.note, (int)(320 - f.note.Coords()->w / 2), (int)cursor - 4, alpha);
        cursor += 18;
    }

    for (int i = 0; i < 2; ++i) btnOut[i] = {0, 0, 0, 0};
    const float bw = card.buttonCount == 1 ? 170 : 134, gap = 14;
    const float total = card.buttonCount * bw + (card.buttonCount - 1) * gap;
    for (int i = 0; i < card.buttonCount; ++i) {
        const SDL_FRect b = {320 - total / 2 + i * (bw + gap), cursor + 2, bw, 36};
        const bool on = i == card.focus;
        FillRoundRect(rend, b, 18, WithAlpha(on ? kIce : SDL_Color{127, 214, 255, 30}, k));
        StrokeRoundRect(rend, b, 18, on ? 2.f : 1.5f, WithAlpha(on ? SDL_Color{255, 255, 255, 220} : kEdge, k));
        SetText(rend, f.button[i], card.buttons[i], on ? kInk : kValue);
        Blit(rend, f.button[i], (int)(b.x + b.w / 2 - f.button[i].Coords()->w / 2.f),
             (int)(b.y + b.h / 2 - f.button[i].Coords()->h / 2.f), alpha);
        btnOut[i] = {(int)b.x, (int)b.y, (int)b.w, (int)b.h};
    }
}

void DrawPauseButton(SDL_Renderer* rend, SDL_Rect r, bool paused) {
    const SDL_FRect box = {(float)r.x, (float)r.y, (float)r.w, (float)r.h};
    const float radius = box.w / 2;
    FillRoundRect(rend, box, radius, {12, 24, 48, 200});
    StrokeRoundRect(rend, box, radius, 2, kEdge);
    const float cx = box.x + box.w / 2, cy = box.y + box.h / 2;
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    if (paused) {
        const SDL_FColor c = {1, 1, 1, 1};
        SDL_Vertex v[3] = {{{cx - 5, cy - 8}, c, {0, 0}}, {{cx - 5, cy + 8}, c, {0, 0}}, {{cx + 8, cy}, c, {0, 0}}};
        SDL_RenderGeometry(rend, nullptr, v, 3, nullptr, 0);
    } else {
        FillRoundRect(rend, {cx - 7, cy - 8, 5, 16}, 1.5f, kValue);
        FillRoundRect(rend, {cx + 2, cy - 8, 5, 16}, 1.5f, kValue);
    }
}

void DrawPopup(SDL_Renderer* rend, TTFText& text, const std::string& label, int x, int y, float t, bool big) {
    t = std::clamp(t, 0.f, 1.f);
    text.UpdateColor(big ? kGold : kValue, kNoShadow);
    text.UpdateText(rend, label.c_str(), 0);
    SDL_Texture* tex = text.Texture();
    if (!tex) return;
    // Big ones pop in slightly oversized and settle; both rise and fade out.
    const float scale = big ? (t < 0.15f ? 0.8f + 0.35f * (t / 0.15f) : t < 0.25f ? 1.15f - 0.15f * ((t - 0.15f) / 0.1f) : 1.f) : 1.f;
    const float w = text.Coords()->w * scale, h = text.Coords()->h * scale;
    const Uint8 alpha = (Uint8)(255 * (t < 0.6f ? 1.f : 1.f - (t - 0.6f) / 0.4f));
    SDL_SetTextureAlphaMod(tex, alpha);
    SDL_FRect fr = {x - w / 2, y - h / 2 - 32 * t, w, h};
    SDL_RenderTexture(rend, tex, nullptr, &fr);
    SDL_SetTextureAlphaMod(tex, 255);
}

}  // namespace modernui
