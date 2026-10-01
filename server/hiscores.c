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

#include "hiscores.h"
#include "weeklystats.h"   /* WEEKLY_TAG_LEN: both boards list "nick#tag" */
#include "log.h"

typedef struct {
        int level;    /* 0 = no run */
        int time_ms;
        int points;   /* most-points boards only */
} Run;

typedef struct {
        Run best[2][HISCORE_BOARDS];  /* [scope][board] */
        char nick[16];                /* the name of this account's latest submission */
        char country[3];              /* ISO alpha-2 its game last reported, or "" */
} HiscoreLine;

static GHashTable* table = NULL;   /* account id -> HiscoreLine* */
static char* file_path = NULL;
static long week_start_day = 0;    /* UTC day index of this week's Monday */

/* Same day math as weeklystats.c, so both boards turn over together. */
static long day_index(time_t t)
{
        return t >= 0 ? (long)(t / 86400) : -(long)((-t + 86399) / 86400);
}

static long monday_of(long day)
{
        long off = ((day + 3) % 7 + 7) % 7;
        return day - off;
}

static int better(int board, const Run* a, const Run* b)
{
        if (board >= HISCORE_BOARD_POINTS && a->points != b->points)
                return a->points > b->points;
        if (a->level != b->level) return a->level > b->level;
        return a->time_ms < b->time_ms;
}

static void save(void)
{
        GHashTableIter iter;
        gpointer key, value;
        char* tmp;
        FILE* f;
        int s, b;

        if (!table || !file_path) return;
        /* Write-then-rename, like weekly_save(). */
        tmp = g_strdup_printf("%s.tmp", file_path);
        f = fopen(tmp, "w");
        if (!f) {
                l2(OUTPUT_TYPE_ERROR, "Failed to save hiscores to %s: %s", tmp, strerror(errno));
                g_free(tmp);
                return;
        }
        fprintf(f, "v3 %ld\n", week_start_day);
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
                HiscoreLine* hl = value;
                fprintf(f, "%s %s %s", (const char*)key, hl->nick, hl->country[0] ? hl->country : "-");
                for (s = 0; s < 2; s++)
                        for (b = 0; b < HISCORE_BOARDS; b++)
                                fprintf(f, " %d %d %d", hl->best[s][b].level,
                                        hl->best[s][b].time_ms, hl->best[s][b].points);
                fputc('\n', f);
        }
        if (fclose(f) != 0 || rename(tmp, file_path) != 0)
                l2(OUTPUT_TYPE_ERROR, "Failed to save hiscores to %s: %s", file_path, strerror(errno));
        g_free(tmp);
}

/* A new week clears every week best; all-time bests stay. */
static void rollover_if_needed(void)
{
        GHashTableIter iter;
        gpointer key, value;
        long monday;
        if (!table) return;
        monday = monday_of(day_index(time(NULL)));
        if (monday == week_start_day) return;
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, &key, &value))
                memset(((HiscoreLine*)value)->best[HISCORE_WEEK], 0,
                       sizeof(((HiscoreLine*)value)->best[HISCORE_WEEK]));
        week_start_day = monday;
        l0(OUTPUT_TYPE_INFO, "Hiscores rolled over to a new week");
        save();
}

void hiscore_init(void)
{
        const char* explicit_path = getenv("FB_SERVER_HISCORE_FILE");
        const char* home = getenv("HOME");
        FILE* f;
        char buf[1024];
        int loaded = 0, version = 3;

        table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        if (explicit_path && *explicit_path)
                file_path = g_strdup(explicit_path);
        else if (home)
                file_path = g_strdup_printf("%s/.fb-server/hiscores.dat", home);
        else
                file_path = g_strdup("/var/lib/fb-server/hiscores.dat");
        {
                char* dir = g_path_get_dirname(file_path);
                mkdir(dir, 0755);
                g_free(dir);
        }
        l1(OUTPUT_TYPE_INFO, "Hiscores file: %s", file_path);

        week_start_day = monday_of(day_index(time(NULL)));
        f = fopen(file_path, "r");
        if (!f)
                return;
        /* Header "v3 <week_start_day>", then per account "<id> <nick>
         * <country or ->" and "<level> <time_ms> <points>" for every board
         * (0-3, see hiscores.h), all-time first, then this week. A "v2" file
         * is the same without the country; a "v1" file (furthest-level boards
         * only, "<level> <time_ms>" for kb/mouse all-time then week) loads
         * into boards 0 and 1. */
        if (fgets(buf, sizeof(buf), f)) {
                long ws;
                if (sscanf(buf, "v3 %ld", &ws) == 1)
                        week_start_day = ws;
                else if (sscanf(buf, "v2 %ld", &ws) == 1) {
                        week_start_day = ws;
                        version = 2;
                } else if (sscanf(buf, "v1 %ld", &ws) == 1) {
                        week_start_day = ws;
                        version = 1;
                }
        }
        while (fgets(buf, sizeof(buf), f)) {
                char id[64], nick[64], country[64] = "-";
                int v[2 * HISCORE_BOARDS * 3], i, used, n = 0;
                const int want = version == 1 ? 8 : 2 * HISCORE_BOARDS * 3;
                const char* p;
                HiscoreLine* hl;
                if (sscanf(buf, "%63s %63s%n", id, nick, &used) != 2)
                        continue;
                p = buf + used;
                if (version >= 3) {
                        if (sscanf(p, "%63s%n", country, &used) != 1)
                                continue;
                        p += used;
                }
                while (n < want && sscanf(p, "%d%n", &v[n], &used) == 1) {
                        p += used;
                        n++;
                }
                if (n != want)
                        continue;
                hl = g_new0(HiscoreLine, 1);
                snprintf(hl->nick, sizeof(hl->nick), "%s", nick);
                if (hiscore_country_ok(country))
                        memcpy(hl->country, country, 3);
                if (version == 1) {
                        for (i = 0; i < 4; i++) {
                                hl->best[i / 2][i % 2].level = v[i * 2];
                                hl->best[i / 2][i % 2].time_ms = v[i * 2 + 1];
                        }
                } else {
                        for (i = 0; i < 2 * HISCORE_BOARDS; i++) {
                                Run* r = &hl->best[i / HISCORE_BOARDS][i % HISCORE_BOARDS];
                                r->level = v[i * 3];
                                r->time_ms = v[i * 3 + 1];
                                r->points = v[i * 3 + 2];
                        }
                }
                g_hash_table_replace(table, g_strdup(id), hl);
                loaded++;
        }
        fclose(f);
        l1(OUTPUT_TYPE_INFO, "Loaded hiscores for %d accounts", loaded);
        rollover_if_needed();
}

int hiscore_country_ok(const char* c)
{
        return c && c[0] >= 'A' && c[0] <= 'Z' && c[1] >= 'A' && c[1] <= 'Z' && c[2] == '\0';
}

int hiscore_submit(const char* id, const char* nick, const char* country, int board, int level,
                   int time_ms, int points)
{
        HiscoreLine* hl;
        Run run;
        int s, improved = 0;

        if (!table || !id || !*id || !nick || !*nick) return 0;
        if (board < 0 || board >= HISCORE_BOARDS) return 0;
        if (level < 1 || level > HISCORE_MAX_LEVEL) return 0;
        if (time_ms <= 0 || time_ms > HISCORE_MAX_TIME_MS) return 0;
        if (board >= HISCORE_BOARD_POINTS ? (points < 1 || points > HISCORE_MAX_POINTS) : points != 0)
                return 0;
        rollover_if_needed();

        hl = g_hash_table_lookup(table, id);
        if (!hl) {
                hl = g_new0(HiscoreLine, 1);
                g_hash_table_insert(table, g_strdup(id), hl);
        }
        if (strcmp(hl->nick, nick)) {
                snprintf(hl->nick, sizeof(hl->nick), "%s", nick);
                improved = 1;  /* a rename alone is worth saving */
        }
        /* A submission without one (a game that hasn't looked it up this
         * time) leaves the last country in place rather than clearing it. */
        if (hiscore_country_ok(country) && strcmp(hl->country, country)) {
                memcpy(hl->country, country, 3);
                improved = 1;
        }
        run.level = level;
        run.time_ms = time_ms;
        run.points = points;
        for (s = 0; s < 2; s++) {
                Run* cur = &hl->best[s][board];
                if (cur->level == 0 || better(board, &run, cur)) {
                        *cur = run;
                        improved = 1;
                }
        }
        if (improved)
                save();
        return 1;
}

int hiscore_forget(const char* id)
{
        if (!table || !id || !*id || !g_hash_table_remove(table, id))
                return 0;
        save();
        return 1;
}

int hiscore_rank(const char* id, int board, enum hiscore_scope scope)
{
        GHashTableIter iter;
        gpointer key, value;
        HiscoreLine* hl;
        const Run* mine;
        int above = 0;

        if (!table || !id || !*id || board < 0 || board >= HISCORE_BOARDS) return 0;
        rollover_if_needed();
        hl = g_hash_table_lookup(table, id);
        if (!hl || hl->best[scope][board].level == 0) return 0;
        mine = &hl->best[scope][board];
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
                const Run* r = &((HiscoreLine*)value)->best[scope][board];
                if (r->level && better(board, r, mine))
                        above++;
        }
        return above + 1;
}

typedef struct {
        const char* id;
        const HiscoreLine* line;
        Run run;
} Entry;

/* qsort has no context argument; top_csv is not reentrant anyway. */
static int sort_board = 0;

static int entry_cmp(const void* a, const void* b)
{
        const Entry* x = a;
        const Entry* y = b;
        int c;
        if (better(sort_board, &x->run, &y->run)) return -1;
        if (better(sort_board, &y->run, &x->run)) return 1;
        c = strcmp(x->line->nick, y->line->nick);
        return c ? c : strcmp(x->id, y->id);
}

void hiscore_top_csv(int board, enum hiscore_scope scope, int n, char* out, size_t outsz)
{
        GHashTableIter iter;
        gpointer key, value;
        Entry* entries;
        guint size, used = 0, i;
        size_t len = 0;

        if (outsz == 0) return;
        out[0] = '\0';
        if (!table || n <= 0 || board < 0 || board >= HISCORE_BOARDS) return;
        rollover_if_needed();
        size = g_hash_table_size(table);
        if (size == 0) return;
        entries = g_new(Entry, size);
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
                const HiscoreLine* hl = value;
                if (hl->best[scope][board].level) {
                        entries[used].id = key;
                        entries[used].line = hl;
                        entries[used].run = hl->best[scope][board];
                        used++;
                }
        }
        sort_board = board;
        qsort(entries, used, sizeof(Entry), entry_cmp);
        for (i = 0; i < used && (int)i < n; i++) {
                int w = snprintf(out + len, outsz - len, "%s%s#%.*s=%d/%d/%d%s%s",
                                 i ? "," : "", entries[i].line->nick, WEEKLY_TAG_LEN,
                                 entries[i].id, entries[i].run.level, entries[i].run.time_ms,
                                 entries[i].run.points, entries[i].line->country[0] ? "/" : "",
                                 entries[i].line->country);
                if (w < 0 || (size_t)w >= outsz - len) {
                        out[len] = '\0';  /* whole entries only */
                        break;
                }
                len += (size_t)w;
        }
        g_free(entries);
}

void hiscore_player_csv(const char* id, int board, char* out, size_t outsz)
{
        HiscoreLine* hl;
        const Run* a;
        const Run* w;
        if (outsz == 0) return;
        out[0] = '\0';
        if (!table || !id || !*id || board < 0 || board >= HISCORE_BOARDS) return;
        rollover_if_needed();
        hl = g_hash_table_lookup(table, id);
        if (!hl) return;
        a = &hl->best[HISCORE_ALLTIME][board];
        w = &hl->best[HISCORE_WEEK][board];
        if (!a->level && !w->level)
                return;
        snprintf(out, outsz, "%d,%d,%d,%d,%d,%d,%d,%d",
                 hiscore_rank(id, board, HISCORE_ALLTIME), a->level, a->time_ms, a->points,
                 hiscore_rank(id, board, HISCORE_WEEK), w->level, w->time_ms, w->points);
}

time_t hiscore_week_start(void)
{
        rollover_if_needed();
        return (time_t)week_start_day * 86400;
}
