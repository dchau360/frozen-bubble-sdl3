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

#ifndef HISCORES_H
#define HISCORES_H

#include <stddef.h>
#include <time.h>

/* The world board for classic single-player runs (protocol 1.7): each
 * account's best run, all-time and this week (Monday 00:00 UTC onward, the
 * same week as weeklystats.c), in two tracks that mirror the game's own local
 * tables -- keyboard/gamepad and mouse/touch -- since those scores are not
 * comparable. A run is "reached level L in T ms"; higher level wins, then
 * lower time. Level 101 means the whole 1-100 set was cleared.
 *
 * Trust: the account is proven (AUTH, account.h) but the score is whatever the
 * client says -- there is no replay verification yet. submit only range-checks
 * it. Keyed by account id, listed as "nick#tag" like the weekly rankings. */

#define HISCORE_TRACKS 2
#define HISCORE_MAX_LEVEL 101
/* A week of play time; anything longer is not a real run. */
#define HISCORE_MAX_TIME_MS (7 * 24 * 3600 * 1000)

enum hiscore_scope { HISCORE_ALLTIME = 0, HISCORE_WEEK = 1 };

/* Load from FB_SERVER_HISCORE_FILE, else $HOME/.fb-server/hiscores.dat, else
 * /var/lib/fb-server/hiscores.dat. Missing file = empty board, no error. */
void hiscore_init(void);

/* Record a run for a signed-in account. Returns 0 when the arguments are out
 * of range (nothing recorded), else 1 -- whether or not the run beat the
 * account's existing bests, which are kept either way. nick must already have
 * passed is_nick_ok(). Saves to disk when anything improved. */
int hiscore_submit(const char* id, const char* nick, int track, int level, int time_ms);

/* The account's competition rank in that board, or 0 when it has no run
 * there. */
int hiscore_rank(const char* id, int track, enum hiscore_scope scope);

/* Up to n "nick#tag=level/time_ms" entries, comma-joined, best first (ties
 * broken by nick), or "" when the board is empty. */
void hiscore_top_csv(int track, enum hiscore_scope scope, int n, char* out, size_t outsz);

/* The account's own "arank,alevel,atime,wrank,wlevel,wtime" for a track (all
 * zero where it has no run), or "" when it has no run in either scope. */
void hiscore_player_csv(const char* id, int track, char* out, size_t outsz);

/* Monday 00:00 UTC that starts the current week, as a Unix time. */
time_t hiscore_week_start(void);

#endif
