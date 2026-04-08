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

#ifndef BUBBLEGAME_H
#define BUBBLEGAME_H

#include "audiomixer.h"
#include "platform.h"
#define PI 3.1415926535897932384626433832795028841972

#include <SDL3/SDL.h>
#include "sdl3_compat.h"
#include "ttftext.h"
#include "networkclient.h"

#include <vector>
#include <array>
#include <algorithm>

#pragma region "BubbleGame Defines"
#define TIME_APPEARS_NEW_ROOT_MP 11
#define TIME_HURRY_WARN_MP 750
#define TIME_HURRY_MAX_MP 1125

#define TIME_APPEARS_NEW_ROOT 8
#define TIME_HURRY_WARN 720
#define TIME_HURRY_MAX 1182

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
// by FrozenBubble::deltaScale (native ~1.25x, browser 3.0x normalized).
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
    SDL_Texture* handle[PENGUIN_HANDLEFC];
    SDL_Texture* wait[PENGUIN_WAITFC];
    SDL_Texture* win[PENGUIN_WINFC];
    SDL_Texture* lose[PENGUIN_LOSEFC];
    // curAnimation up to 13 values
    int curAnimation = 0, curFrame = 1, waitFrame = 0;
    int sleeping = 0;
    bool invertAnimation = false;
    SDL_Renderer *rend;
    SDL_Rect rect;

    void LoadPenguin(SDL_Renderer* renderer, char *whichOne, SDL_Rect rct) {
        rend = renderer;
        rect = rct;

        char rel[256];
        for (int i = 0; i < PENGUIN_HANDLEFC; i++) {
            snprintf(rel, sizeof(rel), "/gfx/pinguins/anime-shooter_%s_%04d.png", whichOne, i + 1);
            handle[i] = IMG_LoadTexture(renderer, ASSET(rel).c_str());
        }
        for (int i = 0; i < PENGUIN_WAITFC; i++) {
            snprintf(rel, sizeof(rel), "/gfx/pinguins/wait_%s_%04d.png", whichOne, i + 1);
            wait[i] = IMG_LoadTexture(renderer, ASSET(rel).c_str());
        }
        for (int i = 0; i < PENGUIN_WINFC; i++) {
            snprintf(rel, sizeof(rel), "/gfx/pinguins/win_%s_%04d.png", whichOne, i + 1);
            win[i] = IMG_LoadTexture(renderer, ASSET(rel).c_str());
        }
        for (int i = 0; i < PENGUIN_LOSEFC; i++) {
            snprintf(rel, sizeof(rel), "/gfx/pinguins/loose_%s_%04d.png", whichOne, i + 1);
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
    int bubbleId; // id to use bubble image
    SDL_Point pos; // current position, top left aligned
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
    SDL_Texture *texture;
    SDL_Renderer *renderer;
    float angle = PI/2.0f;
    SDL_Rect rect = {};
    SDL_Rect lowRct = {};

    void Render(bool /*lowGfx*/){
        // Always render the cannon with rotation regardless of gfx quality
        double degrees = -(((angle*CANON_ROTATIONS)/(PI/2.0f) + 0.5) - CANON_ROTATIONS);
        { SDL_FRect fr = ToFRect(rect); SDL_RenderTextureRotated(renderer, texture, nullptr, &fr, degrees, NULL, SDL_FLIP_NONE); }
    }
};

struct SetupSettings {
    bool chainReaction = false;
    int playerCount = 1;
    bool networkGame = false;
    bool randomLevels = false;
    bool singlePlayerTargetting = false;
    int startLevel = 1;
    bool mpTraining = false;  // 1P multiplayer training mode (timed, score-based)
    bool localMultiplayer = false;  // True for local controller-based multiplayer
    int playerColors[5] = {7, 7, 7, 7, 7};  // Per-player color count (5-8)
    bool disableCompression[5] = {false, false, false, false, false};  // Per-player: skip row compression
    bool aimGuide[5] = {false, false, false, false, false};  // Per-player: show aim trajectory guide
    int victoriesLimit = 0;  // 0 = unlimited; >0 = first to reach this wins the match
};

struct BubbleArray {
    std::array<std::vector<Bubble>, 13> bubbleMap;
    SDL_Point bubbleOffset;
    Penguin penguinSprite;
    Shooter shooterSprite;
    int playerAssigned, nextBubble, curLaunch, leftLimit, rightLimit, topLimit, numSeparators, turnsToCompress = 9, dangerZone = 12, explodeWait = EXPLODE_FRAMEWAIT,
        frozenWait = FROZEN_FRAMEWAIT, waitPrelight = PRELIGHT_SLOW, prelightTime = waitPrelight, framePrelight = PRELIGHT_FRAMEWAIT, hurryTimer = 0, warnTimer = 0, alertColumn = 0;
    int score = 0, chainLevel = 0;  // Score tracking and chain reaction multiplier
    bool shooterLeft = false, shooterRight = false, shooterCenter = false, shooterAction = false, newShoot = true, mpWinner = false, mpDone = false;

    // Player state for multiplayer (original: $pdata{$player}{state} and {left})
    enum class PlayerState { ALIVE, LOST, LEFT };
    PlayerState playerState = PlayerState::ALIVE;
    int lobbyPlayerId = -1;  // The lobby/network player ID (for mapping network messages to player arrays)
    std::string playerNickname = "";  // Player nickname for display
    int winCount = 0;  // Number of rounds won by this player
    int numColors = 7;  // Number of bubble colors for this player (5-8)
    bool compressionDisabled = false;  // If true, rows never drop down for this player
    bool aimGuideEnabled = false;      // If true, draw aim trajectory guide for this player

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

    SDL_Rect compressorRct, lGfxShooterRct, curLaunchRct, nextBubbleRct, onTopRct, frozenBottomRct, hurryRct;
    SDL_Texture *hurryTexture;
    SDL_Point scorePos = {10, 10};  // Score display position (original: $POS{scores})

    // Malus/attack system for multiplayer
    std::vector<int> malusQueue;  // Queue of malus bubbles to generate (frame numbers when awarded)

    // Perl-compatible next-colors queue (original: $pdata{$player}{nextcolors})
    // 8 upcoming bubble IDs, synced via 's' messages so all clients agree on new root row colors
    std::vector<int> nextColors;

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

    void PlacePlayerBubble(int bubbleId, int row, int col) {
        Bubble &bubble = bubbleMap[row][col];
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
    void UpdatePenguin(BubbleArray &bArray);

    void LoadLevelset(const char *path);
    void LoadLevel(int id);

    // Controller management for local multiplayer
    void InitControllers();
    void CloseControllers();

    // Network game methods
    void SendNetworkBubbleShot(BubbleArray &bArray);
    void ProcessNetworkMessages();

    bool playedPause = false;
private:
    const SDL_Renderer *renderer;
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
    bool waitingForOpponentNewGame = false; // Waiting for opponents to press key for new game
    bool opponentReadyForNewGame = false; // Opponent sent 'n' ready signal
    int opponentsReadyCount = 0; // Number of opponents who sent 'n' (for 3+ player)
    int connectedPlayerCount = 0; // Players still connected (decremented when 'l' received)
    Uint32 wasmRoundSyncWaitStart = 0; // WASM joiner: timestamp when waiting for Round 2+ sync messages

    int curLevel = 1, pauseFrame = 0, nextPauseUpd = 2, idxMPWinner = 0;
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
    int playerTargeting[5] = {-1, -1, -1, -1, -1};  // Who each player is targeting (-1 = all/none)
    bool pendingHighscore = false;      // A new highscore was earned, show screen after level completion
    std::array<std::vector<int>, 10> savedLevelGrid;  // Level grid saved at load time for highscore display

    SetupSettings currentSettings;
    AudioMixer *audMixer;

    TTFText inGameText, winsP1Text, winsP2Text, scoreText, comboText, finalScoreText, mpTrainText;
    TTFText playerNameWinText[5];  // "PlayerName: WinCount" for each player (3-5 player mode)
    TTFText targetingText;   // Reused to render targeting indicators in MP mode

    // In-game chat (network games only)
    struct InGameChatMsg { std::string nick; std::string text; int framesLeft; };
    std::vector<InGameChatMsg> inGameChatMessages;
    bool chattingMode = false;
    char chatInputBuf[256] = {};
    TTFText chatLineText;       // Reused per message line
    TTFText chatInputText;      // Input line ("Say: {text}_")

    std::vector<std::array<std::vector<int>, 10>> loadedLevels;
    BubbleArray bubbleArrays[5]; //5 custom arrays wtih different players

    void ChooseFirstBubble(BubbleArray *bArray);
    void PickNextBubble(BubbleArray &bArray);
    void LaunchBubble(BubbleArray &bArray);
    void UpdateSingleBubbles(int id);

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
    void CheckGameState(BubbleArray &bArray);
    int CountLivingPlayers();  // Count players still alive (original: living_players() at line 600)
    void HandlePlayerLoss(BubbleArray &bArray);  // Handle player death and check win conditions

    void DoFrozenAnimation(BubbleArray &bArray, int &waitTime);
    void DoWinAnimation(BubbleArray &bArray, int &waitTime);
    void DoPrelightAnimation(BubbleArray &bArray, int &waitTime);

    void RandomLevel(BubbleArray &bArray);
    bool SyncNetworkLevel();  // Synchronize level for network multiplayer; returns false on sync failure
    void ReloadGame(int level);
    void SubmitScore(BubbleArray &bArray);

    void QuitToTitle();

    // Controllers for local multiplayer (up to 5 players)
    SDL_Gamepad* controllers[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    int numControllersOpen = 0;
};

#endif // BUBBLEGAME_H