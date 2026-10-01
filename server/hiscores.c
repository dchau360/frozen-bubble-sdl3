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
#include <glib/gstdio.h>

#include "hiscores.h"
#include "weeklystats.h"   /* WEEKLY_TAG_LEN: both boards list "nick#tag" */
#include "log.h"
#include "snapshot.h"

typedef struct {
        int level;    /* 0 = no run */
        int time_ms;
        int points;   /* most-points boards only */
        int day;      /* UTC day index the run was set, 0 = from before v4 */
} Run;

typedef struct {
        Run best[2][HISCORE_BOARDS];  /* [scope][board] */
        char nick[16];                /* the name of this account's latest submission */
        char country[3];              /* ISO alpha-2 its game last reported, or "" */
} HiscoreLine;

static GHashTable* table = NULL;   /* account id -> HiscoreLine* */
static char* file_path = NULL;
static long week_start_day = 0;    /* UTC day index of this week's Monday */

static GHashTable* banned = NULL;  /* account id -> unused, from the ban file */
static char* banned_path = NULL;
static gint64 banned_mtime = -1;   /* -1 = not read yet; 0 = no file */
static void refresh_bans(void);

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
        fprintf(f, "v4 %ld\n", week_start_day);
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
                HiscoreLine* hl = value;
                fprintf(f, "%s %s %s", (const char*)key, hl->nick, hl->country[0] ? hl->country : "-");
                for (s = 0; s < 2; s++)
                        for (b = 0; b < HISCORE_BOARDS; b++)
                                fprintf(f, " %d %d %d %d", hl->best[s][b].level,
                                        hl->best[s][b].time_ms, hl->best[s][b].points,
                                        hl->best[s][b].day);
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
        /* Every submit saves, so the file is the finished week as it ended. */
        snapshot_week(file_path, week_start_day);
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
        int loaded = 0, version = 4;

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
        {
                const char* explicit_bans = getenv("FB_SERVER_BANNED_FILE");
                char* dir = g_path_get_dirname(file_path);
                banned_path = explicit_bans && *explicit_bans
                        ? g_strdup(explicit_bans) : g_build_filename(dir, "banned.txt", NULL);
                g_free(dir);
                refresh_bans();
        }

        week_start_day = monday_of(day_index(time(NULL)));
        f = fopen(file_path, "r");
        if (!f)
                return;
        /* Header "v4 <week_start_day>", then per account "<id> <nick>
         * <country or ->" and "<level> <time_ms> <points> <day>" for every
         * board (0-3, see hiscores.h), all-time first, then this week, day
         * being the UTC day index the run was set. A "v3" file is the same
         * without the days, "v2" also without the country; a "v1" file
         * (furthest-level boards only, "<level> <time_ms>" for kb/mouse
         * all-time then week) loads into boards 0 and 1. */
        if (fgets(buf, sizeof(buf), f)) {
                long ws;
                if (sscanf(buf, "v4 %ld", &ws) == 1)
                        week_start_day = ws;
                else if (sscanf(buf, "v3 %ld", &ws) == 1) {
                        week_start_day = ws;
                        version = 3;
                }
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
                int v[2 * HISCORE_BOARDS * 4], i, used, n = 0;
                const int per = version >= 4 ? 4 : 3;
                const int want = version == 1 ? 8 : 2 * HISCORE_BOARDS * per;
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
                                r->level = v[i * per];
                                r->time_ms = v[i * per + 1];
                                r->points = v[i * per + 2];
                                r->day = per == 4 ? v[i * per + 3] : 0;
                        }
                }
                g_hash_table_replace(table, g_strdup(id), hl);
                loaded++;
        }
        fclose(f);
        l1(OUTPUT_TYPE_INFO, "Loaded hiscores for %d accounts", loaded);
        rollover_if_needed();
}

static int is_account_id(const char* s, size_t n)
{
        size_t i;
        if (n != 16) return 0;
        for (i = 0; i < n; i++)
                if (!g_ascii_isxdigit(s[i])) return 0;
        return 1;
}

/* Re-read the ban file when its mtime changes (or it appears/disappears). */
static void refresh_bans(void)
{
        GStatBuf st;
        gint64 mtime = 0;
        FILE* f;
        char buf[1024];

        if (!banned_path) return;
        if (g_stat(banned_path, &st) == 0)
                /* Size too, so a second edit within the same second still
                 * counts; never 0 when the file is there. */
                mtime = ((gint64)st.st_mtime << 20) + ((gint64)st.st_size & 0xfffff) + 1;
        if (mtime == banned_mtime) return;
        banned_mtime = mtime;
        if (!banned)
                banned = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
        g_hash_table_remove_all(banned);
        f = mtime ? fopen(banned_path, "r") : NULL;
        if (!f) return;
        while (fgets(buf, sizeof(buf), f)) {
                char* p = buf;
                size_t n;
                while (*p == ' ' || *p == '\t') p++;
                if (*p == '#') continue;
                n = strspn(p, "0123456789abcdefABCDEF");
                if (!is_account_id(p, n)) continue;
                p[n] = '\0';
                g_hash_table_add(banned, g_ascii_strdown(p, -1));
        }
        fclose(f);
        l2(OUTPUT_TYPE_INFO, "Ban file %s: %u account(s)", banned_path,
           g_hash_table_size(banned));
}

int hiscore_banned(const char* id)
{
        refresh_bans();
        return banned && id && g_hash_table_contains(banned, id);
}

/* Without a re-read: the list functions refresh once and then ask per line. */
static int is_banned(const char* id)
{
        return banned && g_hash_table_contains(banned, id);
}

int hiscore_implausible(int board, int level, int time_ms, int points)
{
        /* Levels a run cleared: a furthest-level run reports the level it
         * just cleared (101 = the whole set of 100), a life the level it
         * reached, which it may not have cleared. */
        int cleared = board >= HISCORE_BOARD_POINTS ? level - 1 : (level > 100 ? 100 : level);
        if ((long long)time_ms < (long long)cleared * HISCORE_MIN_MS_PER_LEVEL) return 1;
        if (board >= HISCORE_BOARD_POINTS &&
            (long long)points > (long long)level * HISCORE_MAX_POINTS_PER_LEVEL) return 1;
        return 0;
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

        if (!table || !id || !*id || !nick || !*nick) return HISCORE_INVALID;
        if (board < 0 || board >= HISCORE_BOARDS) return HISCORE_INVALID;
        if (level < 1 || level > HISCORE_MAX_LEVEL) return HISCORE_INVALID;
        if (time_ms <= 0 || time_ms > HISCORE_MAX_TIME_MS) return HISCORE_INVALID;
        if (board >= HISCORE_BOARD_POINTS ? (points < 1 || points > HISCORE_MAX_POINTS) : points != 0)
                return HISCORE_INVALID;
        if (hiscore_banned(id)) return HISCORE_BANNED;
        if (hiscore_implausible(board, level, time_ms, points)) {
                l4(OUTPUT_TYPE_INFO, "Refused implausible run from %s: board %d level %d, %d ms",
                   id, board, level, time_ms);
                return HISCORE_IMPLAUSIBLE;
        }
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
        run.day = (int)day_index(time(NULL));
        for (s = 0; s < 2; s++) {
                Run* cur = &hl->best[s][board];
                if (cur->level == 0 || better(board, &run, cur)) {
                        *cur = run;
                        improved = 1;
                }
        }
        if (improved)
                save();
        return HISCORE_OK;
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
        if (hiscore_banned(id)) return 0;
        hl = g_hash_table_lookup(table, id);
        if (!hl || hl->best[scope][board].level == 0) return 0;
        mine = &hl->best[scope][board];
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
                const Run* r = &((HiscoreLine*)value)->best[scope][board];
                if (r->level && !is_banned(key) && better(board, r, mine))
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

void hiscore_top_csv(int board, enum hiscore_scope scope, int n, char* out, size_t outsz,
                     char* days, size_t dayssz)
{
        GHashTableIter iter;
        gpointer key, value;
        Entry* entries;
        guint size, used = 0, i;
        size_t len = 0, dlen = 0;

        if (outsz == 0) return;
        out[0] = '\0';
        if (days && dayssz) days[0] = '\0';
        if (!table || n <= 0 || board < 0 || board >= HISCORE_BOARDS) return;
        rollover_if_needed();
        refresh_bans();
        size = g_hash_table_size(table);
        if (size == 0) return;
        entries = g_new(Entry, size);
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
                const HiscoreLine* hl = value;
                if (hl->best[scope][board].level && !is_banned(key)) {
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
                if (days && dayssz) {
                        int d = snprintf(days + dlen, dayssz - dlen, "%s%d", i ? "," : "",
                                         entries[i].run.day);
                        if (d < 0 || (size_t)d >= dayssz - dlen) {
                                days[0] = '\0';  /* all or nothing, so indexes stay aligned */
                                days = NULL;
                        } else
                                dlen += (size_t)d;
                }
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
        if (hiscore_banned(id)) return;
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
