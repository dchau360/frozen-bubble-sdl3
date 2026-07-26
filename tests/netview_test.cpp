// Unit tests for the pure >5-player netview helpers (no SDL required).
//
// The repo's default build type is Release (-DNDEBUG), which would silently
// strip every assert() below and (worse) turn each "auto p = ..." into an
// unused-variable warning under -Wall -Wextra -pedantic. Force asserts to
// stay active regardless of build type so this test actually verifies
// behavior and compiles warning-free.
#undef NDEBUG
#include "netview.h"
#include <cassert>
#include <cstdio>

static NetViewBoardState S(bool alive, bool targetingMe = false,
                           bool inDanger = false, bool visible = false) {
    NetViewBoardState s;
    s.alive = alive;
    s.targetingMe = targetingMe;
    s.inDanger = inDanger;
    s.visible = visible;
    return s;
}

int main() {
    // --- NetViewPageCount ---
    assert(NetViewPageCount(2) == 1);
    assert(NetViewPageCount(5) == 1);
    assert(NetViewPageCount(6) == 2);   // 5 remote boards -> 2 pages of 4
    assert(NetViewPageCount(9) == 2);   // 8 remote boards -> 2 pages
    assert(NetViewPageCount(10) == 3);  // 9 remote boards -> 3 pages
    assert(NetViewPageCount(20) == 5);  // 19 remote boards -> 5 pages

    // --- NetViewBoardVisible ---
    for (int i = 0; i < 5; i++) assert(NetViewBoardVisible(5, 0, i));  // <=5: all visible
    assert(NetViewBoardVisible(20, 0, 0));   // center board: always visible
    assert(NetViewBoardVisible(20, 3, 0));
    assert(NetViewBoardVisible(20, 0, 1));
    assert(NetViewBoardVisible(20, 0, 4));
    assert(!NetViewBoardVisible(20, 0, 5));
    assert(NetViewBoardVisible(20, 1, 5));
    assert(NetViewBoardVisible(20, 1, 8));
    assert(!NetViewBoardVisible(20, 1, 9));
    assert(NetViewBoardVisible(20, 4, 17));
    assert(NetViewBoardVisible(20, 4, 19));
    assert(NetViewBoardVisible(6, 1, 5));    // n=6: board 5 lives on page 1
    assert(!NetViewBoardVisible(6, 0, 5));

    // --- RankNetViewBoards ---
    {   // 6 players, nothing special: lowest-index living board per class
        NetViewBoardState st[6] = {S(true), S(true), S(true), S(true), S(true), S(true)};
        auto p = RankNetViewBoards(6, -1, st);
        assert(p[0] == 1 && p[1] == 2 && p[2] == 3 && p[3] == 4);  // 5 shares class 0, loses tie to 1
    }
    {   // the local target outranks a lower index in the same class
        NetViewBoardState st[6] = {S(true), S(true), S(true), S(true), S(true), S(true)};
        auto p = RankNetViewBoards(6, 5, st);
        assert(p[0] == 5);
    }
    {   // attacker > danger > plain alive; target > attacker (class 0 = boards 1,5,9,13,17)
        NetViewBoardState st[20] = {};
        for (int i = 0; i < 20; i++) st[i] = S(true);
        st[5].inDanger = true;
        auto pDanger = RankNetViewBoards(20, -1, st);
        assert(pDanger[0] == 5);            // danger beats plain alive
        st[9].targetingMe = true;
        auto pAttacker = RankNetViewBoards(20, -1, st);
        assert(pAttacker[0] == 9);          // attacker beats danger
        auto pTarget = RankNetViewBoards(20, 13, st);
        assert(pTarget[0] == 13);           // target beats attacker
    }
    {   // stickiness: visible board keeps its slot on ties, loses to strictly higher rank
        NetViewBoardState st[20] = {};
        for (int i = 0; i < 20; i++) st[i] = S(true);
        st[17].visible = true;              // class 0 currently shows board 17
        auto p = RankNetViewBoards(20, -1, st);
        assert(p[0] == 17);                 // 1/5/9/13 tie with 17, visible keeps slot
        st[9].inDanger = true;              // strictly outranks a plain-alive visible board
        auto p2 = RankNetViewBoards(20, -1, st);
        assert(p2[0] == 9);
    }
    {   // dead boards fill a class only when it has no living candidates
        NetViewBoardState st[6] = {S(true), S(false), S(true), S(true), S(true), S(true)};
        auto p = RankNetViewBoards(6, -1, st);
        assert(p[0] == 5);                  // living 5 beats dead 1
        st[5].alive = false;
        auto p2 = RankNetViewBoards(6, -1, st);
        assert(p2[0] == 1);                 // both dead -> lowest index
    }
    {   // a dead target does not outrank living boards
        NetViewBoardState st[6] = {S(true), S(false), S(true), S(true), S(true), S(true)};
        auto p = RankNetViewBoards(6, 1, st);
        assert(p[0] == 5);
    }
    {   // empty class returns -1 (3 remote boards -> class 3 empty)
        NetViewBoardState st[4] = {S(true), S(true), S(true), S(true)};
        auto p = RankNetViewBoards(4, -1, st);
        assert(p[0] == 1 && p[1] == 2 && p[2] == 3 && p[3] == -1);
    }

    printf("netview-test: all assertions passed\n");
    return 0;
}
