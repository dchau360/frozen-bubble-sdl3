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

#ifndef BUBBLEGAME_INTERNAL_H
#define BUBBLEGAME_INTERNAL_H

// Internal shared declarations for the bubblegame_*.cpp translation units.
// Not part of the public BubbleGame interface (see bubblegame.h).

#include "frozenbubble.h"
#include "bubblegame.h"
#include "audiomixer.h"

#include <string>
#include <utility>
#include <vector>

inline int ranrange(int a, int b) { return a + rand() % ((b - a ) + 1); }
inline float ranrange(float b) { return rand() / (static_cast<float>(RAND_MAX) / b); }

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

// Free-flying bubbles (launched, falling, exploding) and pending malus
// bubbles, shared across the bubblegame_*.cpp translation units.
extern std::vector<SingleBubble> singleBubbles;
extern std::vector<MalusBubble> malusBubbles;

// oddswap mirrors Perl's $pdata{$player}{oddswap}:
//   0 = standard (row 0 has 8 cells, no hex offset)
//   1 = flipped  (row 0 has 7 cells, 16px hex offset)
// Original: next_positions() uses even($b->{cy}+$pdata{$player}{oddswap})
// to select which offset direction to use for rows above/below.
inline std::vector<std::pair<int,int>> GridNeighborOffsets(int row, int oddswap = 0) {
    if ((row + oddswap) % 2 == 0)
        return {{-1,-1}, {-1,0}, {0,-1}, {0,1}, {1,-1}, {1,0}};
    else
        return {{-1,0}, {-1,1}, {0,-1}, {0,1}, {1,0}, {1,1}};
}

// Display name for player `idx` in stats/summary output (bubblegame_render.cpp).
std::string StatsPlayerName(const BubbleArray &arr, int idx, bool networkGame);

// Clears all shining/prelight flags on a board (bubblegame_board.cpp).
void ResetPrelight(BubbleArray &bArray);

#endif // BUBBLEGAME_INTERNAL_H
