/*******************************************************************************
 * Copyright (c) 2026 dchau360
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 ******************************************************************************/

#ifndef WEEKLYSTATS_H
#define WEEKLYSTATS_H

#include <stddef.h>
#include <time.h>

/* Per-nick round wins, round losses and bubbles popped for the current week
 * (Monday 00:00 UTC to the next), with a ranking in each. Fed from the same
 * round-end handling that drives the Discord RESULT alert (game.c's
 * maybe_fire_pending_result()), so it inherits that path's trust posture: the
 * roster and bot flags are this server's own bookkeeping, the winner claim is
 * whatever the reporting client's 'F' said, and popped is the self-reported,
 * ceiling-clamped 'S' figure. Bots are never recorded. Keyed by nick, which is
 * not an account: anyone who takes the same nick shares its line.
 *
 * Separate from stats.c on purpose -- that one counts a mid-game departure as
 * a loss, which this project deliberately never publishes (see CLAUDE.md's
 * round-result section), and resets daily in server-local time. */

enum weekly_category { WEEKLY_WINS = 0, WEEKLY_LOSSES = 1, WEEKLY_POPPED = 2 };

/* Load from FB_SERVER_WEEKLY_FILE, else $HOME/.fb-server/weekly.dat, else
 * /var/lib/fb-server/weekly.dat. Missing file = empty week, no error. */
void weekly_init(void);

void weekly_record_win(const char* nick);
void weekly_record_loss(const char* nick);
void weekly_record_popped(const char* nick, int popped);
/* Write to disk; call once after a round's worth of record_* calls. */
void weekly_save(void);

/* nick's six numbers as "W,L,P,rankW,rankL,rankP" (a rank is 0 when that
 * count is 0), or "" if nick has no line this week. */
void weekly_player_csv(const char* nick, char* out, size_t outsz);

/* Up to n "nick=count" pairs, comma-joined, highest first (ties broken by
 * nick), or "" when nobody has a nonzero count in that category. */
void weekly_top_csv(enum weekly_category cat, int n, char* out, size_t outsz);

/* Monday 00:00 UTC that starts the current week, as a Unix time. */
time_t weekly_week_start(void);

/* Call periodically with time(NULL). Once per UTC day, fires a Discord
 * LEADERBOARD alert with the current standings; at the Monday rollover it
 * posts the finished week's final standings instead, then starts the new week
 * empty. The last day posted is saved with the stats, so a restart or
 * redeploy never posts twice in a day, and a server that was down across
 * midnight (or across Monday) catches up with one post when it comes back. A
 * brand-new stats file starts out as "already posted today". */
void weekly_tick(time_t now);

#endif
