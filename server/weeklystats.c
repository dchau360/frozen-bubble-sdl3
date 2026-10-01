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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <glib.h>

#include "weeklystats.h"
#include "discordalert.h"
#include "log.h"

/* Top-N carried by the daily Discord leaderboard. The lobby's own WEEKLY
 * command asks for its own N (game.c). */
#define LEADERBOARD_TOP_N 5

typedef struct {
        int counts[3];  /* indexed by enum weekly_category */
        char nick[16];  /* the name this account last played a counted round as */
} WeeklyLine;

static GHashTable* table = NULL;   /* account id -> WeeklyLine* */
static char* file_path = NULL;
static long week_start_day = 0;    /* UTC day index of this week's Monday */
static long last_posted_day = -1;  /* UTC day index of the last LEADERBOARD */

static long day_index(time_t t)
{
        /* floor division, so a (theoretical) pre-1970 clock still rounds down */
        return t >= 0 ? (long)(t / 86400) : -(long)((-t + 86399) / 86400);
}

/* 1970-01-01 (day 0) was a Thursday, so Monday is where (day + 3) % 7 == 0. */
static long monday_of(long day)
{
        long off = ((day + 3) % 7 + 7) % 7;
        return day - off;
}

static void ensure_parent_dir(const char* path)
{
        char* copy = g_strdup(path);
        char* slash = strrchr(copy, '/');
        if (slash && slash != copy) {
                *slash = '\0';
                mkdir(copy, 0755);
        }
        g_free(copy);
}

/* Names the game fills in when a player never chose one: "unnamed" (desktop
 * with no $USER), "android_user" (cut to "android_us" by the 10-char nick
 * limit) and "web_user" (the browser build's $USER). A default name is shared
 * by everyone who never set one, so a line for it is many strangers added
 * together, not a player. Also matches the client's NICK_IN_USE retries of
 * them -- the first 9 chars plus a number, e.g. "unnamed2", "android_u3". */
int weekly_is_default_nick(const char* nick)
{
        static const char* const exact[] = { "unnamed", "web_user", "android_us", "android_user" };
        static const char* const stems[] = { "unnamed", "web_user", "android_u" };
        size_t i;
        if (!nick) return 0;
        for (i = 0; i < sizeof(exact) / sizeof(exact[0]); i++)
                if (!strcmp(nick, exact[i])) return 1;
        for (i = 0; i < sizeof(stems) / sizeof(stems[0]); i++) {
                size_t n = strlen(stems[i]);
                const char* tail = nick + n;
                if (strncmp(nick, stems[i], n) || !*tail || strlen(tail) > 2) continue;
                if (strspn(tail, "0123456789") == strlen(tail)) return 1;
        }
        return 0;
}

static WeeklyLine* line_for(const char* id, const char* nick)
{
        WeeklyLine* wl = g_hash_table_lookup(table, id);
        if (!wl) {
                wl = g_new0(WeeklyLine, 1);
                g_hash_table_insert(table, g_strdup(id), wl);
        }
        snprintf(wl->nick, sizeof(wl->nick), "%s", nick);
        return wl;
}

/* A round is only counted for a connection that proved an account (AUTH,
 * game.c) and did not play under one of the default names. */
static int countable(const char* id, const char* nick)
{
        return table && id && *id && nick && *nick && !weekly_is_default_nick(nick);
}

void weekly_init(void)
{
        const char* explicit_path = getenv("FB_SERVER_WEEKLY_FILE");
        const char* home = getenv("HOME");
        long today = day_index(time(NULL));
        FILE* f;
        char buf[512];
        int loaded = 0;

        table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        if (explicit_path && *explicit_path)
                file_path = g_strdup(explicit_path);
        else if (home)
                file_path = g_strdup_printf("%s/.fb-server/weekly.dat", home);
        else
                file_path = g_strdup("/var/lib/fb-server/weekly.dat");
        ensure_parent_dir(file_path);
        l1(OUTPUT_TYPE_INFO, "Weekly stats file: %s", file_path);

        week_start_day = monday_of(today);
        last_posted_day = today;

        f = fopen(file_path, "r");
        if (!f)
                return;
        /* Header: "v2 <week_start_day> <last_posted_day>", then one
         * "<account_id> <nick> <wins> <losses> <popped>" per line. Ids are
         * hex and nicks passed is_nick_ok() ([A-Za-z0-9_-]), so neither
         * contains a space. A v1 file (keyed by nick, before accounts) keeps
         * its week and last-posted day but not its lines: a nick-keyed line
         * cannot be given to any one account. */
        if (fgets(buf, sizeof(buf), f)) {
                long ws, lp;
                int ver = 0;
                if (sscanf(buf, "v%d %ld %ld", &ver, &ws, &lp) == 3) {
                        week_start_day = ws;
                        last_posted_day = lp;
                }
                if (ver != 2) {
                        fclose(f);
                        l1(OUTPUT_TYPE_INFO, "Weekly stats file is v%d, pre-accounts; starting this week's lines empty", ver);
                        return;
                }
        }
        while (fgets(buf, sizeof(buf), f)) {
                char id[64], nick[64];
                int w, l, p;
                if (sscanf(buf, "%63s %63s %d %d %d", id, nick, &w, &l, &p) == 5 &&
                    countable(id, nick)) {
                        WeeklyLine* wl = line_for(id, nick);
                        wl->counts[WEEKLY_WINS] = w;
                        wl->counts[WEEKLY_LOSSES] = l;
                        wl->counts[WEEKLY_POPPED] = p;
                        loaded++;
                }
        }
        fclose(f);
        l1(OUTPUT_TYPE_INFO, "Loaded weekly stats for %d players", loaded);
}

int weekly_forget(const char* id)
{
        if (!table || !id || !*id || !g_hash_table_remove(table, id))
                return 0;
        weekly_save();
        return 1;
}

void weekly_save(void)
{
        GHashTableIter iter;
        gpointer key, value;
        char* tmp;
        FILE* f;

        if (!table || !file_path) return;
        /* Write-then-rename, so a crash mid-save leaves the previous file
         * intact rather than a truncated week. */
        tmp = g_strdup_printf("%s.tmp", file_path);
        f = fopen(tmp, "w");
        if (!f) {
                l2(OUTPUT_TYPE_ERROR, "Failed to save weekly stats to %s: %s", tmp, strerror(errno));
                g_free(tmp);
                return;
        }
        fprintf(f, "v2 %ld %ld\n", week_start_day, last_posted_day);
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
                WeeklyLine* wl = value;
                fprintf(f, "%s %s %d %d %d\n", (const char*)key, wl->nick, wl->counts[WEEKLY_WINS],
                        wl->counts[WEEKLY_LOSSES], wl->counts[WEEKLY_POPPED]);
        }
        if (fclose(f) != 0 || rename(tmp, file_path) != 0)
                l2(OUTPUT_TYPE_ERROR, "Failed to save weekly stats to %s: %s", file_path, strerror(errno));
        g_free(tmp);
}

/* A round can finish just after Monday 00:00 UTC, before the next tick has
 * run; roll over first so it lands in the week it finished in. weekly_tick()
 * is a no-op when nothing has changed, so calling it here is cheap. */
static void rollover_if_needed(void)
{
        weekly_tick(time(NULL));
}

void weekly_record_win(const char* id, const char* nick)
{
        if (!countable(id, nick)) return;
        rollover_if_needed();
        line_for(id, nick)->counts[WEEKLY_WINS]++;
}

void weekly_record_loss(const char* id, const char* nick)
{
        if (!countable(id, nick)) return;
        rollover_if_needed();
        line_for(id, nick)->counts[WEEKLY_LOSSES]++;
}

void weekly_record_popped(const char* id, const char* nick, int popped)
{
        if (!countable(id, nick) || popped <= 0) return;
        rollover_if_needed();
        line_for(id, nick)->counts[WEEKLY_POPPED] += popped;
}

/* Competition ranking ("1, 2, 2, 4"): 1 + how many players have strictly
 * more. 0 when nick's own count is 0 -- nobody is ranked for nothing. */
static int rank_of(enum weekly_category cat, int count)
{
        GHashTableIter iter;
        gpointer key, value;
        int above = 0;
        if (count <= 0) return 0;
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
                if (((WeeklyLine*)value)->counts[cat] > count)
                        above++;
        }
        return above + 1;
}

void weekly_player_csv(const char* id, char* out, size_t outsz)
{
        WeeklyLine* wl;
        out[0] = '\0';
        if (!table || !id || !*id) return;
        rollover_if_needed();
        wl = g_hash_table_lookup(table, id);
        if (!wl) return;
        snprintf(out, outsz, "%d,%d,%d,%d,%d,%d",
                 wl->counts[WEEKLY_WINS], wl->counts[WEEKLY_LOSSES], wl->counts[WEEKLY_POPPED],
                 rank_of(WEEKLY_WINS, wl->counts[WEEKLY_WINS]),
                 rank_of(WEEKLY_LOSSES, wl->counts[WEEKLY_LOSSES]),
                 rank_of(WEEKLY_POPPED, wl->counts[WEEKLY_POPPED]));
}

int weekly_rank(const char* id, enum weekly_category cat)
{
        WeeklyLine* wl;
        if (!table || !id || !*id) return 0;
        rollover_if_needed();
        wl = g_hash_table_lookup(table, id);
        return wl ? rank_of(cat, wl->counts[cat]) : 0;
}

typedef struct {
        const char* id;
        const char* nick;
        int count;
} Entry;

static int entry_cmp(const void* a, const void* b)
{
        const Entry* x = a;
        const Entry* y = b;
        int c;
        if (x->count != y->count) return y->count - x->count;
        c = strcmp(x->nick, y->nick);
        return c ? c : strcmp(x->id, y->id);
}

/* weekly_top_csv() without the rollover check, for weekly_tick()'s own
 * final post of a week it has not yet cleared. */
static void top_csv(enum weekly_category cat, int n, char* out, size_t outsz)
{
        GHashTableIter iter;
        gpointer key, value;
        Entry* entries;
        guint size, used = 0, i;
        size_t len = 0;

        out[0] = '\0';
        if (!table || n <= 0 || outsz == 0) return;
        size = g_hash_table_size(table);
        if (size == 0) return;
        entries = g_new(Entry, size);
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
                int c = ((WeeklyLine*)value)->counts[cat];
                if (c > 0) {
                        entries[used].id = key;
                        entries[used].nick = ((WeeklyLine*)value)->nick;
                        entries[used].count = c;
                        used++;
                }
        }
        qsort(entries, used, sizeof(Entry), entry_cmp);
        for (i = 0; i < used && (int)i < n; i++) {
                int w = snprintf(out + len, outsz - len, "%s%s#%.*s=%d",
                                 i ? "," : "", entries[i].nick,
                                 WEEKLY_TAG_LEN, entries[i].id, entries[i].count);
                if (w < 0 || (size_t)w >= outsz - len) {
                        /* Out of room: drop the partial entry rather than
                         * hand a truncated "nick=12" to anyone parsing it. */
                        out[len] = '\0';
                        break;
                }
                len += (size_t)w;
        }
        g_free(entries);
}

void weekly_top_csv(enum weekly_category cat, int n, char* out, size_t outsz)
{
        rollover_if_needed();
        top_csv(cat, n, out, outsz);
}

time_t weekly_week_start(void)
{
        rollover_if_needed();
        return (time_t)week_start_day * 86400;
}

static void fire_leaderboard(int final)
{
        char wins[512], losses[512], popped[512];
        top_csv(WEEKLY_WINS, LEADERBOARD_TOP_N, wins, sizeof(wins));
        top_csv(WEEKLY_LOSSES, LEADERBOARD_TOP_N, losses, sizeof(losses));
        top_csv(WEEKLY_POPPED, LEADERBOARD_TOP_N, popped, sizeof(popped));
        if (!wins[0] && !losses[0] && !popped[0])
                return;  /* an empty week has nothing worth posting */
        discordalert_fire_leaderboard_event(final, (long)week_start_day * 86400,
                                            wins, losses, popped);
}

void weekly_tick(time_t now)
{
        long today, monday;
        if (!table) return;
        today = day_index(now);
        monday = monday_of(today);
        if (monday != week_start_day) {
                /* The finished week's final standings, labelled with its own
                 * Monday (week_start_day hasn't moved yet), then a clean week.
                 * This replaces that day's ordinary daily post. */
                last_posted_day = today;
                fire_leaderboard(1);
                g_hash_table_remove_all(table);
                week_start_day = monday;
                l0(OUTPUT_TYPE_INFO, "Weekly stats rolled over to a new week");
                weekly_save();
        } else if (today != last_posted_day) {
                last_posted_day = today;
                fire_leaderboard(0);
                weekly_save();
        }
}
