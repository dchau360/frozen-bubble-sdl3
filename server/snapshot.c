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

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "snapshot.h"
#include "log.h"

#define DEFAULT_KEEP 12

static int keep_count(void)
{
        const char* v = getenv("FB_SERVER_SNAPSHOTS");
        if (v && *v) {
                int n = atoi(v);
                return n < 0 ? 0 : n;
        }
        return DEFAULT_KEEP;
}

static gint newest_first(gconstpointer a, gconstpointer b)
{
        return -strcmp(*(const char* const*)a, *(const char* const*)b);
}

/* "<base>.YYYY-MM-DD" exactly, so nothing else beside the file is touched. */
static int is_snapshot_of(const char* name, const char* base)
{
        size_t n = strlen(base);
        const char* d;
        int i;
        if (strncmp(name, base, n) != 0 || name[n] != '.') return 0;
        d = name + n + 1;
        if (strlen(d) != 10) return 0;
        for (i = 0; i < 10; i++) {
                if (i == 4 || i == 7) { if (d[i] != '-') return 0; }
                else if (!g_ascii_isdigit(d[i])) return 0;
        }
        return 1;
}

static void prune(const char* path, int keep)
{
        char* dir = g_path_get_dirname(path);
        char* base = g_path_get_basename(path);
        GDir* d = g_dir_open(dir, 0, NULL);
        GPtrArray* names = g_ptr_array_new_with_free_func(g_free);
        const char* name;
        guint i;

        if (d) {
                while ((name = g_dir_read_name(d)))
                        if (is_snapshot_of(name, base))
                                g_ptr_array_add(names, g_strdup(name));
                g_dir_close(d);
        }
        /* Dates sort as strings, so the newest come first. */
        g_ptr_array_sort(names, newest_first);
        for (i = keep; i < names->len; i++) {
                char* full = g_build_filename(dir, g_ptr_array_index(names, i), NULL);
                if (g_unlink(full) == 0)
                        l1(OUTPUT_TYPE_INFO, "Removed old snapshot %s", full);
                g_free(full);
        }
        g_ptr_array_free(names, TRUE);
        g_free(base);
        g_free(dir);
}

void snapshot_week(const char* path, long week_start_day)
{
        int keep = keep_count();
        GDateTime* dt;
        char* date;
        char* contents = NULL;
        gsize len = 0;
        char* out;
        GError* err = NULL;

        if (!path || keep == 0) return;
        if (!g_file_get_contents(path, &contents, &len, NULL))
                return;   /* nothing saved yet: nothing to back up */
        dt = g_date_time_new_from_unix_utc((gint64)week_start_day * 86400);
        date = g_date_time_format(dt, "%Y-%m-%d");
        g_date_time_unref(dt);
        out = g_strdup_printf("%s.%s", path, date);
        g_free(date);
        if (g_file_set_contents(out, contents, len, &err))
                l1(OUTPUT_TYPE_INFO, "Saved snapshot %s", out);
        else {
                l2(OUTPUT_TYPE_ERROR, "Failed to save snapshot %s: %s", out, err->message);
                g_error_free(err);
        }
        g_free(out);
        g_free(contents);
        prune(path, keep);
}
