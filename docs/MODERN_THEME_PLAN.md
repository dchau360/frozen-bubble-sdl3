# Modern in-game theme — plan (parked)

Status: **to do, not scheduled.** A partial implementation lives on the
`modern-theme` branch (commit `9a65535a`, "WIP: modern in-game theme"); nothing
of it is on `main`.

Preview mockups (private claude.ai artifacts, owner's account):
- Modern in-game theme: https://claude.ai/artifact/6CYU7iGzimRC6iMdM8zmh4
- Bubble skins (Candy gloss was picked): https://claude.ai/artifact/U5wip327kWg84jAspw3Suq
- Wider makeover ideas: https://claude.ai/artifact/UAqJsNAXJPWy1meMUPK1YT

## What it is

An optional look for the game screen, chosen as a setting ("In-game theme":
MODERN / ORIGINAL). The original theme stays available.

**Changes:**
- **Candy gloss bubbles**: a glossy redraw of the 8 colours, plus colour-blind
  variants and 16px minis. Used everywhere bubbles are drawn, including the
  High Scores thumbnails.
- **1-player score panel** on the left: LEVEL, SCORE (counts up), SHOTS and
  TIME on one dark rounded card. It replaces the score text, the "Shots: N"
  line and the "Level N" on the wooden plank.
- **Cards** for Level cleared, Lost and Paused, replacing the panel images:
  - Lost carries the CONTINUE / START OVER buttons.
  - Paused shows the penguin animation and "PRESS PAUSE TO RESUME".
- **Level change**: the next level's bubbles drop in row by row.
- **Popped "+N"** where a scoring shot lands, and a big **"N DROPPED!"** when
  bubbles fall.

**Stays original:** background, launcher, penguin, Hurry warning, pop/fall
motion, scoring, sounds.

## Done on the branch

- `share/gfx/balls/modern/`: 32 PNGs (`bubble-N`, `bubble-colourblind-N`,
  and the `-mini` 16px versions). Rendered at 256px from the Candy gloss
  mockup and halved down to 32px.
- Setting `GFX:ModernTheme` (default on), `GameSettings::modernTheme()`.
- `BubbleGame` loads the modern textures, and `GetBubbleTextures()` picks
  them when the theme is on and they loaded (otherwise falls back to the
  originals).
- `HighscoreManager` thumbnails use the modern bubbles when the theme is on.
- `src/modernui.h/.cpp`: pure drawing helpers. Not built yet, not in
  `CMakeLists.txt`. They provide:
  - rounded rects via `SDL_RenderGeometry`;
  - `Fonts` (Baloo2-ExtraBold / DroidSans);
  - `DrawHud`, `DrawCard` (0–2 buttons, focus highlight, button rects
    returned for tap hit-tests);
  - `DrawPopup`.

## Remaining steps

1. Add `src/modernui.cpp` to the game and core-test sources in
   `CMakeLists.txt`. Load `modernui::Fonts` in the `BubbleGame` constructor.
2. **Gate** everything except the bubbles on
   `modernTheme() && ShowsShotCount()` (1 player; not network, local
   multiplayer or training).
3. **HUD**:
   - Draw it instead of `inGameText` / `DrawScoreText` / `shotsText`.
   - Score count-up is render-time only.
   - Time freezes at game finish.
   - Random levels show "RANDOM".
4. **Popups**:
   - Draw `scorePopups` with the popup font.
   - Add a dropped count to `ScorePopup`, pushed when `fallingCount > 0` in
     1P regardless of theme so replays stay identical.
   - Draw "N DROPPED!" near the centre (y≈236).
5. **Cards**:
   - **Win**: LEVEL CLEARED with stats.
   - **Lose**: with `continuePrompt`, CONTINUE / START OVER.
     - Fill `continueBtnRect` / `startOverBtnRect` from `DrawCard`'s output
       so `HandleFinishedTap` keeps working, and honour
       `continueFocusStartOver`.
     - Skip `RenderContinuePrompt`.
     - Keep the note "Score goes back to 0; the clock keeps running."
   - **Pause**: in `RenderPaused`, with the penguin frame as `art` and no
     buttons (only the PAUSE key resumes, so a hint, not a button).
6. **Drop-in**:
   - Render-only offset per row (~35ms/row, 320ms ease-back from −60px with
     a fade).
   - Start time is set in `NewGame()` / `ReloadGame()`.
   - Must not touch simulation state, so replays don't desync.
7. **1-player menu row** "In-game theme" MODERN / ORIGINAL:
   - `SP_OPT` 8 → 9, plus a new `kSPRow`, label and `press()` →
     `SetValue("GFX:ModernTheme", "")`.
   - Panel grows by 36px to 446, which fits.
   - Follow the CLAUDE.md input-parity checklist: keyboard row, tap row,
     footer hint.
8. Decide whether the default stays **on** or becomes opt-in before
   shipping.
9. Build, full `ctest`, manual play-through of win / lose / continue / pause /
   random levels on desktop, Android and WASM. Then update CLAUDE.md,
   README and CHANGELOG.

## Design numbers (from the mockup)

- **HUD card**: x −16, y 14, 182×196, radius 16, fill `rgba(12,24,48,.9)`,
  2px edge `rgba(127,214,255,.5)`. Rows every 45px: 10px bold label
  `#8ea3c6`, then a 22px Baloo value in white.
- **Result card**: centred at (320, 236), 312 wide, 156 tall (+46 with
  buttons, +40 with art). Baloo 30px title, stats in columns.
  - Buttons are 36px pills: 160 wide alone, 128 as a pair, 14px gap.
    Primary fill `#7fd6ff` with dark ink text.
  - Backdrop dim `rgba(3,8,20,.45)`; card eases in over 380ms.
- **Popups**: Baloo 20px white with a dark outline, rising 34px.
  "N DROPPED!" is 30px gold.
