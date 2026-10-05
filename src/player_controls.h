#ifndef PLAYER_CONTROLS_H
#define PLAYER_CONTROLS_H

// Resolved per-frame shooter intent -- keyboard/gamepad/mouse/touch/bot,
// whichever a player used, collapsed to one small record so a future replay
// can drive ApplyPlayerControls() from a recorded value instead of live
// devices/AI. See docs/REPLAY_PLAN.md / docs/REPLAY_PROGRESS.md.
struct PlayerControls {
    bool left = false, right = false, center = false, fire = false;
    // This frame's fire came from a mouse/touch event specifically (as
    // opposed to a held keyboard/gamepad button or a hurry-forced shot) --
    // needed downstream for scoring/badge input-method classification.
    bool firedByMouse = false;
    // >=0: mouse/touch aim active this frame. Same sentinel as
    // BubbleArray::mouseTargetAngle, which this is captured from.
    float mouseAngle = -1.f;
    // This frame's fire is the swap button (Keys:P1FireNext, a right click,
    // a low touch): with the pocket empty it pockets the loaded bubble and
    // shoots nothing; with a bubble pocketed it fires that one and pockets
    // the loaded one. Only meaningful with fire set.
    bool fireNext = false;
    // A v2.4.140 replay's skip shot (StepRecord::fire == 2): fire the next
    // bubble and keep the loaded one, the rule that build had. Only ever set
    // by ReplayPlayer, so an old replay still plays back as it was recorded.
    bool skipShotLegacy = false;
};

#endif
