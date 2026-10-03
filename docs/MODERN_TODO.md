# Modern improvements — to do

Ideas for making the game feel current, written down 2026-10-03 after the
modern in-game theme shipped (v2.4.133, see [MODERN_THEME_PLAN.md](MODERN_THEME_PLAN.md)).
None of these are started. Sharp HUD text (drawing font text at the screen's
real resolution instead of stretching it from 640×480) is handled on its own
and not listed here. Preview: https://claude.ai/artifact/R2g7vzq6UwJTgCyL6tUEkn (owner's account).

Ordered roughly by how much a player notices against how much work it is.
Anything that changes rules or adds a feature gets a row in
[PARITY.md](PARITY.md) when it ships.

## Game feel (small, could ship together)

These four share a settings row or two and are all render- or audio-only.

1. **Separate music and sound-effect volume.** Today there is only the Sound
   on/off toggle (`GameSettings::soundEnabled()`). Two sliders in the settings
   panel (`mainmenu_panels.cpp`, "Game" header), saved to the ini.
2. **Vibration.** None today. A short buzz on a pop, stronger on a big drop, a
   thump when a malus lands. Gamepads through `SDL_RumbleGamepad`; Android
   phones through JNI (the vendored `SDLControllerManager.java` already
   imports `Vibrator`). A setting, on by default on phones.
3. **Pop pitch variation.** A few percent of random pitch on each pop, and a
   step up for each extra bubble in a chain, so combos sound like combos. The
   music is unchanged (earlier decision).
4. **Screen shake and pop particles.** A slight shake on big drops and chain
   reactions, and a particle burst in place of the original pop sprite (the
   makeover mockup compares the two:
   https://claude.ai/artifact/UAqJsNAXJPWy1meMUPK1YT). Drawing only, same rule
   as the level drop-in: no simulation or replay state. Probably modern theme
   only, and skipped at the low GRAPHICS level.

## Medium

5. **Daily challenge.** One random level per day, seeded from the UTC date,
   the same for everyone, one attempt, its own daily board. The pieces exist:
   accounts, `hiscores.c` boards, the UTC day math from `weeklystats.c`. Needs
   a new board number, a menu entry and a rule for what counts as an attempt.
   Best candidate for bringing players back.
6. **Swap with the next bubble** (Bust-a-Move style). Changes the rules, so it
   must be optional and either kept off the online boards (as the aim guide
   is, through `RunCountsForScores()`) or given its own boards. Needs a
   replay event, since it changes the simulation.
7. **Achievements.** Examples: clear a level in under 10 s, drop 20 bubbles
   at once, reach level 50, win an online match. Kept with the account;
   Google Play Games could mirror them later.
8. **Menus in the modern theme.** The title screen, 1-player menu and lobby
   in the same cards and fonts as the in-game theme. Today a modern game sits
   between classic menus. Mockup (title screen, 1-player menu, game room,
   Set Teams, 20-player room):
   https://claude.ai/artifact/2YrheMEW9StASYVAFjLv4z
   - [x] Title screen: the "Modern" MENU STYLE, the default, existing players moved to it once.
   - [x] 1-player menu: a card with a line under each mode, switches for
     the settings, LEFT/RIGHT flip them.
   - [ ] Game room, including Set Teams and the 20-player layout. Keep the
     button's wording "Set Teams" (user decision).

## Bigger

9. **Phone portrait layout.** A taller board that fills a phone held upright,
   with bigger tap targets. The largest single gain for Android players. The
   fixed 640×480 canvas is the main obstacle.
10. **Ghost races.** Race a translucent copy of your own best run, or the top
    run on the online board, on the same level, driven by the existing replay
    recording. Also a step toward replay-verified highscores (a known issue in
    PARITY.md).
11. **Translations.** Every string is English. Pulling the strings into a
    table is tedious but opens the Play Store to a much larger audience.
    Watch the bitmap fonts in the original theme, which only cover ASCII.
