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

#ifndef BUBBLEGAME_H
#define BUBBLEGAME_H

#include "audiomixer.h"
#include "platform.h"
#define PI 3.1415926535897932384626433832795028841972

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include "sdl3_compat.h"
#include "ttftext.h"
#include "networkclient.h"

#include <vector>
#include <array>
#include <algorithm>

#pragma region "BubbleGame Defines"
#define TIME_APPEARS_NEW_ROOT_MP 11
// Original Perl (bin/frozen-bubble ~line 3300-3302, TARGET_ANIM_SPEED=20ms -> 50fps) default
// hurry timer: warn=250 frames/5.0s, force-fire=375 frames/7.5s, used for every mode except
// the classic numbered single-player campaign. Scaled to this port's 60fps baseline:
// 250*60/50=300, 375*60/50=450.
#define TIME_HURRY_WARN_MP 300
#define TIME_HURRY_MAX_MP 450

// Original Perl (~line 3330-3332) overrides to warn=400 frames/8.0s, force-fire=525 frames/10.5s
// ONLY for the classic numbered single-player campaign (random 1P levels and mp_train keep the
// shorter default above, same as multiplayer). Scaled to 60fps: 400*60/50=480, 525*60/50=630.
#define TIME_HURRY_WARN 480
#define TIME_HURRY_MAX 630

#define HURRY_WARN_FC 154
#define HURRY_WARN_MP_FC 125 

// frame count for animations
#define PENGUIN_HANDLEFC 71
#define PENGUIN_WAITFC 97
#define PENGUIN_WINFC 68
#define PENGUIN_LOSEFC 158

#define PENGUIN_FRAMEWAIT 1
#define TIMEOUT_PENGUIN_SLEEP 200 * 2

#define BUBBLE_STYLES 8
#define BUBBLE_STICKFC 7
// Original Perl source values (bin/frozen-bubble line 94-97):
//   $BUBBLE_SPEED      = 10   → ported as 10/2=5 px/frame at 60fps = 300 px/sec
//   $MALUS_BUBBLE_SPEED = 30  → ported as 30/2=15 px/frame (unused; see MALUS_SPEED in bubblegame.cpp)
//   $LAUNCHER_SPEED    = 0.015 rad/frame
// Current values are the original port defaults; actual runtime speed is multiplied
// by FrozenBubble::deltaScale (3.0x desktop/browser default, 1.25x Android default).
#define BUBBLE_SPEED 10 / 2
#define MALUS_BUBBLE_SPEED 30 / 2
#define LAUNCHER_SPEED 0.015 * 0.6

#define LAUNCHER_DIAMETER 50
#define LAUNCHER_DIAMETER_MINI 25
#define CANON_ROTATIONS 100

#define COMPRESSOR_OFFSET 28
#define FREEFALL_CONSTANT 0.5
#define FROZEN_FRAMEWAIT 2
#define EXPLODE_FRAMEWAIT 2

#define PRELIGHT_SLOW 60
#define PRELIGHT_FAST 20
#define PRELIGHT_FRAMEWAIT 3

#define SCREEN_CENTER_X 640/2
#define SCREEN_CENTER_Y 480/2
#pragma endregion

//hardcoded framecount, theres like a ton of frames here
struct Penguin {
    SDL_Texture* handle[PENGUIN_HANDLEFC] = {};
    SDL_Texture* wait[PENGUIN_WAITFC] = {};
    SDL_Texture* win[PENGUIN_WINFC] = {};
    SDL_Texture* lose[PENGUIN_LOSEFC] = {};
    // curAnimation up to 13 values
    int curAnimation = 0, curFrame = 1, waitFrame = 0;
    int sleeping = 0;
    bool invertAnimation = false;
    SDL_Renderer *rend = nullptr;
    SDL_Rect rect = {};

    Penguin() = default;
    Penguin(const Penguin&) = delete;
    Penguin& operator=(const Penguin&) = delete;
    ~Penguin() {
        for (int i = 0; i < PENGUIN_HANDLEFC; i++) if (handle[i]) SDL_DestroyTexture(handle[i]);
        for (int i = 0; i < PENGUIN_WAITFC; i++) if (wait[i]) SDL_DestroyTexture(wait[i]);
        for (int i = 0; i < PENGUIN_WINFC; i++) if (win[i]) SDL_DestroyTexture(win[i]);
        for (int i = 0; i < PENGUIN_LOSEFC; i++) if (lose[i]) SDL_DestroyTexture(lose[i]);
    }

    void LoadPenguin(SDL_Renderer* renderer, const char *whichOne, SDL_Rect rct) {
        rend = renderer;
        rect = rct;

        char rel[256];
        for (int i = 0; i < PENGUIN_HANDLEFC; i++) {
            snprintf(rel, sizeof(rel), "/gfx/pinguins/anime-shooter_%s_%04d.png", whichOne, i + 1);
            if (handle[i]) SDL_DestroyTexture(handle[i]);
            handle[i] = IMG_LoadTexture(renderer, ASSET(rel).c_str());
        }
        for (int i = 0; i < PENGUIN_WAITFC; i++) {
            snprintf(rel, sizeof(rel), "/gfx/pinguins/wait_%s_%04d.png", whichOne, i + 1);
            if (wait[i]) SDL_DestroyTexture(wait[i]);
            wait[i] = IMG_LoadTexture(renderer, ASSET(rel).c_str());
        }
        for (int i = 0; i < PENGUIN_WINFC; i++) {
            snprintf(rel, sizeof(rel), "/gfx/pinguins/win_%s_%04d.png", whichOne, i + 1);
            if (win[i]) SDL_DestroyTexture(win[i]);
            win[i] = IMG_LoadTexture(renderer, ASSET(rel).c_str());
        }
        for (int i = 0; i < PENGUIN_LOSEFC; i++) {
            snprintf(rel, sizeof(rel), "/gfx/pinguins/loose_%s_%04d.png", whichOne, i + 1);
            if (lose[i]) SDL_DestroyTexture(lose[i]);
            lose[i] = IMG_LoadTexture(renderer, ASSET(rel).c_str());
        }
    }

    int PlayAnimation(int animId) {
        if (animId > 12) return 0;
        curAnimation = animId;
        curFrame = 0;

        //check if animation needs invertion or not
        if (curAnimation == 2 || curAnimation == 7) invertAnimation = true;
        else invertAnimation = false;

        if (curAnimation == 1) curFrame = 21;
        else if (curAnimation == 2) curFrame = 19;
        else if (curAnimation == 7) curFrame =71;

        switch(curAnimation) {
            case 0: curFrame = 20; break;
            case 1: curFrame = curFrame < 21 || curFrame > 50 ? (invertAnimation == true ? (curFrame < 21 ? PlayAnimation(0) : 50) : (curFrame > 50 ? PlayAnimation(0) : 21)) : curFrame; break;
            case 2: curFrame = curFrame < 2 || curFrame > 19 ? (invertAnimation == true ? (curFrame < 2 ? PlayAnimation(3) : 19) : (curFrame > 19 ? PlayAnimation(3) : 2)) : curFrame; break;
            case 3: curFrame = 1; break;
            case 4: curFrame = curFrame < 2 || curFrame > 19 ? (invertAnimation == true ? (curFrame < 2 ? PlayAnimation(0) : 19) : (curFrame > 19 ? PlayAnimation(0) : 2)) : curFrame; break;
            case 5: curFrame = curFrame < 51 || curFrame > 70 ? (invertAnimation == true ? (curFrame < 51 ? PlayAnimation(6) : 70) : (curFrame > 70 ? PlayAnimation(6) : 51)) : curFrame; break;
            case 6: curFrame = 71; break;
            case 7: curFrame = curFrame < 51 || curFrame > 71 ? (invertAnimation == true ? (curFrame < 51 ? PlayAnimation(0) : 71) : (curFrame > 71 ? PlayAnimation(0) : 51)) : curFrame; break;
            case 8: curFrame = curFrame < 1 || curFrame > 74 ? (invertAnimation == true ? (curFrame < 1 ? PlayAnimation(9) : 74) : (curFrame > 74 ? PlayAnimation(9) : 1)) : curFrame; break;
            case 9: curFrame = curFrame < 75 || curFrame > 97 ? (invertAnimation == true ? 97 : 75) : curFrame; break;
            case 10: curFrame = curFrame < 1 || curFrame > 68 ? (invertAnimation == true ? 68 : 1) : curFrame; break;
            case 11: curFrame = curFrame < 1 || curFrame > 64 ? (invertAnimation == true ? (curFrame < 1 ? PlayAnimation(12) : 64) : (curFrame > 64 ? PlayAnimation(12) : 1)) : curFrame; break;
            case 12: curFrame = curFrame < 65 || curFrame > 158 ? (invertAnimation == true ? 158 : 65) : curFrame; break;
        }

        return curFrame;
    }

    void UpdateFrame() {
        //check if animation needs invertion or not
        if (curAnimation == 2 || curAnimation == 7) invertAnimation = true;
        else invertAnimation = false;

        if(waitFrame >= PENGUIN_FRAMEWAIT) {
            waitFrame = 0;
            if (invertAnimation == false) curFrame++;
            else curFrame--;
        }
        else waitFrame++;

        switch(curAnimation) {
            case 0: curFrame = 20; break;
            case 1: curFrame = curFrame < 21 || curFrame > 50 ? (invertAnimation == true ? (curFrame < 21 ? PlayAnimation(0) : 50) : (curFrame > 50 ? PlayAnimation(0) : 21)) : curFrame; break;
            case 2: curFrame = curFrame < 2 || curFrame > 19 ? (invertAnimation == true ? (curFrame < 2 ? PlayAnimation(3) : 19) : (curFrame > 19 ? PlayAnimation(3) : 2)) : curFrame; break;
            case 3: curFrame = 1; break;
            case 4: curFrame = curFrame < 2 || curFrame > 19 ? (invertAnimation == true ? (curFrame < 2 ? PlayAnimation(0) : 19) : (curFrame > 19 ? PlayAnimation(0) : 2)) : curFrame; break;
            case 5: curFrame = curFrame < 51 || curFrame > 70 ? (invertAnimation == true ? (curFrame < 51 ? PlayAnimation(6) : 70) : (curFrame > 70 ? PlayAnimation(6) : 51)) : curFrame; break;
            case 6: curFrame = 71; break;
            case 7: curFrame = curFrame < 51 || curFrame > 71 ? (invertAnimation == true ? (curFrame < 51 ? PlayAnimation(0) : 71) : (curFrame > 71 ? PlayAnimation(0) : 51)) : curFrame; break;
            case 8: curFrame = curFrame < 1 || curFrame > 74 ? (invertAnimation == true ? (curFrame < 1 ? PlayAnimation(9) : 74) : (curFrame > 74 ? PlayAnimation(9) : 1)) : curFrame; break;
            case 9: curFrame = curFrame < 75 || curFrame > 97 ? (invertAnimation == true ? 97 : 75) : curFrame; break;
            case 10: curFrame = curFrame < 1 || curFrame > 68 ? (invertAnimation == true ? 68 : 1) : curFrame; break;
            case 11: curFrame = curFrame < 1 || curFrame > 64 ? (invertAnimation == true ? (curFrame < 1 ? PlayAnimation(12) : 64) : (curFrame > 64 ? PlayAnimation(12) : 1)) : curFrame; break;
            case 12: curFrame = curFrame < 65 || curFrame > 158 ? (invertAnimation == true ? 158 : 65) : curFrame; break;
        }
    }

    SDL_Texture *CurrentFrame() {
        UpdateFrame();
        
        if (curAnimation <= 7) return handle[curFrame - 1];
        else if (curAnimation == 8 || curAnimation == 9) return wait[curFrame - 1];
        else if (curAnimation == 10) return win[curFrame - 1];
        else return lose[curFrame - 1];
    }

    void Render() {
        SDL_FRect fr = ToFRect(rect); SDL_RenderTexture(rend, CurrentFrame(), nullptr, &fr);
    }
};

struct Bubble {
    int bubbleId = -1; // id to use bubble image
    SDL_Point pos = {}; // current position, top left aligned
    bool playerBubble = false; // if bubble was launched by player
    bool shining = false; // doing that shiny animation
    bool frozen = false; // frozen (game over)
    SDL_Rect coords = {}, frozenCoords = {};

    void MeasureRects(SDL_Texture *bubbleT, SDL_Texture *frozenT){
        SDL_Point size;
        float fw, fh; SDL_GetTextureSize(bubbleT, &fw, &fh); size.x = (int)fw; size.y = (int)fh;
        coords = {pos.x, pos.y, size.x, size.y};
        SDL_GetTextureSize(frozenT, &fw, &fh); size.x = (int)fw; size.y = (int)fh;
        frozenCoords = {pos.x - 2, pos.y - 2, size.x, size.y};
    }

    void RenderFrozen(SDL_Renderer *rend, SDL_Texture *frozen) {
        if (bubbleId == -1) return;
        { SDL_FRect fr = ToFRect(frozenCoords); SDL_RenderTexture(rend, frozen, nullptr, &fr); }
    }

    void Render(SDL_Renderer *rend, SDL_Texture *bubbles[], SDL_Texture *shinyTexture, SDL_Texture *frozenTexture) {
        if (bubbleId == -1) return;
        MeasureRects(bubbles[bubbleId], frozenTexture);
        { SDL_FRect fr = ToFRect(coords); SDL_RenderTexture(rend, bubbles[bubbleId], nullptr, &fr); }
        if (frozen) RenderFrozen(rend, frozenTexture);
        if(shining) { SDL_FRect fr = ToFRect(coords); SDL_RenderTexture(rend, shinyTexture, nullptr, &fr); }
    };
};

struct Shooter {
    SDL_Texture *texture = nullptr;
    SDL_Renderer *renderer = nullptr;
    float angle = PI/2.0f;
    SDL_Rect rect = {};
    SDL_Rect lowRct = {};

    void Render(bool /*lowGfx*/){
        // Always render the cannon with rotation regardless of gfx quality
        double degrees = -(((angle*CANON_ROTATIONS)/(PI/2.0f) + 0.5) - CANON_ROTATIONS);
        { SDL_FRect fr = ToFRect(rect); SDL_RenderTextureRotated(renderer, texture, nullptr, &fr, degrees, NULL, SDL_FLIP_NONE); }
    }
};

// >5-player network battle royale: player 0 keeps the case-5 center board and is always
// visible; remote players are parked round-robin at the 4 case-5 mini-board slot
// geometries and paged in/out of view via BubbleGame::netViewPage. Old cap this replaces
// was a bare literal 5 everywhere below.
inline constexpr int MAX_NET_PLAYERS = 20;

struct SetupSettings {
    bool chainReaction = false;
    int playerCount = 1;
    bool networkGame = false;
    bool randomLevels = false;
    bool singlePlayerTargetting = false;
    int startLevel = 1;
    bool mpTraining = false;  // 1P multiplayer training mode (timed, score-based)
    bool localMultiplayer = false;  // True for local controller-based multiplayer
    int playerColors[MAX_NET_PLAYERS] = {8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8};  // Per-player color count (5-8)
    bool disableCompression[MAX_NET_PLAYERS] = {};  // Per-player: skip row compression
    bool aimGuide[MAX_NET_PLAYERS] = {};  // Per-player: show aim trajectory guide
    int victoriesLimit = 0;  // 0 = unlimited; >0 = first to reach this wins the match
    bool mouseEnabled = false;  // Mouse/touchscreen aim+fire for player 1
    bool clearMode = false;    // Clear Mode: win by clearing the board
    bool disableMalus = false; // Disable malus attacks
    bool teamMode = false;
    // Per-player team number (1..teamCount). Widened to MAX_NET_PLAYERS to
    // match playerColors/disableCompression/aimGuide; fully resolved at game
    // start (menu fills it), so the static initializer is just a placeholder.
    int playerTeams[MAX_NET_PLAYERS] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    int teamCount = 2;  // number of teams (2..5); meaningful when teamMode
    bool continueWhenPlayersLeave = true;
    // Per-player: this slot is played by the AI rather than a person. Sized
    // like the other per-player arrays so network rooms can use it too.
    bool playerIsBot[MAX_NET_PLAYERS] = {};
    // How hard the bots try: 0 easy, 1 normal, 2 hard. One setting for all of
    // them -- a per-bot dial is more knobs than the room screen can justify.
    int botSkill = 1;
};

// Shared between the lobby/game-room team UI and in-gameplay team indicators
// (bubblegame_render.cpp) so a team reads the same color everywhere. Indexed
// by playerTeams[i] - 1.
inline constexpr SDL_Color kTeamColors[5] = {
    {255, 112, 112, 255}, {92, 184, 255, 255}, {255, 211, 66, 255},
    {132, 220, 130, 255}, {205, 139, 255, 255}
};
// NetworkClient clamps peer-supplied team numbers to [1, kMaxTeams] before they
// reach these indexing sites; keep the two in step or that clamp stops covering
// the whole array.
static_assert(std::size(kTeamColors) == static_cast<size_t>(kMaxTeams),
              "kTeamColors must have one entry per team that OPTIONS can assign");

struct BubbleArray {
    std::array<std::vector<Bubble>, 13> bubbleMap;
    SDL_Point bubbleOffset;
    Penguin penguinSprite;
    Shooter shooterSprite;
    int playerAssigned = 0, nextBubble = 0, curLaunch = 0, leftLimit = 0, rightLimit = 0, topLimit = 0, numSeparators = 0, turnsToCompress = 9, dangerZone = 12, explodeWait = EXPLODE_FRAMEWAIT,
        frozenWait = FROZEN_FRAMEWAIT, waitPrelight = PRELIGHT_SLOW, prelightTime = waitPrelight, framePrelight = PRELIGHT_FRAMEWAIT, hurryTimer = 0, warnTimer = 0, alertColumn = 0;
    int score = 0, chainLevel = 0;  // Score tracking and chain reaction multiplier
    bool shooterLeft = false, shooterRight = false, shooterCenter = false, shooterAction = false, newShoot = true, mpWinner = false, mpDone = false;
    float mouseTargetAngle = -1.f;  // -1 = inactive; set from mouse/touch position
    bool mouseFirePending = false;   // set on mouse click / touch-up

    // Player state for multiplayer (original: $pdata{$player}{state} and {left})
    enum class PlayerState { ALIVE, LOST, LEFT };
    PlayerState playerState = PlayerState::ALIVE;
    int lobbyPlayerId = -1;  // The lobby/network player ID (for mapping network messages to player arrays)
    std::string playerNickname = "";  // Player nickname for display
    int winCount = 0;  // Number of rounds won by this player
    // Round/match statistics (bubbles fired, bubbles popped, malus sent, malus received).
    // r* = current round (reset every round); m* = whole match (reset at NewGame).
    // In network games each client owns array 0's stats; remote arrays are filled from 'S' sync messages.
    int rFired = 0, rPopped = 0, rSent = 0, rRecv = 0, rKills = 0;
    int mFired = 0, mPopped = 0, mSent = 0, mRecv = 0, mKills = 0;
    // Array index of whoever last sent this player malus (-1 = never attacked this
    // round). Set at every real malus send site (network and local multiplayer);
    // ApplyPlayerLoss credits this player's attacker with a kill on death. Not
    // reset by who's currently ALIVE -- an attacker who has since died still gets
    // credit, matching "last attacker gets the kill" with no time limit.
    int lastAttackerIdx = -1;
    // Transient on-screen alerts: "who just sent you malus and how many" (fades out over time).
    struct MalusAlert { std::string fromNick; int count; int framesLeft; };
    std::vector<MalusAlert> malusAlerts;
    int numColors = 8;  // Number of bubble colors for this player (5-8)
    bool compressionDisabled = false;  // If true, rows never drop down for this player
    bool aimGuideEnabled = false;      // If true, draw aim trajectory guide for this player

    bool suppressFireUntilRelease = false;  // Block fire key for one frame after round transition

    // Bot control. A bot drives the same shooterLeft/Right/Action flags a
    // person's keyboard does rather than placing bubbles directly, so it is
    // subject to every rule a player is -- aim speed, the fire-release
    // interlock, the hurry timer -- and its penguin animates normally.
    bool isBot = false;
    int botSkill = 1;                 // index into BubbleAI::Skill
    unsigned botRng = 0;              // own stream, independent of the game's rand()
    float botTargetAngle = -1.0f;     // < 0 when it has not chosen a shot yet
    int botThinkFrames = 0;           // deliberate pause before it commits

    // Network game action flags (original: $actions{$player}{mp_fire} and {mp_stick})
    bool mpFirePending = false;  // Set to true when we receive 'f' message, cleared after firing
    float pendingAngle = 0.0f;   // The angle from the 'f' message
    bool mpStickPending = false; // Set to true when we receive 's' message, cleared after sticking
    int stickCx = 0, stickCy = 0, stickCol = 0;  // Stick position and color from 's' message

    // Stick effect animation (original: $sticking_bubble / $pdata{sticking_step})
    bool stickAnimActive = false;
    int stickAnimFrame = 0;
    int stickAnimSlowdown = 0;
    SDL_Point stickAnimPos = {0, 0};

    SDL_Rect compressorRct = {}, lGfxShooterRct = {}, curLaunchRct = {}, nextBubbleRct = {}, onTopRct = {}, frozenBottomRct = {}, hurryRct = {};
    SDL_Texture *hurryTexture = nullptr;
    SDL_Point scorePos = {10, 10};  // Score display position (original: $POS{scores})

    // Malus/attack system for multiplayer
    std::vector<int> malusQueue;  // Queue of malus bubbles to generate (frame numbers when awarded)

    // Perl-compatible next-colors queue (original: $pdata{$player}{nextcolors})
    // 8 upcoming bubble IDs, synced via 's' messages so all clients agree on new root row colors
    std::vector<int> nextColors;

    // >5-player network battle royale: player 0 keeps the case-5 center board and is
    // always visible; remote players are parked round-robin at the 4 case-5 mini-board
    // slot geometries and paged in/out of view via BubbleGame::netViewPage.
    bool boardVisible = true;  // Whether this array's board should be rendered this frame
    int parkedSlot = -1;       // -1 for player 0 / <=5-player games; 0-3 mini-slot index otherwise
    bool wasInDanger = false;  // last observed danger state; edge-detects auto view re-ranks
    // >5-player royale: frames left to blink this board's border after actually being
    // attacked (SendMalusToOpponent's send sites set it) -- distinct from sendMalusToOne,
    // which is the local player's persistent manual-target selection, not an attack event.
    // A single attack can hit several boards at once (the <=5-alive-players split
    // fallback), so each board tracks its own timer independently.
    int attackFlashFramesLeft = 0;

    std::vector<int> remainingBubbles() {
        std::vector<int> a;
        for (int i = 0; i < 13; i++) {
            for (const Bubble &bubble : bubbleMap[i]) {
                if (bubble.bubbleId != -1 && std::count(a.begin(), a.end(), bubble.bubbleId) == 0) a.push_back(bubble.bubbleId); 
            }
        }

        return a;
    }

    bool allClear() {
        for (int i = 0; i < 13; i++) {
            for (const Bubble &bubble : bubbleMap[i]) {
                if (bubble.bubbleId != -1) return false; 
            }
        }

        return true;
    }
    
    bool bubbleOnDanger() {
        for (int i = 0; i < 13; i++) {
            for (const Bubble &bubble : bubbleMap[i]) {
                if (i >= dangerZone && bubble.bubbleId != -1) return true; 
            }
        }

        return false;
    }

    void ExpandOffset(int ex, int ey) {
        bubbleOffset.x += ex;
        bubbleOffset.y += ey;
        topLimit += ey;
        leftLimit += ex;
        rightLimit += ex;

        for (int i = 0; i < 13; i++) {
            for (Bubble &bubble : bubbleMap[i]) {
                bubble.pos.x += ex;
                bubble.pos.y += ey;
            }
        }
    }

    // row/col reach here from a peer's `s`, `m`/`M` and malus payloads, none of
    // which are range-checked upstream, so this is the choke point that keeps an
    // arbitrary cell reference off the board. Drop an out-of-range placement:
    // a sender following the protocol never produces one, and a malformed one
    // must not index outside bubbleMap or the row's vector.
    void PlacePlayerBubble(int bubbleId, int row, int col) {
        if (row < 0 || row >= static_cast<int>(bubbleMap.size()))
            return;
        std::vector<Bubble> &cells = bubbleMap[row];
        if (col < 0 || col >= static_cast<int>(cells.size()))
            return;
        Bubble &bubble = cells[col];
        bubble.bubbleId = bubbleId;
        bubble.playerBubble = true;
    }
};

class BubbleGame final
{
public:
    BubbleGame(const SDL_Renderer *renderer);
    BubbleGame(const BubbleGame&) = delete;
    ~BubbleGame();

    void Render(void);
    void RenderPaused(void);
    void NewGame(SetupSettings setup);
    void HandleInput(SDL_Event *e);
    void HandleMouseAim(float mx, float my);  // logical canvas coords (0-640, 0-480)
    void HandleMouseFire();

    // True when a finger press/release pair is an unambiguous "go back" swipe
    // rather than an aim-and-fire. Touch is the only input with no way out of a
    // round otherwise: the keyboard has Escape, gamepads have B, and Android
    // has its own back button, but iOS has none of the three.
    //
    // The gesture has to start at or below the shooter barrel. HandleMouseAim
    // already ignores everything from there down, so that band is the one part
    // of the playfield where a long drag cannot also be someone aiming -- and
    // an accidental match here would quit a game in progress, not merely
    // misfire a bubble. All coordinates are logical canvas coords.
    bool IsTouchBackSwipe(float startX, float startY, float endX, float endY) const;
    void UpdatePenguin(BubbleArray &bArray);
    void DriveBot(BubbleArray &bArray);

    // True when this client simulates the array's physics and is the
    // authority for what happens on it: every array in a local game, the
    // local player in a network game, and any bot this client is hosting.
    // Remote players' boards are replayed from their messages instead, so
    // simulating them here would fight the sync rather than agree with it.
    bool OwnsArray(const BubbleArray &bArray) const {
        return !currentSettings.networkGame || bArray.playerAssigned == 0 || bArray.isBot;
    }
    bool OwnsArrayIndex(int idx) const {
        return idx >= 0 && idx < MAX_NET_PLAYERS && OwnsArray(bubbleArrays[idx]);
    }
    // Route one in-game payload to whichever connection speaks for this
    // array: the player's own, or the bot's separate socket. The server tags
    // messages by the connection they arrive on, so a bot's move cannot be
    // sent down the host's link.
    bool SendGameDataFor(const BubbleArray &bArray, const char *payload);

    void LoadLevelset(const char *path);
    void LoadLevel(int id);

    // Controller management for local multiplayer
    void InitControllers();
    void CloseControllers();

    // Network game methods
    void SendNetworkBubbleShot(BubbleArray &bArray);
    void ProcessNetworkMessages();

    bool playedPause = false;
    bool IsGameFinished() const { return gameFinish; }
    bool IsNetworkGame() const { return currentSettings.networkGame; }
    bool IsChatting() const { return chattingMode; }
    // Tap/click on the round-end screen in logical (640x480) coords.
    // Returns true when consumed by the CHAT button (so the caller must
    // not treat the tap as "next round").
    bool HandleFinishedTap(float lx, float ly);
private:
#ifdef FROZEN_BUBBLE_TEST_ACCESS
    friend struct BubbleGameTestAccess;

    // Keep the post-round decision path real in production-object tests while
    // stopping immediately before the expensive screen transition/board load.
    bool testCapturePostRoundTransition = false;
    bool testQuitToTitleRequested = false;
    int testReloadLevel = -1;
#endif

    const SDL_Renderer *renderer = nullptr;
    SDL_Texture *background = nullptr, *pauseBackground = nullptr, *prePauseBackground = nullptr;

    SDL_Texture *imgColorblindBubbles[BUBBLE_STYLES];
    SDL_Texture *imgBubbles[BUBBLE_STYLES];

    SDL_Texture *imgMiniColorblindBubbles[BUBBLE_STYLES];
    SDL_Texture *imgMiniBubbles[BUBBLE_STYLES];

    SDL_Texture *imgBubbleStick[BUBBLE_STICKFC + 1];
    SDL_Texture *imgMiniBubbleStick[BUBBLE_STICKFC + 1];

    SDL_Texture *imgBubbleFrozen, *imgMiniBubbleFrozen;
    SDL_Texture *imgBubblePrelight, *imgMiniBubblePrelight;

    SDL_Texture *pausePenguin[35];
    SDL_Texture *dotTexture[2];

    SDL_Texture *soloStatePanels[2];
    SDL_Texture *multiStatePanels[2];

    SDL_Texture *shooterTexture, *miniShooterTexture, *lowShooterTexture, *compressorTexture, *sepCompressorTexture, *onTopTexture, *miniOnTopTexture;

    // "Left" overlay textures for dead remote players (original: $imgbin{left_rp1}, etc.)
    SDL_Texture *leftRp1, *leftRp1Mini, *leftRp2Mini, *leftRp3Mini, *leftRp4Mini;

    // Single player targeting attack sprites (original: $imgbin{attack}{rp1..4}, $imgbin{attackme}{rp1..4})
    SDL_Texture *imgAttack[4] = {};    // attack_rp1..4.png - shown on targeted opponent
    SDL_Texture *imgAttackMe[4] = {};  // attackme_rp1..4.png - shown on local player when targeted

    SDL_Rect panelRct;

    bool lowGfx = false, gameWon = false, gameLost = false, gameFinish = false, firstRenderDone = false, gameMpDone = false;
    bool gameMatchOver = false; // Victories limit reached - match is over, return to lobby
    enum class RoundWinCause { Elimination, Clear, Departure, Remote };

    // True only when this round's win came from clearing the board (CheckGameState's
    // allClear() branch, or its network-derived equivalent) rather than from
    // an elimination path. Drives the clear-win banner/sound.
    bool wonByClearing = false;
    int roundWinnerIdx = -1;
    bool roundStatsFinalized = false; // Per-round stats rolled into match totals (and 'S' broadcast) once per round
    int roundsPlayed = 0;       // Completed rounds this match (gates the lobby summary)
    bool waitingForOpponentNewGame = false; // Waiting for opponents to press key for new game
    bool opponentReadyForNewGame = false; // Opponent sent 'n' ready signal
    int opponentsReadyCount = 0; // Number of opponents who sent 'n' (for 3+ player)
    int connectedPlayerCount = 0; // Players still connected (decremented when 'l' received)
#ifdef __WASM_PORT__
    Uint32 wasmRoundSyncWaitStart = 0; // WASM joiner: timestamp when waiting for Round 2+ sync messages
#endif

    int curLevel = 1, pauseFrame = 0, nextPauseUpd = 2;
    int winsP1 = 0, winsP2 = 0; // 2p mode stuff
    Uint32 timePaused = 0;
    int comboDisplayTimer = 0; // Timer for showing combo text
    int frameCount = 0;  // Global frame counter for malus timing
    int networkFrameCounter = 0; // Frame counter for network ping timing

    // Multiplayer training state
    Uint32 mpTrainStartTime = 0;  // SDL_GetTicks() when mp_train round started
    int mpTrainScore = 0;         // Accumulated score (malus destroyed)
    bool mpTrainDone = false;     // 2-minute timer expired

    // Single player targeting state (original: $pdata{sendmalustoone})
    int sendMalusToOne = -1;           // -1 = split to all, 1-4 = opponent bubbleArrays index
    std::vector<int> attackingMe;       // opponent array indices currently targeting local player (p1)
    // Who each player is targeting (-1 = all/none); no default-member-init since it's now
    // sized MAX_NET_PLAYERS -- reset by the NewGame/ReloadGame loops before first read.
    int playerTargeting[MAX_NET_PLAYERS];

    // >5-player battle royale: which page of 4 parked remote-player mini-boards is
    // currently visible (page 0 = slots/players 1-4, page 1 = 5-8, ...). Player 0's
    // center board is always visible regardless of page. See ApplyNetViewPage().
    int netViewPage = 0;
    // >5-player royale: true = auto slot selection (RankNetViewBoards picks the
    // 4 visible remote boards on events); false = manual Tab paging via netViewPage.
    bool netViewAuto = true;
    // >5-player royale, dead local player only: board index pinned into view by
    // keys 1-4 while spectating (-1 = none). ApplyNetViewAuto() overrides the
    // pinned board's slot-class pick and lazily clears the pin when the pinned
    // player dies or leaves.
    int netViewPinnedIdx = -1;
    bool pendingHighscore = false;      // A new highscore was earned, show screen after level completion
    std::array<std::vector<int>, 10> savedLevelGrid;  // Level grid saved at load time for highscore display

    SetupSettings currentSettings;
    AudioMixer *audMixer = nullptr;

    void PlaySFX(const char* id) {
        if (audMixer != nullptr) audMixer->PlaySFX(id);
    }

    TTFText inGameText, winsP1Text, winsP2Text, scoreText, comboText, finalScoreText, mpTrainText;
    TTFText clearWinText;    // "Board Cleared — <Name> Wins!" banner, shown when wonByClearing
    TTFText playerNameWinText[MAX_NET_PLAYERS];  // "PlayerName: WinCount" for each player (3-5 player mode)
    TTFText targetingText;   // Reused to render targeting indicators in MP mode
    TTFText statsText;       // Reused per cell to render the post-round stats table
    TTFText malusAlertText;  // Reused to render "incoming malus" toasts

    // In-game chat (network games only)
    struct InGameChatMsg { std::string nick; std::string text; int framesLeft; };
    std::vector<InGameChatMsg> inGameChatMessages;
    bool chattingMode = false;
    char chatInputBuf[256] = {};
    TTFText chatLineText;       // Reused per message line
    TTFText chatInputText;      // Input line ("Say: {text}_")
    SDL_Rect statsChatBtn = {0, 0, 0, 0}; // Tappable CHAT button on the round-end stats panel

    std::vector<std::array<std::vector<int>, 10>> loadedLevels;
    BubbleArray bubbleArrays[MAX_NET_PLAYERS]; //custom arrays wtih different players

    void ChooseFirstBubble(BubbleArray *bArray);
    void PickNextBubble(BubbleArray &bArray);
    void LaunchBubble(BubbleArray &bArray);
    void UpdateSingleBubbles(int id);
    void UpdateSingleBubblesAtScale(float deltaScale);

    void ExpandNewLane(BubbleArray &bArray);
    void Update2PText();
    void UpdatePlayerNameWinText();  // Update "PlayerName: WinCount" for 3-5 player mode
    void UpdateScoreText(BubbleArray &bArray);
    SDL_Texture** GetBubbleTextures(bool mini = false); // Returns appropriate bubble textures based on colorblind mode and size

    void CheckPossibleDestroy(BubbleArray &bArray);
    void AssignChainReactions(BubbleArray &bArray);  // Assign chain reaction targets to falling bubbles (original line 814-865)
    int CheckAirBubbles(BubbleArray &bArray);  // Returns falling bubble count
    void SendMalusToOpponent(int malusCount);   // Send malus attack to opponent
    void SetSendMalusToOne(int opponentIdx);    // Set/clear single-player targeting (original: set_sendmalustoone)
    void ProcessMalusQueue(BubbleArray &bArray, int currentFrame);  // Generate malus bubbles from queue
    void CheckGameState(BubbleArray &bArray, bool countForRoot = true);
    void AddMalusAlert(BubbleArray &target, const std::string &fromNick, int count);  // Queue an incoming-malus toast
    void RenderMalusAlerts(SDL_Renderer *rend);  // Draw + age the incoming-malus toasts
    void FinalizeRoundStats();   // Roll per-round stats into match totals; broadcast 'S' in network games
    void RenderRoundStats(SDL_Renderer *rend);  // Post-round per-player stats table overlay
    void RenderMultiplayerResultPanel(SDL_Renderer *rend);
    void UpdateMultiplayerCompletionState();
    void SendLobbyMatchSummary();  // Leader posts the match summary to the lobby chatroom
    void StartInGameChat();
    void FinishInGameChat(bool sendMessage);
    int CountLivingPlayers();  // Count players still alive (original: living_players() at line 600)
    int CountLivingTeams();   // Count distinct teams with at least one alive player
    void ApplyPlayerLoss(BubbleArray& player);
    void ResolveDangerZoneLosses();
    void ResolveRoundOutcome(
        int assertedWinnerIdx = -1,
        RoundWinCause cause = RoundWinCause::Elimination,
        bool sendNetworkFinish = false);
    void CommitRoundWin(int winnerIdx, RoundWinCause cause, bool sendNetworkFinish);
    void FinishRoundAsDraw();
    void UpdateDepartureMatchTermination();
    void HandlePlayerDeparture(int playerIdx);
    bool HasDepartedPlayers() const;
    int CountConnectedPlayers() const;
    int CountConnectedTeams() const;

    void DoFrozenAnimation(BubbleArray &bArray, int &waitTime);
    void DoWinAnimation(BubbleArray &bArray, int &waitTime);
    void DoPrelightAnimation(BubbleArray &bArray, int &waitTime);

    void RandomLevel(BubbleArray &bArray);
    bool SyncNetworkLevel();  // Synchronize level for network multiplayer; returns false on sync failure
    void ReloadGame(int level);
    void SubmitScore(BubbleArray &bArray);

    // >5-player battle royale view paging: assigns BubbleArray::boardVisible for the
    // current netViewPage. <=5-player games always show every board (page 0, all visible).
    void ApplyNetViewPage();
    void CycleNetViewPage();  // Tab: advance to the next page of remote boards; no-op for <=5 players
    void ApplyNetViewAuto();  // auto mode: rank boards and assign boardVisible per slot class
    void ReRankNetView();     // event hook: re-run auto ranking (no-op when manual/<=5/local)

    void RenderRoyaleHud(SDL_Renderer *rend);  // >5-player-only: alive count + page indicator

    void QuitToTitle();

    // Controllers for local multiplayer (up to 5 players)
    SDL_Gamepad* controllers[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    int numControllersOpen = 0;
};

#endif // BUBBLEGAME_H
