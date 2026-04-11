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

#include "frozenbubble.h"
#include "bubblegame.h"
#include "audiomixer.h"
#include "highscoremanager.h"
#include "transitionmanager.h"
#include "gamesettings.h"
#include "platform.h"

#include <fstream>
#include <sstream>
#include <set>
#include <queue>
#include <map>

#include <cmath>
#include <algorithm>

inline int ranrange(int a, int b) { return a + rand() % ((b - a ) + 1); }
inline float ranrange(float b) { return (rand()) / (static_cast <float> (RAND_MAX/b)); }

struct SingleBubble {
    int assignedArray; // assigned board to use
    int bubbleId; // id to use bubble image
    float posX, posY; // current position as floats for accurate movement
    float oldPosX, oldPosY; // old position as floats
    SDL_Point pos; // integer position for rendering/collision
    SDL_Point oldpos; // old integer position
    float direction = PI/2.0f; // angle
    bool falling = false; // is falling from the top
    bool launching = false; // is launched from shooter
    int leftLimit, rightLimit, topLimit; // limit before bouncing
    bool lowGfx = false; // running on lowgfx
    int bubbleSize = 32; // bubble size (change when creating for small variants)
    float speedX = 0, speedY = 0, genSpeed = 0; // used for falling bubbles
    bool chainExists = false; // enable chain reaction animation
    SDL_Point chainDest = {}; //where to land when chain reacting
    bool chainGoingUp = false; // flag that chain reaction bubble is going up (physics phase)
    int chainRow = -1, chainCol = -1; // grid position for chain reaction landing
    bool chainReachedDest = false; // flag that chain reaction bubble reached destination
    bool exploding = false; // if bubble is exploding animation
    bool shouldClear = false; // if the bubble should be deleted now
    int waitForFall = 0; // frames to wait before falling
    SDL_Rect rect = {}; // rendering rect

    void CopyBubbleProperties(Bubble *prop) {
        bubbleId = (*prop).bubbleId;
        pos = (*prop).pos;
    }

    void GenerateFreeFall(bool explode = false, int waitTime = 0) {
        speedX = (ranrange(3) - 1.5) / (bubbleSize >= 32 ? 1 : 2);
        speedY = (-ranrange(4) - 2) / (bubbleSize >= 32 ? 1 : 2);
        if (!explode) {
            falling = true;
            waitForFall = waitTime;
        }
        else exploding = true;
    }

    bool IsCollision(Bubble *bubble) {
        if (bubble->bubbleId == -1) return false;
        if (pos.y < topLimit) return true; // end if out of bounds ontop
        double distanceCollision = pow(bubbleSize * 0.82, 2);
        double xs = pow(bubble->pos.x - pos.x, 2);
        if (xs > distanceCollision) return false;
        return (xs + pow(bubble->pos.y - pos.y, 2)) < distanceCollision; 
    }

    void UpdatePosition() {
        float ds = FrozenBubble::Instance()->deltaScale;
        if (launching) {
            // Update old positions
            oldPosX = posX;
            oldPosY = posY;

            float dx = ((float)BUBBLE_SPEED) * cosf(direction) * ds;
            float dy = ((float)BUBBLE_SPEED) * sinf(direction) * ds;

            // Update float positions
            posX += dx;  // Move in direction of angle (cos gives correct sign)
            posY -= dy;  // Always move up (negative Y)

            // Sync integer positions for rendering/collision
            oldpos = pos;
            pos.x = (int)posX;
            pos.y = (int)posY;
            if (pos.x < leftLimit) {
                AudioMixer::Instance()->PlaySFX("rebound");
                posX = 2.0f * leftLimit - posX;
                pos.x = (int)posX;
                direction -= 2.0f * (direction-PI/2.0f);
            }
            if (pos.x > rightLimit - bubbleSize) {
                AudioMixer::Instance()->PlaySFX("rebound");
                posX = 2.0f * (rightLimit - bubbleSize) - posX;
                pos.x = (int)posX;
                direction += 2.0f * (PI/2.0f-direction);
            }
        }
        else if (falling) {
            if (waitForFall > 0) {
                waitForFall--;
            }
            else {
                if (!chainExists) {
                    // Falling bubbles should have both horizontal and vertical movement
                    posX += speedX * 0.5 * ds;
                    posY += genSpeed * 0.5 * ds;
                    genSpeed += FREEFALL_CONSTANT * 0.5 * ds;
                    pos.x = (int)posX;
                    pos.y = (int)posY;
                }
                else {
                    // Chain reaction movement - physics-based parabolic arc like original FB
                    // BUT only start the swooping animation after bubble falls below threshold (maxy)
                    // This creates visible delay after explosion and makes arc animation visible
                    const float maxy = 380.0f; // Threshold Y position (from original FB)
                    const float acceleration = FREEFALL_CONSTANT * 3.0f;

                    // Only start chain reaction physics if bubble has fallen below maxy OR already started
                    if (posY > maxy || chainGoingUp) {
                        if (!chainGoingUp) {
                            // First time: calculate horizontal speed needed to reach destination
                            float time_to_zero = genSpeed / acceleration;
                            float distance_to_zero = genSpeed * (genSpeed / acceleration + 1.0f) / 2.0f;
                            float tobe_sqrted = 1.0f + 8.0f / acceleration * (posY - chainDest.y + distance_to_zero);

                            if (tobe_sqrted < 0) {
                                // Avoid sqrt of negative number
                                speedX = 0;
                            } else {
                                float time_to_destination = (-1.0f + sqrt(tobe_sqrted)) / 2.0f;
                                if (time_to_zero + time_to_destination == 0) {
                                    // Avoid division by zero
                                    speedX = 0;
                                } else {
                                    speedX = (chainDest.x - posX) / (time_to_zero + time_to_destination);
                                }
                            }
                            chainGoingUp = true;
                        }

                        // Apply physics: decelerate (going up), then accelerate (falling back down)
                        genSpeed -= acceleration * ds;
                        posX += speedX * ds;

                        // Stop horizontal movement if we've reached destination X
                        if (fabs(posX - chainDest.x) < fabs(speedX * ds)) {
                            posX = chainDest.x;
                            speedX = 0;
                        }

                        posY += genSpeed * ds;

                        // Check if reached destination Y (going upward, so Y decreases)
                        // Trigger when we've reached or passed the target
                        if (posY <= chainDest.y) {
                            posY = chainDest.y;
                            posX = chainDest.x;
                            shouldClear = true;
                            chainReachedDest = true;
                            SDL_Log("Chain reaction: bubble %d reached dest at (%.1f,%.1f)", bubbleId, posX, posY);
                        }

                        pos.x = (int)posX;
                        pos.y = (int)posY;
                    } else {
                        // Before reaching maxy threshold, just fall normally
                        posY += genSpeed * ds;
                        genSpeed += FREEFALL_CONSTANT * 0.5 * ds;
                        pos.x = (int)posX;
                        pos.y = (int)posY;
                    }
                }
                if(pos.y > 470) shouldClear = true;
            }
        }
        else if (exploding) {
            posX += speedX * 0.5;
            posY += speedY * 0.5;
            speedY += FREEFALL_CONSTANT * 0.5;
            pos.x = (int)posX;
            pos.y = (int)posY;
            if(pos.y > 470) shouldClear = true;
        }
    }

    void Render(SDL_Renderer *rend, SDL_Texture *bubbles[]) {
        if (bubbleId == -1) return;
        rect.x = pos.x;
        rect.y = pos.y;
        rect.w = rect.h = bubbleSize;
        { SDL_FRect fr = ToFRect(rect); SDL_RenderTexture(rend, bubbles[bubbleId], nullptr, &fr); }
    };
};

// Malus bubble structure for attack system
struct MalusBubble {
    int assignedArray;  // Which player's board this malus bubble belongs to
    int bubbleId;       // Bubble color/id
    int cx, cy;         // Grid position where it will stick
    int stickY;         // Final Y grid position after sticking
    float posX, posY;   // Current falling position
    SDL_Point pos;      // Integer position for rendering
    bool shouldStick = false;  // Flag to trigger sticking
    bool shouldClear = false;  // Flag to delete the bubble

    void Render(SDL_Renderer *rend, SDL_Texture **bubbles, bool useMini) {
        SDL_Rect rect;
        rect.x = pos.x;
        rect.y = pos.y;
        // Use appropriate size based on whether this is for a mini player
        rect.w = rect.h = useMini ? 16 : 32;
        { SDL_FRect fr = ToFRect(rect); SDL_RenderTexture(rend, bubbles[bubbleId], nullptr, &fr); }
    }
};

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

    finalScoreText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 28);
    finalScoreText.UpdateAlignment(TTF_HORIZONTAL_ALIGN_CENTER);
    finalScoreText.UpdateColor({255, 255, 255, 255}, {0, 0, 0, 255});

    mpTrainText.LoadFont(ASSET("/gfx/DroidSans.ttf").c_str(), 16);
    mpTrainText.UpdateColor({255, 255, 100, 255}, {0, 0, 0, 255});

    // Initialize player name/win text for multiplayer (3-5 players)
    // Player 0 (center/local) gets larger font (22), others get smaller font (16)
    for (int i = 0; i < 5; i++) {
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

void BubbleGame::LoadLevelset(const char *path) {
    std::ifstream lvlSet(path);
    std::string curLine;

    loadedLevels.clear();
    if(lvlSet.is_open())
    {
        int idx = 0;
        std::string curChar;
        std::array<std::vector<int>, 10> level;
        std::vector<int> line;
        while(std::getline(lvlSet, curLine))
        {
            if (curLine.empty())
            {
                idx = 0;
                loadedLevels.push_back(level);
            }
            else {
                std::stringstream ss(curLine);
                while(std::getline(ss, curChar, ' '))
                {
                    if(curChar.empty()) continue;
                    else if(curChar == "-") line.push_back(-1);
                    else {
                        line.push_back(stoi(curChar));
                    }
                }

                level[idx] = line;
                line.clear();
                idx++;
            }
        }
    }
    else {
        SDL_LogError(1, "No such levelset (%s).", path);
    }
}

// singleplayer function, you generate random arrays for multiplayer
void BubbleGame::LoadLevel(int id){
    std::array<std::vector<int>, 10> level = loadedLevels[id - 1];
    savedLevelGrid = level;  // Save grid at load time for highscore display (bubbles are gone after clear)
    int bubbleSize = 32;
    int rowSize = bubbleSize * 7 / 8;  // ROW_SIZE = 28

    SDL_Point &offset = bubbleArrays[0].bubbleOffset;
    std::array<std::vector<Bubble>, 13> &bubbleMap = bubbleArrays[0].bubbleMap;

    // Clear existing bubbles before loading new level
    for (size_t i = 0; i < bubbleMap.size(); i++) {
        bubbleMap[i].clear();
    }

    for (size_t i = 0; i < level.size(); i++)
    {
        int smallerSep = level[i].size() % 2 == 0 ? 0 : bubbleSize / 2;
        for (size_t j = 0; j < level[i].size(); j++)
        {
            bubbleMap[i].push_back(Bubble{level[i][j], {(smallerSep + bubbleSize * ((int)j)) + offset.x, (rowSize * ((int)i)) + offset.y}});
        }
    }
    if(bubbleMap[9].size() % 2 == 0) {
        for (int i = 0; i < 3; i++)
        {
            int smallerSep = i % 2 == 0 ? 0 : bubbleSize / 2;
            for (int j = 0; j < (i % 2 == 0 ? 7 : 8); j++) bubbleMap[10 + i].push_back({-1, {(smallerSep + bubbleSize * j) + offset.x, (rowSize * (10 + i)) + offset.y}});
        }
    } else {
        for (int i = 1; i < 4; i++)
        {
            int smallerSep = i % 2 == 0 ? bubbleSize / 2 : 0;
            for (int j = 0; j < (i % 2 == 0 ? 7 : 8); j++) bubbleMap[10 + (i - 1)].push_back({-1, {(smallerSep + bubbleSize * j) + offset.x, (rowSize * (9 + i)) + offset.y}});
        }
    }

    char lvnm[64];
    snprintf(lvnm, sizeof(lvnm), "Level %i", id);
    inGameText.UpdateText(renderer, lvnm, 0);
    inGameText.UpdatePosition({75 - (inGameText.Coords()->w / 2), 105});
}

void BubbleGame::RandomLevel(BubbleArray &bArray){
    int bubbleSize = 32;
    int rowSize = bubbleSize * 7 / 8;  // ROW_SIZE = 28

    SDL_Point &offset = bArray.bubbleOffset;
    std::array<std::vector<Bubble>, 13> &bubbleMap = bArray.bubbleMap;

    // Clear existing bubbles before generating new level
    for (size_t i = 0; i < bubbleMap.size(); i++) {
        bubbleMap[i].clear();
    }

    int r = ranrange(0,1);
    int untilend = (13 / 2) - 1;
    for (size_t i = 0; i < bArray.bubbleMap.size(); i++)
    {
        int size = (i + r) % 2 == 0 ? 8 : 7;
        int smallerSep = (i + r) % 2 == 0 ? 0 : bubbleSize / 2;
        for (int j = 0; j < size; j++)
        {
            bubbleMap[i].push_back(Bubble{(int)i < untilend ? ranrange(0, bArray.numColors - 1) : -1, {(smallerSep + bubbleSize * ((int)j)) + offset.x, (rowSize * ((int)i)) + offset.y}});
        }
    }

    if(currentSettings.playerCount < 2) {
        inGameText.UpdateText(renderer, "Random level", 0);
        inGameText.UpdatePosition({75 - (inGameText.Coords()->w / 2), 105});
    }
    else if (currentSettings.playerCount == 2) {
        Update2PText();
    }
    else if (currentSettings.playerCount >= 3) {
        UpdatePlayerNameWinText();
    }
}

bool BubbleGame::SyncNetworkLevel() {
    // Synchronize level generation for network multiplayer
    SDL_Log("SyncNetworkLevel: Starting level synchronization");

    NetworkClient* netClient = NetworkClient::Instance();
    if (!netClient || !netClient->IsConnected()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SyncNetworkLevel: Not connected!");
        return false;
    }

    const int bubbleSize = 32;  // Full-size bubble pixel width

    // Clear all players' maps
    for (int playerIdx = 0; playerIdx < currentSettings.playerCount; playerIdx++) {
        for (size_t i = 0; i < bubbleArrays[playerIdx].bubbleMap.size(); i++) {
            bubbleArrays[playerIdx].bubbleMap[i].clear();
        }
    }

    bool isLeader = netClient->IsLeader();
    SDL_Log("SyncNetworkLevel: We are %s, syncing for %d players",
            isLeader ? "LEADER" : "JOINER", currentSettings.playerCount);

    // Generate/receive the initial 5 rows of bubbles (like original)
    for (int cy = 0; cy < 5; cy++) {
        int rowSize = (cy % 2 == 0) ? 8 : 7;  // 8 bubbles on even rows, 7 on odd rows

        for (int cx = 0; cx < rowSize; cx++) {
            int bubbleId;

            if (isLeader) {
                // Leader generates and sends
                bubbleId = ranrange(0, bubbleArrays[0].numColors - 1);  // Random bubble color
                SDL_Log("Leader sending bubble: cx=%d cy=%d id=%d", cx, cy, bubbleId);
                netClient->SendBubble(cx, cy, bubbleId);
            } else {
                // Joiner receives
                int recv_cx, recv_cy, recv_id;
                if (!netClient->WaitForBubble(recv_cx, recv_cy, recv_id)) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to receive bubble at cx=%d cy=%d", cx, cy);
                    return false;
                }
                if (recv_cx != cx || recv_cy != cy) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Bubble position mismatch! Expected (%d,%d) got (%d,%d)",
                                cx, cy, recv_cx, recv_cy);
                }
                bubbleId = recv_id;
            }

            // Add bubble to ALL active players (handles 2-5 player games)
            for (int playerIdx = 0; playerIdx < currentSettings.playerCount; playerIdx++) {
                // Mini players use half bubble size for spacing AND mini textures
                // Original: local $BUBBLE_SIZE = $BUBBLE_SIZE / 2; local $ROW_SIZE = $ROW_SIZE / 2;
                bool isMini = (currentSettings.playerCount >= 3 && playerIdx >= 1);
                int effectiveBubbleSize = isMini ? bubbleSize / 2 : bubbleSize;  // 16 for mini, 32 for full
                int effectiveRowSize = effectiveBubbleSize * 7 / 8;  // 14 for mini, 28 for full

                SDL_Point& offset = bubbleArrays[playerIdx].bubbleOffset;
                int smallerSep = (cy % 2 == 0) ? 0 : effectiveBubbleSize / 2;

                int posX = (smallerSep + effectiveBubbleSize * cx) + offset.x;
                int posY = (effectiveRowSize * cy) + offset.y;

                // Log first few bubbles for each player to verify calculation
                if (cx <= 2 && cy <= 1) {
                    SDL_Log("Player %d bubble [%d][%d]: isMini=%d, bubbleSize=%d, rowSize=%d, pos=(%d,%d)",
                            playerIdx, cy, cx, isMini, effectiveBubbleSize, effectiveRowSize, posX, posY);
                }

                bubbleArrays[playerIdx].bubbleMap[cy].push_back(Bubble{
                    bubbleId,
                    {posX, posY}
                });
            }
        }
    }

    // Fill remaining rows with empty (-1)
    for (size_t cy = 5; cy < bubbleArrays[0].bubbleMap.size(); cy++) {
        int rowSize = (cy % 2 == 0) ? 8 : 7;

        for (int cx = 0; cx < rowSize; cx++) {
            // Add empty bubbles to ALL active players
            for (int playerIdx = 0; playerIdx < currentSettings.playerCount; playerIdx++) {
                // Mini players use half bubble size for spacing AND mini textures
                bool isMini = (currentSettings.playerCount >= 3 && playerIdx >= 1);
                int effectiveBubbleSize = isMini ? bubbleSize / 2 : bubbleSize;  // 16 for mini, 32 for full
                int effectiveRowSize = effectiveBubbleSize * 7 / 8;  // 14 for mini, 28 for full

                SDL_Point& offset = bubbleArrays[playerIdx].bubbleOffset;
                int smallerSep = (cy % 2 == 0) ? 0 : effectiveBubbleSize / 2;

                bubbleArrays[playerIdx].bubbleMap[cy].push_back(Bubble{
                    -1,
                    {(smallerSep + effectiveBubbleSize * cx) + offset.x, (effectiveRowSize * ((int)cy)) + offset.y}
                });
            }
        }
    }

    SDL_Log("SyncNetworkLevel: Bubble grid synchronized");

    // Now sync the next and tobe bubbles
    int nextBubbleId, tobeBubbleId;

    if (isLeader) {
        // Leader generates and sends
        nextBubbleId = ranrange(0, bubbleArrays[0].numColors - 1);
        tobeBubbleId = ranrange(0, bubbleArrays[0].numColors - 1);
        SDL_Log("Leader sending next=%d tobe=%d", nextBubbleId, tobeBubbleId);
        netClient->SendNextBubble(nextBubbleId);
        netClient->SendTobeBubble(tobeBubbleId);
    } else {
        // Joiner receives
        if (!netClient->WaitForNextBubble(nextBubbleId)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to receive next bubble");
            return false;
        }
        if (!netClient->WaitForTobeBubble(tobeBubbleId)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to receive tobe bubble");
            return false;
        }
    }

    // Set the launcher bubbles for ALL players (they get the same bubbles)
    // In original: next_num → tobe_launched (curLaunch), tobe_num → next_bubble
    // See frozen-bubble line 3450-3456: generate_new_bubble is called for ALL players in iter_players
    for (int i = 0; i < currentSettings.playerCount; i++) {
        bubbleArrays[i].curLaunch = nextBubbleId;  // Current bubble in launcher
        bubbleArrays[i].nextBubble = tobeBubbleId; // Next bubble shown on side
        SDL_Log("Initialized player %d: curLaunch=%d, nextBubble=%d", i, nextBubbleId, tobeBubbleId);

        // Initialize nextColors queue (Perl: $pdata{$player}{nextcolors})
        // 8 upcoming colors, pre-generated for use when new root row is added
        bubbleArrays[i].nextColors.clear();
        for (int k = 0; k < 8; k++) bubbleArrays[i].nextColors.push_back(ranrange(0, bubbleArrays[i].numColors - 1));
    }

    SDL_Log("SyncNetworkLevel: Complete! next=%d tobe=%d", nextBubbleId, tobeBubbleId);

    if (currentSettings.playerCount == 2) {
        Update2PText();
    } else if (currentSettings.playerCount >= 3) {
        UpdatePlayerNameWinText();
    }
    return true;
}

void SetupGameMetrics(BubbleArray *bArray, int playerCount, bool lowGfx, bool localMultiplayer = false){
    switch (playerCount) {
        case 2:
            if (lowGfx) {
                bArray[0].lGfxShooterRct.w = bArray[0].lGfxShooterRct.h = 2;
                bArray[1].lGfxShooterRct.w = bArray[1].lGfxShooterRct.h = 2;
            }
            bArray[0].curLaunchRct = {SCREEN_CENTER_X+148, 480-89, 32, 32};
            bArray[0].nextBubbleRct = {SCREEN_CENTER_X+148, 480-40, 32, 32};
            bArray[0].onTopRct = {SCREEN_CENTER_X+144, 480-43, 39, 39};
            bArray[0].frozenBottomRct = {SCREEN_CENTER_X+143, 480-43, 39, 39};

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

    // Reset game state flags
    gameFinish = gameWon = gameLost = gameMatchOver = false;
    gameMpDone = false;
    sendMalusToOne = -1;
    attackingMe.clear();
    for (int i = 0; i < 5; i++) playerTargeting[i] = -1;
    pendingHighscore = false;
    curLevel = setup.startLevel;
    connectedPlayerCount = setup.playerCount;  // Reset connected count for new game

    // Reset multiplayer training state
    mpTrainScore = 0;
    mpTrainDone = false;
    mpTrainStartTime = 0;

    winsP1 = winsP2 = 0;
    for (int i = 0; i < 5; i++) {
        bubbleArrays[i].winCount = 0;
        // Apply per-player color count (5-8); default 7 for single player
        int nc = (setup.playerCount >= 2) ? setup.playerColors[i] : 7;
        nc = (nc < 5) ? 5 : (nc > 8) ? 8 : nc;
        bubbleArrays[i].numColors = nc;
        bubbleArrays[i].compressionDisabled = setup.disableCompression[i];
        bubbleArrays[i].aimGuideEnabled = setup.aimGuide[i];
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
            bubbleArrays[0].shooterSprite.rect = {SCREEN_CENTER_X + 110, 480 - 123, 100, 100};
            bubbleArrays[0].shooterSprite.angle = PI/2.0f;
            bubbleArrays[0].bubbleOffset = {354, 40};
            bubbleArrays[0].leftLimit = SCREEN_CENTER_X + 32;
            bubbleArrays[0].rightLimit = 640 - 28;
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
            // Next bubble at next_bubble position {112, 440}
            bubbleArrays[0].nextBubbleRct = {112, 440, 32, 32};
            bubbleArrays[0].onTopRct = {108, 437, 39, 39};  // next_bubble + on_top_next_relpos {-4,-3}
            bubbleArrays[0].frozenBottomRct = {108, 437, 39, 39};
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
            // Next bubble: next_bubble position {56, 216} (ABSOLUTE)
            bubbleArrays[1].nextBubbleRct = {56, 216, 32, 32};
            bubbleArrays[1].onTopRct = {54, 214, 39, 39};  // next_bubble + on_top_next_relpos {-2,-2}
            bubbleArrays[1].frozenBottomRct = {54, 214, 39, 39};
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
            bubbleArrays[0].nextBubbleRct = {112, 440, 32, 32};
            bubbleArrays[0].onTopRct = {108, 437, 39, 39};
            bubbleArrays[0].frozenBottomRct = {108, 437, 39, 39};
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
            bubbleArrays[1].nextBubbleRct = {56, 216, 32, 32};
            bubbleArrays[1].onTopRct = {54, 214, 39, 39};
            bubbleArrays[1].frozenBottomRct = {54, 214, 39, 39};
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
            bubbleArrays[3].nextBubbleRct = {56, 445, 32, 32};
            bubbleArrays[3].onTopRct = {54, 443, 39, 39};
            bubbleArrays[3].frozenBottomRct = {54, 443, 39, 39};
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
            bubbleArrays[0].nextBubbleRct = {112, 440, 32, 32};
            bubbleArrays[0].onTopRct = {108, 437, 39, 39};
            bubbleArrays[0].frozenBottomRct = {108, 437, 39, 39};
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
            bubbleArrays[1].nextBubbleRct = {56, 216, 32, 32};
            bubbleArrays[1].onTopRct = {54, 214, 39, 39};
            bubbleArrays[1].frozenBottomRct = {54, 214, 39, 39};
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
            bubbleArrays[3].nextBubbleRct = {56, 445, 32, 32};
            bubbleArrays[3].onTopRct = {54, 443, 39, 39};
            bubbleArrays[3].frozenBottomRct = {54, 443, 39, 39};
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
    }

    // Set lobby player IDs for network games
    if (currentSettings.networkGame) {
        NetworkClient* netClient = NetworkClient::Instance();
        if (netClient && netClient->IsConnected()) {
            bubbleArrays[0].lobbyPlayerId = netClient->GetMyPlayerId();
            bubbleArrays[0].playerNickname = netClient->GetPlayerNickname(bubbleArrays[0].lobbyPlayerId);
            SDL_Log("Set local player (array 0) lobbyPlayerId = %d, nickname = '%s'",
                    bubbleArrays[0].lobbyPlayerId, bubbleArrays[0].playerNickname.c_str());

            // Pre-populate remote players' lobbyPlayerIds and nicks from GAME_CAN_START mapping
            // This ensures correct nick targeting for malus messages even before the first shot
            const auto& idToNick = netClient->GetPlayerIdToNick();
            int remoteSlot = 1;
            for (const auto& kv : idToNick) {
                if (remoteSlot >= currentSettings.playerCount) break;
                if (kv.first == (int)netClient->GetMyPlayerId()) continue;  // Skip local player
                bubbleArrays[remoteSlot].lobbyPlayerId = kv.first;
                bubbleArrays[remoteSlot].playerNickname = kv.second;
                SDL_Log("Pre-assigned remote player (array %d) lobbyPlayerId = %d, nickname = '%s'",
                        remoteSlot, kv.first, kv.second.c_str());
                remoteSlot++;
            }
            // Any remaining remote slots are unknown
            for (int i = remoteSlot; i < currentSettings.playerCount; i++) {
                bubbleArrays[i].lobbyPlayerId = -1;
                bubbleArrays[i].playerNickname = "";
            }

            // Remap per-player settings (aimGuide, compression, colors) from lobby slot order
            // to bubbleArray order. setup.aimGuide[i] is indexed by the game room's player list
            // (host=0, first joiner=1, ...), but bubbleArrays[0] is always the local player.
            // Match by nick to apply the right setting to each array.
            const GameRoom* room = netClient->GetCurrentGame();
            if (room) {
                const auto& roomPlayers = room->players;
                for (int arr = 0; arr < currentSettings.playerCount; arr++) {
                    const std::string& nick = bubbleArrays[arr].playerNickname;
                    for (int slot = 0; slot < (int)roomPlayers.size() && slot < 5; slot++) {
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

    SetupGameMetrics(bubbleArrays, currentSettings.playerCount, lowGfx, currentSettings.localMultiplayer);

    // Clear any remaining single bubbles from previous game
    singleBubbles.clear();

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
    if (level >= (int)loadedLevels.size() && !currentSettings.randomLevels){
        QuitToTitle();
        return;
    }

    SDL_Renderer *rend = const_cast<SDL_Renderer*>(renderer);

    TransitionManager::Instance()->DoSnipIn(rend);
    firstRenderDone = false;

    gameFinish = gameWon = gameLost = gameMatchOver = false;
    gameMpDone = false;
    sendMalusToOne = -1;
    attackingMe.clear();
    for (int i = 0; i < 5; i++) playerTargeting[i] = -1;

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
            // Reset fields that change during a round (bubbleOffset shifts with compressor)
            bubbleArrays[0].shooterSprite.angle = PI/2.0f;
            bubbleArrays[0].bubbleOffset = {190, 44};
            bubbleArrays[0].turnsToCompress = 12;
            bubbleArrays[0].numSeparators = 0;

            bubbleArrays[1].shooterSprite.angle = PI/2.0f;
            bubbleArrays[1].bubbleOffset = {20, 19};
            bubbleArrays[1].turnsToCompress = 12;
            bubbleArrays[1].numSeparators = 0;

            bubbleArrays[2].shooterSprite.angle = PI/2.0f;
            bubbleArrays[2].bubbleOffset = {492, 19};
            bubbleArrays[2].turnsToCompress = 12;
            bubbleArrays[2].numSeparators = 0;
            break;
        case 4:
            bubbleArrays[0].shooterSprite.angle = PI/2.0f;
            bubbleArrays[0].bubbleOffset = {190, 44};
            bubbleArrays[0].turnsToCompress = 12;
            bubbleArrays[0].numSeparators = 0;
            bubbleArrays[0].curLaunchRct = {302, 390, 32, 32};
            bubbleArrays[0].nextBubbleRct = {112, 440, 32, 32};
            bubbleArrays[0].onTopRct = {108, 437, 39, 39};
            bubbleArrays[0].frozenBottomRct = {108, 437, 39, 39};

            bubbleArrays[1].shooterSprite.angle = PI/2.0f;
            bubbleArrays[1].bubbleOffset = {20, 19};
            bubbleArrays[1].turnsToCompress = 12;
            bubbleArrays[1].numSeparators = 0;
            bubbleArrays[1].curLaunchRct = {68, 192, 32, 32};
            bubbleArrays[1].nextBubbleRct = {56, 216, 32, 32};
            bubbleArrays[1].onTopRct = {54, 214, 39, 39};
            bubbleArrays[1].frozenBottomRct = {54, 214, 39, 39};

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
            bubbleArrays[3].nextBubbleRct = {56, 445, 32, 32};
            bubbleArrays[3].onTopRct = {54, 443, 39, 39};
            bubbleArrays[3].frozenBottomRct = {54, 443, 39, 39};
            break;
        case 5:
            bubbleArrays[0].shooterSprite.angle = PI/2.0f;
            bubbleArrays[0].bubbleOffset = {190, 44};
            bubbleArrays[0].turnsToCompress = 12;
            bubbleArrays[0].numSeparators = 0;
            bubbleArrays[0].curLaunchRct = {302, 390, 32, 32};
            bubbleArrays[0].nextBubbleRct = {112, 440, 32, 32};
            bubbleArrays[0].onTopRct = {108, 437, 39, 39};
            bubbleArrays[0].frozenBottomRct = {108, 437, 39, 39};

            bubbleArrays[1].shooterSprite.angle = PI/2.0f;
            bubbleArrays[1].bubbleOffset = {20, 19};
            bubbleArrays[1].turnsToCompress = 12;
            bubbleArrays[1].numSeparators = 0;
            bubbleArrays[1].curLaunchRct = {68, 192, 32, 32};
            bubbleArrays[1].nextBubbleRct = {56, 216, 32, 32};
            bubbleArrays[1].onTopRct = {54, 214, 39, 39};
            bubbleArrays[1].frozenBottomRct = {54, 214, 39, 39};

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
            bubbleArrays[3].nextBubbleRct = {56, 445, 32, 32};
            bubbleArrays[3].onTopRct = {54, 443, 39, 39};
            bubbleArrays[3].frozenBottomRct = {54, 443, 39, 39};

            bubbleArrays[4].shooterSprite.angle = PI/2.0f;
            bubbleArrays[4].bubbleOffset = {492, 247};
            bubbleArrays[4].turnsToCompress = 12;
            bubbleArrays[4].numSeparators = 0;
            bubbleArrays[4].curLaunchRct = {540, 420, 32, 32};
            bubbleArrays[4].nextBubbleRct = {548, 445, 32, 32};
            bubbleArrays[4].onTopRct = {546, 443, 39, 39};
            bubbleArrays[4].frozenBottomRct = {546, 443, 39, 39};
            break;
    }

    RemoveArray(bubbleArrays, currentSettings.playerCount);
    SetupGameMetrics(bubbleArrays, currentSettings.playerCount, lowGfx, currentSettings.localMultiplayer);

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
    }
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

void BubbleGame::LaunchBubble(BubbleArray &bArray) {
    audMixer->PlaySFX("launch");
    SDL_Log("Launching bubble at angle: %.4f radians (%.2f degrees from center), lowGfx=%d, cos=%.4f",
            bArray.shooterSprite.angle,
            (bArray.shooterSprite.angle - PI/2.0f) * 180.0f / PI,
            lowGfx,
            cosf(bArray.shooterSprite.angle));
    // Use curLaunchRct for launch position (set per-player in NewGame/ReloadGame)
    float startX = (float)bArray.curLaunchRct.x;
    float startY = (float)bArray.curLaunchRct.y;
    singleBubbles.push_back({bArray.playerAssigned, bArray.curLaunch, startX, startY, startX, startY, {(int)startX, (int)startY}, {}, bArray.shooterSprite.angle, false, true, bArray.leftLimit, bArray.rightLimit, bArray.topLimit, lowGfx});
    PickNextBubble(bArray);
    FrozenBubble::Instance()->totalBubbles++;
    bArray.hurryTimer = 0;
    bArray.chainLevel = 0; // Reset chain level for new shot

    // Send shot to network if this is a network game AND this is the local player
    // Don't send if this is a remote player's shot (we're just replicating their fire from mpFirePending)
    if (currentSettings.networkGame && bArray.playerAssigned == 0) {
        SendNetworkBubbleShot(bArray);
    }
}

void BubbleGame::UpdatePenguin(BubbleArray &bArray) {
    if (gameFinish) return;

    // In network games, only process keyboard input for local player (array 0)
    // Remote player's actions (array 1) come from network messages (mpFirePending flag)
    // But we still need to process the fire logic for all players (original: iter_players at line 2105)
    // Original checks mp_fire for ALL players, not just local (line 2141)
    // In local multiplayer, ALL players are local (each uses their own controller)
    bool isLocalPlayer = (bArray.playerAssigned == 0) || !currentSettings.networkGame;

    // Process keyboard input only for local players (original: is_local_player($::p))
    if (isLocalPlayer) {
        // Use configured keys from settings
        // Don't accept input if player has lost or game is finished (except local player 0 in finished state)
        // Original: checks if $pdata{state} eq 'game'
        bool acceptInput = (bArray.playerState == BubbleArray::PlayerState::ALIVE);
        if (!acceptInput && bArray.playerAssigned == 0 && gameFinish) {
            // Allow local player to continue for a bit during finish sequence
            acceptInput = false;
        }

        GameSettings* gs = GameSettings::Instance();
        PlayerKeys* allPlayerKeys[5] = {
            &gs->player1Keys, &gs->player2Keys, &gs->player3Keys,
            &gs->player4Keys, &gs->player5Keys
        };
        int pIdx = (bArray.playerAssigned >= 0 && bArray.playerAssigned < 5) ? bArray.playerAssigned : 0;
        PlayerKeys& keys = *allPlayerKeys[pIdx];

        if (acceptInput) {
            if (currentSettings.localMultiplayer && bArray.playerAssigned >= 0 && bArray.playerAssigned < 5) {
                // Local multiplayer: keyboard first (always works, matches 1P path),
                // then OR with controller DPAD for real gamepads.
                // Note: on Android TV, opening SDL_Gamepad can suppress d-pad
                // keyboard events, so keyboard must be read unconditionally first.
                int idx = bArray.playerAssigned;
                // IsKeyPressed() handles real keyboard and virtual controller scancodes.
                // Also OR with controllerInputs[] which frozenbubble.cpp writes directly
                // (SDL_GetKeyboardState ignores synthetic SDL_PushEvent KEYDOWN events).
                bArray.shooterLeft   = IsKeyPressed(keys.left)   || controllerInputs[idx].left;
                bArray.shooterRight  = IsKeyPressed(keys.right)  || controllerInputs[idx].right;
                bArray.shooterCenter = IsKeyPressed(keys.center) || controllerInputs[idx].center;
                bArray.shooterAction = IsKeyPressed(keys.fire)   || controllerInputs[idx].fire;
                if (idx < numControllersOpen && controllers[idx]) {
                    SDL_Gamepad* ctrl = controllers[idx];
                    bArray.shooterLeft   = bArray.shooterLeft   || SDL_GetGamepadButton(ctrl, SDL_GAMEPAD_BUTTON_DPAD_LEFT)  != 0;
                    bArray.shooterRight  = bArray.shooterRight  || SDL_GetGamepadButton(ctrl, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) != 0;
                    bArray.shooterCenter = bArray.shooterCenter || SDL_GetGamepadButton(ctrl, SDL_GAMEPAD_BUTTON_DPAD_UP)    != 0;
                    bArray.shooterAction = bArray.shooterAction || SDL_GetGamepadButton(ctrl, SDL_GAMEPAD_BUTTON_SOUTH)      != 0
                                        || SDL_GetGamepadButton(ctrl, SDL_GAMEPAD_BUTTON_DPAD_UP)                            != 0;
                }
            } else if (bArray.playerAssigned == 0) {
                bArray.shooterAction = IsKeyPressed(keys.fire);
                bArray.shooterLeft   = IsKeyPressed(keys.left);
                bArray.shooterRight  = IsKeyPressed(keys.right);
                bArray.shooterCenter = IsKeyPressed(keys.center);
            }
            else if (bArray.playerAssigned == 1) {
                bArray.shooterAction = IsKeyPressed(keys.fire);
                bArray.shooterLeft   = IsKeyPressed(keys.left);
                bArray.shooterRight  = IsKeyPressed(keys.right);
                bArray.shooterCenter = IsKeyPressed(keys.center);
            }
        } else {
            // Player is dead or game finished - clear all shooter flags
            bArray.shooterAction = false;
            bArray.shooterLeft = false;
            bArray.shooterRight = false;
            bArray.shooterCenter = false;
        }
    }

    // Hurry timer and warnings only for local players (remote players have their own timers)
    if (isLocalPlayer) {
        if (currentSettings.playerCount < 2) {
            if (bArray.hurryTimer >= TIME_HURRY_WARN) {
                if (bArray.warnTimer <= HURRY_WARN_FC / 2){
                    if(bArray.warnTimer == 0) audMixer->PlaySFX("hurry");
                    { SDL_FRect fr = ToFRect(bArray.hurryRct); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), bArray.hurryTexture, nullptr, &fr); }
                }
                bArray.warnTimer++;
                if (bArray.warnTimer > HURRY_WARN_FC) {
                    bArray.warnTimer = 0;
                }
                if (bArray.hurryTimer >= TIME_HURRY_MAX) {
                    bArray.shooterAction = true;
                }
            }
        }
        else {
            if (bArray.hurryTimer >= TIME_HURRY_WARN_MP) {
                if (bArray.warnTimer <= HURRY_WARN_MP_FC / 2){
                    if(bArray.warnTimer == 0) audMixer->PlaySFX("hurry");
                    { SDL_FRect fr = ToFRect(bArray.hurryRct); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), bArray.hurryTexture, nullptr, &fr); }
                }
                bArray.warnTimer++;
                if (bArray.warnTimer > HURRY_WARN_MP_FC) {
                    bArray.warnTimer = 0;
                }
                if (bArray.hurryTimer >= TIME_HURRY_MAX_MP) {
                    bArray.shooterAction = true;
                }
            }
        }
        bArray.hurryTimer++;
    }

    float &angle = bArray.shooterSprite.angle;
    Penguin &penguin = bArray.penguinSprite;

    // Mouse/touch aim: snap angle directly, before the fire check so the correct angle is used
    if (bArray.mouseTargetAngle >= 0.f) {
        angle = bArray.mouseTargetAngle;
        if (angle < 0.1f) angle = 0.1f;
        if (angle > (float)PI - 0.1f) angle = (float)PI - 0.1f;
    }
    // Mouse/touch fire: inject as shooterAction before the fire check
    if (bArray.mouseFirePending) {
        bArray.shooterAction = true;
        bArray.mouseFirePending = false;
    }

    // Check if we should fire: either local player action or remote player mp_fire flag (original line 2141)
    // Block local player from firing while malus bubbles are falling (original line 2146: !@{$malus_bubble{$::p}})
    bool localMalusInFlight = false;
    if (currentSettings.networkGame && bArray.playerAssigned == 0) {
        for (const auto& mb : malusBubbles)
            if (mb.assignedArray == 0 && !mb.shouldClear) { localMalusInFlight = true; break; }
    }

    // For remote players (mp_fire), fire immediately regardless of newShoot state
    // For local players, only fire if newShoot is true (no bubble currently in flight)
    if(bArray.mpFirePending || (!localMalusInFlight && bArray.shooterAction == true && bArray.newShoot == true)) {
        penguin.sleeping = 0;
        if(penguin.curAnimation != 1) penguin.PlayAnimation(1);

        // For remote players with mp_fire, use the angle from the network message
        if (bArray.mpFirePending) {
            angle = bArray.pendingAngle;
            SDL_Log("Launching remote player %d bubble with angle %.3f from network", bArray.playerAssigned, angle);
        }

        LaunchBubble(bArray);
        bArray.shooterAction = false;
        bArray.mpFirePending = false;  // Clear mp_fire flag (original line 2165)
        bArray.newShoot = false;
        return;
    }

    if (angle < 0.1) angle = 0.1;
    if (angle > PI-0.1) angle = PI-0.1;

    if (bArray.shooterLeft || bArray.shooterRight || bArray.shooterCenter) {
        float ds = FrozenBubble::Instance()->deltaScale;
        float launchStep = (float)LAUNCHER_SPEED * ds;
        if (bArray.shooterLeft) {
            angle += launchStep;  // Move LEFT = increase angle (toward π)
            if(penguin.curAnimation != 1 && (penguin.curAnimation > 7 || penguin.curAnimation < 2)) penguin.PlayAnimation(2);
        }
        else if (bArray.shooterRight) {
            angle -= launchStep;  // Move RIGHT = decrease angle (toward 0)
            if(penguin.curAnimation != 1 && (penguin.curAnimation > 7 || penguin.curAnimation < 2)) penguin.PlayAnimation(5);
        }
        else if (bArray.shooterCenter) {
            if (angle >= PI/2.0f - launchStep && angle <= PI/2.0f + launchStep) angle = PI/2.0f;
            else angle += (angle < PI/2.0f) ? launchStep : -launchStep;
        }

        penguin.sleeping = 0;
    }
    else if(!bArray.shooterLeft && !bArray.shooterRight && !bArray.shooterCenter) {
        penguin.sleeping++;
        if (penguin.sleeping > TIMEOUT_PENGUIN_SLEEP && (penguin.curAnimation > 9 || penguin.curAnimation < 8)) penguin.PlayAnimation(8);
    }

    if (!bArray.shooterLeft && penguin.curAnimation == 3) penguin.PlayAnimation(4);
    if (!bArray.shooterRight && penguin.curAnimation == 6) penguin.PlayAnimation(7);
}

// only called for a new game.
void BubbleGame::ChooseFirstBubble(BubbleArray *bArray) {
    // Original lines 3431-3456: next_num and tobe_num picked once from player 0's colors,
    // then ALL players get the same values.
    std::vector<int> p0Bubbles = bArray[0].remainingBubbles();
    int firstColor = p0Bubbles[ranrange(1, p0Bubbles.size()) - 1];
    int nextColor  = p0Bubbles[ranrange(1, p0Bubbles.size()) - 1];
    for (int i = 0; i < currentSettings.playerCount; i++) {
        bArray[i].curLaunch  = firstColor;
        bArray[i].nextBubble = nextColor;
    }
}

void BubbleGame::PickNextBubble(BubbleArray &bArray) {
    bArray.curLaunch = bArray.nextBubble;
    std::vector<int> currentBubbles = bArray.remainingBubbles();
    bArray.nextBubble = currentBubbles[ranrange(1, currentBubbles.size()) - 1];
    // Rotate nextColors queue: remove first (just used), append new random
    // Matches Perl: nextcolors is updated after each shot so all clients can compute future root rows
    if (!bArray.nextColors.empty()) bArray.nextColors.erase(bArray.nextColors.begin());
    bArray.nextColors.push_back(ranrange(0, bArray.numColors - 1));
}

// Build the space-separated nextcolors string for 's' messages (Perl: "@{$pdata{$::p}{nextcolors}}")
static std::string BuildNextColorsStr(const BubbleArray &bArray) {
    std::string s;
    for (int c : bArray.nextColors) {
        if (!s.empty()) s += ' ';
        s += std::to_string(c);
    }
    return s;
}

// oddswap mirrors Perl's $pdata{$player}{oddswap}:
//   0 = standard (row 0 has 8 cells, no hex offset)
//   1 = flipped  (row 0 has 7 cells, 16px hex offset)
// Original: next_positions() uses even($b->{cy}+$pdata{$player}{oddswap})
// to select which offset direction to use for rows above/below.
static std::vector<std::pair<int,int>> GridNeighborOffsets(int row, int oddswap = 0) {
    if ((row + oddswap) % 2 == 0)
        return {{-1,-1}, {-1,0}, {0,-1}, {0,1}, {1,-1}, {1,0}};
    else
        return {{-1,0}, {-1,1}, {0,-1}, {0,1}, {1,0}, {1,1}};
}

static bool IsGridAdjacent(int r1, int c1, int r2, int c2, int oddswap = 0) {
    for (auto [dr, dc] : GridNeighborOffsets(r1, oddswap))
        if (r1 + dr == r2 && c1 + dc == c2) return true;
    return false;
}

// anchorRow/anchorCol: grid position of the hit bubble (-1 if none, e.g. ceiling hit).
// When provided, the result is guaranteed to be a free cell adjacent to the anchor.
void GetClosestFreeCell(SingleBubble &sBubble, BubbleArray &bArray, int *row, int *col,
                        int anchorRow = -1, int anchorCol = -1) {
    // Original: get_array_closest_pos() at frozen-bubble lines 636-641
    // Uses MIDPOINT between old and new position (line 2208-2209)

    const int BUBBLE_SIZE = 32;  // Original: line 91
    const int ROW_SIZE = 28;     // Original: BUBBLE_SIZE * 7/8

    // oddswap: 0 if row 0 has 8 cells (standard), 1 if row 0 has 7 cells (flipped)
    // Matches Perl's $pdata{$player}{oddswap} set by RandomLevel's r value
    int oddswap = (bArray.bubbleMap[0].size() == 8) ? 0 : 1;

    float midX = (sBubble.oldPosX + sBubble.posX) / 2.0f;
    float midY = (sBubble.oldPosY + sBubble.posY) / 2.0f;

    // Formula: int(($y-top_limit+ROW_SIZE/2) / ROW_SIZE)
    // Use bubbleOffset (not leftLimit/topLimit) as the base — bubbleOffset is what the grid
    // cells are actually positioned from. leftLimit/topLimit can differ by a few pixels.
    int cy = (int)((midY - bArray.bubbleOffset.y + ROW_SIZE/2.0f) / ROW_SIZE);

    // Clamp row first so we can read the actual row size
    if (cy < 0) cy = 0;
    if (cy > 12) cy = 12;

    // Row offset: 16px if row has 7 bubbles (odd-shifted), 0 if 8 bubbles.
    // Derived from actual row size so it works for both r=0 and r=1 RandomLevel starts.
    int oddRowOffset = (bArray.bubbleMap[cy].size() == 7) ? BUBBLE_SIZE / 2 : 0;
    int cx = (int)((midX - bArray.bubbleOffset.x + BUBBLE_SIZE/2.0f - oddRowOffset) / BUBBLE_SIZE);

    // Clamp column using actual row size (not assumed parity) to avoid out-of-bounds
    if (cx < 0) cx = 0;
    int maxCol = (int)bArray.bubbleMap[cy].size() - 1;
    if (cx > maxCol) cx = maxCol;

    // If we have an anchor (hit bubble's grid pos), ensure the result is adjacent to it.
    // This prevents placement at a disconnected cell that CheckAirBubbles would immediately remove.
    if (anchorRow >= 0 && anchorCol >= 0) {
        bool calcFree = (bArray.bubbleMap[cy][cx].bubbleId == -1);
        bool calcAdj  = IsGridAdjacent(anchorRow, anchorCol, cy, cx, oddswap);

        if (!calcFree || !calcAdj) {
            // Find the free neighbor of the anchor whose pixel center is closest to midpoint
            float bestDist = 1e30f;
            int bestRow = cy, bestCol = cx;
            bool foundAdj = false;

            for (auto [dr, dc] : GridNeighborOffsets(anchorRow, oddswap)) {
                int nr = anchorRow + dr;
                int nc = anchorCol + dc;
                if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
                if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;
                if (bArray.bubbleMap[nr][nc].bubbleId != -1) continue; // occupied

                int cellRowOffset = (bArray.bubbleMap[nr].size() == 7) ? BUBBLE_SIZE / 2 : 0;
                float cellX = (float)(bArray.bubbleOffset.x + nc * BUBBLE_SIZE + cellRowOffset);
                float cellY = (float)(bArray.bubbleOffset.y + nr * ROW_SIZE);
                float dist  = (cellX - midX)*(cellX - midX) + (cellY - midY)*(cellY - midY);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestRow  = nr;
                    bestCol  = nc;
                    foundAdj = true;
                }
            }

            if (foundAdj) {
                if (!calcAdj)
                    SDL_Log("GetClosestFreeCell: midpoint gave [%d][%d] not adj to anchor [%d][%d]; using [%d][%d]",
                            cy, cx, anchorRow, anchorCol, bestRow, bestCol);
                cy = bestRow;
                cx = bestCol;
            } else {
                // All anchor neighbors occupied — fall back to original BFS from anchor
                SDL_Log("WARNING: all anchor [%d][%d] neighbors occupied, BFS fallback", anchorRow, anchorCol);
                std::queue<std::pair<int,int>> q;
                std::set<std::pair<int,int>> vis;
                q.push({anchorRow, anchorCol});
                vis.insert({anchorRow, anchorCol});
                bool found = false;
                while (!q.empty() && !found) {
                    auto [r, c] = q.front(); q.pop();
                    for (auto [dr, dc] : GridNeighborOffsets(r, oddswap)) {
                        int nr = r + dr, nc = c + dc;
                        if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
                        if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;
                        if (vis.count({nr, nc})) continue;
                        vis.insert({nr, nc});
                        if (bArray.bubbleMap[nr][nc].bubbleId == -1) {
                            cy = nr; cx = nc; found = true; break;
                        }
                        q.push({nr, nc});
                    }
                }
            }
        }
    } else if (bArray.bubbleMap[cy][cx].bubbleId != -1) {
        // Ceiling hit (no anchor) — BFS from calculated position
        SDL_Log("WARNING: Calculated position [%d][%d] is occupied, BFS fallback", cy, cx);
        std::queue<std::pair<int,int>> q;
        std::set<std::pair<int,int>> vis;
        q.push({cy, cx}); vis.insert({cy, cx});
        bool found = false;
        while (!q.empty() && !found) {
            auto [r, c] = q.front(); q.pop();
            for (auto [dr, dc] : GridNeighborOffsets(r, oddswap)) {
                int nr = r + dr, nc = c + dc;
                if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
                if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;
                if (vis.count({nr, nc})) continue;
                vis.insert({nr, nc});
                if (bArray.bubbleMap[nr][nc].bubbleId == -1) {
                    cy = nr; cx = nc; found = true; break;
                }
                q.push({nr, nc});
            }
        }
    }

    *row = cy;
    *col = cx;
}

void BubbleGame::UpdateSingleBubbles(int /*id*/) {
    // Original: iter_players processes ALL players in one loop (line 2105)
    // We process ALL bubbles from ALL players together, like the original

    // Track positions already reserved by chain reactions to prevent conflicts
    std::set<std::pair<int, int>> occupiedPositions;
    for (const auto &sb : singleBubbles) {
        if (sb.chainExists && sb.chainRow != -1 && sb.chainCol != -1) {
            occupiedPositions.insert({sb.chainRow, sb.chainCol});
        }
    }

    for (size_t i = 0; i < singleBubbles.size(); i++) {
        SingleBubble &sBubble = singleBubbles[i];

        if (sBubble.shouldClear) continue;
        // Process ALL players' bubbles (original processes all in iter_players loop)
        // Don't skip based on assignedArray - process all of them!

        BubbleArray *bArray = &bubbleArrays[sBubble.assignedArray];

        // For chain reaction bubbles, update position FIRST to set chainReachedDest flag
        if (sBubble.chainExists && !sBubble.chainReachedDest) {
            sBubble.UpdatePosition();
        } else if (!sBubble.chainExists) {
            // For non-chain bubbles, update as normal
            sBubble.UpdatePosition();
        }

        // NOW check if chain reaction completed (after UpdatePosition set the flag)
        if (sBubble.chainReachedDest && sBubble.chainRow != -1 && sBubble.chainCol != -1) {
            SDL_Log("Chain reaction completed! Placing bubble %d at grid[%d][%d]",
                    sBubble.bubbleId, sBubble.chainRow, sBubble.chainCol);
            bArray->PlacePlayerBubble(sBubble.bubbleId, sBubble.chainRow, sBubble.chainCol);
            bArray->newShoot = true;
            audMixer->PlaySFX("stick");
            bArray->stickAnimActive = true; bArray->stickAnimFrame = 0; bArray->stickAnimSlowdown = 0;
            bArray->stickAnimPos = {(int)sBubble.pos.x, (int)sBubble.pos.y};
            sBubble.shouldClear = true; // Now clear it
            CheckPossibleDestroy(*bArray);
            // Chain reaction bubble landed — check if more chains are still in flight
            // before releasing malus (original line 2218: no chain reactions in flight)
            {
                int arr = bArray->playerAssigned;
                bool chainInFlight = false;
                for (const auto& sb : singleBubbles)
                    if (!sb.shouldClear && sb.assignedArray == arr && sb.chainExists && !sb.chainReachedDest) { chainInFlight = true; break; }
                if (!chainInFlight && (currentSettings.networkGame ? arr == 0 : currentSettings.playerCount >= 2))
                    ProcessMalusQueue(bubbleArrays[arr], frameCount);
            }
            CheckGameState(*bArray);
            continue;
        }

        // Handle launching bubble collisions
        if (sBubble.launching) {
            // Check if remote player has mpStickPending flag (original line 2190)
            BubbleArray* launchArray = &bubbleArrays[sBubble.assignedArray];
            if (launchArray->mpStickPending) {
                // Stick bubble at exact position from 's' message (original line 2192)
                SDL_Log("Processing mp_stick for player %d: cx=%d cy=%d col=%d",
                        sBubble.assignedArray, launchArray->stickCx, launchArray->stickCy, launchArray->stickCol);

                launchArray->PlacePlayerBubble(launchArray->stickCol, launchArray->stickCy, launchArray->stickCx);
                launchArray->newShoot = true;
                launchArray->mpStickPending = false;  // Clear flag (original line 2191)

                sBubble.shouldClear = true;
                audMixer->PlaySFX("stick");
                launchArray->stickAnimActive = true; launchArray->stickAnimFrame = 0; launchArray->stickAnimSlowdown = 0;
                launchArray->stickAnimPos = {(int)sBubble.pos.x, (int)sBubble.pos.y};
                CheckPossibleDestroy(*launchArray);
                CheckGameState(*launchArray);
                continue;  // Skip normal collision detection
            }

            // In network games, skip collision detection for remote players' bubbles
            // They will be placed via 's' message to ensure sync (handled above)
            if (currentSettings.networkGame && sBubble.assignedArray != 0) {
                // This is remote player's bubble - wait for 's' message
                continue;
            }

            // Original line 2195: ceiling check FIRST, independent of existing bubbles
            if (sBubble.pos.y <= bArray->topLimit) {
                int row, col;
                GetClosestFreeCell(sBubble, *bArray, &row, &col);
                SDL_Log("Ceiling hit: placing at row=%d col=%d pos=(%.1f,%.1f)", row, col, (float)sBubble.pos.x, (float)sBubble.pos.y);
                if (currentSettings.networkGame && sBubble.assignedArray == 0) {
                    NetworkClient* netClient = NetworkClient::Instance();
                    if (netClient->IsConnected() && netClient->GetState() == IN_GAME) {
                        char stickData[128];
                        snprintf(stickData, sizeof(stickData), "s%d:%d:%d:%s", col, row, sBubble.bubbleId,
                                 BuildNextColorsStr(*bArray).c_str());
                        netClient->SendGameData(stickData);
                    }
                }
                bArray->PlacePlayerBubble(sBubble.bubbleId, row, col);
                bArray->newShoot = true;
                audMixer->PlaySFX("stick");
                bArray->stickAnimActive = true; bArray->stickAnimFrame = 0; bArray->stickAnimSlowdown = 0;
                bArray->stickAnimPos = {(int)sBubble.pos.x, (int)sBubble.pos.y};
                sBubble.shouldClear = true;
                CheckPossibleDestroy(*bArray);
                // Original line 2218-2250: malus generation happens at stick time, only if no chain
                // reactions are still in flight. We release queued malus here so it falls AFTER the shot sticks.
                // Use bArray->playerAssigned (not sBubble.assignedArray) since CheckPossibleDestroy may
                // push_back to singleBubbles causing reallocation, making sBubble a dangling reference.
                {
                    int arr = bArray->playerAssigned;
                    bool chainInFlight = false;
                    for (const auto& sb : singleBubbles)
                        if (sb.assignedArray == arr && sb.chainExists && !sb.chainReachedDest) { chainInFlight = true; break; }
                    if (!chainInFlight && (currentSettings.networkGame ? arr == 0 : currentSettings.playerCount >= 2))
                        ProcessMalusQueue(bubbleArrays[arr], frameCount);
                }
                CheckGameState(*bArray);
                goto STOP_ITER;
            }

            for (int hitRow = 0; hitRow < (int)bArray->bubbleMap.size(); hitRow++) {
                for (int hitCol = 0; hitCol < (int)bArray->bubbleMap[hitRow].size(); hitCol++) {
                    Bubble &bubble = bArray->bubbleMap[hitRow][hitCol];
                    if (sBubble.IsCollision(&bubble)) {
                        int row, col;
                        GetClosestFreeCell(sBubble, *bArray, &row, &col, hitRow, hitCol);
                        SDL_Log("Bubble stuck at row=%d, col=%d, position=(%.1f, %.1f)",
                                row, col, (float)sBubble.pos.x, (float)sBubble.pos.y);

                        // In network game, send stick position to opponent
                        if (currentSettings.networkGame && sBubble.assignedArray == 0) {
                            NetworkClient* netClient = NetworkClient::Instance();
                            if (netClient->IsConnected() && netClient->GetState() == IN_GAME) {
                                // Send: s{cx}:{cy}:{bubbleColor}:{nextcolors...}
                                // Perl format (frozen-bubble line 2199): "s$cx:$cy:$col:@{nextcolors}"
                                char stickData[256];
                                snprintf(stickData, sizeof(stickData), "s%d:%d:%d:%s",
                                    col, row, sBubble.bubbleId, BuildNextColorsStr(*bArray).c_str());
                                SDL_Log("Sending stick: col=%d row=%d color=%d nextColors=%s",
                                        col, row, sBubble.bubbleId, BuildNextColorsStr(*bArray).c_str());
                                netClient->SendGameData(stickData);
                            }
                        }

                        bArray->PlacePlayerBubble(sBubble.bubbleId, row, col);
                        bArray->newShoot = true;
                        audMixer->PlaySFX("stick");
                        bArray->stickAnimActive = true; bArray->stickAnimFrame = 0; bArray->stickAnimSlowdown = 0;
                        bArray->stickAnimPos = {(int)sBubble.pos.x, (int)sBubble.pos.y};
                        sBubble.shouldClear = true;
                        CheckPossibleDestroy(*bArray);
                        // Release queued malus at stick time (original line 2218-2250)
                        // Use bArray->playerAssigned (not sBubble.assignedArray) — see note above.
                        {
                            int arr = bArray->playerAssigned;
                            bool chainInFlight = false;
                            for (const auto& sb : singleBubbles)
                                if (sb.assignedArray == arr && sb.chainExists && !sb.chainReachedDest) { chainInFlight = true; break; }
                            if (!chainInFlight && (currentSettings.networkGame ? arr == 0 : currentSettings.playerCount >= 2))
                                ProcessMalusQueue(bubbleArrays[arr], frameCount);
                        }
                        CheckGameState(*bArray);
                        goto STOP_ITER;
                    }
                };
            }
        }
        // Handle falling bubbles without chain reactions
        // Chain reaction targets are now assigned ONCE in AssignChainReactions() (called from CheckPossibleDestroy)
        // This matches original frozen-bubble line 814-865 which processes @falling bubbles once after creating them
        // Falling bubbles just fall normally here - no per-frame chain reaction checks
        else if (sBubble.falling && !sBubble.chainExists && !sBubble.exploding) {
            // Just let the bubble continue falling
            // Chain reactions are handled by AssignChainReactions() when bubbles are first created
        }

        STOP_ITER:
        continue;
    }
    singleBubbles.erase(std::remove_if(singleBubbles.begin(), singleBubbles.end(), [](const SingleBubble &s){ return s.shouldClear; }), singleBubbles.end());

    // Update malus bubbles (they rise upward to stick position)
    // Original port used 2.0 px/frame; increased 25% to 2.5 for better feel.
    // Multiplied by deltaScale so speed is frame-rate-independent.
    const float MALUS_SPEED = 2.5f * FrozenBubble::Instance()->deltaScale;
    for (auto &malus : malusBubbles) {
        if (malus.shouldClear) continue;

        // Get the BubbleArray for this malus bubble
        BubbleArray* malusArray = &bubbleArrays[malus.assignedArray];

        // Move upward toward stick position
        // Mini players use half bubble size
        bool isMini = (currentSettings.playerCount >= 3 && malusArray->playerAssigned >= 1);
        int bubbleSize = isMini ? 16 : 32;
        int rowSize = bubbleSize * 7 / 8;  // 14 for mini, 28 for full
        float targetY = (rowSize * malus.stickY) + malusArray->bubbleOffset.y;

        if (malus.posY > targetY) {
            malus.posY -= MALUS_SPEED;
            malus.pos.y = (int)malus.posY;
        } else {
            // Reached stick position - place bubble in grid
            malus.shouldStick = true;
        }

        // Stick the malus bubble to the grid
        if (malus.shouldStick) {
            SDL_Log("Malus bubble sticking at cx=%d stickY=%d", malus.cx, malus.stickY);
            malusArray->PlacePlayerBubble(malus.bubbleId, malus.stickY, malus.cx);
            // Malus landing must NOT be a match activator.
            // Original uses real_stick_bubble() (no match check) for malus sticking.
            // Reset playerBubble so CheckPossibleDestroy below doesn't treat it as an
            // activator — otherwise malus landing next to same-colored bubbles would
            // generate and send malus back to opponents.
            malusArray->bubbleMap[malus.stickY][malus.cx].playerBubble = false;
            malusArray->newShoot = true;

            // Send 'M' message to sync sticking (original line 1456-1466)
            if (currentSettings.networkGame && malusArray->playerAssigned == 0) {
                NetworkClient* netClient = NetworkClient::Instance();
                if (netClient && netClient->IsConnected()) {
                    char MMsg[64];
                    snprintf(MMsg, sizeof(MMsg), "M%d:%d", malus.cx, malus.stickY);
                    SDL_Log("Sending malus stick: cx=%d stickY=%d", malus.cx, malus.stickY);
                    netClient->SendGameData(MMsg);
                }
            }

            malus.shouldClear = true;
            CheckPossibleDestroy(*malusArray);
            // Original uses real_stick_bubble() for malus sticking (line 2505/2514),
            // which does NOT increment the newroot counter. Do NOT call CheckGameState here.
        }
    }

    // Clean up malus bubbles
    malusBubbles.erase(std::remove_if(malusBubbles.begin(), malusBubbles.end(),
                                      [](const MalusBubble &m){ return m.shouldClear; }),
                       malusBubbles.end());
}

void GetGroupedCount(BubbleArray &bArray, std::vector<Bubble*> *bubbleCount, int row, int col, int *curStack) {
    int targetId = (*bubbleCount)[0]->bubbleId;
    int oddswap = (bArray.bubbleMap[0].size() == 8) ? 0 : 1;

    for (auto [dr, dc] : GridNeighborOffsets(row, oddswap)) {
        int nr = row + dr;
        int nc = col + dc;
        if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
        if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;
        Bubble *nb = &bArray.bubbleMap[nr][nc];
        if (nb->bubbleId != targetId) continue;
        if (std::count(bubbleCount->begin(), bubbleCount->end(), nb) > 0) continue;

        bubbleCount->push_back(nb);
        (*curStack)++;
        GetGroupedCount(bArray, bubbleCount, nr, nc, curStack);
    }
}

void BubbleGame::AssignChainReactions(BubbleArray &bArray) {
    // Assign chain reaction targets to falling bubbles ONCE when they're created
    // Original: frozen-bubble line 814-865 in stick_bubble function
    // This creates the cascading chain reaction effect when chain bubbles land and trigger more groups

    SDL_Log("AssignChainReactions: Checking %zu falling bubbles for chain targets", singleBubbles.size());

    // Track positions already reserved by chain reactions to prevent conflicts
    std::set<std::pair<int, int>> occupiedPositions;

    // Track bubbles that are part of groups targeted by earlier chain reactions
    // Original line 829: prevents chaining to bubbles in chained_bubbles groups
    std::set<std::pair<int, int>> chainedGroupBubbles;

    // Mark all occupied positions in the grid
    for (size_t row = 0; row < bArray.bubbleMap.size(); row++) {
        for (size_t col = 0; col < bArray.bubbleMap[row].size(); col++) {
            if (bArray.bubbleMap[row][col].bubbleId != -1) {
                occupiedPositions.insert({row, col});
            }
        }
    }

    // Also mark positions already reserved by other chain reactions
    for (const auto &sb : singleBubbles) {
        if (sb.chainExists && sb.chainRow != -1 && sb.chainCol != -1) {
            occupiedPositions.insert({sb.chainRow, sb.chainCol});
        }
    }

    // Calculate distance_to_root for all bubbles (Original: frozen-bubble lines 801-810)
    // This is used to prioritize chain reactions for groups closer to the root
    std::map<std::pair<int, int>, int> distanceToRoot;

    // Initialize all bubbles to distance 0
    for (size_t row = 0; row < bArray.bubbleMap.size(); row++) {
        for (size_t col = 0; col < bArray.bubbleMap[row].size(); col++) {
            if (bArray.bubbleMap[row][col].bubbleId != -1) {
                distanceToRoot[{row, col}] = 0;
            }
        }
    }

    // BFS from root bubbles (top row, row 0) to calculate distances
    std::queue<std::pair<int, int>> queue;
    std::set<std::pair<int, int>> visited;

    // Start with bubbles in top row (root_bubbles in original)
    for (size_t col = 0; col < bArray.bubbleMap[0].size(); col++) {
        if (bArray.bubbleMap[0][col].bubbleId != -1) {
            queue.push({0, col});
            visited.insert({0, col});
            distanceToRoot[{0, col}] = 1;  // Root bubbles have distance 1
        }
    }

    // BFS to assign distances
    while (!queue.empty()) {
        auto [row, col] = queue.front();
        queue.pop();
        int currentDistance = distanceToRoot[{row, col}];

        // Get neighbor offsets — use oddswap to handle both r=0 and r=1 grid orientations
        int oddswap = (bArray.bubbleMap[0].size() == 8) ? 0 : 1;
        for (auto [dr, dc] : GridNeighborOffsets(row, oddswap)) {
            int nr = row + dr;
            int nc = col + dc;
            if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
            if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;
            if (visited.count({nr, nc}) > 0) continue;
            if (bArray.bubbleMap[nr][nc].bubbleId == -1) continue;

            visited.insert({nr, nc});
            distanceToRoot[{nr, nc}] = currentDistance + 1;
            queue.push({nr, nc});
        }
    }

    // Collect all potential chain targets (grouped bubbles) with their distances
    // Original line 818-821: finds grouped_bubbles
    struct ChainTarget {
        int row, col;
        int bubbleId;
        int distance;
    };
    std::vector<ChainTarget> potentialTargets;

    for (size_t row = 0; row < bArray.bubbleMap.size(); row++) {
        for (size_t col = 0; col < bArray.bubbleMap[row].size(); col++) {
            Bubble &gridBubble = bArray.bubbleMap[row][col];
            if (gridBubble.bubbleId == -1) continue;

            // Check if this bubble has a neighbor of the same color (part of a group)
            bool hasNeighborSameColor = false;
            int oddswap = (bArray.bubbleMap[0].size() == 8) ? 0 : 1;
            for (auto [dr, dc] : GridNeighborOffsets(row, oddswap)) {
                int nr = row + dr;
                int nc = col + dc;
                if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
                if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;
                if (bArray.bubbleMap[nr][nc].bubbleId == gridBubble.bubbleId) {
                    hasNeighborSameColor = true;
                    break;
                }
            }

            if (hasNeighborSameColor) {
                int dist = distanceToRoot.count({row, col}) > 0 ? distanceToRoot[{row, col}] : 999;
                potentialTargets.push_back({(int)row, (int)col, gridBubble.bubbleId, dist});
            }
        }
    }

    // Sort by distance_to_root (Original line 828)
    // Groups closer to root are processed first
    std::sort(potentialTargets.begin(), potentialTargets.end(),
              [](const ChainTarget &a, const ChainTarget &b) {
                  return a.distance < b.distance;
              });

    SDL_Log("AssignChainReactions: Found %zu potential targets, sorted by distance", potentialTargets.size());

    // Process potential targets in distance order (Original line 828: sort by distance_to_root)
    // This ensures groups closer to the root get priority for chain reactions
    for (const auto &target : potentialTargets) {
        int row = target.row;
        int col = target.col;
        int bubbleId = target.bubbleId;

        // Skip if this bubble is part of a group already targeted by another chain reaction
        // Original line 832-833: prevents chaining to same group twice
        if (chainedGroupBubbles.count({row, col}) > 0) {
            continue;
        }

        SDL_Log("  Examining target at [%d][%d] color=%d distance=%d", row, col, bubbleId, target.distance);

        // Find free adjacent positions for this target
        // Original line 830: next_positions($pos, $player)
        int oddswap2 = (bArray.bubbleMap[0].size() == 8) ? 0 : 1;

        // Find first free position adjacent to this target
        int freeRow = -1, freeCol = -1;
        for (auto [dr, dc] : GridNeighborOffsets(row, oddswap2)) {
            int nr = row + dr;
            int nc = col + dc;
            if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
            if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;

            // Original line 834: check if position is free
            if (bArray.bubbleMap[nr][nc].bubbleId == -1 &&
                occupiedPositions.count({nr, nc}) == 0) {
                freeRow = nr;
                freeCol = nc;
                break;  // Use first free position found
            }
        }

        // If no free position, skip this target
        if (freeRow == -1) {
            continue;
        }

        // Find a matching falling bubble that hasn't been assigned yet
        // Original line 836-861: foreach my $falling (@falling)
        for (auto &sBubble : singleBubbles) {
            // Skip if: not falling, already chained, wrong player, wrong color, launching, or exploding
            // CRITICAL: Must ONLY process bubbles that are genuinely falling from disconnection
            if (!sBubble.falling || sBubble.chainExists ||
                sBubble.assignedArray != bArray.playerAssigned ||
                sBubble.bubbleId != bubbleId ||
                sBubble.launching || sBubble.exploding || sBubble.shouldClear) {
                continue;
            }

            // Found a match! Assign chain reaction target
            // Original line 839-842: assigns chaindestx, chaindesty
            SDL_Log("    Chain target found! Bubble color=%d will rise to [%d][%d]",
                    sBubble.bubbleId, freeRow, freeCol);

            sBubble.chainExists = true;
            sBubble.chainRow = freeRow;
            sBubble.chainCol = freeCol;
            // Compute pixel position from grid coords using actual row offset
            // (matches how bubble pos.x is set in LoadLevel/RandomLevel)
            int chainRowOffset = (bArray.bubbleMap[freeRow].size() == 7) ? 16 : 0;
            sBubble.chainDest = {
                bArray.bubbleOffset.x + freeCol * 32 + chainRowOffset,
                bArray.bubbleOffset.y + freeRow * 28
            };
            occupiedPositions.insert({freeRow, freeCol}); // Reserve this position

            // Mark all bubbles in this group as "chained" to prevent other chains from targeting them
            // Original line 846-858: calculates chained_bubbles group
            // When this chain lands and explodes the group, these bubbles won't exist anymore
            std::set<std::pair<int, int>> visited;
            std::queue<std::pair<int, int>> queue;
            queue.push({row, col});
            visited.insert({row, col});

            while (!queue.empty()) {
                auto [r, c] = queue.front();
                queue.pop();
                chainedGroupBubbles.insert({r, c});

                // Find all neighbors of same color
                std::vector<std::pair<int, int>> neighborOffsets;
                if (r % 2 == 0) {
                    neighborOffsets = {{-1,-1}, {-1,0}, {0,-1}, {0,1}, {1,-1}, {1,0}};
                } else {
                    neighborOffsets = {{-1,0}, {-1,1}, {0,-1}, {0,1}, {1,0}, {1,1}};
                }

                for (auto [dr, dc] : neighborOffsets) {
                    int nr = r + dr;
                    int nc = c + dc;
                    if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
                    if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;
                    if (visited.count({nr, nc}) > 0) continue;
                    if (bArray.bubbleMap[nr][nc].bubbleId == sBubble.bubbleId) {
                        visited.insert({nr, nc});
                        queue.push({nr, nc});
                    }
                }
            }

            SDL_Log("    Marked %zu bubbles in chain target group as unavailable", chainedGroupBubbles.size());

            // Found a chain target, move to next target position
            // Original line 859: last; (exits inner foreach loop)
            break;
        }
    }
}

void BubbleGame::CheckPossibleDestroy(BubbleArray &bArray){
    int totalDestroyed = 0;  // Track destroyed bubbles for malus calculation

    for (size_t i = 0; i < bArray.bubbleMap.size(); i++) {
        for (size_t j = 0; j < bArray.bubbleMap[i].size(); j++) {
            if (bArray.bubbleMap[i][j].playerBubble == true) { // activator
                std::vector<Bubble*> bubbleCount;
                int groupedCount = 0;
                bubbleCount.push_back(&bArray.bubbleMap[i][j]);
                GetGroupedCount(bArray, &bubbleCount, i, j, &groupedCount);
                if (groupedCount >= 2) {
                    SDL_Log("Match found: %d bubbles (chainReaction=%d)", groupedCount + 1, currentSettings.chainReaction);
                    audMixer->PlaySFX("destroy_group");

                    // Calculate score: 10 points per bubble (groupedCount+1 = total including activator), with chain multiplier
                    int baseScore = (groupedCount + 1) * 10;
                    int multiplier = 1 + bArray.chainLevel;
                    bArray.score += baseScore * multiplier;
                    bArray.chainLevel++; // Increment chain level for subsequent groups

                    // Show combo text if chain level > 0
                    if (bArray.chainLevel > 1) {
                        comboDisplayTimer = 60; // Display for 60 frames (1 second at 60fps)
                        char comboStr[32];
                        snprintf(comboStr, sizeof(comboStr), "COMBO x%d!", bArray.chainLevel);
                        comboText.UpdateText(renderer, comboStr, 0);
                        comboText.UpdatePosition({SCREEN_CENTER_X - (comboText.Coords()->w / 2), 200});
                    }

                    totalDestroyed += groupedCount;  // Excludes activator, matching original @will_destroy

                    for (Bubble *bubble : bubbleCount) {
                        float startX = (float)bubble->pos.x;
                        float startY = (float)bubble->pos.y;
                        SingleBubble bubs = {bArray.playerAssigned, bArray.curLaunch, startX, startY, startX, startY, bubble->pos, {}, bArray.shooterSprite.angle, false, false, bArray.leftLimit, bArray.rightLimit, bArray.topLimit, lowGfx};
                        bubs.CopyBubbleProperties(bubble);
                        bubs.GenerateFreeFall(true);
                        singleBubbles.push_back(bubs);
                        bubble->bubbleId = -1;
                        bubble->playerBubble = false;
                    }
                } else {
                    // No match — reset activator flag so it isn't re-triggered on future shots
                    bArray.bubbleMap[i][j].playerBubble = false;
                }
                bubbleCount.clear();
                continue;
            }
        }
    }

    // Original Perl: air bubble check and malus only run inside the match block (else of @will_destroy <= 1).
    // If no match occurred (totalDestroyed == 0), skip both — mirroring the original behavior.
    int fallingCount = 0;
    if (totalDestroyed > 0) {
        fallingCount = CheckAirBubbles(bArray);

        // Assign chain reaction targets to newly falling bubbles (original line 814-865)
        // This happens ONCE per stick event, not every frame
        // In network games, only run chain reactions for the LOCAL player (array 0).
        // Remote players handle chain reactions on their own clients; running AssignChainReactions
        // on mini-player arrays (1+) would use wrong 32px column spacing for the 16px mini grid,
        // corrupting the singleBubbles list and potentially interfering with local chain reactions.
        bool shouldRunChainReactions = currentSettings.chainReaction && fallingCount > 0 &&
            (!currentSettings.networkGame || bArray.playerAssigned == 0);
        if (shouldRunChainReactions) {
            AssignChainReactions(bArray);
        }
    }

    // Calculate malus: destroyed + falling - 2 (original formula at line 958)
    int malusValue = totalDestroyed + fallingCount - 2;
    if (malusValue > 0) {
        if (currentSettings.mpTraining && bArray.playerAssigned == 0) {
            // mp_train: malus converted to score (original malus_change at line 1185)
            mpTrainScore += malusValue;
        } else if (currentSettings.networkGame && bArray.playerAssigned == 0) {
            // Only local player (array 0) sends malus
            SDL_Log("Awarding %d malus to opponent (%d destroyed + %d falling - 2)",
                    malusValue, totalDestroyed, fallingCount);
            SendMalusToOpponent(malusValue);
        } else if (!currentSettings.networkGame && currentSettings.playerCount >= 2) {
            // Local multiplayer: distribute malus directly to all other living players' queues
            int attackerIdx = bArray.playerAssigned;
            int livingOpponents = 0;
            for (int i = 0; i < currentSettings.playerCount; i++)
                if (i != attackerIdx && bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE)
                    livingOpponents++;
            if (livingOpponents > 0) {
                int malusEach = (malusValue + livingOpponents - 1) / livingOpponents;
                for (int i = 0; i < currentSettings.playerCount; i++) {
                    if (i != attackerIdx && bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE) {
                        for (int m = 0; m < malusEach; m++)
                            bubbleArrays[i].malusQueue.push_back(frameCount);
                        SDL_Log("Local malus: %d bubbles queued for player %d", malusEach, i);
                    }
                }
            }
        }
    }
}

bool isAttached(BubbleArray &bArray, int row, int col) {
    bool biggerThan = (bArray.bubbleMap[row].size() > bArray.bubbleMap[row - 1].size()) ? true : false;
    if(biggerThan) {
        if (col > 0) { if (bArray.bubbleMap[row-1][col-1].bubbleId != -1) return true; }
        if ((size_t)col < bArray.bubbleMap[row].size() - 1) { if (bArray.bubbleMap[row-1][col].bubbleId != -1) return true; }
    }
    else {
        if (bArray.bubbleMap[row-1][col].bubbleId != -1) return true;
        if (bArray.bubbleMap[row-1][col+1].bubbleId != -1) return true;
    }
    return false;
}

void CheckIfAttached(BubbleArray &bArray, int row, int col, int fc, bool *attached) {
    *attached = isAttached(bArray, row, col);
    if (*attached != true && bArray.bubbleMap[row][col].bubbleId != -1) { //if atp attached is still false, try the others!
        if (col > 0) {
            if (col-1 != fc && bArray.bubbleMap[row][col - 1].bubbleId != -1) CheckIfAttached(bArray, row, col - 1, col, attached);
        }
        if (*attached != true && (size_t)col < bArray.bubbleMap[row].size() - 1) {
            if (col+1 != fc && bArray.bubbleMap[row][col + 1].bubbleId != -1) CheckIfAttached(bArray, row, col + 1, col, attached);
        }
    }
}

void DoFalling(std::vector<SDL_Point> &map, std::vector<SingleBubble> &bubbles, bool &lowGfx) {
    if (map.size() < 1 || bubbles.size() < 1) return;
    SDL_Log("DoFalling called: %d bubbles to fall, lowGfx=%d", (int)bubbles.size(), lowGfx);
    int maxy = map[map.size() - 1].y;
    int shiftSameLine = 0, line = maxy;
    for (size_t i = map.size(); i > 0; i--) { //original FB does backwards sorting for the formula
        int y = map[i - 1].y;
        shiftSameLine = line != y ? 0 : shiftSameLine;
        line = y;
        // Use falling=true (not explode) so AssignChainReactions can find these bubbles.
        // Only the directly-matched group in CheckPossibleDestroy gets exploding=true.
        // Original: disconnected bubbles go into @falling_bubble (falling), not @exploding_bubble.
        bubbles[i - 1].GenerateFreeFall(false, (maxy - y) * 5 + shiftSameLine);
        singleBubbles.push_back(bubbles[i - 1]);
        shiftSameLine++;
    }
    map.clear();
    bubbles.clear();
}

int BubbleGame::CheckAirBubbles(BubbleArray &bArray) {
    // Original algorithm: frozen-bubble lines 800-812
    // Use BFS from row 0 (ceiling) to mark all connected bubbles
    // Any bubble NOT marked is falling

    std::vector<SDL_Point> fallingLocs;
    std::vector<SingleBubble> singlesFalling;
    int fallingCount = 0;

    // Mark all bubbles as unvisited (distance = 0 means not connected)
    std::set<std::pair<int, int>> connected;
    std::queue<std::pair<int, int>> queue;

    // Start with root bubbles (row 0) - these are always attached
    for (size_t col = 0; col < bArray.bubbleMap[0].size(); col++) {
        if (bArray.bubbleMap[0][col].bubbleId != -1) {
            queue.push({0, col});
            connected.insert({0, col});
        }
    }

    // BFS to mark all connected bubbles
    while (!queue.empty()) {
        auto [row, col] = queue.front();
        queue.pop();

        // Get neighbor offsets — use oddswap to handle both r=0 and r=1 grid orientations
        int oddswap = (bArray.bubbleMap[0].size() == 8) ? 0 : 1;
        for (auto [dr, dc] : GridNeighborOffsets(row, oddswap)) {
            int nr = row + dr;
            int nc = col + dc;

            // Check bounds
            if (nr < 0 || nr >= (int)bArray.bubbleMap.size()) continue;
            if (nc < 0 || nc >= (int)bArray.bubbleMap[nr].size()) continue;

            // Skip if already visited or empty
            if (connected.count({nr, nc}) > 0) continue;
            if (bArray.bubbleMap[nr][nc].bubbleId == -1) continue;

            // Mark as connected and add to queue
            connected.insert({nr, nc});
            queue.push({nr, nc});
        }
    }

    // Now find all bubbles that are NOT connected (these should fall)
    for (size_t i = 1; i < bArray.bubbleMap.size(); i++) {  // Start from row 1 (row 0 always attached)
        for (size_t j = 0; j < bArray.bubbleMap[i].size(); j++) {
            if (bArray.bubbleMap[i][j].bubbleId == -1) continue;

            // If not in connected set, it should fall
            if (connected.count({i, j}) == 0) {
                if (bArray.bubbleMap[i][j].playerBubble)
                    SDL_Log("AIR_BUBBLE: Newly placed playerBubble at row=%zu col=%zu removed (not connected to ceiling)", i, j);
                float startX = (float)bArray.bubbleMap[i][j].pos.x;
                float startY = (float)bArray.bubbleMap[i][j].pos.y;
                SingleBubble bubbly = {bArray.playerAssigned, bArray.curLaunch, startX, startY, startX, startY,
                                       bArray.bubbleMap[i][j].pos, {}, bArray.shooterSprite.angle, false, false,
                                       bArray.leftLimit, bArray.rightLimit, bArray.topLimit, lowGfx};
                bubbly.CopyBubbleProperties(&bArray.bubbleMap[i][j]);
                singlesFalling.push_back(bubbly);
                fallingLocs.push_back({(int)j, (int)i});
                bArray.bubbleMap[i][j].bubbleId = -1;
                bArray.bubbleMap[i][j].playerBubble = false;
                fallingCount++;
            }
        }
    }

    // Award points for falling bubbles: 20 points per bubble, with chain multiplier
    if (fallingCount > 0) {
        int baseScore = fallingCount * 20;
        int multiplier = 1 + bArray.chainLevel;
        bArray.score += baseScore * multiplier;
    }

    if (singlesFalling.size() > 0) {
        SDL_Log("DoFalling: %d bubbles falling after match clear (chainReaction=%d)",
                (int)singlesFalling.size(), currentSettings.chainReaction);
    }
    DoFalling(fallingLocs, singlesFalling, lowGfx);

    return fallingCount;  // Return count for malus calculation
}

void BubbleGame::DoFrozenAnimation(BubbleArray &bArray, int &waitTime){
    if (waitTime <= 0) {
        waitTime = FROZEN_FRAMEWAIT;
        for (int i = (int)bArray.bubbleMap.size() - 1; i >= 0; i--) {
            for (int j = (int)bArray.bubbleMap[i].size() - 1; j >= 0; j--) {
                if (bArray.bubbleMap[i][j].frozen != true && bArray.bubbleMap[i][j].bubbleId != -1) {
                    bArray.bubbleMap[i][j].frozen = true;
                    return;
                }
                else continue;
            }
        }
        if (currentSettings.playerCount < 2) {
            audMixer->PlaySFX("noh");
            gameLost = true;
        }
        else bArray.mpDone = true;
    }
    else waitTime--;
}

void BubbleGame::DoWinAnimation(BubbleArray &bArray, int &waitTime){
    if (waitTime <= 0) {
        waitTime = EXPLODE_FRAMEWAIT;
        for (int i = (int)bArray.bubbleMap.size() - 1; i >= 0; i--) {
            for (int j = (int)bArray.bubbleMap[i].size() - 1; j >= 0; j--) {
                if (bArray.bubbleMap[i][j].bubbleId != -1) {
                    float startX = (float)bArray.bubbleMap[i][j].pos.x;
                    float startY = (float)bArray.bubbleMap[i][j].pos.y;
                    SingleBubble bubbly = {bArray.playerAssigned, bArray.curLaunch, startX, startY, startX, startY, bArray.bubbleMap[i][j].pos, {}, bArray.shooterSprite.angle, false, false, bArray.leftLimit, bArray.rightLimit, bArray.topLimit, lowGfx};
                    bubbly.CopyBubbleProperties(&bArray.bubbleMap[i][j]);
                    bubbly.GenerateFreeFall(true, 0);
                    singleBubbles.push_back(bubbly);
                    bArray.bubbleMap[i][j].bubbleId = -1;
                    bArray.bubbleMap[i][j].playerBubble = false;
                    return;
                }
                else continue;
            }
        }
        bArray.mpDone = true;
    }
    else waitTime--;
}

void BubbleGame::DoPrelightAnimation(BubbleArray &bArray, int &waitTime){
    if (waitTime <= 0) {
        if(bArray.framePrelight <= 0) {
            for (size_t i = 0; i < bArray.bubbleMap.size(); i++) {
                if (lowGfx) continue;
                for (size_t j = 0; j < bArray.bubbleMap[i].size(); j++) {
                    if (j == (size_t)bArray.alertColumn) {
                        bArray.bubbleMap[i][j].shining = true;
                    }
                    else {
                        bArray.bubbleMap[i][j].shining = false;
                        continue;
                    }
                }
            }
            bArray.alertColumn++;
            if (bArray.alertColumn > 8) {
                waitTime = bArray.waitPrelight;
                bArray.alertColumn = 0;
            }
            bArray.framePrelight = PRELIGHT_FRAMEWAIT;
        }
        else bArray.framePrelight--;
    }
    else waitTime--;
}

void ResetPrelight(BubbleArray &bArray) {
    for (size_t i = 0; i < bArray.bubbleMap.size(); i++) {
        for (size_t j = 0; j < bArray.bubbleMap[i].size(); j++) {
            bArray.bubbleMap[i][j].shining = false;
        }
    }
}

void BubbleGame::ExpandNewLane(BubbleArray &bArray) {
    int newSize = bArray.bubbleMap[0].size() == 7 ? 8 : 7;

    // Mini players use half bubble size
    bool isMini = (currentSettings.playerCount >= 3 && bArray.playerAssigned >= 1);
    int bubbleSize = isMini ? 16 : 32;
    int rowSize = bubbleSize * 7 / 8;  // 14 for mini, 28 for full
    int shiftY = rowSize;  // Shift amount equals ROW_SIZE

    for (std::size_t i = bArray.bubbleMap.size() - 1; i > 0; --i) {
        bArray.bubbleMap[i] = bArray.bubbleMap[i - 1];
        for (Bubble &bubble : bArray.bubbleMap[i]) {
            bubble.pos.y += shiftY;
        }
    }
    bArray.bubbleMap[0].clear();

    SDL_Point &offset = bArray.bubbleOffset;
    int smallerSep = newSize % 2 == 0 ? 0 : bubbleSize / 2;
    for (int j = 0; j < newSize; j++)
    {
        // Use nextColors queue for Perl-compatible color selection (original: ExpandNewLane uses nextcolors, line 951)
        int colorId;
        if (!bArray.nextColors.empty()) {
            colorId = bArray.nextColors.front();
            bArray.nextColors.erase(bArray.nextColors.begin());
            bArray.nextColors.push_back(ranrange(0, bArray.numColors - 1));  // Replenish queue
        } else {
            colorId = ranrange(0, bArray.numColors - 1);
        }
        bArray.bubbleMap[0].push_back(Bubble{colorId, {(smallerSep + bubbleSize * ((int)j)) + offset.x, offset.y}});
    }
}

void BubbleGame::SendMalusToOpponent(int malusCount) {
    if (!currentSettings.networkGame) return;

    NetworkClient* netClient = NetworkClient::Instance();
    if (!netClient || !netClient->IsConnected() || netClient->GetState() != IN_GAME) {
        return;
    }

    // Original logic at frozen-bubble line 1204-1227
    // Two modes:
    // 1. Split malus to ALL living opponents (default)
    // 2. Send ALL malus to ONE specific target (single player targetting mode)

    // Count living opponents (exclude local player at array 0)
    std::vector<int> livingOpponents;
    for (int i = 1; i < currentSettings.playerCount; i++) {
        if (bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE) {
            livingOpponents.push_back(i);
        }
    }

    if (livingOpponents.empty()) {
        SDL_Log("No living opponents to send malus to");
        return;
    }

    // Single player targeting mode: send all malus to one opponent (original lines 1217-1227)
    // When enabled, if no manual target is selected (sendMalusToOne == -1), auto-pick the
    // first living opponent so the setting actually focuses fire instead of splitting.
    if (currentSettings.singlePlayerTargetting) {
        if (sendMalusToOne == -1 ||
            sendMalusToOne >= currentSettings.playerCount ||
            bubbleArrays[sendMalusToOne].playerState != BubbleArray::PlayerState::ALIVE) {
            sendMalusToOne = -1;
            for (int idx : livingOpponents) {
                sendMalusToOne = idx;
                break;
            }
        }
    }
    if (currentSettings.singlePlayerTargetting && sendMalusToOne != -1) {
        if (sendMalusToOne < currentSettings.playerCount &&
            bubbleArrays[sendMalusToOne].playerState == BubbleArray::PlayerState::ALIVE) {
            std::string targetNick = bubbleArrays[sendMalusToOne].playerNickname;

            // Fallback to lobbyPlayerId if nickname is empty
            if (targetNick.empty()) {
                int lobbyId = bubbleArrays[sendMalusToOne].lobbyPlayerId;
                if (lobbyId >= 0 && netClient) {
                    targetNick = netClient->GetPlayerNickname(lobbyId);
                    SDL_Log("Using fallback nickname '%s' from lobbyPlayerId %d for target array %d",
                           targetNick.c_str(), lobbyId, sendMalusToOne);
                }
            }

            // Final fallback: generate a nickname
            if (targetNick.empty()) {
                int lobbyId = bubbleArrays[sendMalusToOne].lobbyPlayerId;
                char fallbackNick[32];
                if (lobbyId >= 0) {
                    snprintf(fallbackNick, sizeof(fallbackNick), "player%d", lobbyId);
                } else {
                    snprintf(fallbackNick, sizeof(fallbackNick), "player%d", sendMalusToOne);
                }
                targetNick = fallbackNick;
            }

            if (!targetNick.empty()) {
                char malusMsg[128];
                snprintf(malusMsg, sizeof(malusMsg), "g%s:%d", targetNick.c_str(), malusCount);
                SDL_Log("Targeting: Sending all %d malus to %s (array %d)",
                        malusCount, targetNick.c_str(), sendMalusToOne);
                netClient->SendGameData(malusMsg);
                return;
            }
        }
        // Target died/invalid - fall through to split mode
        sendMalusToOne = -1;
    }

    // Divide malus equally among living opponents (original line 1207)
    // Use ceiling division: int($numb/(@living-1) + 0.99)
    int malusPerOpponent = (malusCount + livingOpponents.size() - 1) / livingOpponents.size();

    SDL_Log("Sending malus: %d total split to %zu opponents = %d per opponent",
            malusCount, livingOpponents.size(), malusPerOpponent);

    // Send 'g' message to each living opponent (original line 1208-1215)
    // Format: g{opponentNick}:{count}
    // Use lobbyPlayerId for more reliable targeting when nicknames might be empty/duplicate
    for (int opponentIdx : livingOpponents) {
        std::string targetNick = bubbleArrays[opponentIdx].playerNickname;

        // Fallback to lobbyPlayerId if nickname is empty
        if (targetNick.empty()) {
            int lobbyId = bubbleArrays[opponentIdx].lobbyPlayerId;
            if (lobbyId >= 0 && netClient) {
                targetNick = netClient->GetPlayerNickname(lobbyId);
                SDL_Log("Using fallback nickname '%s' from lobbyPlayerId %d for array %d",
                       targetNick.c_str(), lobbyId, opponentIdx);
            }
        }

        // Final fallback: use "player{lobbyId}" format
        if (targetNick.empty()) {
            int lobbyId = bubbleArrays[opponentIdx].lobbyPlayerId;
            if (lobbyId >= 0) {
                char fallbackNick[32];
                snprintf(fallbackNick, sizeof(fallbackNick), "player%d", lobbyId);
                targetNick = fallbackNick;
            } else {
                char fallbackNick[32];
                snprintf(fallbackNick, sizeof(fallbackNick), "player%d", opponentIdx);
                targetNick = fallbackNick;
            }
            SDL_Log("Using generated nickname '%s' for array %d (no lobbyPlayerId)",
                   targetNick.c_str(), opponentIdx);
        }

        char malusMsg[128];
        snprintf(malusMsg, sizeof(malusMsg), "g%s:%d", targetNick.c_str(), malusPerOpponent);
        SDL_Log("  -> Sending %d malus to %s (array %d, lobbyId=%d)",
                malusPerOpponent, targetNick.c_str(), opponentIdx,
                bubbleArrays[opponentIdx].lobbyPlayerId);
        netClient->SendGameData(malusMsg);
    }
}

// Set single player malus targeting (original: sub set_sendmalustoone at line 1330)
// opponentIdx: 1-4 = target that opponent's bubbleArrays slot, -1 = clear (split to all)
void BubbleGame::SetSendMalusToOne(int opponentIdx) {
    sendMalusToOne = opponentIdx;
    playerTargeting[0] = opponentIdx;

    NetworkClient* netClient = NetworkClient::Instance();
    if (!netClient || !netClient->IsConnected()) return;

    if (opponentIdx == -1) {
        // Clear targeting - broadcast to all so they remove the "attacking me" indicator
        netClient->SendGameData("A");
        SDL_Log("Cleared malus target (sending to all)");
    } else if (opponentIdx < currentSettings.playerCount &&
               bubbleArrays[opponentIdx].playerState == BubbleArray::PlayerState::ALIVE) {
        const std::string& nick = bubbleArrays[opponentIdx].playerNickname;
        if (!nick.empty()) {
            char aMsg[128];
            snprintf(aMsg, sizeof(aMsg), "A%s", nick.c_str());
            netClient->SendGameData(aMsg);
            SDL_Log("Set malus target to %s (array %d)", nick.c_str(), opponentIdx);
        }
    }
}

void BubbleGame::ProcessMalusQueue(BubbleArray &bArray, int currentFrame) {
    if (!currentSettings.networkGame && !currentSettings.mpTraining && currentSettings.playerCount < 2) return;
    if (bArray.malusQueue.empty()) return;

    const int MALUS_FREEZE_FRAMES = 20;  // Wait 20 frames after receiving malus (original line 2219)
    const int MAX_MALUS_BUBBLES = 7;     // Max 7 malus bubbles falling at once

    // Count currently falling malus bubbles for this player
    int fallingMalusCount = 0;
    for (const auto &mb : malusBubbles) {
        if (mb.assignedArray == bArray.playerAssigned && !mb.shouldClear) {
            fallingMalusCount++;
        }
    }

    // Check if we can process malus from queue (original line 2227)
    if (bArray.malusQueue.empty() ||
        currentFrame <= bArray.malusQueue[0] + MALUS_FREEZE_FRAMES ||
        fallingMalusCount >= MAX_MALUS_BUBBLES) {
        return;
    }

    // Calculate top_of_cx: highest bubble in each column (original line 2221-2226)
    int top_of_cx[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (size_t row = 0; row < bArray.bubbleMap.size(); row++) {
        for (size_t col = 0; col < bArray.bubbleMap[row].size(); col++) {
            if (bArray.bubbleMap[row][col].bubbleId != -1) {
                if ((int)row > top_of_cx[col]) {
                    top_of_cx[col] = row;
                }
            }
        }
    }

    // Generate ALL malus bubbles at once (original while loop at line 2227)
    std::vector<MalusBubble> newMalusBubbles;
    while (!bArray.malusQueue.empty() &&
           currentFrame > bArray.malusQueue[0] + MALUS_FREEZE_FRAMES &&
           fallingMalusCount < MAX_MALUS_BUBBLES) {

        // Remove one malus from queue
        bArray.malusQueue.erase(bArray.malusQueue.begin());

        // Generate random bubble color (original: int(rand(@bubbles_images)) = 0..7)
        int bubbleId = ranrange(0, bArray.numColors - 1);

        // Choose column (original line 2231-2240): int(rand(7)) = 0..6
        int cx = ranrange(0, 6);

        // If column is full (stickY would exceed row 12), try adjacent columns
        // (original's noinstantdeath logic avoids placing on full columns)
        int attempts = 0;
        while (top_of_cx[cx] >= 12 && attempts < 7) {
            cx = (cx + 1) % 7;
            attempts++;
        }
        if (top_of_cx[cx] >= 12) {
            // All columns full - skip this malus
            fallingMalusCount--;
            continue;
        }

        // Calculate where bubble will stick (original line 2241-2243)
        int cy = 12;  // Always starts at row 12
        int stickY = top_of_cx[cx] + 1;  // Stick one row below highest in column

        // Update top_of_cx for next bubble in this column
        top_of_cx[cx] = stickY;

        // Calculate screen position (original calc_real_pos at line 2244)
        // Mini players use half bubble size
        bool isMini = (currentSettings.playerCount >= 3 && bArray.playerAssigned >= 1);
        int bubbleSize = isMini ? 16 : 32;
        int rowSize = bubbleSize * 7 / 8;  // 14 for mini, 28 for full
        int smallerSep = (cy % 2 == 0) ? 0 : bubbleSize / 2;
        float startX = (smallerSep + bubbleSize * cx) + bArray.bubbleOffset.x;
        float startY = (rowSize * cy) + bArray.bubbleOffset.y;

        MalusBubble malus = {
            bArray.playerAssigned,
            bubbleId,
            cx, cy,
            stickY,
            startX, startY,
            {(int)startX, (int)startY},
            false,
            false
        };

        newMalusBubbles.push_back(malus);
        fallingMalusCount++;
    }

    if (newMalusBubbles.empty()) return;

    // Sort by cx (original line 2252)
    std::sort(newMalusBubbles.begin(), newMalusBubbles.end(),
              [](const MalusBubble &a, const MalusBubble &b) { return a.cx < b.cx; });

    // Shift Y positions with spacing (original line 2253-2254)
    int shifting = 0;
    for (auto &malus : newMalusBubbles) {
        shifting += 7;
        int randomShift = ranrange(0, 20);
        malus.posY += shifting + randomShift;
        malus.pos.y = (int)malus.posY;
    }

    // Add all to global list
    for (const auto &malus : newMalusBubbles) {
        malusBubbles.push_back(malus);
    }

    // Send ALL 'm' messages (original line 2255-2256)
    // Only local player (array 0) sends messages
    if (bArray.playerAssigned == 0) {
        NetworkClient* netClient = NetworkClient::Instance();
        if (netClient && netClient->IsConnected()) {
            for (const auto &malus : newMalusBubbles) {
                char mMsg[64];
                snprintf(mMsg, sizeof(mMsg), "m%d:%d:%d:%d",
                         malus.bubbleId, malus.cx, malus.cy, malus.stickY);
                SDL_Log("Sending malus bubble: color=%d cx=%d cy=%d stickY=%d",
                        malus.bubbleId, malus.cx, malus.cy, malus.stickY);
                netClient->SendGameData(mMsg);
            }
        }
    }

    audMixer->PlaySFX("malus");
}

void BubbleGame::Update2PText() {
    char plyp[16];
    snprintf(plyp, sizeof(plyp), "%i", winsP1);
    winsP1Text.UpdateText(renderer, plyp, 0);
    winsP1Text.UpdatePosition({(SCREEN_CENTER_X + 160), 12});

    snprintf(plyp, sizeof(plyp), "%i", winsP2);
    winsP2Text.UpdateText(renderer, plyp, 0);
    winsP2Text.UpdatePosition({(SCREEN_CENTER_X - 170), 12});
}

void BubbleGame::UpdatePlayerNameWinText() {
    // Update "PlayerName: WinCount" for each player in 3-5 player mode
    // Based on original Frozen Bubble 2 multiplayer layout (see CLAUDE.md and fb2-3-to-5-player.jpg)

    for (int i = 0; i < currentSettings.playerCount; i++) {
        BubbleArray &bArray = bubbleArrays[i];

        // Format: "PlayerName: WinCount"
        char nameWinStr[128];
        if (!bArray.playerNickname.empty()) {
            snprintf(nameWinStr, sizeof(nameWinStr), "%s: %d",
                     bArray.playerNickname.c_str(), bArray.winCount);
        } else {
            // Fallback: Try to get nickname from NetworkClient if we have lobbyPlayerId
            NetworkClient* netClient = NetworkClient::Instance();
            if (netClient && bArray.lobbyPlayerId >= 0) {
                std::string nick = netClient->GetPlayerNickname(bArray.lobbyPlayerId);
                if (!nick.empty()) {
                    bArray.playerNickname = nick;  // Cache it
                    snprintf(nameWinStr, sizeof(nameWinStr), "%s: %d", nick.c_str(), bArray.winCount);
                } else {
                    snprintf(nameWinStr, sizeof(nameWinStr), "Player %d: %d", i + 1, bArray.winCount);
                }
            } else {
                snprintf(nameWinStr, sizeof(nameWinStr), "Player %d: %d", i + 1, bArray.winCount);
            }
        }

        playerNameWinText[i].UpdateText(renderer, nameWinStr, 0);

        // Use fixed positions based on player layout (matching original FB2)
        // Positions are centered above/below each player's grid
        int textX, textY;

        switch (currentSettings.playerCount) {
            case 3:
                // 3 players: center (p1), top-left (rp1), top-right (rp2)
                if (i == 0) {
                    textX = 320; textY = 12;  // Center player at top
                } else if (i == 1) {
                    textX = 83; textY = 2;  // Top-left mini
                } else {
                    textX = 553; textY = 2;  // Top-right mini
                }
                break;
            case 4:
                // 4 players: center (p1), top-left (rp1), top-right (rp2), bottom-left (rp3)
                if (i == 0) {
                    textX = 320; textY = 12;
                } else if (i == 1) {
                    textX = 83; textY = 2;
                } else if (i == 2) {
                    textX = 553; textY = 2;
                } else {
                    textX = 83; textY = 298;  // Bottom-left mini
                }
                break;
            case 5:
                // 5 players: center (p1), all 4 corners
                if (i == 0) {
                    textX = 320; textY = 12;
                } else if (i == 1) {
                    textX = 83; textY = 2;
                } else if (i == 2) {
                    textX = 553; textY = 2;
                } else if (i == 3) {
                    textX = 83; textY = 298;
                } else {
                    textX = 553; textY = 298;  // Bottom-right mini
                }
                break;
            default:
                // Shouldn't reach here, but fallback
                textX = 320; textY = 12;
                break;
        }

        playerNameWinText[i].UpdatePosition({textX - (playerNameWinText[i].Coords()->w / 2), textY});
    }
}

void BubbleGame::UpdateScoreText(BubbleArray &bArray) {
    char scoreStr[64];
    // For 2-player network games, show only player nickname (no score) in wooden banners
    // For 3+ player games, show "Nickname: Score"
    // For single player, show "Score: X"
    if (currentSettings.networkGame && !bArray.playerNickname.empty()) {
        if (currentSettings.playerCount == 2) {
            // 2-player: show just nickname (scores shown separately)
            snprintf(scoreStr, sizeof(scoreStr), "%s", bArray.playerNickname.c_str());
        } else {
            // 3+ players: show nickname with score
            snprintf(scoreStr, sizeof(scoreStr), "%s: %d", bArray.playerNickname.c_str(), bArray.score);
        }
    } else {
        snprintf(scoreStr, sizeof(scoreStr), "Score: %d", bArray.score);
    }

    // Use shared scoreText object but update and render for each player
    // In multiplayer, this gets called once per player in the render loop
    // Each call updates the text and renders immediately at the player's score position
    scoreText.UpdateText(renderer, scoreStr, 0);
    scoreText.UpdatePosition(bArray.scorePos);

    // Render immediately (original: print_scores renders each player's score in the loop)
    { SDL_FRect fr = ToFRect(*scoreText.Coords()); SDL_RenderTexture(const_cast<SDL_Renderer*>(renderer), scoreText.Texture(), nullptr, &fr); }
}

SDL_Texture** BubbleGame::GetBubbleTextures(bool mini) {
    GameSettings *settings = GameSettings::Instance();
    if (mini) {
        if (settings->colorBlind()) {
            return imgMiniColorblindBubbles;
        }
        return imgMiniBubbles;
    } else {
        if (settings->colorBlind()) {
            return imgColorblindBubbles;
        }
        return imgBubbles;
    }
}

void BubbleGame::SubmitScore(BubbleArray &bArray) {
    SDL_Log("Level %d completed with score: %d", curLevel, bArray.score);
    if (currentSettings.networkGame || currentSettings.playerCount > 1) return;  // Only track 1P levelset scores

    SDL_Log("SubmitScore: getting elapsed time");
    float elapsedSeconds = (SDL_GetTicks() - FrozenBubble::Instance()->startTime) / 1000.0f;
    SDL_Log("SubmitScore: elapsedSeconds=%.1f, getting hm instance", elapsedSeconds);
    HighscoreManager* hm = HighscoreManager::Instance();
    SDL_Log("SubmitScore: calling AppendToLevels, savedLevelGrid rows: %zu", savedLevelGrid.size());
    for (size_t i = 0; i < savedLevelGrid.size(); i++)
        SDL_Log("  row %zu: %zu cells", i, savedLevelGrid[i].size());

    // Store current level grid for highscore display
    hm->AppendToLevels(savedLevelGrid, curLevel);
    SDL_Log("SubmitScore: AppendToLevels done");

    // Check if this qualifies as a top-10 score and save it
    if (hm->CheckAndAddScore(curLevel, elapsedSeconds)) {
        pendingHighscore = true;
        SDL_Log("New high score! Level %d in %.1fs", curLevel, elapsedSeconds);
    }
    SDL_Log("SubmitScore: done");
}

// Count living players (original: sub living_players() at line 600)
// Original checks: !$pdata{$::p_}{left} && $pdata{$::p_}{state} eq 'ingame'
int BubbleGame::CountLivingPlayers() {
    int livingCount = 0;
    SDL_Log("CountLivingPlayers: Checking %d players", currentSettings.playerCount);
    for (int i = 0; i < currentSettings.playerCount; i++) {
        bool isAlive = (bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE);
        SDL_Log("  Player %d: state=%d (0=ALIVE,1=LOST,2=LEFT), isAlive=%d, lobbyId=%d",
                i, (int)bubbleArrays[i].playerState, isAlive, bubbleArrays[i].lobbyPlayerId);
        if (isAlive) {
            livingCount++;
        }
    }
    SDL_Log("  Total living: %d", livingCount);
    return livingCount;
}

// Handle player loss and check win conditions (original: sub lose() at line 1906-1968)
void BubbleGame::HandlePlayerLoss(BubbleArray &bArray) {
    SDL_Log("HandlePlayerLoss: player %d lost", bArray.playerAssigned);

    // Mark player as lost (original line 1926: $pdata{$player}{state} = 'lost')
    bArray.playerState = BubbleArray::PlayerState::LOST;

    // Play lose sound
    audMixer->PlaySFX("lose");

    if (currentSettings.networkGame && currentSettings.playerCount >= 2) {
        // NOTE: Don't send death notification - in the original, each client independently
        // detects deaths by checking synchronized bubble positions via 's' messages.
        // The 'l' message means "left" (disconnected), not "lost" (died).
        // Original: frozen-bubble lines 1925-1960
        // Multiplayer network game
        int livingCount = CountLivingPlayers();
        SDL_Log("Living players: %d", livingCount);

        if (livingCount == 1) {
            // Find the winner (the last living player)
            int winnerIdx = -1;
            for (int i = 0; i < currentSettings.playerCount; i++) {
                if (bubbleArrays[i].playerState == BubbleArray::PlayerState::ALIVE) {
                    winnerIdx = i;
                    break;
                }
            }

            if (winnerIdx >= 0) {
                // We have a winner! Game ends immediately (original lines 1933-1944)
                SDL_Log("Winner found: player %d", winnerIdx);
                bubbleArrays[winnerIdx].mpWinner = true;
                bubbleArrays[winnerIdx].penguinSprite.PlayAnimation(10);

                // Guard: only process once per round.
                // Multiple clients independently detect the same winner and each send 'F'.
                // The server relays each 'F' to the *other* players (not back to sender).
                // So if we get here first (before receiving any 'F'), we count locally and
                // the incoming 'F' from the other client is blocked by the !gameFinish guard
                // in the 'F' handler. If 'F' somehow arrived first, gameFinish is already
                // set and we skip the increment here.
                if (!gameFinish) {
                    gameFinish = true;

                    if (winnerIdx == 0) {
                        winsP1++;
                    } else {
                        winsP2++;
                    }
                    bubbleArrays[winnerIdx].winCount++;
                    Update2PText();

                    // Check victories limit
                    if (currentSettings.victoriesLimit > 0 &&
                        bubbleArrays[winnerIdx].winCount >= currentSettings.victoriesLimit) {
                        gameMatchOver = true;
                        SDL_Log("Match over! Player %d reached %d victories",
                                winnerIdx, currentSettings.victoriesLimit);
                    }

                    // Send 'F' to notify other clients (original line 1943)
                    NetworkClient* netClient = NetworkClient::Instance();
                    if (netClient && netClient->IsConnected() && netClient->GetState() == IN_GAME && bubbleArrays[winnerIdx].playerState != BubbleArray::PlayerState::LEFT) {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "F%s", bubbleArrays[winnerIdx].playerNickname.c_str());
                        netClient->SendGameData(msg);
                        SDL_Log("Sent win notification: %s", bubbleArrays[winnerIdx].playerNickname.c_str());
                    }
                }
            }
        } else if (livingCount == 0) {
            // All players dead - draw game (no winner)
            // Original handles this in the 'finished' state processing (line 2336, 2353)
            SDL_Log("Draw game - all players are dead!");
            gameFinish = true;
            gameLost = true;  // Mark as lost (no winner)

            // Don't send F message since there's no winner
            // Each client will independently detect all players are dead
        } else if (livingCount > 1) {
            // More players still alive, game continues (original lines 1946-1950)
            SDL_Log("Game continues, %d players still alive", livingCount);
        }
    } else if (currentSettings.playerCount == 2 && !currentSettings.networkGame) {
        // Local 2-player game
        int winnerIdx = bArray.playerAssigned == 0 ? 1 : 0;
        gameFinish = true;
        bubbleArrays[winnerIdx].mpWinner = true;
        bubbleArrays[winnerIdx].penguinSprite.PlayAnimation(10);

        if (winnerIdx == 0) winsP1++;
        else winsP2++;
        Update2PText();
    }
}

void BubbleGame::CheckGameState(BubbleArray &bArray) {
    if (bArray.compressionDisabled) return;  // Per-player compression disabled
    bArray.turnsToCompress--;
    if (bArray.turnsToCompress == 1) bArray.waitPrelight = PRELIGHT_FAST;
    if (bArray.turnsToCompress == 0) {
        ResetPrelight(bArray);
        bArray.waitPrelight = PRELIGHT_SLOW;
        if (currentSettings.playerCount < 2) {
            bArray.turnsToCompress = 9;
            bArray.dangerZone--;
            bArray.numSeparators++;
            bArray.ExpandOffset(0, 28);
            bArray.compressorRct.y += 28;
            audMixer->PlaySFX("newroot_solo");
        }
        else {
            ExpandNewLane(bArray);
            bArray.turnsToCompress = 12;
            audMixer->PlaySFX("newroot");
        }
    }
    if (bArray.allClear()) {
        // Award bonus for clearing the level!
        if (currentSettings.playerCount < 2) {
            int clearBonus = 1000;
            bArray.score += clearBonus;

            // Submit score when level is cleared
            SubmitScore(bArray);
        }

        gameFinish = true;
        if (currentSettings.playerCount < 2) gameWon = true;
        else {
            audMixer->PlaySFX("lose");
            bArray.mpWinner = true;

            // In network games, only local player (array 0) processes wins
            if (currentSettings.networkGame && bArray.playerAssigned == 0) {
                // Local player cleared all bubbles - we won!
                // Count locally: server does NOT relay our own 'F' back to us.
                winsP1++;
                bArray.winCount++;
                Update2PText();

                if (currentSettings.victoriesLimit > 0 &&
                    bArray.winCount >= currentSettings.victoriesLimit) {
                    gameMatchOver = true;
                    SDL_Log("Match over! Local player reached %d victories",
                            currentSettings.victoriesLimit);
                }

                // Send 'F' to inform opponents (original line 1943)
                NetworkClient* netClient = NetworkClient::Instance();
                if (netClient && netClient->IsConnected() && netClient->GetState() == IN_GAME) {
                    std::string fMsg = "F" + netClient->GetPlayerNick();
                    netClient->SendGameData(fMsg.c_str());
                    SDL_Log("Sent win notification: F%s", netClient->GetPlayerNick().c_str());
                }
            } else if (!currentSettings.networkGame) {
                // Non-network multiplayer (local 2P)
                if (bArray.playerAssigned == 0) winsP1++;
                else winsP2++;
                Update2PText();
            }
        }
        panelRct = {SCREEN_CENTER_X - 173, 480 - 289, 329, 159};
        bArray.penguinSprite.PlayAnimation(10);
    }
    // Check if ANY player hit the danger zone (original: verify_if_end() at line 1970-1975)
    // In multiplayer, we need to check ALL players, not just the current one
    if (currentSettings.networkGame && currentSettings.playerCount >= 2) {
        // Check all players for danger zone (original: iter_players with cy > 11 check)
        for (int i = 0; i < currentSettings.playerCount; i++) {
            BubbleArray &checkArray = bubbleArrays[i];
            if (checkArray.bubbleOnDanger() && checkArray.playerState == BubbleArray::PlayerState::ALIVE) {
                if (i == 0) {
                    // Local player lost
                    panelRct = {SCREEN_CENTER_X - 173, 480 - 248, 345, 124};
                    checkArray.curLaunchRct = {checkArray.curLaunchRct.x - 1, checkArray.curLaunchRct.y - 1, 34, 48};
                }
                checkArray.penguinSprite.PlayAnimation(11);
                HandlePlayerLoss(checkArray);
            }
        }
    } else {
        // Single player or local 2P - only check the current player
        if (bArray.bubbleOnDanger() && bArray.playerState == BubbleArray::PlayerState::ALIVE) {
            panelRct = {SCREEN_CENTER_X - 173, 480 - 248, 345, 124};
            bArray.curLaunchRct = {bArray.curLaunchRct.x - 1, bArray.curLaunchRct.y - 1, 34, 48};
            bArray.penguinSprite.PlayAnimation(11);
            HandlePlayerLoss(bArray);
        }
    }
}

static void DrawAimGuide(SDL_Renderer* rend, const BubbleArray& bArray) {
    const int BUBBLE_SIZE = 32;
    const int ROW_SIZE = 28;
    const float speed = (float)(BUBBLE_SPEED);  // 5 pixels/step

    float px = (float)bArray.curLaunchRct.x;
    float py = (float)bArray.curLaunchRct.y;
    float angle = bArray.shooterSprite.angle;
    float dx = speed * cosf(angle);
    float dy = speed * sinf(angle);

    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);

    for (int step = 0; step < 400; step++) {
        px += dx;
        py -= dy;  // Negative = move up

        if (px < bArray.leftLimit) {
            px = 2.0f * bArray.leftLimit - px;
            dx = -dx;
        }
        if (px > bArray.rightLimit - BUBBLE_SIZE) {
            px = 2.0f * (bArray.rightLimit - BUBBLE_SIZE) - px;
            dx = -dx;
        }

        if (py <= (float)bArray.topLimit) break;

        // Check grid collision every 2 steps
        if (step % 2 == 0) {
            int cy = (int)((py - bArray.bubbleOffset.y + ROW_SIZE / 2.0f) / ROW_SIZE);
            if (cy >= 0 && cy < 13) {
                int oddRowOffset = ((int)bArray.bubbleMap[cy].size() == 7) ? BUBBLE_SIZE / 2 : 0;
                int cx = (int)((px - bArray.bubbleOffset.x + BUBBLE_SIZE / 2.0f - oddRowOffset) / BUBBLE_SIZE);
                if (cx >= 0 && cx < (int)bArray.bubbleMap[cy].size() &&
                    bArray.bubbleMap[cy][cx].bubbleId != -1) {
                    break;
                }
            }
        }

        // Draw dot every 8 steps, fading with distance
        if (step % 8 == 0) {
            int alpha = 200 - step / 2;
            if (alpha < 30) alpha = 30;
            SDL_SetRenderDrawColor(rend, 255, 255, 255, (Uint8)alpha);
            SDL_Rect dot = {(int)px + BUBBLE_SIZE / 2 - 3, (int)py + BUBBLE_SIZE / 2 - 3, 6, 6};
            { SDL_FRect fr = ToFRect(dot); SDL_RenderFillRect(rend, &fr); }
        }
    }

    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);
}

void BubbleGame::Render() {
    SDL_Renderer *rend = const_cast<SDL_Renderer*>(renderer);
    SDL_RenderTexture(rend, background, nullptr, nullptr);

    // Process network messages if this is a network game
    if (currentSettings.networkGame) {
        ProcessNetworkMessages();

        // Send ping every second to prevent idle timeout (60 FPS = 60 frames/sec)
        // This matches the original Perl implementation which sends 'p' every second
        networkFrameCounter++;
        if (networkFrameCounter >= 60) {
            networkFrameCounter = 0;
            NetworkClient* netClient = NetworkClient::Instance();
            if (netClient->IsConnected() && netClient->GetState() == IN_GAME) {
                netClient->SendGameData("p");
            }
        }

        // Check if both players are ready for new game after round ends
        if (waitingForOpponentNewGame && opponentReadyForNewGame) {
#ifdef __WASM_PORT__
            // WASM joiner: WaitForBubble inside ReloadGame->SyncNetworkLevel cannot yield,
            // so queue the 40 sync messages first (same fix as initial game start).
            NetworkClient* netClientRound = NetworkClient::Instance();
            if (netClientRound && !netClientRound->IsLeader()) {
                if (wasmRoundSyncWaitStart == 0) wasmRoundSyncWaitStart = SDL_GetTicks();
                size_t qSize = netClientRound->MessageQueueSize();
                bool timedOut = (SDL_GetTicks() - wasmRoundSyncWaitStart > 5000);
                SDL_Log("WASM round sync wait: queue=%d waited=%dms", (int)qSize, (int)(SDL_GetTicks() - wasmRoundSyncWaitStart));
                if (qSize < 40 && !timedOut) {
                    return;  // come back next frame
                }
                SDL_Log("WASM round sync: proceeding queue=%d timedOut=%d", (int)qSize, timedOut);
                wasmRoundSyncWaitStart = 0;
            }
#endif
            SDL_Log("All players ready - starting new game (detected in render loop)");
            waitingForOpponentNewGame = false;
            opponentReadyForNewGame = false;
            opponentsReadyCount = 0;
            // In network games, check who won by looking at mpWinner flags
            // The player who cleared their board or whose opponent hit danger wins
            bool localPlayerWon = bubbleArrays[0].mpWinner;
            SDL_Log("Starting next round: localPlayerWon=%d", localPlayerWon);
            if (localPlayerWon) {
                ReloadGame(++curLevel);
            } else {
                // Opponent won or we lost - replay same level
                ReloadGame(curLevel);
            }
        }

        // Increment global frame counter (used by malus timing)
        frameCount++;
        // NOTE: ProcessMalusQueue for local player is called inside the stick handler
        // (after bubble sticks), matching original Perl behavior at line 2217-2258.
        // This ensures malus only falls AFTER the local player fires and their bubble sticks.
    }

    // NOTE: Local multiplayer malus queues are processed at stick time (inside the stick handlers above),
    // matching original Perl behavior where malus only falls after the recipient sticks their bubble.
    if (!currentSettings.networkGame && currentSettings.playerCount >= 2 && !gameFinish) {
        frameCount++;
    }

    // Multiplayer training mode: periodically inject random malus, enforce 2-min timer
    if (currentSettings.mpTraining && !gameFinish) {
        if (mpTrainStartTime == 0) mpTrainStartTime = SDL_GetTicks();
        Uint32 elapsed = SDL_GetTicks() - mpTrainStartTime;
        const Uint32 TRAIN_DURATION = 120 * 1000;  // 2 minutes in ms

        if (!mpTrainDone && elapsed >= TRAIN_DURATION) {
            // Time's up — show score and end
            mpTrainDone = true;
            gameFinish = true;
            gameWon = true;
            // Store training score as level=101 (sentinel for mp_train) with time=score
            if (HighscoreManager::Instance()->CheckAndAddScore(mpTrainScore, 0.0f))
                pendingHighscore = true;
        } else if (!mpTrainDone) {
            // Randomly inject malus rows (original: rand($mptrainingdiff*(1000/$TARGET_ANIM_SPEED)) == 0)
            // mptrainingdiff default = 30 seconds between attacks; at 60fps: 30*60=1800 frames avg
            BubbleArray &arr = bubbleArrays[0];
            if (arr.malusQueue.empty()) {
                int roll = rand() % 1800;
                if (roll == 0) {
                    int count = 1 + rand() % 6;
                    for (int i = 0; i < count; i++)
                        arr.malusQueue.push_back(frameCount);
                }
            }
        }
    }

    // Process malus queue for mp_training (not in networkGame block)
    if (currentSettings.mpTraining && !gameFinish) {
        frameCount++;
        ProcessMalusQueue(bubbleArrays[0], frameCount);
    }

    if(playedPause) {
        audMixer->PauseMusic(true);
        playedPause = false;
        FrozenBubble::Instance()->startTime += SDL_GetTicks() - timePaused;
    }

    if(currentSettings.playerCount == 1) {
        BubbleArray &curArray = bubbleArrays[0];

        SDL_Rect rct;
        for (int i = 1; i < 10; i++) {
            rct.x = curArray.rightLimit;
            rct.y = 104 - (7 * i) - i;
            rct.w = rct.h = 7;
            { SDL_FRect fr = ToFRect(rct); SDL_RenderTexture(rend, dotTexture[i == curArray.turnsToCompress ? 1 : 0], nullptr, &fr); }
        }
        for (int i = 0; i < curArray.numSeparators; i++) {
            rct.x = SCREEN_CENTER_X - 95;
            rct.y = (28 * i);
            rct.w = 188;
            rct.h = 28;
            { SDL_FRect fr = ToFRect(rct); SDL_RenderTexture(rend, sepCompressorTexture, nullptr, &fr); }
        }
        { SDL_FRect fr = ToFRect(curArray.compressorRct); SDL_RenderTexture(rend, compressorTexture, nullptr, &fr); }

        if (curArray.turnsToCompress <= 2) {
            DoPrelightAnimation(curArray, curArray.prelightTime);
        }
        SDL_Texture** useBubbles = GetBubbleTextures();
        for (const std::vector<Bubble> &vecBubble : curArray.bubbleMap) for (Bubble bubble : vecBubble) bubble.Render(rend, useBubbles, imgBubblePrelight, imgBubbleFrozen);

        // Stick effect animation (original: $sticking_bubble / sticking_step)
        if (curArray.stickAnimActive) {
            SDL_Rect sr = {curArray.stickAnimPos.x - 16, curArray.stickAnimPos.y - 16, 32, 32};
            { SDL_FRect fr = ToFRect(sr); SDL_RenderTexture(rend, imgBubbleStick[curArray.stickAnimFrame], nullptr, &fr); }
            if (++curArray.stickAnimSlowdown >= 2) {
                curArray.stickAnimSlowdown = 0;
                if (++curArray.stickAnimFrame > BUBBLE_STICKFC) curArray.stickAnimActive = false;
            }
        }

        if(gameFinish) {
            if (!gameWon && !gameLost) DoFrozenAnimation(curArray, curArray.frozenWait);

            if (gameLost) {
                { SDL_FRect fr = ToFRect(panelRct); SDL_RenderTexture(rend, soloStatePanels[0], nullptr, &fr); }
                // Show final score on lose screen
                char finalScore[64];
                snprintf(finalScore, sizeof(finalScore), "Final Score: %d", curArray.score);
                finalScoreText.UpdateText(renderer, finalScore, 0);
                finalScoreText.UpdatePosition({SCREEN_CENTER_X - (finalScoreText.Coords()->w / 2), panelRct.y + panelRct.h - 40});
                { SDL_FRect fr = ToFRect(*finalScoreText.Coords()); SDL_RenderTexture(rend, finalScoreText.Texture(), nullptr, &fr); }
            }
            else if (gameWon) {
                { SDL_FRect fr = ToFRect(panelRct); SDL_RenderTexture(rend, soloStatePanels[1], nullptr, &fr); }
                // Show final score on win screen (training shows mp_train score, normal shows bubble score)
                char finalScore[64];
                if (currentSettings.mpTraining)
                    snprintf(finalScore, sizeof(finalScore), "Training Score: %d", mpTrainScore);
                else
                    snprintf(finalScore, sizeof(finalScore), "Final Score: %d", curArray.score);
                finalScoreText.UpdateText(renderer, finalScore, 0);
                finalScoreText.UpdatePosition({SCREEN_CENTER_X - (finalScoreText.Coords()->w / 2), panelRct.y + panelRct.h - 40});
                { SDL_FRect fr = ToFRect(*finalScoreText.Coords()); SDL_RenderTexture(rend, finalScoreText.Texture(), nullptr, &fr); }
            }
        }

        if(singleBubbles.size() > 0) {
            UpdateSingleBubbles(0);
            for (SingleBubble &bubble : singleBubbles) bubble.Render(rend, useBubbles);
        }

        // Render malus bubbles in mp_training mode
        if (currentSettings.mpTraining) {
            for (MalusBubble &malus : malusBubbles) {
                malus.Render(rend, useBubbles, false);
            }
        }

        { SDL_FRect fr = ToFRect(curArray.curLaunchRct); SDL_RenderTexture(rend, gameFinish && !gameWon ? imgBubbleFrozen : useBubbles[curArray.curLaunch], nullptr, &fr); }
        { SDL_FRect fr = ToFRect(curArray.nextBubbleRct); SDL_RenderTexture(rend, useBubbles[curArray.nextBubble], nullptr, &fr); }
        { SDL_FRect fr = ToFRect(curArray.onTopRct); SDL_RenderTexture(rend, onTopTexture, nullptr, &fr); }
        if (gameFinish && !gameWon) { SDL_FRect fr = ToFRect(curArray.frozenBottomRct); SDL_RenderTexture(rend, imgBubbleFrozen, nullptr, &fr); }

        UpdatePenguin(curArray);
        if(!lowGfx) curArray.penguinSprite.Render();
        curArray.shooterSprite.Render(lowGfx);
        if (curArray.aimGuideEnabled && !gameFinish) DrawAimGuide(rend, curArray);
        { SDL_FRect fr = ToFRect(*inGameText.Coords()); SDL_RenderTexture(rend, inGameText.Texture(), nullptr, &fr); }

        // Display score (UpdateScoreText now renders immediately)
        UpdateScoreText(curArray);

        // Multiplayer training: show countdown timer and training score
        if (currentSettings.mpTraining && mpTrainStartTime > 0) {
            const Uint32 TRAIN_DURATION = 120 * 1000;
            Uint32 elapsed = SDL_GetTicks() - mpTrainStartTime;
            int remaining = (elapsed < TRAIN_DURATION) ? (int)((TRAIN_DURATION - elapsed) / 1000) : 0;
            int m = remaining / 60;
            int s = remaining % 60;
            char trainBuf[64];
            snprintf(trainBuf, sizeof(trainBuf), "%d'%02d\"  Score: %d", m, s, mpTrainScore);
            mpTrainText.UpdateText(renderer, trainBuf, 0);
            mpTrainText.UpdatePosition({32, 177});
            { SDL_FRect fr = ToFRect(*mpTrainText.Coords()); SDL_RenderTexture(rend, mpTrainText.Texture(), nullptr, &fr); }
        }

        // Display combo text if timer is active
        if (comboDisplayTimer > 0) {
            { SDL_FRect fr = ToFRect(*comboText.Coords()); SDL_RenderTexture(rend, comboText.Texture(), nullptr, &fr); }
            comboDisplayTimer--;
        }
    }
    else { //iterate until all penguins & status are rendered
        // Update ALL players' bubbles ONCE before rendering (original: iter_players at line 2105)
        // This ensures all players are processed in a single unified loop
        UpdateSingleBubbles(0);  // id parameter ignored now - processes all players

        for (int i = 0; i < currentSettings.playerCount; i++) {
            BubbleArray &curArray = bubbleArrays[i];

            SDL_Rect rct;
            for (int i = 1; i < 13; i++) {
                rct.x = curArray.rightLimit;
                rct.y = 104 - (7 * i) - i;
                rct.w = rct.h = 7;
                { SDL_FRect fr = ToFRect(rct); SDL_RenderTexture(rend, dotTexture[i == curArray.turnsToCompress ? 1 : 0], nullptr, &fr); }
            }

            // Use mini textures for remote players (playerAssigned >= 1) in 3-5 player games
            bool useMini = (currentSettings.playerCount >= 3 && curArray.playerAssigned >= 1);
            SDL_Texture** useBubbles = GetBubbleTextures(useMini);
            SDL_Texture* useFrozen = useMini ? imgMiniBubbleFrozen : imgBubbleFrozen;
            SDL_Texture* usePrelight = useMini ? imgMiniBubblePrelight : imgBubblePrelight;

            // Don't render shooter bubbles for LOST players (prevents crashes from invalid bubble indices)
            // In network games, losing players become spectators and shouldn't have active bubbles
            if (curArray.playerState != BubbleArray::PlayerState::LOST) {
                { SDL_FRect fr = ToFRect(curArray.curLaunchRct); SDL_RenderTexture(rend, gameFinish && !curArray.mpWinner ? useFrozen : useBubbles[curArray.curLaunch], nullptr, &fr); }
                { SDL_FRect fr = ToFRect(curArray.nextBubbleRct); SDL_RenderTexture(rend, useBubbles[curArray.nextBubble], nullptr, &fr); }
                { SDL_FRect fr = ToFRect(curArray.onTopRct); SDL_RenderTexture(rend, onTopTexture, nullptr, &fr); }
            }
            if (gameFinish && !curArray.mpWinner) { SDL_FRect fr = ToFRect(curArray.frozenBottomRct); SDL_RenderTexture(rend, useFrozen, nullptr, &fr); }

            if (curArray.turnsToCompress <= 2) {
                DoPrelightAnimation(curArray, curArray.prelightTime);
            }
            for (const std::vector<Bubble> &vecBubble : curArray.bubbleMap) for (Bubble bubble : vecBubble) bubble.Render(rend, useBubbles, usePrelight, useFrozen);

            // Stick effect animation (original: $sticking_bubble / sticking_step)
            if (curArray.stickAnimActive) {
                SDL_Texture* stickTex = useMini ? imgMiniBubbleStick[curArray.stickAnimFrame] : imgBubbleStick[curArray.stickAnimFrame];
                int sz = useMini ? 16 : 32;
                SDL_Rect sr = {curArray.stickAnimPos.x - sz/2, curArray.stickAnimPos.y - sz/2, sz, sz};
                { SDL_FRect fr = ToFRect(sr); SDL_RenderTexture(rend, stickTex, nullptr, &fr); }
                if (++curArray.stickAnimSlowdown >= 2) {
                    curArray.stickAnimSlowdown = 0;
                    if (++curArray.stickAnimFrame > BUBBLE_STICKFC) curArray.stickAnimActive = false;
                }
            }

            if(gameFinish) {
                if (!curArray.mpWinner) DoFrozenAnimation(curArray, curArray.frozenWait);
                else {
                    DoWinAnimation(curArray, curArray.explodeWait);
                    idxMPWinner = i;
                }
            }

            UpdatePenguin(curArray);
            if(!lowGfx) curArray.penguinSprite.Render();
            curArray.shooterSprite.Render(lowGfx);
            if (curArray.aimGuideEnabled && !gameFinish &&
                curArray.playerState == BubbleArray::PlayerState::ALIVE) {
                DrawAimGuide(rend, curArray);
            }

            // NOTE: UpdateSingleBubbles is now called ONCE before the loop (line 2416)
            // Don't call it here per-player anymore

            // Display score with nickname for each player (original: print_scores at line 1868)
            // In 3+ player games, skip score text — win counts are shown via UpdatePlayerNameWinText
            // at the same screen positions, so rendering both would cause overlapping text.
            if (currentSettings.playerCount < 3) {
                UpdateScoreText(curArray);
            }

            // Display "left" overlay for players who actually disconnected (original line 1951-1955)
            // NOTE: LOST = died (still in game), LEFT = disconnected. Only show for LEFT.
            if (currentSettings.networkGame && curArray.playerAssigned >= 1 &&
                curArray.playerState == BubbleArray::PlayerState::LEFT) {
                // Determine which texture and position to use based on player and mini graphics
                SDL_Texture* leftTexture = nullptr;
                SDL_Rect leftRect = {0, 0, 0, 0};

                bool isMini = (currentSettings.playerCount >= 3);
                if (isMini) {
                    // Mini left overlays for 3-5 player games
                    if (curArray.playerAssigned == 1) {
                        leftTexture = leftRp1Mini;
                        leftRect = {20, 19, 128, 173};  // rp1 position
                    } else if (curArray.playerAssigned == 2) {
                        leftTexture = leftRp2Mini;
                        leftRect = {492, 19, 128, 173};  // rp2 position
                    } else if (curArray.playerAssigned == 3) {
                        leftTexture = leftRp3Mini;
                        leftRect = {20, 287, 128, 173};  // rp3 position
                    } else if (curArray.playerAssigned == 4) {
                        leftTexture = leftRp4Mini;
                        leftRect = {492, 287, 128, 173};  // rp4 position
                    }
                } else {
                    // Full size left overlay for 2-player game
                    leftTexture = leftRp1;
                    leftRect = {320, 0, 320, 480};  // rp1 position (right side)
                }

                if (leftTexture) {
                    { SDL_FRect fr = ToFRect(leftRect); SDL_RenderTexture(rend, leftTexture, nullptr, &fr); }
                }
            }

            // Render targeting attack indicator on the targeted opponent's board
            // (original: put_image_to_background($imgbin{attack}{...}) in set_sendmalustoone at line 1338)
            // Attack positions from Stuff.pm POS_MP: rp1={25,213}, rp2={496,214}, rp3={24,442}, rp4={496,442}
            if (currentSettings.singlePlayerTargetting && sendMalusToOne == i &&
                curArray.playerAssigned >= 1 && curArray.playerAssigned <= 4) {
                static const SDL_Point attackPos[4] = {{25, 213}, {496, 214}, {24, 442}, {496, 442}};
                int rpIdx = curArray.playerAssigned - 1;
                if (imgAttack[rpIdx]) {
                    SDL_Rect attackRct;
                    { float fw, fh; SDL_GetTextureSize(imgAttack[rpIdx], &fw, &fh); attackRct.w = (int)fw; attackRct.h = (int)fh; }
                    attackRct.x = attackPos[rpIdx].x;
                    attackRct.y = attackPos[rpIdx].y;
                    { SDL_FRect fr = ToFRect(attackRct); SDL_RenderTexture(rend, imgAttack[rpIdx], nullptr, &fr); }
                }
            }

            // Show targeting text: who this player is targeting
            if (currentSettings.singlePlayerTargetting && !gameFinish &&
                playerTargeting[i] >= 0 && playerTargeting[i] < currentSettings.playerCount) {
                const std::string& targetNick = bubbleArrays[playerTargeting[i]].playerNickname;
                if (!targetNick.empty()) {
                    char tgtBuf[64];
                    snprintf(tgtBuf, sizeof(tgtBuf), "> %s", targetNick.c_str());
                    targetingText.UpdateText(rend, tgtBuf, 0);
                    // Position: near each player's shooter area
                    int tx, ty;
                    if (curArray.playerAssigned == 0) {
                        tx = curArray.shooterSprite.rect.x + curArray.shooterSprite.rect.w / 2 - 30;
                        ty = curArray.shooterSprite.rect.y - 20;
                    } else {
                        tx = curArray.shooterSprite.rect.x;
                        ty = curArray.shooterSprite.rect.y + curArray.shooterSprite.rect.h;
                    }
                    targetingText.UpdatePosition({tx, ty});
                    { SDL_FRect fr = ToFRect(*targetingText.Coords()); SDL_RenderTexture(rend, targetingText.Texture(), nullptr, &fr); }
                }
            }
        }

        // Render "attackme" indicators on local player board when opponents are targeting us
        // (original: redraw_attackingme() at line 1345)
        // attackme position from Stuff.pm: p1 attackme={185, 448}, each attacker offset by 24px
        if (currentSettings.singlePlayerTargetting && !attackingMe.empty() && !gameFinish) {
            for (size_t k = 0; k < attackingMe.size(); k++) {
                int attackerArray = attackingMe[k];
                if (attackerArray >= 1 && attackerArray <= 4) {
                    int rpIdx = attackerArray - 1;
                    if (imgAttackMe[rpIdx]) {
                        SDL_Rect amRct;
                        { float fw, fh; SDL_GetTextureSize(imgAttackMe[rpIdx], &fw, &fh); amRct.w = (int)fw; amRct.h = (int)fh; }
                        amRct.x = 185 + ((int)k * 24);
                        amRct.y = 448;
                        { SDL_FRect fr = ToFRect(amRct); SDL_RenderTexture(rend, imgAttackMe[rpIdx], nullptr, &fr); }
                    }
                }
            }
        }

        // Check all players for danger zone every frame in network multiplayer (original: verify_if_end() at line 2319)
        // Original checks: if ($pdata{state} eq 'game' && any { $_->{cy} > 11 })
        // Only check while global game state is "game" (not finished/won)
        if (!gameFinish && currentSettings.networkGame && currentSettings.playerCount >= 2) {
            static int checkCounter = 0;
            checkCounter++;
            for (int i = 0; i < currentSettings.playerCount; i++) {
                BubbleArray &checkArray = bubbleArrays[i];
                // Check if player is alive AND has bubbles in danger zone (cy > 11 means row 12+)
                bool inDanger = checkArray.bubbleOnDanger();
                bool isAlive = (checkArray.playerState == BubbleArray::PlayerState::ALIVE);

                // Log every 60 frames (once per second at 60fps) for debugging
                if (checkCounter % 60 == 0 && i < 3) {
                    SDL_Log("Player %d: alive=%d, inDanger=%d, lobbyId=%d",
                            i, isAlive, inDanger, checkArray.lobbyPlayerId);
                }

                if (isAlive && inDanger) {
                    SDL_Log("!!! Player %d hit danger zone!", i);
                    if (i == 0) {
                        // Local player lost
                        panelRct = {SCREEN_CENTER_X - 173, 480 - 248, 345, 124};
                        checkArray.curLaunchRct = {checkArray.curLaunchRct.x - 1, checkArray.curLaunchRct.y - 1, 34, 48};
                    }
                    checkArray.penguinSprite.PlayAnimation(11);
                    HandlePlayerLoss(checkArray);
                }
            }
        }

        if (gameFinish) {
            if (currentSettings.playerCount == 2) {
                if (gameMpDone) { SDL_FRect fr = ToFRect(panelRct); SDL_RenderTexture(rend, multiStatePanels[idxMPWinner], nullptr, &fr); }
                else {
                    for (int i = 0; i < currentSettings.playerCount; i++) {
                        if (bubbleArrays[i].mpDone == false) {
                            gameMpDone = false;
                            break;
                        } else gameMpDone = true;
                    }
                }
            }
        }

        if(singleBubbles.size() > 0) {
            SDL_Texture** useBubbles = GetBubbleTextures();
            for (SingleBubble &bubble : singleBubbles) bubble.Render(rend, useBubbles);
        }

        // Render malus bubbles (attack bubbles)
        if(malusBubbles.size() > 0) {
            for (MalusBubble &malus : malusBubbles) {
                // Determine if this malus bubble belongs to a mini player
                // In 3+ player games: array 0 (center) is full size, arrays 1+ are mini
                bool useMini = (currentSettings.playerCount >= 3 && malus.assignedArray >= 1);
                SDL_Texture** bubbles = GetBubbleTextures(useMini);
                malus.Render(rend, bubbles, useMini);
            }
        }

        // Render win counters and player names
        if (currentSettings.playerCount == 2) {
            // 2-player mode: show simple win counters
            { SDL_FRect fr = ToFRect(*winsP1Text.Coords()); SDL_RenderTexture(rend, winsP1Text.Texture(), nullptr, &fr); }
            { SDL_FRect fr = ToFRect(*winsP2Text.Coords()); SDL_RenderTexture(rend, winsP2Text.Texture(), nullptr, &fr); }
        } else if (currentSettings.playerCount >= 3) {
            // Update names every frame to pick up nicknames as they become available
            UpdatePlayerNameWinText();

            // 3-5 player mode: show player name and win count
            for (int i = 0; i < currentSettings.playerCount; i++) {
                if (playerNameWinText[i].Texture()) {
                    { SDL_FRect fr = ToFRect(*playerNameWinText[i].Coords()); SDL_RenderTexture(rend, playerNameWinText[i].Texture(), nullptr, &fr); }
                }
            }
        }
    }

    // In-game chat overlay (network games only)
    if (currentSettings.networkGame) {
        if (!inGameChatMessages.empty() || chattingMode) {
            const int lineH   = 18;
            const int maxShow = 3;
            const int chatX   = 5;
            // Base Y: bottom of screen with room for maxShow lines
            const int baseY   = 480 - maxShow * lineH - 2;

            // Semi-transparent dark background
            int bgY = baseY - 2;
            int bgH = maxShow * lineH + 4;
            if (chattingMode) { bgY -= lineH + 4; bgH += lineH + 4; }
            SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
            SDL_Rect chatBg = {0, bgY, 640, bgH};
            SDL_SetRenderDrawColor(rend, 0, 0, 0, 170);
            { SDL_FRect fr = ToFRect(chatBg); SDL_RenderFillRect(rend, &fr); }
            SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_NONE);

            // Draw input line (yellow) above the message area
            if (chattingMode) {
                char inputLine[512];
                snprintf(inputLine, sizeof(inputLine), "Say: %s_", chatInputBuf);
                chatInputText.UpdateText(rend, inputLine, 630);
                chatInputText.UpdatePosition({chatX, bgY + 2});
                if (chatInputText.Texture())
                    { SDL_FRect fr = ToFRect(*chatInputText.Coords()); SDL_RenderTexture(rend, chatInputText.Texture(), nullptr, &fr); }
            }

            // Draw last maxShow messages (white)
            int count = (int)inGameChatMessages.size();
            int start = count > maxShow ? count - maxShow : 0;
            for (int i = start; i < count; i++) {
                char lineBuf[512];
                snprintf(lineBuf, sizeof(lineBuf), "%s: %s",
                         inGameChatMessages[i].nick.c_str(),
                         inGameChatMessages[i].text.c_str());
                chatLineText.UpdateText(rend, lineBuf, 630);
                chatLineText.UpdatePosition({chatX, baseY + (i - start) * lineH});
                if (chatLineText.Texture())
                    { SDL_FRect fr = ToFRect(*chatLineText.Coords()); SDL_RenderTexture(rend, chatLineText.Texture(), nullptr, &fr); }
            }
        }
    }

    if (!firstRenderDone) {
        TransitionManager::Instance()->TakeSnipOut(rend);
        firstRenderDone = true;
    }
}

void BubbleGame::RenderPaused() {
    SDL_Renderer *rend = const_cast<SDL_Renderer*>(renderer);

    if(!playedPause) {
        audMixer->PauseMusic();
        audMixer->PlaySFX("pause");
        playedPause = true;
        pauseFrame = 0;

        if(prePauseBackground != nullptr) SDL_DestroyTexture(prePauseBackground);

        SDL_Surface *sfc = SDL_RenderReadPixels(rend, NULL);
        prePauseBackground = SDL_CreateTextureFromSurface(rend, sfc);
        SDL_DestroySurface(sfc);
    }

    SDL_RenderClear(rend);
    SDL_RenderTexture(rend, prePauseBackground, nullptr, nullptr);
    SDL_RenderTexture(rend, pauseBackground, nullptr, nullptr);

    if (nextPauseUpd <= 0){
        pauseFrame++;
        nextPauseUpd = 2;
        if(pauseFrame >= 34) {
            pauseFrame = 12;
        }
    }
    else nextPauseUpd--;

    SDL_Rect pauseRct = {SCREEN_CENTER_X - 95, SCREEN_CENTER_Y - 72, 190, 143};
    { SDL_FRect fr = ToFRect(pauseRct); SDL_RenderTexture(rend, pausePenguin[pauseFrame], nullptr, &fr); }

    timePaused = SDL_GetTicks();
}

void BubbleGame::HandleMouseAim(float mx, float my) {
    if (!currentSettings.mouseEnabled) return;
    if (currentSettings.playerCount < 1) return;
    BubbleArray& bArr = bubbleArrays[0];
    const SDL_Rect& r = bArr.shooterSprite.rect;
    float sx = r.x + r.w * 0.5f;
    float sy = r.y + r.h * 0.5f;
    float dx = mx - sx;
    float dy = sy - my;  // flip y: up = positive
    // Only aim when mouse is above the shooter barrel (prevent backward shots)
    if (my > sy - 5.f) return;
    float ang = atan2f(dy, dx);
    if (ang < 0.1f) ang = 0.1f;
    if (ang > (float)PI - 0.1f) ang = (float)PI - 0.1f;
    bArr.mouseTargetAngle = ang;
}

void BubbleGame::HandleMouseFire() {
    if (!currentSettings.mouseEnabled) return;
    if (currentSettings.playerCount < 1) return;
    bubbleArrays[0].mouseFirePending = true;
}

void BubbleGame::HandleInput(SDL_Event *e) {
    // Map gamepad/D-pad to keyboard-equivalent actions
    if (e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        if (currentSettings.localMultiplayer) {
            // In local multiplayer, per-player movement is polled directly in UpdatePenguin.
            // Only handle global buttons (B=quit, START=pause) via fake key events.
            // A button also handled when game is finished so players can continue to next round.
            SDL_KeyboardEvent fake{};
            fake.type = SDL_EVENT_KEY_DOWN;
            fake.down = true;
            switch (e->gbutton.button) {
                case SDL_GAMEPAD_BUTTON_EAST:    fake.key = SDLK_ESCAPE; break;
                case SDL_GAMEPAD_BUTTON_START:   fake.key = SDLK_P; break;
                case SDL_GAMEPAD_BUTTON_SOUTH:
                    if (gameFinish) { fake.key = SDLK_SPACE; break; }
                    return;
                default: return;
            }
            SDL_Event fakeEvent;
            fakeEvent.type = SDL_EVENT_KEY_DOWN;
            fakeEvent.key = fake;
            HandleInput(&fakeEvent);
            return;
        }
        SDL_KeyboardEvent fake{};
        fake.type = SDL_EVENT_KEY_DOWN;
        fake.down = true;
        switch (e->gbutton.button) {
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  fake.key = SDLK_LEFT; break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: fake.key = SDLK_RIGHT; break;
            case SDL_GAMEPAD_BUTTON_SOUTH:      fake.key = SDLK_SPACE; break;
            case SDL_GAMEPAD_BUTTON_EAST:       fake.key = SDLK_ESCAPE; break;
            case SDL_GAMEPAD_BUTTON_START:      fake.key = SDLK_P; break;
            case SDL_GAMEPAD_BUTTON_WEST:       fake.key = SDLK_RETURN; break; // X=Chat
            default: return;
        }
        SDL_Event fakeEvent;
        fakeEvent.type = SDL_EVENT_KEY_DOWN;
        fakeEvent.key = fake;
        HandleInput(&fakeEvent);
        return;
    }

    switch(e->type) {
        case SDL_EVENT_TEXT_INPUT:
            if (chattingMode) {
                size_t curLen = strlen(chatInputBuf);
                size_t addLen = strlen(e->text.text);
                if (curLen + addLen < sizeof(chatInputBuf) - 1)
                    strcat(chatInputBuf, e->text.text);
            }
            return;
        case SDL_EVENT_KEY_DOWN:
            // Backspace: allow key-repeat so holding it deletes continuously
            if (chattingMode && e->key.key == SDLK_BACKSPACE) {
                size_t len = strlen(chatInputBuf);
                if (len > 0) chatInputBuf[len - 1] = '\0';
                return;
            }
            if(e->key.repeat) break;
            switch(e->key.key) {
                case SDLK_AC_BACK:
                case SDLK_ESCAPE:
                    if (chattingMode) {
                        chattingMode = false;
                        chatInputBuf[0] = '\0';
                        SDL_StopTextInput(SDL_GetKeyboardFocus());
                        break;
                    }
                    QuitToTitle();
                    break;
                case SDLK_F11: // mute / unpause audio
                    if(audMixer->IsHalted() == true) {
                        audMixer->MuteAll(true);
                        audMixer->PlayMusic("main1p");
                    }
                    else audMixer->MuteAll();
                    break;
                case SDLK_C: // toggle colorblind mode
                    {
                        GameSettings *settings = GameSettings::Instance();
                        bool currentMode = settings->colorBlind();
                        settings->SetValue("GFX:ColorblindBubbles", currentMode ? "false" : "true");
                        SDL_Log("Colorblind mode: %s", currentMode ? "OFF" : "ON");
                    }
                    break;
                // Single player targeting keys (original lines 1681-1690)
                // Keys 1-4 target opponents rp1-rp4, key 0 or 5 clears targeting
                case SDLK_1:
                case SDLK_2:
                case SDLK_3:
                case SDLK_4:
                case SDLK_0:
                    if (currentSettings.networkGame && currentSettings.playerCount >= 3 &&
                        currentSettings.singlePlayerTargetting && !gameFinish) {
                        int target = -1;
                        if (e->key.key == SDLK_1) target = 1;
                        else if (e->key.key == SDLK_2 && currentSettings.playerCount >= 3) target = 2;
                        else if (e->key.key == SDLK_3 && currentSettings.playerCount >= 4) target = 3;
                        else if (e->key.key == SDLK_4 && currentSettings.playerCount >= 5) target = 4;
                        // SDLK_0 stays -1 to clear targeting
                        SetSendMalusToOne(target);
                    }
                    break;
                case SDLK_SPACE:
                    // Fire TV remote A button (mapped to SDLK_SPACE) should also continue
                    // the round when the game is finished — same as SDLK_RETURN below.
                    // During active play SDLK_SPACE is the fire key and is handled elsewhere,
                    // so only fall through here when gameFinish is set.
                    if (gameFinish && singleBubbles.size() == 0)
                        goto handle_return;
                    break;
                case SDLK_RETURN:
                handle_return:
                    // Chat: during active network game, RETURN enters/sends chat
                    if (currentSettings.networkGame && !gameFinish) {
                        if (chattingMode) {
                            if (strlen(chatInputBuf) > 0) {
                                NetworkClient* netClientChat = NetworkClient::Instance();
                                char talkMsg[258];
                                snprintf(talkMsg, sizeof(talkMsg), "t%s", chatInputBuf);
                                netClientChat->SendGameData(talkMsg);
                                // Show our own message locally
                                InGameChatMsg self;
                                self.nick = netClientChat->GetPlayerNick();
                                if (self.nick.empty()) self.nick = "Me";
                                self.text = chatInputBuf;
                                self.framesLeft = 600;
                                inGameChatMessages.push_back(self);
                                if (inGameChatMessages.size() > 10)
                                    inGameChatMessages.erase(inGameChatMessages.begin());
                            }
                            chatInputBuf[0] = '\0';
                            chattingMode = false;
                            SDL_StopTextInput(SDL_GetKeyboardFocus());
                        } else {
                            chattingMode = true;
                            chatInputBuf[0] = '\0';
                            SDL_StartTextInput(SDL_GetKeyboardFocus());
                        }
                        break;
                    }
                    if (!gameFinish || (gameFinish && singleBubbles.size() > 0)) break;

                    // In network game, synchronize new game with opponent
                    if (currentSettings.networkGame) {
                        // If match is over (victories limit reached), return to lobby
                        if (gameMatchOver) {
                            SDL_Log("Match over - victories limit reached, returning to lobby");
                            QuitToTitle();
                            break;
                        }
                        // If all opponents have disconnected, no one to play with - return to lobby
                        if (connectedPlayerCount <= 1) {
                            SDL_Log("No connected opponents remain - returning to lobby");
                            QuitToTitle();
                            break;
                        }
                        // Send 'n' and start waiting - render loop will start game when both ready
                        // Guard: don't send 'n' again if already waiting (prevents double-counting at peers)
                        if (!waitingForOpponentNewGame) {
                            SDL_Log("Sending newgame signal 'n' to opponent (gameFinish=%d, gameWon=%d, gameLost=%d)",
                                    gameFinish, gameWon, gameLost);
                            NetworkClient* netClient = NetworkClient::Instance();
                            if (netClient->IsConnected() && netClient->GetState() == IN_GAME) {
                                netClient->SendGameData("n");
                                waitingForOpponentNewGame = true;
                                // Don't reset opponentsReadyCount here — 'n' messages from peers may have
                                // already arrived before we pressed ENTER, and resetting would lose them.
                                // The count is reset in the render loop when the new game actually starts.
                                SDL_Log("Waiting for all opponents to be ready (already have %d/%d)...",
                                        opponentsReadyCount, connectedPlayerCount - 1);
                            }
                        }
                    } else {
                        // Single player or local multiplayer
                        if (currentSettings.mpTraining && mpTrainDone) {
                            // Training ended: show highscores or return to title
                            if (pendingHighscore) {
                                pendingHighscore = false;
                                HighscoreManager::Instance()->ShowNewScorePanel(0);
                                HighscoreManager::Instance()->ShowScoreScreen(0);
                            } else {
                                QuitToTitle();
                            }
                        } else if (gameWon || gameMpDone) {
                            ++curLevel;
                            // If a highscore was earned and this would end the game, show score screen first
                            bool willEnd = (curLevel >= (int)loadedLevels.size() && !currentSettings.randomLevels);
                            if (willEnd && pendingHighscore) {
                                pendingHighscore = false;
                                HighscoreManager::Instance()->ShowNewScorePanel(0);
                                HighscoreManager::Instance()->ShowScoreScreen(0);
                            } else {
                                ReloadGame(curLevel);
                            }
                        } else if (gameLost) {
                            ReloadGame(curLevel);
                        }
                    }
                    break;
            }
            break;
    }
}

void BubbleGame::QuitToTitle() {
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
            netClient->PartGame();  // Notify server we left
        }
        FrozenBubble::Instance()->CallNetLobbyReturn();
    } else {
        FrozenBubble::Instance()->CallMenuReturn();
    }
    firstRenderDone = false;
}

void BubbleGame::SendNetworkBubbleShot(BubbleArray &bArray) {
    if (!currentSettings.networkGame) return;

    NetworkClient* netClient = NetworkClient::Instance();
    if (!netClient->IsConnected() || netClient->GetState() != IN_GAME) {
        SDL_Log("SendNetworkBubbleShot: Not connected or not in game (connected=%d, state=%d)",
                netClient->IsConnected(), netClient->GetState());
        return;
    }

    // Send shot in original protocol format: f{angle}:{nextcolor}
    // The color sent is the player's NEW next bubble (what will come after current)
    // This matches original frozen-bubble line 2163: gsend(sprintf("f%.3f:$pdata{$::p}{nextcolor}", $angle{$::p}))
    for (const SingleBubble &sBubble : singleBubbles) {
        if (sBubble.launching && sBubble.assignedArray == bArray.playerAssigned) {
            char shotData[128];
            snprintf(shotData, sizeof(shotData), "f%.3f:%d",
                sBubble.direction,
                bArray.nextBubble);  // Send the NEW next bubble color, not the launched bubble's color
            SDL_Log("Sending shot: angle=%.3f, nextBubble=%d (launched=%d)",
                    sBubble.direction, bArray.nextBubble, sBubble.bubbleId);
            netClient->SendGameData(shotData);
            break;
        }
    }
}

void BubbleGame::ProcessNetworkMessages() {
    NetworkClient* netClient = NetworkClient::Instance();
    if (!netClient->IsConnected()) {
        static int notConnectedCount = 0;
        if (notConnectedCount++ % 60 == 0) {  // Log once per second at 60fps
            SDL_Log("ProcessNetworkMessages: Not connected (state=%d)", netClient->GetState());
        }
        return;
    }

    // Update network client
    netClient->Update();

    // Process all pending messages
    while (netClient->HasMessage()) {
        std::string msg = netClient->GetNextMessage();

        // Parse network game messages in format GAMEMSG:{senderId}:{data}
        if (msg.find("GAMEMSG:") == 0) {
            int senderId;
            char gameData[512];
            if (sscanf(msg.c_str(), "GAMEMSG:%d:%511[^\n]", &senderId, gameData) == 2) {
                SDL_Log("Processing game message from player %d: %s", senderId, gameData);

                // Ignore our own messages echoed back
                if (senderId == netClient->GetMyPlayerId()) {
                    SDL_Log("Ignoring our own message (ID=%d)", senderId);
                    continue;
                }

                // Parse game data - original protocol (first character is message type)
                char msgType = gameData[0];
                switch (msgType) {
                    case 'f': {
                        // Fire: f{angle}:{nextcolor}
                        // The color in message is opponent's NEW next bubble (after their current shot)
                        // Create their shot with their CURRENT bubble, then update their next
                        // This matches original frozen-bubble line 1404: ($angle{$player}, $pdata{$player}{nextcolor}) = $params
                        float angle;
                        int opponentNewNextColor;
                        if (sscanf(gameData + 1, "%f:%d", &angle, &opponentNewNextColor) == 2) {
                            // Find which player array this sender is using (original: $actions{$player}{mp_fire} = 1)
                            int opponentIdx = -1;
                            for (int i = 0; i < currentSettings.playerCount; i++) {
                                if (bubbleArrays[i].lobbyPlayerId == senderId) {
                                    opponentIdx = i;
                                    break;
                                }
                            }

                            // If not found, assign to next available remote slot
                            if (opponentIdx == -1) {
                                NetworkClient* netClient = NetworkClient::Instance();
                                for (int i = 1; i < currentSettings.playerCount; i++) {
                                    if (bubbleArrays[i].lobbyPlayerId == -1) {
                                        bubbleArrays[i].lobbyPlayerId = senderId;
                                        bubbleArrays[i].playerNickname = netClient->GetPlayerNickname(senderId);
                                        opponentIdx = i;
                                        SDL_Log("'f' message: Assigned lobbyId %d (nick='%s') to player array %d",
                                                senderId, bubbleArrays[i].playerNickname.c_str(), i);
                                        break;
                                    }
                                }
                            }

                            if (opponentIdx < 0) {
                                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not find/assign player for 'f' message from senderId %d", senderId);
                                break;
                            }

                            BubbleArray &opponentArray = bubbleArrays[opponentIdx];

                            // Set flag to fire in the game loop (original line 1403: $actions{$player}{mp_fire} = 1)
                            // Store angle and update nextcolor (original line 1404)
                            opponentArray.mpFirePending = true;
                            opponentArray.pendingAngle = angle;
                            opponentArray.shooterSprite.angle = angle;  // Update shooter angle for visual display
                            opponentArray.nextBubble = opponentNewNextColor;  // Update their next bubble color

                            SDL_Log("Received fire command from player %d (array %d): angle=%.3f, nextColor=%d - will fire in game loop",
                                    senderId, opponentIdx, angle, opponentNewNextColor);
                        } else {
                            SDL_Log("ERROR: Failed to parse fire message: %s", gameData);
                        }
                        break;
                    }
                    case 'p': {
                        // Ping - ignore (keepalive only)
                        break;
                    }
                    case 'n': {
                        // Newgame - an opponent is ready for next round
                        opponentsReadyCount++;
                        SDL_Log("Opponent ready for new game (received 'n'), count=%d/%d",
                                opponentsReadyCount, connectedPlayerCount - 1);
                        if (opponentsReadyCount >= connectedPlayerCount - 1) {
                            opponentReadyForNewGame = true;
                        }

                        // Auto-respond for ALL player counts (original: receiving 'n' triggers mp_newgame->()
                        // which immediately sends our own 'n' without requiring local keypress).
                        // This means only ONE player needs to press a key; everyone else auto-responds.
                        if (!waitingForOpponentNewGame && gameFinish && !gameMatchOver) {
                            SDL_Log("Opponent pressed key first - auto-sending 'n' (%dP game)", currentSettings.playerCount);
                            NetworkClient* netClient = NetworkClient::Instance();
                            if (netClient->IsConnected() && netClient->GetState() == IN_GAME) {
                                netClient->SendGameData("n");
                                waitingForOpponentNewGame = true;
                            }
                        }
                        break;
                    }
                    case 's': {
                        // Stick: s{cx}:{cy}:{bubbleColor}:{nc0} {nc1} ... {nc7}
                        // Perl format: "s$cx:$cy:$col:@{$pdata{$::p}{nextcolors}}" (space-sep 8 colors)
                        // Opponent's bubble has stuck - place it at exact transmitted position
                        // Format matches original: frozen-bubble line 1418-1425
                        int cx, cy, bubbleColor;
                        if (sscanf(gameData + 1, "%d:%d:%d", &cx, &cy, &bubbleColor) == 3) {
                            // Parse nextColors: find the 3rd colon, then read space-separated ints
                            std::vector<int> recvNextColors;
                            const char* p = gameData + 1;
                            int colons = 0;
                            while (*p && colons < 3) { if (*p == ':') colons++; p++; }
                            while (*p) {
                                int c; int consumed = 0;
                                if (sscanf(p, "%d%n", &c, &consumed) == 1) {
                                    recvNextColors.push_back(c);
                                    p += consumed;
                                    while (*p == ' ') p++;
                                } else break;
                            }
                            int nextBubble = recvNextColors.empty() ? 0 : recvNextColors[0];
                            SDL_Log("Received stick: col=%d row=%d color=%d nextColors[%zu] from lobbyId=%d", cx, cy, bubbleColor, recvNextColors.size(), senderId);

                            // Find or assign this remote player's array
                            int opponentIdx = -1;
                            for (int i = 0; i < currentSettings.playerCount; i++) {
                                if (bubbleArrays[i].lobbyPlayerId == senderId) {
                                    opponentIdx = i;
                                    break;
                                }
                            }

                            // If not found, assign to next available remote slot
                            if (opponentIdx == -1) {
                                NetworkClient* netClient = NetworkClient::Instance();
                                for (int i = 1; i < currentSettings.playerCount; i++) {
                                    if (bubbleArrays[i].lobbyPlayerId == -1) {
                                        bubbleArrays[i].lobbyPlayerId = senderId;
                                        bubbleArrays[i].playerNickname = netClient->GetPlayerNickname(senderId);
                                        opponentIdx = i;
                                        SDL_Log("'s' message: Assigned lobbyId %d (nick='%s') to player array %d",
                                                senderId, bubbleArrays[i].playerNickname.c_str(), i);
                                        break;
                                    }
                                }
                            }

                            if (opponentIdx < 0) {
                                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not find/assign player for senderId %d", senderId);
                                break;
                            }

                            BubbleArray &opponentArray = bubbleArrays[opponentIdx];

                            // Set flag for stick to be processed in game loop (original line 1422: $actions{$player}{mp_stick} = 1)
                            // Store stick data (original line 1423)
                            opponentArray.mpStickPending = true;
                            opponentArray.stickCx = cx;
                            opponentArray.stickCy = cy;
                            opponentArray.stickCol = bubbleColor;
                            opponentArray.nextBubble = nextBubble;  // Update their next bubble (front of nextColors)
                            // Sync full nextColors queue (Perl-compatible: used by ExpandNewLane for new root row)
                            if (!recvNextColors.empty()) {
                                opponentArray.nextColors = recvNextColors;
                            }

                            SDL_Log("Set mp_stick flag for player %d (array %d): cx=%d cy=%d col=%d nextBubble=%d nextColors[%zu]",
                                    senderId, opponentIdx, cx, cy, bubbleColor, nextBubble, recvNextColors.size());
                        } else {
                            SDL_Log("ERROR: Failed to parse stick message: %s", gameData);
                        }
                        break;
                    }
                    case 'g': {
                        // Receive malus attack from opponent
                        // Format: g{destPlayerNick}:{count}
                        // Original at line 1425-1432
                        char destNick[64];
                        int malusCount;
                        if (sscanf(gameData + 1, "%63[^:]:%d", destNick, &malusCount) == 2) {
                            // Only process if this message is for us (original line 1428)
                            NetworkClient* netClient = NetworkClient::Instance();
                            std::string myNick = netClient ? netClient->GetPlayerNick() : "";
                            SDL_Log("'g' message: dest='%s' count=%d myNick='%s' senderId=%d myId=%d",
                                    destNick, malusCount, myNick.c_str(), senderId,
                                    netClient ? netClient->GetMyPlayerId() : -1);

                            if (netClient && myNick == destNick) {
                                SDL_Log("  -> YES, this malus is FOR ME! Adding to my queue");


                                // Add to local player's malus queue (opponent attacks us, so array 0)
                                // Store current frame number for each malus
                                for (int i = 0; i < malusCount; i++) {
                                    bubbleArrays[0].malusQueue.push_back(frameCount);
                                }
                            } else {
                                SDL_Log("  -> NO, not for me (dest='%s' != myNick='%s'), IGNORING",
                                        destNick, myNick.c_str());
                            }
                        } else {
                            SDL_Log("ERROR: Failed to parse malus message: %s", gameData);
                        }
                        break;
                    }
                    case 'm': {
                        // Receive malus bubble from opponent (they generated it, we display it)
                        // Format: m{bubbleId}:{cx}:{cy}:{stick_y}
                        // Original at line 1435-1451
                        // Skip our own 'm' messages echoed back by server (original: only process from others)
                        {
                            NetworkClient* netClientM = NetworkClient::Instance();
                            if (netClientM && (int)netClientM->GetMyPlayerId() == senderId) {
                                SDL_Log("Ignoring own 'm' echo from server");
                                break;
                            }
                        }
                        int bubbleId, cx, cy, stickY;
                        if (sscanf(gameData + 1, "%d:%d:%d:%d", &bubbleId, &cx, &cy, &stickY) == 4) {
                            SDL_Log("Received opponent's malus bubble from senderId=%d: color=%d cx=%d cy=%d stickY=%d",
                                    senderId, bubbleId, cx, cy, stickY);

                            // Find which array this opponent belongs to
                            int opponentIdx = -1;
                            for (int i = 0; i < currentSettings.playerCount; i++) {
                                if (bubbleArrays[i].lobbyPlayerId == senderId) {
                                    opponentIdx = i;
                                    break;
                                }
                            }

                            if (opponentIdx < 0) {
                                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                           "Received 'm' message from unknown senderId %d, ignoring", senderId);
                                break;
                            }

                            BubbleArray &opponentArray = bubbleArrays[opponentIdx];
                            // Mini players use half bubble size
                            bool isMini = (currentSettings.playerCount >= 3 && opponentIdx >= 1);
                            int bubbleSize = isMini ? 16 : 32;
                            int rowSize = bubbleSize * 7 / 8;  // 14 for mini, 28 for full
                            int smallerSep = (cy % 2 == 0) ? 0 : bubbleSize / 2;
                            float startX = (smallerSep + bubbleSize * cx) + opponentArray.bubbleOffset.x;
                            float startY = (rowSize * cy) + opponentArray.bubbleOffset.y;

                            MalusBubble malus = {
                                opponentIdx,  // opponent's array index
                                bubbleId,
                                cx, cy,
                                stickY,
                                startX, startY,
                                {(int)startX, (int)startY},
                                false,
                                false
                            };

                            malusBubbles.push_back(malus);
                        } else {
                            SDL_Log("ERROR: Failed to parse malus bubble message: %s", gameData);
                        }
                        break;
                    }
                    case 'M': {
                        // Opponent's malus bubble stuck
                        // Format: M{cx}:{stick_y}
                        // Original at line 1453-1466
                        int cx, stickY;
                        if (sscanf(gameData + 1, "%d:%d", &cx, &stickY) == 2) {
                            SDL_Log("Opponent's malus bubble stuck from senderId=%d: cx=%d stickY=%d",
                                    senderId, cx, stickY);

                            // Find which array this opponent belongs to
                            int opponentIdx = -1;
                            for (int i = 0; i < currentSettings.playerCount; i++) {
                                if (bubbleArrays[i].lobbyPlayerId == senderId) {
                                    opponentIdx = i;
                                    break;
                                }
                            }

                            if (opponentIdx < 0) {
                                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                           "Received 'M' message from unknown senderId %d, ignoring", senderId);
                                break;
                            }

                            // Find and stick the corresponding malus bubble on opponent's board
                            for (auto &malus : malusBubbles) {
                                if (malus.assignedArray == opponentIdx && malus.cx == cx && malus.stickY == stickY && !malus.shouldClear) {
                                    SDL_Log("Found opponent's malus bubble to stick on array %d", opponentIdx);
                                    BubbleArray &opponentArray = bubbleArrays[opponentIdx];
                                    opponentArray.PlacePlayerBubble(malus.bubbleId, stickY, cx);
                                    opponentArray.newShoot = true;
                                    malus.shouldClear = true;
                                    CheckPossibleDestroy(opponentArray);
                                    // Don't check game state for opponent - they will send 'F' message if they win/lose
                                    break;
                                }
                            }
                        } else {
                            SDL_Log("ERROR: Failed to parse malus stick message: %s", gameData);
                        }
                        break;
                    }
                    case 'F': {
                        // Finish/Win notification from remote player
                        // Perl format: "F{winnerNick}" (no separator) - original line 1467-1470
                        // Also handle legacy C++ format "F:{idx}" for backward compat
                        std::string winnerNick = gameData + 1;  // Everything after 'F'
                        SDL_Log("Received win notification: F'%s'", winnerNick.c_str());

                        int winnerPlayer = -1;

                        // Try legacy format first: "F:{digit}"
                        if (winnerNick.size() >= 2 && winnerNick[0] == ':' && isdigit((unsigned char)winnerNick[1])) {
                            winnerPlayer = winnerNick[1] - '0';
                        } else {
                            // Perl format: match nick to player arrays
                            NetworkClient* netClient = NetworkClient::Instance();
                            for (int i = 0; i < currentSettings.playerCount; i++) {
                                if (bubbleArrays[i].playerNickname == winnerNick) {
                                    winnerPlayer = i;
                                    break;
                                }
                            }
                            // If nick matches our own nick, winner is local player (array 0)
                            if (winnerPlayer == -1 && netClient && netClient->GetPlayerNick() == winnerNick) {
                                winnerPlayer = 0;
                            }
                        }

                        if (winnerPlayer >= 0 && winnerPlayer < currentSettings.playerCount) {
                            // Guard: only process the first 'F' per round (multiple clients may send it)
                            if (!gameFinish) {
                                gameFinish = true;
                                bubbleArrays[winnerPlayer].mpWinner = true;
                                bubbleArrays[winnerPlayer].penguinSprite.PlayAnimation(10);

                                if (winnerPlayer == 0) {
                                    winsP1++;
                                    SDL_Log("Win counter updated: We won! winsP1=%d", winsP1);
                                } else {
                                    winsP2++;
                                    SDL_Log("Win counter updated: Opponent won! winsP2=%d", winsP2);
                                }
                                bubbleArrays[winnerPlayer].winCount++;
                                Update2PText();
                                panelRct = {SCREEN_CENTER_X - 173, 480 - 289, 329, 159};

                                // Check victories limit
                                if (currentSettings.victoriesLimit > 0 &&
                                    bubbleArrays[winnerPlayer].winCount >= currentSettings.victoriesLimit) {
                                    gameMatchOver = true;
                                    SDL_Log("Match over! Player %d reached %d victories",
                                            winnerPlayer, currentSettings.victoriesLimit);
                                }
                            }
                        } else {
                            SDL_Log("ERROR: Could not identify winner from F message: '%s'", winnerNick.c_str());
                        }
                        break;
                    }
                    case 't': {
                        // In-game chat from remote player
                        InGameChatMsg chatMsg;
                        chatMsg.nick = netClient->GetPlayerNickname(senderId);
                        if (chatMsg.nick.empty()) chatMsg.nick = "Player";
                        chatMsg.text = gameData + 1;
                        chatMsg.framesLeft = 600;  // 10 seconds at 60 fps
                        inGameChatMessages.push_back(chatMsg);
                        if (inGameChatMessages.size() > 10)
                            inGameChatMessages.erase(inGameChatMessages.begin());
                        audMixer->PlaySFX("chatted");
                        break;
                    }
                    case 'l': {
                        // Player-left notification: a remote player disconnected mid-game
                        SDL_Log("Received player-left ('l') from lobby player ID %d", senderId);

                        // Track that one fewer player is connected (for new-game sync threshold)
                        if (connectedPlayerCount > 1) connectedPlayerCount--;
                        SDL_Log("connectedPlayerCount now %d", connectedPlayerCount);

                        // Find which player array this senderId corresponds to
                        int playerIdx = -1;
                        for (int i = 0; i < currentSettings.playerCount; i++) {
                            if (bubbleArrays[i].lobbyPlayerId == senderId) {
                                playerIdx = i;
                                break;
                            }
                        }

                        if (playerIdx >= 0) {
                            SDL_Log("Marking player array %d (lobbyId=%d) as LEFT (disconnected)", playerIdx, senderId);
                            bubbleArrays[playerIdx].playerState = BubbleArray::PlayerState::LEFT;
                            bubbleArrays[playerIdx].penguinSprite.PlayAnimation(11);

                            // Clear targeting if targeted player died (original: set_sendmalustoone(undef) at line 1947)
                            if (sendMalusToOne == playerIdx) {
                                SetSendMalusToOne(-1);
                            }
                            // Remove from attackingMe if they were targeting us
                            attackingMe.erase(std::remove(attackingMe.begin(), attackingMe.end(), playerIdx),
                                              attackingMe.end());

                            // Check if we have a winner now
                            int livingCount = CountLivingPlayers();
                            SDL_Log("After remote disconnect: %d players alive", livingCount);

                            if (livingCount == 1) {
                                // Find winner
                                for (int w = 0; w < currentSettings.playerCount; w++) {
                                    if (bubbleArrays[w].playerState == BubbleArray::PlayerState::ALIVE) {
                                        SDL_Log("Winner found: player %d", w);
                                        gameFinish = true;
                                        bubbleArrays[w].mpWinner = true;
                                        bubbleArrays[w].penguinSprite.PlayAnimation(10);

                                        if (w == 0) winsP1++;
                                        else winsP2++;
                                        bubbleArrays[w].winCount++;
                                        Update2PText();
                                        break;
                                    }
                                }
                            }
                        } else {
                            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                       "Received player-left from unknown player ID %d", senderId);
                        }

                        // If already waiting for new-game sync, check if the reduced threshold is now met
                        // (the disconnected player will never send 'n', so count them as ready)
                        if (waitingForOpponentNewGame && opponentsReadyCount >= connectedPlayerCount - 1) {
                            SDL_Log("All remaining connected opponents ready after disconnect - starting new game");
                            opponentReadyForNewGame = true;
                        }
                        break;
                    }
                    case 'A': {
                        // Opponent changed their targeting (original: command 'A' at line 1477)
                        // Format: A{targetNick} = opponent is targeting that player
                        //         A (empty)     = opponent cleared targeting
                        // Find which array the sender belongs to
                        int senderIdx = -1;
                        for (int i = 0; i < currentSettings.playerCount; i++) {
                            if (bubbleArrays[i].lobbyPlayerId == senderId) {
                                senderIdx = i;
                                break;
                            }
                        }
                        if (senderIdx < 0) break;

                        NetworkClient* netClient = NetworkClient::Instance();
                        std::string myNick = netClient ? netClient->GetPlayerNick() : "";
                        const char* targetNick = gameData + 1;  // Skip 'A' prefix

                        if (strlen(targetNick) == 0 || myNick != targetNick) {
                            // Opponent cleared target or is targeting someone else - remove from attackingMe
                            attackingMe.erase(std::remove(attackingMe.begin(), attackingMe.end(), senderIdx),
                                              attackingMe.end());
                        } else {
                            // Opponent is targeting us (targetNick == myNick)
                            if (std::find(attackingMe.begin(), attackingMe.end(), senderIdx) == attackingMe.end()) {
                                attackingMe.push_back(senderIdx);
                            }
                        }
                        // Track all players' targets (not just who's targeting me)
                        if (strlen(targetNick) == 0) {
                            playerTargeting[senderIdx] = -1;  // cleared
                        } else {
                            // Find which array has targetNick
                            int targetIdx = -1;
                            for (int i = 0; i < currentSettings.playerCount; i++) {
                                if (bubbleArrays[i].playerNickname == targetNick) { targetIdx = i; break; }
                            }
                            playerTargeting[senderIdx] = targetIdx;
                        }
                        SDL_Log("'A' message: sender=%d targetNick='%s' myNick='%s' attackingMe.size=%zu",
                                senderIdx, targetNick, myNick.c_str(), attackingMe.size());
                        break;
                    }
                    default:
                        SDL_Log("Unknown game message type: %c", msgType);
                        break;
                    }
            }
        } else if (msg.find("GAME_START") == 0) {
            SDL_Log("Network game starting!");
        } else if (msg.find("PLAYER_PART") == 0) {
            SDL_Log("Opponent left the game");
        }
    }
}