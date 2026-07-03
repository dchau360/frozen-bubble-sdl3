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
#include "bubblegame_internal.h"

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
    // Mini players use half bubble size for spacing AND mini textures.
    // Original: local $BUBBLE_SIZE = $BUBBLE_SIZE / 2; local $ROW_SIZE = $ROW_SIZE / 2;
    // (iter_players + mini_graphics() in bin/frozen-bubble, applied to every
    // real_stick_bubble() call for rp* players). Mirrors SyncNetworkLevel's
    // effectiveBubbleSize, which already does this for network games.
    bool isMini = (currentSettings.playerCount >= 3 && bArray.playerAssigned >= 1);
    int bubbleSize = isMini ? 16 : 32;
    int rowSize = bubbleSize * 7 / 8;  // ROW_SIZE = 28 (14 for mini)

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
