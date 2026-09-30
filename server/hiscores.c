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
} Run;

typedef struct {
        Run best[2][HISCORE_TRACKS];  /* [scope][track] */
        char nick[16];                /* the name of this account's latest submission */
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

static int better(const Run* a, const Run* b)
{
        if (a->level != b->level) return a->level > b->level;
        return a->time_ms < b->time_ms;
}

static void save(void)
{
        GHashTableIter iter;
        gpointer key, value;
        char* tmp;
        FILE* f;
        int s, t;

        if (!table || !file_path) return;
        /* Write-then-rename, like weekly_save(). */
        tmp = g_strdup_printf("%s.tmp", file_path);
        f = fopen(tmp, "w");
        if (!f) {
                l2(OUTPUT_TYPE_ERROR, "Failed to save hiscores to %s: %s", tmp, strerror(errno));
                g_free(tmp);
                return;
        }
        fprintf(f, "v1 %ld\n", week_start_day);
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
                HiscoreLine* hl = value;
                fprintf(f, "%s %s", (const char*)key, hl->nick);
                for (s = 0; s < 2; s++)
                        for (t = 0; t < HISCORE_TRACKS; t++)
                                fprintf(f, " %d %d", hl->best[s][t].level, hl->best[s][t].time_ms);
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
        char buf[512];
        int loaded = 0;

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
        /* Header "v1 <week_start_day>", then per account
         * "<id> <nick> <level> <time_ms>" x (all-time kb, all-time mouse,
         * week kb, week mouse). */
        if (fgets(buf, sizeof(buf), f)) {
                long ws;
                if (sscanf(buf, "v1 %ld", &ws) == 1)
                        week_start_day = ws;
        }
        while (fgets(buf, sizeof(buf), f)) {
                char id[64], nick[64];
                int v[8], i;
                HiscoreLine* hl;
                if (sscanf(buf, "%63s %63s %d %d %d %d %d %d %d %d", id, nick,
                           &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]) != 10)
                        continue;
                hl = g_new0(HiscoreLine, 1);
                snprintf(hl->nick, sizeof(hl->nick), "%s", nick);
                for (i = 0; i < 4; i++) {
                        hl->best[i / 2][i % 2].level = v[i * 2];
                        hl->best[i / 2][i % 2].time_ms = v[i * 2 + 1];
                }
                g_hash_table_replace(table, g_strdup(id), hl);
                loaded++;
        }
        fclose(f);
        l1(OUTPUT_TYPE_INFO, "Loaded hiscores for %d accounts", loaded);
        rollover_if_needed();
}

int hiscore_submit(const char* id, const char* nick, int track, int level, int time_ms)
{
        HiscoreLine* hl;
        Run run;
        int s, improved = 0;

        if (!table || !id || !*id || !nick || !*nick) return 0;
        if (track < 0 || track >= HISCORE_TRACKS) return 0;
        if (level < 1 || level > HISCORE_MAX_LEVEL) return 0;
        if (time_ms <= 0 || time_ms > HISCORE_MAX_TIME_MS) return 0;
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
        run.level = level;
        run.time_ms = time_ms;
        for (s = 0; s < 2; s++) {
                Run* cur = &hl->best[s][track];
                if (cur->level == 0 || better(&run, cur)) {
                        *cur = run;
                        improved = 1;
                }
        }
        if (improved)
                save();
        return 1;
}

int hiscore_rank(const char* id, int track, enum hiscore_scope scope)
{
        GHashTableIter iter;
        gpointer key, value;
        HiscoreLine* hl;
        const Run* mine;
        int above = 0;

        if (!table || !id || !*id || track < 0 || track >= HISCORE_TRACKS) return 0;
        rollover_if_needed();
        hl = g_hash_table_lookup(table, id);
        if (!hl || hl->best[scope][track].level == 0) return 0;
        mine = &hl->best[scope][track];
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
                const Run* r = &((HiscoreLine*)value)->best[scope][track];
                if (r->level && better(r, mine))
                        above++;
        }
        return above + 1;
}

typedef struct {
        const char* id;
        const HiscoreLine* line;
        Run run;
} Entry;

static int entry_cmp(const void* a, const void* b)
{
        const Entry* x = a;
        const Entry* y = b;
        int c;
        if (better(&x->run, &y->run)) return -1;
        if (better(&y->run, &x->run)) return 1;
        c = strcmp(x->line->nick, y->line->nick);
        return c ? c : strcmp(x->id, y->id);
}

void hiscore_top_csv(int track, enum hiscore_scope scope, int n, char* out, size_t outsz)
{
        GHashTableIter iter;
        gpointer key, value;
        Entry* entries;
        guint size, used = 0, i;
        size_t len = 0;

        if (outsz == 0) return;
        out[0] = '\0';
        if (!table || n <= 0 || track < 0 || track >= HISCORE_TRACKS) return;
        rollover_if_needed();
        size = g_hash_table_size(table);
        if (size == 0) return;
        entries = g_new(Entry, size);
        g_hash_table_iter_init(&iter, table);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
                const HiscoreLine* hl = value;
                if (hl->best[scope][track].level) {
                        entries[used].id = key;
                        entries[used].line = hl;
                        entries[used].run = hl->best[scope][track];
                        used++;
                }
        }
        qsort(entries, used, sizeof(Entry), entry_cmp);
        for (i = 0; i < used && (int)i < n; i++) {
                int w = snprintf(out + len, outsz - len, "%s%s#%.*s=%d/%d",
                                 i ? "," : "", entries[i].line->nick, WEEKLY_TAG_LEN,
                                 entries[i].id, entries[i].run.level, entries[i].run.time_ms);
                if (w < 0 || (size_t)w >= outsz - len) {
                        out[len] = '\0';  /* whole entries only */
                        break;
                }
                len += (size_t)w;
        }
        g_free(entries);
}

void hiscore_player_csv(const char* id, int track, char* out, size_t outsz)
{
        HiscoreLine* hl;
        if (outsz == 0) return;
        out[0] = '\0';
        if (!table || !id || !*id || track < 0 || track >= HISCORE_TRACKS) return;
        rollover_if_needed();
        hl = g_hash_table_lookup(table, id);
        if (!hl || (!hl->best[HISCORE_ALLTIME][track].level && !hl->best[HISCORE_WEEK][track].level))
                return;
        snprintf(out, outsz, "%d,%d,%d,%d,%d,%d",
                 hiscore_rank(id, track, HISCORE_ALLTIME),
                 hl->best[HISCORE_ALLTIME][track].level, hl->best[HISCORE_ALLTIME][track].time_ms,
                 hiscore_rank(id, track, HISCORE_WEEK),
                 hl->best[HISCORE_WEEK][track].level, hl->best[HISCORE_WEEK][track].time_ms);
}

time_t hiscore_week_start(void)
{
        rollover_if_needed();
        return (time_t)week_start_day * 86400;
}
