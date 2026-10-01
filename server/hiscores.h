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
 * account's best, all-time and this week (Monday 00:00 UTC onward, the same
 * week as weeklystats.c), on four boards: two rankings, each in two tracks
 * that mirror the game's own local tables -- keyboard/gamepad and
 * mouse/touch -- since those scores are not comparable.
 *
 *   board 0/1  furthest level (kb/mouse): a run is "reached level L in T ms";
 *              higher level wins, then lower time. Level 101 means the whole
 *              1-100 set was cleared. points is always 0 here.
 *   board 2/3  most points (kb/mouse): the most points one life scored (the
 *              game zeroes the score on every death), with the level that
 *              life got to and the run time at that moment; more points wins,
 *              then higher level, then lower time.
 *
 * Trust: the account is proven (AUTH, account.h) but the score is whatever the
 * client says -- there is no replay verification yet. submit range-checks it
 * and refuses what no real run could do (HISCORE_MIN_MS_PER_LEVEL,
 * HISCORE_MAX_POINTS_PER_LEVEL), and an operator can ban an account id
 * (banned.txt, below). Keyed by account id, listed as "nick#tag" like the
 * weekly rankings. */

#define HISCORE_TRACKS 2
#define HISCORE_BOARDS (2 * HISCORE_TRACKS)
#define HISCORE_BOARD_POINTS 2   /* first most-points board */
#define HISCORE_MAX_LEVEL 101
/* A week of play time; anything longer is not a real run. */
#define HISCORE_MAX_TIME_MS (7 * 24 * 3600 * 1000)
/* Far past anything a real life scores, just short of overflow worries. */
#define HISCORE_MAX_POINTS 100000000
/* Plausibility, deliberately loose so only impossible runs are refused (the
 * fastest real run on the board averaged ~11s a level, and a life scores
 * ~1,800 a level including the 1,000 clear bonus): a run can't clear levels
 * faster than this on average, even at the 5x speed setting ... */
#define HISCORE_MIN_MS_PER_LEVEL 1000
/* ... and a life can't score more than this per level it reached. */
#define HISCORE_MAX_POINTS_PER_LEVEL 20000

/* hiscore_submit()'s results. */
#define HISCORE_INVALID 0      /* out of range: a broken or foreign client */
#define HISCORE_OK 1
#define HISCORE_IMPLAUSIBLE 2  /* in range, but no real run could do it */
#define HISCORE_BANNED 3       /* the account is in the ban file */

enum hiscore_scope { HISCORE_ALLTIME = 0, HISCORE_WEEK = 1 };

/* Load from FB_SERVER_HISCORE_FILE, else $HOME/.fb-server/hiscores.dat, else
 * /var/lib/fb-server/hiscores.dat. Missing file = empty board, no error.
 *
 * Bans: FB_SERVER_BANNED_FILE, else banned.txt beside the hiscores file. One
 * account id (16 hex digits) per line; anything after it on the line is
 * ignored, so a whole hiscores.dat line can be pasted in, and '#' lines are
 * comments. Re-read whenever it changes, no restart needed. A banned account's
 * runs are refused and its line is hidden from every list and rank but kept on
 * disk, so taking the id out again restores it. */
void hiscore_init(void);

/* Record a run for a signed-in account. country is what that connection's
 * COUNTRY command said (country_tag, game.c), or "" -- kept per account as
 * its latest, and an empty one leaves the stored country alone. Returns
 * HISCORE_INVALID when the arguments are out of range, HISCORE_BANNED or
 * HISCORE_IMPLAUSIBLE (nothing recorded in any of those), else HISCORE_OK --
 * whether or not the run beat the account's existing bests, which are kept
 * either way. points must be 0 on a
 * furthest-level board and at least 1 on a most-points one. nick must already
 * have passed is_nick_ok(). Saves to disk when anything improved. */
int hiscore_submit(const char* id, const char* nick, const char* country, int board, int level,
                   int time_ms, int points);

/* 1 if no real run could reach `level` in `time_ms` (or score `points` on a
 * most-points board), by the HISCORE_*_PER_LEVEL limits above. */
int hiscore_implausible(int board, int level, int time_ms, int points);

/* 1 if the account id is in the ban file (re-read first if it changed). */
int hiscore_banned(const char* id);

/* An ISO 3166-1 alpha-2 shape: two capital letters. */
int hiscore_country_ok(const char* c);

/* Drop every run the account has, on every board, all-time and this week
 * (DELETEACCOUNT), saving if it had any. Returns 1 if it had a line. */
int hiscore_forget(const char* id);

/* The account's competition rank in that board, or 0 when it has no run
 * there. */
int hiscore_rank(const char* id, int board, enum hiscore_scope scope);

/* Up to n "nick#tag=level/time_ms/points[/CC]" entries (CC the account's
 * country, when it has one: a fourth field older parsers never read), comma-joined, best first
 * (ties broken by nick), or "" when the board is empty. */
void hiscore_top_csv(int board, enum hiscore_scope scope, int n, char* out, size_t outsz);

/* The account's own "arank,alevel,atime,apoints,wrank,wlevel,wtime,wpoints"
 * for a board (all zero where it has no run), or "" when it has no run in
 * either scope. */
void hiscore_player_csv(const char* id, int board, char* out, size_t outsz);

/* Monday 00:00 UTC that starts the current week, as a Unix time. */
time_t hiscore_week_start(void);

#endif
