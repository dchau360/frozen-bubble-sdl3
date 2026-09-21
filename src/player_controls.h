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
};

#endif
