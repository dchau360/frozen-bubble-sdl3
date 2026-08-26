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

#include "frozenbubble.h"
#include "bubblegame.h"
#include "audiomixer.h"
#include "highscoremanager.h"
#include "transitionmanager.h"
#include "gamesettings.h"
#include "platform.h"
#include "netview.h"

#include <fstream>
#include <sstream>
#include <set>
#include <queue>
#include <map>

#include <cmath>
#include <algorithm>
#include "bubblegame_internal.h"

std::vector<SingleBubble> singleBubbles;
std::vector<MalusBubble> malusBubbles;

BubbleGame::BubbleGame(const SDL_Renderer *renderer) 
    : renderer(renderer)
{
    // We mostly just load images here. Everything else should be setup in NewGame() instead.
    SDL_Renderer *rend = const_cast<SDL_Renderer*>(renderer);

    char rel[256];
    for (int i = 1; i <= BUBBLE_STYLES; i++)
    {
        snprintf(rel, sizeof(rel), "/gfx/balls/bubble-%d.gif", i);
        imgBubbles[i - 1] = IMG_LoadTexture(rend, ASSET(rel).c_str());
        if (!imgBubbles[i - 1]) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load bubble %d: %s (path: %s)", i, SDL_GetError(), ASSET(rel).c_str());
        snprintf(rel, sizeof(rel), "/gfx/balls/bubble-colourblind-%d.gif", i);
        imgColorblindBubbles[i - 1] = IMG_LoadTexture(rend, ASSET(rel).c_str());
        snprintf(rel, sizeof(rel), "/gfx/balls/bubble-%d-mini.png", i);
        imgMiniBubbles[i - 1] = IMG_LoadTexture(rend, ASSET(rel).c_str());
        snprintf(rel, sizeof(rel), "/gfx/balls/bubble-colourblind-%d-mini.png", i);
        imgMiniColorblindBubbles[i - 1] = IMG_LoadTexture(rend, ASSET(rel).c_str());
    }

    for (int i = 0; i <= BUBBLE_STICKFC; i++) {
        snprintf(rel, sizeof(rel), "/gfx/balls/stick_effect_%d.png", i);
        imgBubbleStick[i] = IMG_LoadTexture(rend, ASSET(rel).c_str());
        snprintf(rel, sizeof(rel), "/gfx/balls/stick_effect_%d-mini.png", i);
        imgMiniBubbleStick[i] = IMG_LoadTexture(rend, ASSET(rel).c_str());
    }

    for (int i = 0; i < 35; i++) {
        snprintf(rel, sizeof(rel), "/gfx/pause_%04d.png", i + 1);
        pausePenguin[i] = IMG_LoadTexture(rend, ASSET(rel).c_str());
    }

    imgBubbleFrozen = IMG_LoadTexture(rend, ASSET("/gfx/balls/bubble_lose.png").c_str());
    imgMiniBubbleFrozen = IMG_LoadTexture(rend, ASSET("/gfx/balls/bubble_lose-mini.png").c_str());

    imgBubblePrelight = IMG_LoadTexture(rend, ASSET("/gfx/balls/bubble_prelight.png").c_str());
    imgMiniBubblePrelight = IMG_LoadTexture(rend, ASSET("/gfx/balls/bubble_prelight-mini.png").c_str());

    shooterTexture = IMG_LoadTexture(rend, ASSET("/gfx/shooter.png").c_str());
    miniShooterTexture = IMG_LoadTexture(rend, ASSET("/gfx/shooter-mini.png").c_str());
    lowShooterTexture = IMG_LoadTexture(rend, ASSET("/gfx/shooter-lowgfx.png").c_str());

    compressorTexture = IMG_LoadTexture(rend, ASSET("/gfx/compressor_main.png").c_str());
    sepCompressorTexture = IMG_LoadTexture(rend, ASSET("/gfx/compressor_ext.png").c_str());

    onTopTexture = IMG_LoadTexture(rend, ASSET("/gfx/on_top_next.png").c_str());
    miniOnTopTexture = IMG_LoadTexture(rend, ASSET("/gfx/on_top_next-mini.png").c_str());

    // Load attack/attackme sprites for single player targeting (original lines 2925-2926)
    // attack_rp{n}.png = shown on the targeted opponent's board
    // attackme_rp{n}.png = shown on local player's board when being targeted
    for (int i = 1; i <= 4; i++) {
        snprintf(rel, sizeof(rel), "/gfx/attack_rp%d.png", i);
        imgAttack[i - 1] = IMG_LoadTexture(rend, ASSET(rel).c_str());
        snprintf(rel, sizeof(rel), "/gfx/attackme_rp%d.png", i);
        imgAttackMe[i - 1] = IMG_LoadTexture(rend, ASSET(rel).c_str());
    }

    // Load "left" overlay images for dead remote players (original line 2872-2873)
    leftRp1 = IMG_LoadTexture(rend, ASSET("/gfx/left-rp1.png").c_str());
    leftRp1Mini = IMG_LoadTexture(rend, ASSET("/gfx/left-rp1-mini.png").c_str());
    leftRp2Mini = IMG_LoadTexture(rend, ASSET("/gfx/left-rp2-mini.png").c_str());
    leftRp3Mini = IMG_LoadTexture(rend, ASSET("/gfx/left-rp3-mini.png").c_str());
    leftRp4Mini = IMG_LoadTexture(rend, ASSET("/gfx/left-rp4-mini.png").c_str());

    dotTexture[0] = IMG_LoadTexture(rend, ASSET("/gfx/dot_green.png").c_str());
    dotTexture[1] = IMG_LoadTexture(rend, ASSET("/gfx/dot_red.png").c_str());

    soloStatePanels[0] = IMG_LoadTexture(rend, ASSET("/gfx/lose_panel.png").c_str());
    soloStatePanels[1] = IMG_LoadTexture(rend, ASSET("/gfx/win_panel_1player.png").c_str());

    multiStatePanels[0] = IMG_LoadTexture(rend, ASSET("/gfx/win_panel_p1.png").c_str());
    multiStatePanels[1] = IMG_LoadTexture(rend, ASSET("/gfx/win_panel_p2.png").c_str());

    pauseBackground = IMG_LoadTexture(rend, ASSET("/gfx/back_paused.png").c_str());

    inGameText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 20);
    inGameText.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
    inGameText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});

    // Without a font, UpdateText early-returns before creating outTexture or
    // writing coords.w/h, so the multiplayer targeting indicator rendered a null
    // texture through an indeterminate rect and never appeared at all
    // (audit finding BUG-043). Smaller than inGameText: it sits just above or
    // below a player's shooter and has to fit beside the mini boards.
    targetingText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 12);
    targetingText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});

    winsP1Text.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 20);
    winsP1Text.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
    winsP1Text.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});

    winsP2Text.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 20);
    winsP2Text.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
    winsP2Text.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});

    scoreText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 24);
    scoreText.UpdateAlignment(TTF_HORIZONTAL_ALIGN_LEFT);
    scoreText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});

    comboText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 32);
    comboText.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
    comboText.UpdateColor({255, 255, 0, 255}, {0, 0, 0, 255}); // Yellow text

    clearWinText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 32);
    clearWinText.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
    clearWinText.UpdateColor({255, 255, 0, 255}, {0, 0, 0, 255}); // Yellow text

    finalScoreText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 28);
    finalScoreText.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
    finalScoreText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});

    mpTrainText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 16);
    mpTrainText.UpdateColor({255, 255, 100, 255}, {0, 0, 0, 255});

    // Initialize player name/win text for multiplayer (3-5 players)
    // Player 0 (center/local) gets larger font (22), others get smaller font (16)
    for (int i = 0; i < MAX_NET_PLAYERS; i++) {
        int fontSize = (i == 0) ? 22 : 16;
        playerNameWinText[i].LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), fontSize);
        playerNameWinText[i].UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
        playerNameWinText[i].UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});
    }

    // In-game chat text (white on transparent — overlay drawn separately)
    chatLineText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 14);
    chatLineText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 0});
    chatInputText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 14);
    chatInputText.UpdateColor({255, 255, 100, 255}, {0, 0, 0, 0});  // Yellow for input

    statsText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 14);
    statsText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 0});

    malusAlertText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 16);
    malusAlertText.UpdateColor({255, 140, 40, 255}, {0, 0, 0, 0});  // Orange "incoming malus" toast
}


BubbleGame::~BubbleGame() {
    CloseControllers();
    SDL_DestroyTexture(background);
    SDL_DestroyTexture(pauseBackground);
    for (int i = 0; i < BUBBLE_STYLES; i++)  {
        SDL_DestroyTexture(imgBubbles[i]);
        SDL_DestroyTexture(imgColorblindBubbles[i]);
        SDL_DestroyTexture(imgMiniBubbles[i]);
        SDL_DestroyTexture(imgMiniColorblindBubbles[i]);
    }
    for (int i = 0; i < MAX_NET_PLAYERS; i++) {
        if (bubbleArrays[i].hurryTexture) SDL_DestroyTexture(bubbleArrays[i].hurryTexture);
    }
}


void BubbleGame::InitControllers() {
    CloseControllers();
    numControllersOpen = 0;
    int numIds = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&numIds);
    SDL_Log("InitControllers: %d gamepad(s) detected", numIds);
    if (ids) {
        for (int i = 0; i < numIds && numControllersOpen < 5; i++) {
            SDL_Gamepad *gp = SDL_OpenGamepad(ids[i]);
            if (gp) {
                controllers[numControllersOpen++] = gp;
                SDL_Log("Opened controller %d: %s", numControllersOpen, SDL_GetGamepadName(gp));
            }
        }
        SDL_free(ids);
    }
    SDL_Log("InitControllers: opened %d controller(s)", numControllersOpen);
}


void BubbleGame::CloseControllers() {
    for (int i = 0; i < 5; i++) {
        if (controllers[i]) {
            SDL_CloseGamepad(controllers[i]);
            controllers[i] = nullptr;
        }
    }
    numControllersOpen = 0;
}


void SetupGameMetrics(BubbleArray *bArray, int playerCount, bool lowGfx){
    if (playerCount > 5) {
        // >5-player battle royale: layout positions (curLaunchRct/nextBubbleRct/onTopRct/
        // frozenBottomRct/etc.) are set in NewGame/ReloadGame's default: case, mirroring how
        // the 3-5 player cases below leave those rects alone. Only lowGfx shooter sizing
        // needs to happen here -- falling through to the case-1/default arm below would
        // overwrite the royale center board's rects with single-player values.
        if (lowGfx) {
            for (int i = 0; i < playerCount; i++) {
                bArray[i].lGfxShooterRct.w = bArray[i].lGfxShooterRct.h = 2;
            }
        }
        return;
    }
    switch (playerCount) {
        case 2:
            if (lowGfx) {
                bArray[0].lGfxShooterRct.w = bArray[0].lGfxShooterRct.h = 2;
                bArray[1].lGfxShooterRct.w = bArray[1].lGfxShooterRct.h = 2;
            }
            // Board center is (354+610)/2=482 (leftLimit/rightLimit set in
            // NewGame's case 2, matching the original Perl's %POS_2P p1 and
            // bubbleOffset.x=354's own grid exactly) -- curLaunchRct/
            // nextBubbleRct center on that (482-16=466), matching the
            // original's tobe_launched formula ((left_limit+right_limit)/2 -
            // BUBBLE_SIZE/2); onTopRct follows next_bubble + Perl's
            // on_top_next_relpos.x=-4.
            bArray[0].curLaunchRct = {SCREEN_CENTER_X+146, 480-89, 32, 32};
            bArray[0].nextBubbleRct = {SCREEN_CENTER_X+146, 480-40, 32, 32};
            bArray[0].onTopRct = {SCREEN_CENTER_X+142, 480-43, 39, 39};
            bArray[0].frozenBottomRct = {SCREEN_CENTER_X+141, 480-43, 39, 39};

            bArray[1].curLaunchRct = {SCREEN_CENTER_X-176, 480-89, 32, 32};
            bArray[1].nextBubbleRct = {SCREEN_CENTER_X-176, 480-40, 32, 32};
            bArray[1].onTopRct = {SCREEN_CENTER_X-179, 480-43, 39, 39};
            bArray[1].frozenBottomRct = {SCREEN_CENTER_X-180, 480-43, 39, 39};
            break;
        case 3:
        case 4:
        case 5:
            // 3-5 player layouts - positions are set in NewGame/ReloadGame switch cases
            // Only need to set lowGfx shooter sizes here
            if (lowGfx) {
                for (int i = 0; i < playerCount; i++) {
                    bArray[i].lGfxShooterRct.w = bArray[i].lGfxShooterRct.h = 2;
                }
            }
            // curLaunchRct, nextBubbleRct, onTopRct, frozenBottomRct are NOT set here
            // because each player has different positions based on their layout
            // These will need to be set in the NewGame/ReloadGame switch cases
            break;
        case 1:
        default:
            if (lowGfx) bArray[0].lGfxShooterRct.w = bArray[0].lGfxShooterRct.h = 2;
            bArray[0].compressorRct = {SCREEN_CENTER_X - 128, -5 + (28 * bArray[0].numSeparators), 252, 56};
            bArray[0].curLaunchRct = {SCREEN_CENTER_X - 16, 480 - 89, 32, 32};
            bArray[0].nextBubbleRct = {SCREEN_CENTER_X - 16, 480 - 40, 32, 32};
            bArray[0].onTopRct = {SCREEN_CENTER_X - 19, 480 - 43, 39, 39};
            bArray[0].frozenBottomRct = {SCREEN_CENTER_X - 18, 480 - 42, 34, 48};
            break;
    }
}


static void ResetRoundInputState(BubbleArray &player) {
    player.shooterLeft = player.shooterRight = player.shooterCenter = false;
    player.shooterAction = false;
    player.newShoot = true;
    player.mouseTargetAngle = -1.f;
    player.mouseFirePending = false;
    player.suppressFireUntilRelease = true;
    player.mpFirePending = false;
    player.pendingAngle = PI / 2.0f;
    player.mpStickPending = false;
    player.stickCx = player.stickCy = player.stickCol = 0;
    player.stickAnimActive = false;
    player.stickAnimFrame = player.stickAnimSlowdown = 0;
    player.stickAnimPos = {0, 0};
    player.wasInDanger = false;
    player.attackFlashFramesLeft = 0;
    player.rKills = 0;
    player.lastAttackerIdx = -1;
}


// Applies the 5P-layout mini-board geometry for slot 0-3 (rp1-rp4). Values are verbatim
// copies of the case-5 block's per-slot geometry above (bubbleArrays[1] through
// bubbleArrays[4] in NewGame's case 5); do not "normalize" the small hand-authored offsets
// between slots. Textures other than shooterTexture (i.e. hurryTexture) are set by the
// caller inline, matching how every other player-count case in this file loads them.
static void ApplyMiniSlotGeometry(BubbleArray& b, int slot, SDL_Renderer* rend,
                                  SDL_Texture* shooterTexture) {
    struct SlotGeo {
        SDL_Rect penguin, shooter, hurry, curLaunch, nextBubble, onTop, frozenBottom;
        SDL_Point offset; int left, right, top;
    };
    static const SlotGeo kSlots[4] = {
        // slot 0 = rp1 top-left
        {{94,211,40,30},{59,175,50,50},{5,128,122,51},{68,192,32,32},{76,216,32,32},{74,214,39,39},{74,214,39,39},{20,19},20,148,19},
        // slot 1 = rp2 top-right
        {{94,211,40,30},{531,175,50,50},{5,128,122,51},{540,192,32,32},{548,216,32,32},{546,214,39,39},{546,214,39,39},{492,19},492,620,19},
        // slot 2 = rp3 bottom-left
        {{94,439,40,30},{59,404,50,50},{5,345,122,51},{68,420,32,32},{76,445,32,32},{74,443,39,39},{74,443,39,39},{20,247},20,148,247},
        // slot 3 = rp4 bottom-right
        {{94,439,40,30},{531,404,50,50},{5,345,122,51},{540,420,32,32},{548,445,32,32},{546,443,39,39},{546,443,39,39},{492,247},492,620,247},
    };
    const SlotGeo& g = kSlots[slot];
    b.penguinSprite.LoadPenguin(rend, "p2", g.penguin);
    b.shooterSprite = {shooterTexture, rend};
    b.shooterSprite.rect = g.shooter;
    b.shooterSprite.angle = PI/2.0f;
    b.bubbleOffset = g.offset;
    b.leftLimit = g.left; b.rightLimit = g.right; b.topLimit = g.top;
    b.hurryRct = g.hurry;
    b.curLaunchRct = g.curLaunch; b.nextBubbleRct = g.nextBubble;
    b.onTopRct = g.onTop; b.frozenBottomRct = g.frozenBottom;
    b.numSeparators = 0;
    b.turnsToCompress = 12;
}


void BubbleGame::NewGame(SetupSettings setup) {
    // Clear any stale controller input state from previous session
    for (int i = 0; i < 5; i++) controllerInputs[i] = {};
    memset(virtualKeyState, 0, sizeof(virtualKeyState));
    audMixer = AudioMixer::Instance();
    SDL_Renderer *rend = const_cast<SDL_Renderer*>(renderer);
    currentSettings = setup;
    currentSettings.mouseEnabled = GameSettings::Instance()->mouseEnabled;

    lowGfx = GameSettings::Instance()->gfxLevel() > 2;

    SDL_Log("NewGame: chainReaction=%d, playerCount=%d, networkGame=%d, randomLevels=%d, lowGfx=%d, gfxLevel=%d",
            currentSettings.chainReaction, currentSettings.playerCount,
            currentSettings.networkGame, currentSettings.randomLevels,
            lowGfx, GameSettings::Instance()->gfxLevel());

    if (background != nullptr) SDL_DestroyTexture(background);

    // Every switch case below reloads hurry_pN.png into whichever player slots
    // it uses; destroy the previous match's textures first so restarting a
    // game (or shrinking the player count) doesn't leak one per player per
    // match start (BUG-042).
    for (int i = 0; i < MAX_NET_PLAYERS; i++) {
        if (bubbleArrays[i].hurryTexture) {
            SDL_DestroyTexture(bubbleArrays[i].hurryTexture);
            bubbleArrays[i].hurryTexture = nullptr;
        }
    }

    // Reset game state flags
    gameFinish = gameWon = gameLost = gameMatchOver = false;
    wonByClearing = false;
    roundWinnerIdx = -1;
    gameMpDone = false;
    sendMalusToOne = -1;
    attackingMe.clear();

    // Next-round handshake state must not leak in from a previous match: if the
    // last game was quit while waiting on the stats screen, a stale
    // waitingForOpponentNewGame would silently swallow this match's 'n' sends.
    waitingForOpponentNewGame = false;
    opponentReadyForNewGame = false;
    opponentsReadyCount = 0;
    netViewPage = 0;
    netViewAuto = true;
    netViewPinnedIdx = -1;
    for (int i = 0; i < MAX_NET_PLAYERS; i++) playerTargeting[i] = -1;
    for (int i = 0; i < currentSettings.playerCount; i++) ResetRoundInputState(bubbleArrays[i]);
    pendingHighscore = false;
    curLevel = setup.startLevel;
    connectedPlayerCount = setup.playerCount;  // Reset connected count for new game

    // Reset multiplayer training state
    mpTrainScore = 0;
    mpTrainDone = false;
    mpTrainStartTime = 0;

    winsP1 = winsP2 = 0;
    roundStatsFinalized = false;
    roundsPlayed = 0;
    for (int i = 0; i < MAX_NET_PLAYERS; i++) {
        bubbleArrays[i].winCount = 0;
        // Reset both round and match statistics at the start of a new match.
        bubbleArrays[i].rFired = bubbleArrays[i].rPopped = bubbleArrays[i].rSent = bubbleArrays[i].rRecv = 0;
        bubbleArrays[i].mFired = bubbleArrays[i].mPopped = bubbleArrays[i].mSent = bubbleArrays[i].mRecv = 0;
        bubbleArrays[i].rKills = bubbleArrays[i].mKills = 0;
        // Apply per-player color count (5-8); default 8 for single player
        int nc = (setup.playerCount >= 2) ? setup.playerColors[i] : 8;
        nc = (nc < 5) ? 5 : (nc > 8) ? 8 : nc;
        bubbleArrays[i].numColors = nc;
        bubbleArrays[i].compressionDisabled = setup.disableCompression[i];
        bubbleArrays[i].aimGuideEnabled = setup.aimGuide[i];
        bubbleArrays[i].isBot = setup.playerIsBot[i];
        bubbleArrays[i].botSkill = setup.botSkill;
        // Seed per slot so bots in the same game diverge immediately instead
        // of all opening with the same shot.
        bubbleArrays[i].botRng =
            static_cast<unsigned>(SDL_GetTicks()) * 2654435761u + static_cast<unsigned>(i) * 40503u + 1u;
        bubbleArrays[i].botTargetAngle = -1.0f;
        bubbleArrays[i].botThinkFrames = 0;
    }

    // Initialize controllers for local multiplayer
    // On Android, opening a TV remote as SDL_Gamepad captures its d-pad events,
    // causing SDL_GetKeyboardState to stop seeing them. Use keyboard-only on Android.
    if (currentSettings.localMultiplayer) {
#ifndef __ANDROID__
        InitControllers();
#endif
        SDL_Log("Local multiplayer: %d controllers opened, need %d",
                numControllersOpen, currentSettings.playerCount);
    }

    // Local multiplayer 2x2 equal-size grid layout for 2-5 players
    // Each quadrant is 320x240; we use mini bubbles (16px) centered in each quadrant
    // 2-player uses top-left and top-right
    // 3-player uses top-left, top-right, bottom-left (bottom-right quadrant is empty)
    // 4-player uses all 4 quadrants
    // 5-player uses all 4 quadrants plus center player (full size)
    // Local multiplayer (2-4 players) uses the same visual layouts as network equivalents:
    // - 2P: backgrnd.png, side-by-side full grids (case 2)
    // - 3P: back_multiplayer.png, center(P1) + TL+TR mini (case 3)
    // - 4P: back_multiplayer.png, center(P1) + TL+TR+BL mini (case 4)
    // This falls through to the switch below; no special block needed.

    SDL_Log("NewGame: Entering switch with playerCount=%d", currentSettings.playerCount);
    switch (currentSettings.playerCount) {
        case 1:
            SDL_Log("NewGame: Case 1 - Single player");
            background = IMG_LoadTexture(rend, ASSET("/gfx/back_one_player.png").c_str());
            bubbleArrays[0].penguinSprite.LoadPenguin(rend, "p1", {SCREEN_CENTER_X + 84, 480 - 60, 80, 60});
            bubbleArrays[0].shooterSprite = {shooterTexture, rend};
            bubbleArrays[0].shooterSprite.rect = {SCREEN_CENTER_X - 50, 480 - 123, 100, 100};
            bubbleArrays[0].shooterSprite.angle = PI/2.0f;
            bubbleArrays[0].bubbleOffset = {190, 51};
            bubbleArrays[0].leftLimit = SCREEN_CENTER_X - 128;
            bubbleArrays[0].rightLimit = SCREEN_CENTER_X + 128;
            bubbleArrays[0].topLimit = 51;
            bubbleArrays[0].hurryRct = {SCREEN_CENTER_X - 122, 480 - 214, 244, 102};
            bubbleArrays[0].numSeparators = 0;
            bubbleArrays[0].playerAssigned = 0;
            bubbleArrays[0].turnsToCompress = 9;
            bubbleArrays[0].dangerZone = 12;
            bubbleArrays[0].hurryTimer = bubbleArrays[0].warnTimer = 0;
            bubbleArrays[0].hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p1.png").c_str());
            audMixer->PlayMusic("main1p");
            break;
        case 2:
            SDL_Log("NewGame: Case 2 - Two players");
            background = IMG_LoadTexture(rend, ASSET("/gfx/backgrnd.png").c_str());
            bubbleArrays[0].penguinSprite.LoadPenguin(rend, "p1", {SCREEN_CENTER_X + 244, 480 - 60, 80, 60});
            bubbleArrays[0].shooterSprite = {shooterTexture, rend};
            bubbleArrays[0].shooterSprite.rect = {SCREEN_CENTER_X + 112, 480 - 123, 100, 100};
            bubbleArrays[0].shooterSprite.angle = PI/2.0f;
            bubbleArrays[0].bubbleOffset = {354, 40};
            // leftLimit/rightLimit match the original Perl's %POS_2P p1 exactly
            // (left_limit=354, right_limit=610), which line up exactly with
            // bubbleOffset.x=354's own 8-column-of-32px grid (354 to 610, zero
            // extra margin either side). A previous board-width fix here set
            // rightLimit to reproduce player 2's absolute 256px board width
            // (640-32=608) without checking it against this board's own grid
            // position, leaving the left wall 2px outside the grid while the
            // right wall sat 2px inside it -- an asymmetric collision box
            // around a symmetric visual grid that biased shots bouncing off
            // the right wall.
            bubbleArrays[0].leftLimit = SCREEN_CENTER_X + 34;
            bubbleArrays[0].rightLimit = 640 - 30;
            bubbleArrays[0].topLimit = 31;
            bubbleArrays[0].hurryRct = {SCREEN_CENTER_X + 40, 480 - 214, 244, 102};
            bubbleArrays[0].numSeparators = 0;
            bubbleArrays[0].playerAssigned = 0;
            bubbleArrays[0].turnsToCompress = 12;
            bubbleArrays[0].mpWinner = false;
            bubbleArrays[0].mpDone = false;
            bubbleArrays[0].playerState = BubbleArray::PlayerState::ALIVE;
            bubbleArrays[0].hurryTimer = bubbleArrays[0].warnTimer = 0;
            bubbleArrays[0].scorePos = {470, 90};  // Right wooden banner (player 0 is right side)
            bubbleArrays[0].hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p1.png").c_str());

            bubbleArrays[1].penguinSprite.LoadPenguin(rend, "p2", {-5, 480 - 60, 80, 60});
            bubbleArrays[1].shooterSprite = {shooterTexture, rend};
            bubbleArrays[1].shooterSprite.rect = {SCREEN_CENTER_X - 210, 480 - 123, 100, 100};
            bubbleArrays[1].shooterSprite.angle = PI/2.0f;
            bubbleArrays[1].bubbleOffset = {31, 40};
            bubbleArrays[1].leftLimit = 32;
            bubbleArrays[1].rightLimit = 288;
            bubbleArrays[1].topLimit = 31;
            bubbleArrays[1].hurryRct = {36, 480 - 214, 244, 102};
            bubbleArrays[1].numSeparators = 0;
            bubbleArrays[1].playerAssigned = 1;
            bubbleArrays[1].turnsToCompress = 12;
            bubbleArrays[1].mpWinner = false;
            bubbleArrays[1].mpDone = false;
            bubbleArrays[1].playerState = BubbleArray::PlayerState::ALIVE;
            bubbleArrays[1].hurryTimer = bubbleArrays[1].warnTimer = 0;
            bubbleArrays[1].scorePos = {147, 90};  // Left wooden banner (player 1 is left side)
            bubbleArrays[1].hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p2.png").c_str());
            audMixer->PlayMusic("main2p");
            break;
        case 3:
            SDL_Log("NewGame: Case 3 - Three players");
            // 3-player multiplayer layout (original POS_MP from Stuff.pm lines 65-105)
            // p1 in center (full size), rp1 top-left (mini), rp2 top-right (mini)
            background = IMG_LoadTexture(rend, ASSET("/gfx/back_multiplayer.png").c_str());

            // p1 - Center player (full size)
            bubbleArrays[0].penguinSprite.LoadPenguin(rend, "p1", {213, 420, 80, 60});
            bubbleArrays[0].shooterSprite = {shooterTexture, rend};
            bubbleArrays[0].shooterSprite.rect = {268, 356, 100, 100};
            bubbleArrays[0].shooterSprite.angle = PI/2.0f;
            bubbleArrays[0].bubbleOffset = {190, 44};
            bubbleArrays[0].leftLimit = 190;
            bubbleArrays[0].rightLimit = 446;
            bubbleArrays[0].topLimit = 44;
            bubbleArrays[0].hurryRct = {10, 265, 244, 102};
            // Current bubble: (left_limit+right_limit)/2 - BUBBLE_SIZE/2, initial_bubble_y
            // = (190+446)/2 - 16 = 302, 390
            bubbleArrays[0].curLaunchRct = {302, 390, 32, 32};
            // Next bubble: left_limit + next_bubble.x = 190 + 112 = 302
            bubbleArrays[0].nextBubbleRct = {302, 440, 32, 32};
            bubbleArrays[0].onTopRct = {298, 437, 39, 39};  // next_bubble + on_top_next_relpos {-4,-3}
            bubbleArrays[0].frozenBottomRct = {298, 437, 39, 39};
            bubbleArrays[0].numSeparators = 0;
            bubbleArrays[0].playerAssigned = 0;
            bubbleArrays[0].turnsToCompress = 12;
            bubbleArrays[0].mpWinner = false;
            bubbleArrays[0].mpDone = false;
            bubbleArrays[0].playerState = BubbleArray::PlayerState::ALIVE;
            bubbleArrays[0].hurryTimer = bubbleArrays[0].warnTimer = 0;
            bubbleArrays[0].scorePos = {320, 12};  // Center top (original: scores => { x => 320, 'y' => 12 })
            bubbleArrays[0].hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p1.png").c_str());

            // rp1 - Top-left player (mini)
            bubbleArrays[1].penguinSprite.LoadPenguin(rend, "p2", {94, 211, 40, 30});
            bubbleArrays[1].shooterSprite = {shooterTexture, rend};
            bubbleArrays[1].shooterSprite.rect = {59, 175, 50, 50};
            bubbleArrays[1].shooterSprite.angle = PI/2.0f;
            bubbleArrays[1].bubbleOffset = {20, 19};
            bubbleArrays[1].leftLimit = 20;
            bubbleArrays[1].rightLimit = 148;
            bubbleArrays[1].topLimit = 19;
            bubbleArrays[1].hurryRct = {5, 128, 122, 51};
            // rp1: current bubble at (20+148)/2 - 16 = 68, initial_bubble_y=192
            bubbleArrays[1].curLaunchRct = {68, 192, 32, 32};
            // Next bubble: left_limit + next_bubble.x = 20 + 56 = 76
            bubbleArrays[1].nextBubbleRct = {76, 216, 32, 32};
            bubbleArrays[1].onTopRct = {74, 214, 39, 39};  // next_bubble + on_top_next_relpos {-2,-2}
            bubbleArrays[1].frozenBottomRct = {74, 214, 39, 39};
            bubbleArrays[1].numSeparators = 0;
            bubbleArrays[1].playerAssigned = 1;
            bubbleArrays[1].turnsToCompress = 12;
            bubbleArrays[1].mpWinner = false;
            bubbleArrays[1].mpDone = false;
            bubbleArrays[1].playerState = BubbleArray::PlayerState::ALIVE;
            bubbleArrays[1].scorePos = {83, 2};  // Top-left (original rp1: scores => { x => 83, 'y' => 2 })
            bubbleArrays[1].hurryTimer = bubbleArrays[1].warnTimer = 0;
            bubbleArrays[1].hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p2.png").c_str());

            // rp2 - Top-right player (mini)
            bubbleArrays[2].penguinSprite.LoadPenguin(rend, "p2", {94, 211, 40, 30});
            bubbleArrays[2].shooterSprite = {shooterTexture, rend};
            bubbleArrays[2].shooterSprite.rect = {531, 175, 50, 50};
            bubbleArrays[2].shooterSprite.angle = PI/2.0f;
            bubbleArrays[2].bubbleOffset = {492, 19};
            bubbleArrays[2].leftLimit = 492;
            bubbleArrays[2].rightLimit = 620;
            bubbleArrays[2].topLimit = 19;
            bubbleArrays[2].hurryRct = {5, 128, 122, 51};
            // rp2: current bubble at (492+620)/2 - 16 = 540, initial_bubble_y=192
            bubbleArrays[2].curLaunchRct = {540, 192, 32, 32};
            // Next bubble at left_limit + next_bubble.x = 492 + 56 = 548
            bubbleArrays[2].nextBubbleRct = {548, 216, 32, 32};
            bubbleArrays[2].onTopRct = {546, 214, 39, 39};  // next_bubble + on_top_next_relpos {-2,-2}
            bubbleArrays[2].frozenBottomRct = {546, 214, 39, 39};
            bubbleArrays[2].numSeparators = 0;
            bubbleArrays[2].playerAssigned = 2;
            bubbleArrays[2].turnsToCompress = 12;
            bubbleArrays[2].mpWinner = false;
            bubbleArrays[2].mpDone = false;
            bubbleArrays[2].playerState = BubbleArray::PlayerState::ALIVE;
            bubbleArrays[2].hurryTimer = bubbleArrays[2].warnTimer = 0;
            bubbleArrays[2].scorePos = {553, 2};  // Top-right (original rp2: scores => { x => 553, 'y' => 2 })
            bubbleArrays[2].hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p2.png").c_str());

            audMixer->PlayMusic("main2p");
            break;
        case 4:
            SDL_Log("NewGame: Case 4 - Four players");
            // 4-player multiplayer layout (original POS_MP from Stuff.pm)
            // p1 in center (full), rp1 top-left (mini), rp2 top-right (mini), rp3 bottom-left (mini)
            background = IMG_LoadTexture(rend, ASSET("/gfx/back_multiplayer.png").c_str());

            // p1 - Center player (full size) - same as 3-player
            bubbleArrays[0].penguinSprite.LoadPenguin(rend, "p1", {213, 420, 80, 60});
            bubbleArrays[0].shooterSprite = {shooterTexture, rend};
            bubbleArrays[0].shooterSprite.rect = {268, 356, 100, 100};
            bubbleArrays[0].shooterSprite.angle = PI/2.0f;
            bubbleArrays[0].bubbleOffset = {190, 44};
            bubbleArrays[0].leftLimit = 190;
            bubbleArrays[0].rightLimit = 446;
            bubbleArrays[0].topLimit = 44;
            bubbleArrays[0].hurryRct = {10, 265, 244, 102};
            bubbleArrays[0].curLaunchRct = {302, 390, 32, 32};
            bubbleArrays[0].nextBubbleRct = {302, 440, 32, 32};
            bubbleArrays[0].onTopRct = {298, 437, 39, 39};
            bubbleArrays[0].frozenBottomRct = {298, 437, 39, 39};
            bubbleArrays[0].numSeparators = 0;
            bubbleArrays[0].playerAssigned = 0;
            bubbleArrays[0].turnsToCompress = 12;
            bubbleArrays[0].mpWinner = false;
            bubbleArrays[0].mpDone = false;
            bubbleArrays[0].playerState = BubbleArray::PlayerState::ALIVE;
            bubbleArrays[0].hurryTimer = bubbleArrays[0].warnTimer = 0;
            bubbleArrays[0].hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p1.png").c_str());

            // rp1 - Top-left player (mini) - same as 3-player
            bubbleArrays[1].penguinSprite.LoadPenguin(rend, "p2", {94, 211, 40, 30});
            bubbleArrays[1].shooterSprite = {shooterTexture, rend};
            bubbleArrays[1].shooterSprite.rect = {59, 175, 50, 50};
            bubbleArrays[1].shooterSprite.angle = PI/2.0f;
            bubbleArrays[1].bubbleOffset = {20, 19};
            bubbleArrays[1].leftLimit = 20;
            bubbleArrays[1].rightLimit = 148;
            bubbleArrays[1].topLimit = 19;
            bubbleArrays[1].hurryRct = {5, 128, 122, 51};
            bubbleArrays[1].curLaunchRct = {68, 192, 32, 32};
            bubbleArrays[1].nextBubbleRct = {76, 216, 32, 32};
            bubbleArrays[1].onTopRct = {74, 214, 39, 39};
            bubbleArrays[1].frozenBottomRct = {74, 214, 39, 39};
            bubbleArrays[1].numSeparators = 0;
            bubbleArrays[1].playerAssigned = 1;
            bubbleArrays[1].turnsToCompress = 12;
            bubbleArrays[1].mpWinner = false;
            bubbleArrays[1].mpDone = false;
            bubbleArrays[1].playerState = BubbleArray::PlayerState::ALIVE;
            bubbleArrays[1].hurryTimer = bubbleArrays[1].warnTimer = 0;
            bubbleArrays[1].hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p2.png").c_str());

            // rp2 - Top-right player (mini) - same as 3-player
            bubbleArrays[2].penguinSprite.LoadPenguin(rend, "p2", {94, 211, 40, 30});
            bubbleArrays[2].shooterSprite = {shooterTexture, rend};
            bubbleArrays[2].shooterSprite.rect = {531, 175, 50, 50};
            bubbleArrays[2].shooterSprite.angle = PI/2.0f;
            bubbleArrays[2].bubbleOffset = {492, 19};
            bubbleArrays[2].leftLimit = 492;
            bubbleArrays[2].rightLimit = 620;
            bubbleArrays[2].topLimit = 19;
            bubbleArrays[2].hurryRct = {5, 128, 122, 51};
            bubbleArrays[2].curLaunchRct = {540, 192, 32, 32};
            bubbleArrays[2].nextBubbleRct = {548, 216, 32, 32};
            bubbleArrays[2].onTopRct = {546, 214, 39, 39};
            bubbleArrays[2].frozenBottomRct = {546, 214, 39, 39};
            bubbleArrays[2].numSeparators = 0;
            bubbleArrays[2].playerAssigned = 2;
            bubbleArrays[2].turnsToCompress = 12;
            bubbleArrays[2].mpWinner = false;
            bubbleArrays[2].mpDone = false;
            bubbleArrays[2].playerState = BubbleArray::PlayerState::ALIVE;
            bubbleArrays[2].hurryTimer = bubbleArrays[2].warnTimer = 0;
            bubbleArrays[2].hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p2.png").c_str());

            // rp3 - Bottom-left player (mini)
            bubbleArrays[3].penguinSprite.LoadPenguin(rend, "p2", {94, 439, 40, 30});
            bubbleArrays[3].shooterSprite = {shooterTexture, rend};
            bubbleArrays[3].shooterSprite.rect = {59, 404, 50, 50};
            bubbleArrays[3].shooterSprite.angle = PI/2.0f;
            bubbleArrays[3].bubbleOffset = {20, 247};
            bubbleArrays[3].leftLimit = 20;
            bubbleArrays[3].rightLimit = 148;
            bubbleArrays[3].topLimit = 247;
            bubbleArrays[3].hurryRct = {5, 345, 122, 51};
            // rp3: curLaunch at (20+148)/2-16=68, initial_bubble_y=420
            bubbleArrays[3].curLaunchRct = {68, 420, 32, 32};
            bubbleArrays[3].nextBubbleRct = {76, 445, 32, 32};
            bubbleArrays[3].onTopRct = {74, 443, 39, 39};
            bubbleArrays[3].frozenBottomRct = {74, 443, 39, 39};
            bubbleArrays[3].numSeparators = 0;
            bubbleArrays[3].playerAssigned = 3;
            bubbleArrays[3].turnsToCompress = 12;
            bubbleArrays[3].mpWinner = false;
            bubbleArrays[3].mpDone = false;
            bubbleArrays[3].playerState = BubbleArray::PlayerState::ALIVE;
            bubbleArrays[3].hurryTimer = bubbleArrays[3].warnTimer = 0;
            bubbleArrays[3].scorePos = {83, 465};  // Bottom-left (original rp3: scores => { x => 83, 'y' => 465 })
            bubbleArrays[3].hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p2.png").c_str());

            audMixer->PlayMusic("main2p");
            break;
        case 5:
            SDL_Log("NewGame: Case 5 - Five players");
            // 5-player multiplayer layout (original POS_MP from Stuff.pm)
            // p1 in center (full), rp1/rp2 top corners (mini), rp3/rp4 bottom corners (mini)
            background = IMG_LoadTexture(rend, ASSET("/gfx/back_multiplayer.png").c_str());

            // p1 - Center player (full size) - same as 3/4-player
            bubbleArrays[0].penguinSprite.LoadPenguin(rend, "p1", {213, 420, 80, 60});
            bubbleArrays[0].shooterSprite = {shooterTexture, rend};
            bubbleArrays[0].shooterSprite.rect = {268, 356, 100, 100};
            bubbleArrays[0].shooterSprite.angle = PI/2.0f;
            bubbleArrays[0].bubbleOffset = {190, 44};
            bubbleArrays[0].leftLimit = 190;
            bubbleArrays[0].rightLimit = 446;
            bubbleArrays[0].topLimit = 44;
            bubbleArrays[0].hurryRct = {10, 265, 244, 102};
            bubbleArrays[0].curLaunchRct = {302, 390, 32, 32};
            bubbleArrays[0].nextBubbleRct = {302, 440, 32, 32};
            bubbleArrays[0].onTopRct = {298, 437, 39, 39};
            bubbleArrays[0].frozenBottomRct = {298, 437, 39, 39};
            bubbleArrays[0].numSeparators = 0;
            bubbleArrays[0].playerAssigned = 0;
            bubbleArrays[0].turnsToCompress = 12;
            bubbleArrays[0].mpWinner = false;
            bubbleArrays[0].mpDone = false;
            bubbleArrays[0].playerState = BubbleArray::PlayerState::ALIVE;
            bubbleArrays[0].hurryTimer = bubbleArrays[0].warnTimer = 0;
            bubbleArrays[0].hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p1.png").c_str());

            // rp1 - Top-left player (mini) - same as 3/4-player
            bubbleArrays[1].penguinSprite.LoadPenguin(rend, "p2", {94, 211, 40, 30});
            bubbleArrays[1].shooterSprite = {shooterTexture, rend};
            bubbleArrays[1].shooterSprite.rect = {59, 175, 50, 50};
            bubbleArrays[1].shooterSprite.angle = PI/2.0f;
            bubbleArrays[1].bubbleOffset = {20, 19};
            bubbleArrays[1].leftLimit = 20;
            bubbleArrays[1].rightLimit = 148;
            bubbleArrays[1].topLimit = 19;
            bubbleArrays[1].hurryRct = {5, 128, 122, 51};
            bubbleArrays[1].curLaunchRct = {68, 192, 32, 32};
            bubbleArrays[1].nextBubbleRct = {76, 216, 32, 32};
            bubbleArrays[1].onTopRct = {74, 214, 39, 39};
            bubbleArrays[1].frozenBottomRct = {74, 214, 39, 39};
            bubbleArrays[1].numSeparators = 0;
            bubbleArrays[1].playerAssigned = 1;
            bubbleArrays[1].turnsToCompress = 12;
            bubbleArrays[1].mpWinner = false;
            bubbleArrays[1].mpDone = false;
            bubbleArrays[1].playerState = BubbleArray::PlayerState::ALIVE;
            bubbleArrays[1].hurryTimer = bubbleArrays[1].warnTimer = 0;
            bubbleArrays[1].hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p2.png").c_str());

            // rp2 - Top-right player (mini) - same as 3/4-player
            bubbleArrays[2].penguinSprite.LoadPenguin(rend, "p2", {94, 211, 40, 30});
            bubbleArrays[2].shooterSprite = {shooterTexture, rend};
            bubbleArrays[2].shooterSprite.rect = {531, 175, 50, 50};
            bubbleArrays[2].shooterSprite.angle = PI/2.0f;
            bubbleArrays[2].bubbleOffset = {492, 19};
            bubbleArrays[2].leftLimit = 492;
            bubbleArrays[2].rightLimit = 620;
            bubbleArrays[2].topLimit = 19;
            bubbleArrays[2].hurryRct = {5, 128, 122, 51};
            bubbleArrays[2].curLaunchRct = {540, 192, 32, 32};
            bubbleArrays[2].nextBubbleRct = {548, 216, 32, 32};
            bubbleArrays[2].onTopRct = {546, 214, 39, 39};
            bubbleArrays[2].frozenBottomRct = {546, 214, 39, 39};
            bubbleArrays[2].numSeparators = 0;
            bubbleArrays[2].playerAssigned = 2;
            bubbleArrays[2].turnsToCompress = 12;
            bubbleArrays[2].mpWinner = false;
            bubbleArrays[2].mpDone = false;
            bubbleArrays[2].playerState = BubbleArray::PlayerState::ALIVE;
            bubbleArrays[2].hurryTimer = bubbleArrays[2].warnTimer = 0;
            bubbleArrays[2].hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p2.png").c_str());

            // rp3 - Bottom-left player (mini) - same as 4-player
            bubbleArrays[3].penguinSprite.LoadPenguin(rend, "p2", {94, 439, 40, 30});
            bubbleArrays[3].shooterSprite = {shooterTexture, rend};
            bubbleArrays[3].shooterSprite.rect = {59, 404, 50, 50};
            bubbleArrays[3].shooterSprite.angle = PI/2.0f;
            bubbleArrays[3].bubbleOffset = {20, 247};
            bubbleArrays[3].leftLimit = 20;
            bubbleArrays[3].rightLimit = 148;
            bubbleArrays[3].topLimit = 247;
            bubbleArrays[3].hurryRct = {5, 345, 122, 51};
            bubbleArrays[3].curLaunchRct = {68, 420, 32, 32};
            bubbleArrays[3].nextBubbleRct = {76, 445, 32, 32};
            bubbleArrays[3].onTopRct = {74, 443, 39, 39};
            bubbleArrays[3].frozenBottomRct = {74, 443, 39, 39};
            bubbleArrays[3].numSeparators = 0;
            bubbleArrays[3].playerAssigned = 3;
            bubbleArrays[3].turnsToCompress = 12;
            bubbleArrays[3].mpWinner = false;
            bubbleArrays[3].mpDone = false;
            bubbleArrays[3].playerState = BubbleArray::PlayerState::ALIVE;
            bubbleArrays[3].hurryTimer = bubbleArrays[3].warnTimer = 0;
            bubbleArrays[3].scorePos = {83, 465};
            bubbleArrays[3].hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p2.png").c_str());

            // rp4 - Bottom-right player (mini)
            bubbleArrays[4].penguinSprite.LoadPenguin(rend, "p2", {94, 439, 40, 30});
            bubbleArrays[4].shooterSprite = {shooterTexture, rend};
            bubbleArrays[4].shooterSprite.rect = {531, 404, 50, 50};
            bubbleArrays[4].shooterSprite.angle = PI/2.0f;
            bubbleArrays[4].bubbleOffset = {492, 247};
            bubbleArrays[4].leftLimit = 492;
            bubbleArrays[4].rightLimit = 620;
            bubbleArrays[4].topLimit = 247;
            bubbleArrays[4].hurryRct = {5, 345, 122, 51};
            // rp4: curLaunch at (492+620)/2-16=540, initial_bubble_y=420
            bubbleArrays[4].curLaunchRct = {540, 420, 32, 32};
            bubbleArrays[4].nextBubbleRct = {548, 445, 32, 32};
            bubbleArrays[4].onTopRct = {546, 443, 39, 39};
            bubbleArrays[4].frozenBottomRct = {546, 443, 39, 39};
            bubbleArrays[4].numSeparators = 0;
            bubbleArrays[4].playerAssigned = 4;
            bubbleArrays[4].turnsToCompress = 12;
            bubbleArrays[4].mpWinner = false;
            bubbleArrays[4].mpDone = false;
            bubbleArrays[4].playerState = BubbleArray::PlayerState::ALIVE;
            bubbleArrays[4].hurryTimer = bubbleArrays[4].warnTimer = 0;
            bubbleArrays[4].scorePos = {553, 465};  // Bottom-right (original rp4: scores => { x => 553, 'y' => 465 })
            bubbleArrays[4].hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p2.png").c_str());

            audMixer->PlayMusic("main2p");
            break;
        default: {  // 6..MAX_NET_PLAYERS network battle royale
            SDL_Log("NewGame: royale with %d players", currentSettings.playerCount);
            background = IMG_LoadTexture(rend, ASSET("/gfx/back_multiplayer.png").c_str());

            // p1 - Center player (full size) - identical to case 5's bubbleArrays[0] block
            bubbleArrays[0].penguinSprite.LoadPenguin(rend, "p1", {213, 420, 80, 60});
            bubbleArrays[0].shooterSprite = {shooterTexture, rend};
            bubbleArrays[0].shooterSprite.rect = {268, 356, 100, 100};
            bubbleArrays[0].shooterSprite.angle = PI/2.0f;
            bubbleArrays[0].bubbleOffset = {190, 44};
            bubbleArrays[0].leftLimit = 190;
            bubbleArrays[0].rightLimit = 446;
            bubbleArrays[0].topLimit = 44;
            bubbleArrays[0].hurryRct = {10, 265, 244, 102};
            bubbleArrays[0].curLaunchRct = {302, 390, 32, 32};
            bubbleArrays[0].nextBubbleRct = {302, 440, 32, 32};
            bubbleArrays[0].onTopRct = {298, 437, 39, 39};
            bubbleArrays[0].frozenBottomRct = {298, 437, 39, 39};
            bubbleArrays[0].numSeparators = 0;
            bubbleArrays[0].playerAssigned = 0;
            bubbleArrays[0].turnsToCompress = 12;
            bubbleArrays[0].mpWinner = false;
            bubbleArrays[0].mpDone = false;
            bubbleArrays[0].playerState = BubbleArray::PlayerState::ALIVE;
            bubbleArrays[0].hurryTimer = bubbleArrays[0].warnTimer = 0;
            bubbleArrays[0].hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p1.png").c_str());
            bubbleArrays[0].parkedSlot = -1;

            for (int i = 1; i < currentSettings.playerCount; i++) {
                BubbleArray& b = bubbleArrays[i];
                int slot = (i - 1) % 4;
                ApplyMiniSlotGeometry(b, slot, rend, shooterTexture);
                b.parkedSlot = slot;
                b.playerAssigned = i;
                b.mpWinner = false;
                b.mpDone = false;
                b.playerState = BubbleArray::PlayerState::ALIVE;
                b.hurryTimer = b.warnTimer = 0;
                b.hurryTexture = IMG_LoadTexture(rend, ASSET("/gfx/hurry_p2.png").c_str());
            }

            audMixer->PlayMusic("main2p");
            break;
        }
    }

    ApplyNetViewPage();
    ReRankNetView();  // auto mode refines page-0 visibility (no-op when manual/<=5)

    // Set lobby player IDs for network games
    if (currentSettings.networkGame) {
        NetworkClient* netClient = NetworkClient::Instance();
        if (netClient && netClient->IsConnected()) {
            bubbleArrays[0].lobbyPlayerId = netClient->GetMyPlayerId();
            bubbleArrays[0].playerNickname = netClient->GetPlayerNickname(bubbleArrays[0].lobbyPlayerId);
            SDL_Log("Set local player (array 0) lobbyPlayerId = %d, nickname = '%s'",
                    bubbleArrays[0].lobbyPlayerId, bubbleArrays[0].playerNickname.c_str());

            const auto& idToNick = netClient->GetPlayerIdToNick();

            // Pre-populate remote players' lobbyPlayerIds and nicks from GAME_CAN_START mapping
            // This ensures correct nick targeting for malus messages even before the first shot.
            // Our own bots are in this list like anyone else and are seated by
            // the same rule -- every client runs it over the same room, so all
            // of them agree on which board is whose. SeatBots below then claims
            // the ones that turn out to be ours.
            std::vector<int> roomPlayerIds;
            for (const auto& kv : idToNick) roomPlayerIds.push_back(kv.first);
            const std::map<int, int> remoteSeats =
                AssignRemoteSeats(roomPlayerIds, (int)netClient->GetMyPlayerId(),
                                  currentSettings.playerCount);
            for (const auto& seat : remoteSeats) {
                bubbleArrays[seat.first].lobbyPlayerId = seat.second;
                bubbleArrays[seat.first].playerNickname = idToNick.at(seat.second);
                SDL_Log("Pre-assigned remote player (array %d) lobbyPlayerId = %d, nickname = '%s'",
                        seat.first, seat.second,
                        bubbleArrays[seat.first].playerNickname.c_str());
            }
            // Any board nobody was seated on is unknown
            for (int i = 1; i < currentSettings.playerCount; i++) {
                if (remoteSeats.count(i)) continue;
                bubbleArrays[i].lobbyPlayerId = -1;
                bubbleArrays[i].playerNickname = "";
            }

            SeatBots();

            // Remap per-player settings (aimGuide, compression, colors) from lobby slot order
            // to bubbleArray order. setup.aimGuide[i] is indexed by the game room's player list
            // (host=0, first joiner=1, ...), but bubbleArrays[0] is always the local player.
            // Match by nick to apply the right setting to each array.
            const GameRoom* room = netClient->GetCurrentGame();
            if (room) {
                const auto& roomPlayers = room->players;
                for (int arr = 0; arr < currentSettings.playerCount; arr++) {
                    const std::string& nick = bubbleArrays[arr].playerNickname;
                    for (int slot = 0; slot < (int)roomPlayers.size() && slot < MAX_NET_PLAYERS; slot++) {
                        if (roomPlayers[slot].nick == nick) {
                            int nc = currentSettings.playerColors[slot];
                            nc = (nc < 5) ? 5 : (nc > 8) ? 8 : nc;
                            bubbleArrays[arr].numColors = nc;
                            bubbleArrays[arr].compressionDisabled = currentSettings.disableCompression[slot];
                            bubbleArrays[arr].aimGuideEnabled = currentSettings.aimGuide[slot];
                            SDL_Log("Remapped slot %d ('%s') -> array %d: colors=%d compress=%d aim=%d",
                                    slot, nick.c_str(), arr, nc,
                                    currentSettings.disableCompression[slot],
                                    currentSettings.aimGuide[slot]);
                            break;
                        }
                    }
                }
            }
        }
    }

    // Log final configuration for each player
    for (int i = 0; i < currentSettings.playerCount; i++) {
        SDL_Log("Player %d: bubbleOffset=(%d,%d), shooterRect=(%d,%d,%d,%d), penguinRect=(%d,%d,%d,%d)",
                i, bubbleArrays[i].bubbleOffset.x, bubbleArrays[i].bubbleOffset.y,
                bubbleArrays[i].shooterSprite.rect.x, bubbleArrays[i].shooterSprite.rect.y,
                bubbleArrays[i].shooterSprite.rect.w, bubbleArrays[i].shooterSprite.rect.h,
                bubbleArrays[i].penguinSprite.rect.x, bubbleArrays[i].penguinSprite.rect.y,
                bubbleArrays[i].penguinSprite.rect.w, bubbleArrays[i].penguinSprite.rect.h);
    }

    SetupGameMetrics(bubbleArrays, currentSettings.playerCount, lowGfx);

    // Clear transient round state carried over from a previous match. Starting a
    // distinct match reuses these owners, and a new match can have fewer players
    // than the last one, so anything left here may still be addressed to an array
    // that is now inactive and whose rows RemoveArray has cleared. ReloadGame
    // clears the same set between rounds; only singleBubbles was cleared here.
    singleBubbles.clear();
    malusBubbles.clear();
    for (int i = 0; i < currentSettings.playerCount; i++) {
        bubbleArrays[i].malusQueue.clear();
        bubbleArrays[i].malusAlerts.clear();
    }
    frameCount = 0;

    if (!currentSettings.randomLevels) {
        LoadLevelset(ASSET("/data/levels").c_str());
        LoadLevel(curLevel);
    }
    else {
        // Initialize curLaunch and nextBubble with safe defaults BEFORE sync
        // This ensures they have valid values even if SyncNetworkLevel fails/returns early
        for (int i = 0; i < currentSettings.playerCount; i++) {
            bubbleArrays[i].curLaunch = 0;  // Default to bubble color 0
            bubbleArrays[i].nextBubble = 0;
        }

        // For multiplayer, generate one layout and use same bubble IDs for all players
        if (currentSettings.networkGame) {
            // Network game (2-5 players) - synchronize level between all players
            if (!SyncNetworkLevel()) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "NewGame: Level sync failed - returning to lobby");
                QuitToTitle();
                return;
            }
        } else if (currentSettings.playerCount == 2) {
            // Local 2P game - generate layout for player 1
            RandomLevel(bubbleArrays[0]);

            // Copy bubble IDs to player 2 with their own positions
            int bubbleSize = 32;
            int rowSize = bubbleSize * 7 / 8;  // ROW_SIZE = 28
            SDL_Point &offset2 = bubbleArrays[1].bubbleOffset;

            // Clear player 2's map
            for (size_t i = 0; i < bubbleArrays[1].bubbleMap.size(); i++) {
                bubbleArrays[1].bubbleMap[i].clear();
            }

            // Copy bubble IDs from player 1 to player 2 with player 2's positions
            for (size_t i = 0; i < bubbleArrays[0].bubbleMap.size(); i++) {
                int smallerSep = bubbleArrays[0].bubbleMap[i].size() % 2 == 0 ? 0 : bubbleSize / 2;
                for (size_t j = 0; j < bubbleArrays[0].bubbleMap[i].size(); j++) {
                    int bubbleId = bubbleArrays[0].bubbleMap[i][j].bubbleId;
                    bubbleArrays[1].bubbleMap[i].push_back(Bubble{
                        bubbleId,
                        {(smallerSep + bubbleSize * ((int)j)) + offset2.x, (rowSize * ((int)i)) + offset2.y}
                    });
                }
            }

            Update2PText();
        }
        else {
            // Single player
            for (int i = 0; i < currentSettings.playerCount; i++) RandomLevel(bubbleArrays[i]);
        }
    }

    FrozenBubble::Instance()->startTime = SDL_GetTicks();
    FrozenBubble::Instance()->currentState = MainGame;

    // Only choose random bubbles if not synced via network
    // Network games with randomLevels call SyncNetworkLevel() which already sets curLaunch and nextBubble
    if (!(currentSettings.networkGame && currentSettings.randomLevels)) {
        ChooseFirstBubble(bubbleArrays);
    }
}

void RemoveArray(BubbleArray *bArray, int playerCount) {
    for (int i = 0; i < playerCount; i++) {
        for (size_t j = 0; j < bArray[i].bubbleMap.size(); j++) {
            bArray[i].bubbleMap[j].clear();
        }
    }
}


void BubbleGame::ReloadGame(int level) {
#ifdef FROZEN_BUBBLE_TEST_ACCESS
    if (testCapturePostRoundTransition) {
        testReloadLevel = level;
        return;
    }
#endif
    if (level >= (int)loadedLevels.size() && !currentSettings.randomLevels){
        QuitToTitle();
        return;
    }

    SDL_Renderer *rend = const_cast<SDL_Renderer*>(renderer);

    TransitionManager::Instance()->DoSnipIn(rend);
    firstRenderDone = false;

    gameFinish = gameWon = gameLost = gameMatchOver = false;
    wonByClearing = false;
    roundWinnerIdx = -1;
    gameMpDone = false;
    sendMalusToOne = -1;
    attackingMe.clear();
    for (int i = 0; i < MAX_NET_PLAYERS; i++) playerTargeting[i] = -1;
    for (int i = 0; i < currentSettings.playerCount; i++) {
        BubbleArray &player = bubbleArrays[i];
        // Finished rounds can still receive late fire/stick packets. They are not
        // consumed while gameFinish is set, so none may survive into the new board.
        ResetRoundInputState(player);
    }

    // Reset all players to ALIVE state for new round (especially important for 3-5 player games)
    // But keep LEFT players as LEFT — they disconnected and can't come back (original: left = 1 persists)
    for (int i = 0; i < currentSettings.playerCount; i++) {
        if (bubbleArrays[i].playerState != BubbleArray::PlayerState::LEFT) {
            bubbleArrays[i].playerState = BubbleArray::PlayerState::ALIVE;
        }
        bubbleArrays[i].mpWinner = false;
        bubbleArrays[i].mpDone = false;
        bubbleArrays[i].penguinSprite.PlayAnimation(0);
        bubbleArrays[i].hurryTimer = bubbleArrays[i].warnTimer = 0;
    }

    // Local multiplayer reload uses same switch cases as network equivalents (no special block needed).

    switch (currentSettings.playerCount) {
        case 2:
            bubbleArrays[0].shooterSprite.angle = PI/2.0f;
            bubbleArrays[0].bubbleOffset = {354, 40};
            bubbleArrays[0].turnsToCompress = 12;
            bubbleArrays[0].numSeparators = 0;

            bubbleArrays[1].shooterSprite.angle = PI/2.0f;
            bubbleArrays[1].bubbleOffset = {31, 40};
            bubbleArrays[1].turnsToCompress = 12;
            bubbleArrays[1].numSeparators = 0;
            break;
        case 1:
            bubbleArrays[0].shooterSprite.angle = PI/2.0f;
            bubbleArrays[0].bubbleOffset = {190, 51};
            bubbleArrays[0].topLimit = 51;
            bubbleArrays[0].numSeparators = 0;
            bubbleArrays[0].turnsToCompress = 9;
            bubbleArrays[0].dangerZone = 12;
            break;
        case 3:
            // Reset fields that change during a round (bubbleOffset shifts with compressor).
            // curLaunchRct has to be restored here too: losing enlarges it to
            // {x-1, y-1, 34, 48} for the death visual, and 3 players is the one
            // count nothing else puts back -- SetupGameMetrics below restores it
            // for 1 and 2 players but deliberately skips 3-5, and cases 4, 5 and
            // the royale default restore it themselves. Without this the enlarged
            // (and one-pixel-drifted) launcher carried into every later round.
            bubbleArrays[0].shooterSprite.angle = PI/2.0f;
            bubbleArrays[0].bubbleOffset = {190, 44};
            bubbleArrays[0].turnsToCompress = 12;
            bubbleArrays[0].numSeparators = 0;
            bubbleArrays[0].curLaunchRct = {302, 390, 32, 32};

            bubbleArrays[1].shooterSprite.angle = PI/2.0f;
            bubbleArrays[1].bubbleOffset = {20, 19};
            bubbleArrays[1].turnsToCompress = 12;
            bubbleArrays[1].numSeparators = 0;
            bubbleArrays[1].curLaunchRct = {68, 192, 32, 32};

            bubbleArrays[2].shooterSprite.angle = PI/2.0f;
            bubbleArrays[2].bubbleOffset = {492, 19};
            bubbleArrays[2].turnsToCompress = 12;
            bubbleArrays[2].numSeparators = 0;
            bubbleArrays[2].curLaunchRct = {540, 192, 32, 32};
            break;
        case 4:
            bubbleArrays[0].shooterSprite.angle = PI/2.0f;
            bubbleArrays[0].bubbleOffset = {190, 44};
            bubbleArrays[0].turnsToCompress = 12;
            bubbleArrays[0].numSeparators = 0;
            bubbleArrays[0].curLaunchRct = {302, 390, 32, 32};
            bubbleArrays[0].nextBubbleRct = {302, 440, 32, 32};
            bubbleArrays[0].onTopRct = {298, 437, 39, 39};
            bubbleArrays[0].frozenBottomRct = {298, 437, 39, 39};

            bubbleArrays[1].shooterSprite.angle = PI/2.0f;
            bubbleArrays[1].bubbleOffset = {20, 19};
            bubbleArrays[1].turnsToCompress = 12;
            bubbleArrays[1].numSeparators = 0;
            bubbleArrays[1].curLaunchRct = {68, 192, 32, 32};
            bubbleArrays[1].nextBubbleRct = {76, 216, 32, 32};
            bubbleArrays[1].onTopRct = {74, 214, 39, 39};
            bubbleArrays[1].frozenBottomRct = {74, 214, 39, 39};

            bubbleArrays[2].shooterSprite.angle = PI/2.0f;
            bubbleArrays[2].bubbleOffset = {492, 19};
            bubbleArrays[2].turnsToCompress = 12;
            bubbleArrays[2].numSeparators = 0;
            bubbleArrays[2].curLaunchRct = {540, 192, 32, 32};
            bubbleArrays[2].nextBubbleRct = {548, 216, 32, 32};
            bubbleArrays[2].onTopRct = {546, 214, 39, 39};
            bubbleArrays[2].frozenBottomRct = {546, 214, 39, 39};

            bubbleArrays[3].shooterSprite.angle = PI/2.0f;
            bubbleArrays[3].bubbleOffset = {20, 247};
            bubbleArrays[3].turnsToCompress = 12;
            bubbleArrays[3].numSeparators = 0;
            bubbleArrays[3].curLaunchRct = {68, 420, 32, 32};
            bubbleArrays[3].nextBubbleRct = {76, 445, 32, 32};
            bubbleArrays[3].onTopRct = {74, 443, 39, 39};
            bubbleArrays[3].frozenBottomRct = {74, 443, 39, 39};
            break;
        case 5:
            bubbleArrays[0].shooterSprite.angle = PI/2.0f;
            bubbleArrays[0].bubbleOffset = {190, 44};
            bubbleArrays[0].turnsToCompress = 12;
            bubbleArrays[0].numSeparators = 0;
            bubbleArrays[0].curLaunchRct = {302, 390, 32, 32};
            bubbleArrays[0].nextBubbleRct = {302, 440, 32, 32};
            bubbleArrays[0].onTopRct = {298, 437, 39, 39};
            bubbleArrays[0].frozenBottomRct = {298, 437, 39, 39};

            bubbleArrays[1].shooterSprite.angle = PI/2.0f;
            bubbleArrays[1].bubbleOffset = {20, 19};
            bubbleArrays[1].turnsToCompress = 12;
            bubbleArrays[1].numSeparators = 0;
            bubbleArrays[1].curLaunchRct = {68, 192, 32, 32};
            bubbleArrays[1].nextBubbleRct = {76, 216, 32, 32};
            bubbleArrays[1].onTopRct = {74, 214, 39, 39};
            bubbleArrays[1].frozenBottomRct = {74, 214, 39, 39};

            bubbleArrays[2].shooterSprite.angle = PI/2.0f;
            bubbleArrays[2].bubbleOffset = {492, 19};
            bubbleArrays[2].turnsToCompress = 12;
            bubbleArrays[2].numSeparators = 0;
            bubbleArrays[2].curLaunchRct = {540, 192, 32, 32};
            bubbleArrays[2].nextBubbleRct = {548, 216, 32, 32};
            bubbleArrays[2].onTopRct = {546, 214, 39, 39};
            bubbleArrays[2].frozenBottomRct = {546, 214, 39, 39};

            bubbleArrays[3].shooterSprite.angle = PI/2.0f;
            bubbleArrays[3].bubbleOffset = {20, 247};
            bubbleArrays[3].turnsToCompress = 12;
            bubbleArrays[3].numSeparators = 0;
            bubbleArrays[3].curLaunchRct = {68, 420, 32, 32};
            bubbleArrays[3].nextBubbleRct = {76, 445, 32, 32};
            bubbleArrays[3].onTopRct = {74, 443, 39, 39};
            bubbleArrays[3].frozenBottomRct = {74, 443, 39, 39};

            bubbleArrays[4].shooterSprite.angle = PI/2.0f;
            bubbleArrays[4].bubbleOffset = {492, 247};
            bubbleArrays[4].turnsToCompress = 12;
            bubbleArrays[4].numSeparators = 0;
            bubbleArrays[4].curLaunchRct = {540, 420, 32, 32};
            bubbleArrays[4].nextBubbleRct = {548, 445, 32, 32};
            bubbleArrays[4].onTopRct = {546, 443, 39, 39};
            bubbleArrays[4].frozenBottomRct = {546, 443, 39, 39};
            break;
        default: {  // 6..MAX_NET_PLAYERS network battle royale
            // Player 0: identical to case 5's bubbleArrays[0] reload block
            bubbleArrays[0].shooterSprite.angle = PI/2.0f;
            bubbleArrays[0].bubbleOffset = {190, 44};
            bubbleArrays[0].turnsToCompress = 12;
            bubbleArrays[0].numSeparators = 0;
            bubbleArrays[0].curLaunchRct = {302, 390, 32, 32};
            bubbleArrays[0].nextBubbleRct = {302, 440, 32, 32};
            bubbleArrays[0].onTopRct = {298, 437, 39, 39};
            bubbleArrays[0].frozenBottomRct = {298, 437, 39, 39};

            // Parked remotes: only reset the fields that shift mid-round (angle, offset,
            // compressor state, and the launch/next/onTop/frozenBottom rects), same subset
            // case 5's reload resets -- do NOT reload penguin/shooter textures here
            // (ApplyMiniSlotGeometry is NewGame-only; reusing it here would reload mascot
            // textures from disk every round).
            static const SDL_Point kSlotOffset[4] = {{20,19},{492,19},{20,247},{492,247}};
            static const SDL_Rect kSlotCurLaunch[4] = {{68,192,32,32},{540,192,32,32},{68,420,32,32},{540,420,32,32}};
            static const SDL_Rect kSlotNextBubble[4] = {{76,216,32,32},{548,216,32,32},{76,445,32,32},{548,445,32,32}};
            static const SDL_Rect kSlotOnTop[4] = {{74,214,39,39},{546,214,39,39},{74,443,39,39},{546,443,39,39}};
            for (int i = 1; i < currentSettings.playerCount; i++) {
                BubbleArray& b = bubbleArrays[i];
                int slot = (i - 1) % 4;
                b.shooterSprite.angle = PI/2.0f;
                b.bubbleOffset = kSlotOffset[slot];
                b.turnsToCompress = 12;
                b.numSeparators = 0;
                b.curLaunchRct = kSlotCurLaunch[slot];
                b.nextBubbleRct = kSlotNextBubble[slot];
                b.onTopRct = kSlotOnTop[slot];
                b.frozenBottomRct = kSlotOnTop[slot];  // frozenBottomRct == onTopRct in every case-5 slot
            }
            break;
        }
    }

    netViewPinnedIdx = -1;  // spectate pin never carries into a new round
    netViewPage = 0;
    ApplyNetViewPage();
    ReRankNetView();  // auto mode refines page-0 visibility (no-op when manual/<=5)

    RemoveArray(bubbleArrays, currentSettings.playerCount);
    SetupGameMetrics(bubbleArrays, currentSettings.playerCount, lowGfx);

    // Clear mid-round state that doesn't persist between rounds
    singleBubbles.clear();
    malusBubbles.clear();
    for (int i = 0; i < currentSettings.playerCount; i++) {
        bubbleArrays[i].malusQueue.clear();
        bubbleArrays[i].chainLevel = 0;
        bubbleArrays[i].score = 0;
        bubbleArrays[i].explodeWait = EXPLODE_FRAMEWAIT;
        bubbleArrays[i].frozenWait = FROZEN_FRAMEWAIT;
        bubbleArrays[i].prelightTime = PRELIGHT_SLOW;
        bubbleArrays[i].waitPrelight = PRELIGHT_SLOW;
        // Reset per-round stats; match totals (m*) persist across rounds.
        bubbleArrays[i].rFired = bubbleArrays[i].rPopped = 0;
        bubbleArrays[i].rSent = bubbleArrays[i].rRecv = 0;
        bubbleArrays[i].rKills = 0;
        bubbleArrays[i].malusAlerts.clear();
    }
    roundStatsFinalized = false;
    frameCount = 0;

    if (!currentSettings.randomLevels) {
        LoadLevel(level);
        ChooseFirstBubble(bubbleArrays);
    }
    else {
        // For multiplayer, generate one layout and use same bubble IDs for all players
        if (currentSettings.networkGame) {
            // Network game (2-5 players) - synchronize level between all players
            // SyncNetworkLevel also syncs initial bubbles, so DON'T call ChooseFirstBubble after
            if (!SyncNetworkLevel()) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ReloadGame: Level sync failed - returning to lobby");
                QuitToTitle();
                return;
            }
        } else if (currentSettings.playerCount == 2) {
            // Local 2P game - generate layout for player 1
            RandomLevel(bubbleArrays[0]);

            // Copy bubble IDs to player 2 with their own positions
            int bubbleSize = 32;
            int rowSize = bubbleSize * 7 / 8;  // ROW_SIZE = 28
            SDL_Point &offset2 = bubbleArrays[1].bubbleOffset;

            // Clear player 2's map
            for (size_t i = 0; i < bubbleArrays[1].bubbleMap.size(); i++) {
                bubbleArrays[1].bubbleMap[i].clear();
            }

            // Copy bubble IDs from player 1 to player 2 with player 2's positions
            for (size_t i = 0; i < bubbleArrays[0].bubbleMap.size(); i++) {
                int smallerSep = bubbleArrays[0].bubbleMap[i].size() % 2 == 0 ? 0 : bubbleSize / 2;
                for (size_t j = 0; j < bubbleArrays[0].bubbleMap[i].size(); j++) {
                    int bubbleId = bubbleArrays[0].bubbleMap[i][j].bubbleId;
                    bubbleArrays[1].bubbleMap[i].push_back(Bubble{
                        bubbleId,
                        {(smallerSep + bubbleSize * ((int)j)) + offset2.x, (rowSize * ((int)i)) + offset2.y}
                    });
                }
            }
            // Local 2P needs random initial bubbles
            ChooseFirstBubble(bubbleArrays);
        } else {
            // Single player
            for (int i = 0; i < currentSettings.playerCount; i++) {
                RandomLevel(bubbleArrays[i]);
            }
            ChooseFirstBubble(bubbleArrays);
        }
    }

}


void BubbleGame::ApplyNetViewPage() {
    int n = currentSettings.playerCount;
    if (n <= 5) {  // classic layouts: everything visible
        for (int i = 0; i < n; i++) bubbleArrays[i].boardVisible = true;
        return;
    }
    if (netViewPage >= NetViewPageCount(n)) netViewPage = 0;
    for (int i = 0; i < n; i++)
        bubbleArrays[i].boardVisible = NetViewBoardVisible(n, netViewPage, i);
}

void BubbleGame::CycleNetViewPage() {
    if (!currentSettings.networkGame || currentSettings.playerCount <= 5) return;
    if (netViewAuto) {
        // Leave auto mode into manual paging, starting at page 0.
        netViewAuto = false;
        netViewPage = 0;
        ApplyNetViewPage();
    } else if (netViewPage + 1 >= NetViewPageCount(currentSettings.playerCount)) {
        // Tab past the last manual page returns to auto mode.
        netViewAuto = true;
        ApplyNetViewAuto();
    } else {
        netViewPage++;
        ApplyNetViewPage();
    }
    PlaySFX("menu_change");
}

void BubbleGame::ApplyNetViewAuto() {
    int n = currentSettings.playerCount;
    NetViewBoardState states[MAX_NET_PLAYERS];
    for (int i = 0; i < n; i++) {
        states[i].alive = bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE;
        states[i].targetingMe = (i != 0) && (playerTargeting[i] == 0);
        states[i].inDanger = bubbleArrays[i].bubbleOnDanger();
        states[i].visible = bubbleArrays[i].boardVisible;
    }
    std::array<int, 4> picks = RankNetViewBoards(n, sendMalusToOne, states);

    // Spectate pin: keep the pinned board on screen regardless of ranking.
    // Lazily clear the pin once that player is no longer alive (died/left).
    if (netViewPinnedIdx > 0 &&
        (netViewPinnedIdx >= n ||
         bubbleArrays[netViewPinnedIdx].playerState != BubbleArray::PlayerState::ALIVE))
        netViewPinnedIdx = -1;
    if (netViewPinnedIdx > 0)
        picks[(netViewPinnedIdx - 1) % 4] = netViewPinnedIdx;

    bubbleArrays[0].boardVisible = true;
    for (int i = 1; i < n; i++) bubbleArrays[i].boardVisible = false;
    for (int c = 0; c < 4; c++)
        if (picks[c] >= 0) bubbleArrays[picks[c]].boardVisible = true;
}

void BubbleGame::ReRankNetView() {
    if (!currentSettings.networkGame || currentSettings.playerCount <= 5 || !netViewAuto) return;
    ApplyNetViewAuto();
}


void BubbleGame::QuitToTitle() {
#ifdef FROZEN_BUBBLE_TEST_ACCESS
    if (testCapturePostRoundTransition) {
        testQuitToTitleRequested = true;
        return;
    }
#endif
    SDL_Log("!!! QuitToTitle() called - returning to menu (gameFinish=%d, gameWon=%d, gameLost=%d)",
            gameFinish, gameWon, gameLost);
    // Clear shared controller input state so stale presses don't carry into menus
    for (int i = 0; i < 5; i++) controllerInputs[i] = {};
    memset(virtualKeyState, 0, sizeof(virtualKeyState));
    if (currentSettings.localMultiplayer) {
        CloseControllers();
    }
    RemoveArray(bubbleArrays, currentSettings.playerCount);

    // For network games, send PART and return to lobby instead of main menu
    if (currentSettings.networkGame) {
        NetworkClient* netClient = NetworkClient::Instance();
        if (netClient && netClient->IsConnected()) {
            netClient->PartGame();  // Notify server we left (transitions us to IN_LOBBY)
            // Leader posts the match summary to the lobby chatroom (now that TALK is valid again).
            SendLobbyMatchSummary();
        }
        FrozenBubble::Instance()->CallNetLobbyReturn();
    } else {
        FrozenBubble::Instance()->CallMenuReturn();
    }
    firstRenderDone = false;
}
