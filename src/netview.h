// src/netview.h
#ifndef NETVIEW_H
#define NETVIEW_H

#include <array>

// Pure view-selection logic for the >5-player battle royale mini-board slots.
// No SDL, no BubbleGame state -- unit-tested by tests/netview_test.cpp.

// Number of manual pages of 4 remote boards (1 for playerCount <= 5).
int NetViewPageCount(int playerCount);

// Whether board `boardIndex` is visible on manual page `page`. Board 0 (the
// local player's center board) is always visible; for playerCount <= 5 every
// board is visible regardless of page.
bool NetViewBoardVisible(int playerCount, int page, int boardIndex);

// Per-board inputs to the auto ranker, indexed by board (0 = local player;
// index 0 is ignored -- the center board is always shown).
struct NetViewBoardState {
    bool alive = false;        // playerState == ALIVE
    bool targetingMe = false;  // this player is targeting the local player
    bool inDanger = false;     // any bubble with cy > 11
    bool visible = false;      // currently shown (stickiness input)
};

// Auto mode: picks the board to show in each of the 4 parked slot classes
// (class of board i = (i - 1) % 4; boards are parked permanently at phase-1
// geometries, so at most one board per class can be shown). Returns one board
// index per class, -1 for a class with no boards. Priority within a class:
// the local player's target > boards targeting the local player > living
// boards in danger > living boards > dead boards; ties go to the lowest
// index. The currently visible board in a class keeps its slot unless
// strictly outranked.
std::array<int, 4> RankNetViewBoards(int playerCount, int localTarget,
                                     const NetViewBoardState *states);

#endif // NETVIEW_H
