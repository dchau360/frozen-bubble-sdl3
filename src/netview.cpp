// src/netview.cpp
#include "netview.h"

int NetViewPageCount(int playerCount) {
    if (playerCount <= 5) return 1;
    return ((playerCount - 1) + 3) / 4;
}

bool NetViewBoardVisible(int playerCount, int page, int boardIndex) {
    if (boardIndex == 0 || playerCount <= 5) return true;
    return ((boardIndex - 1) / 4) == page;
}

// Rank of a board within its slot class; higher wins the slot.
static int NetViewScore(int boardIndex, int localTarget, const NetViewBoardState &s) {
    if (!s.alive) return 0;  // dead/left boards fill a slot only when nothing living exists
    if (boardIndex == localTarget) return 4;
    if (s.targetingMe) return 3;
    if (s.inDanger) return 2;
    return 1;
}

std::array<int, 4> RankNetViewBoards(int playerCount, int localTarget,
                                     const NetViewBoardState *states) {
    std::array<int, 4> picks = {-1, -1, -1, -1};
    std::array<int, 4> pickScore = {-1, -1, -1, -1};
    std::array<bool, 4> pickVisible = {false, false, false, false};
    for (int i = 1; i < playerCount; i++) {
        int cls = (i - 1) % 4;
        int score = NetViewScore(i, localTarget, states[i]);
        bool wins;
        if (picks[cls] == -1) {
            wins = true;
        } else if (states[i].visible && !pickVisible[cls]) {
            // Stickiness: the currently visible board keeps its slot unless the
            // incumbent pick strictly outranks it.
            wins = score >= pickScore[cls];
        } else {
            // Covers non-visible challengers against a visible incumbent (must
            // strictly outrank) and plain ties (lower index -- earlier i -- keeps).
            wins = score > pickScore[cls];
        }
        if (wins) {
            picks[cls] = i;
            pickScore[cls] = score;
            pickVisible[cls] = states[i].visible;
        }
    }
    return picks;
}
