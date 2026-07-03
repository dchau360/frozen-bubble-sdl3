/*******************************************************************************
 *
 * Copyright (c) 2004-2012 Guillaume Cottenceau
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>

#include "stats.h"
#include "log.h"

// Global hash table for player stats (nick -> PlayerStats*)
static GHashTable* stats_table = NULL;

// Stats file path
static char* stats_file_path = NULL;

// Helper: get today's date at midnight as time_t
static time_t get_today_midnight(void)
{
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    tm_info->tm_hour = 0;
    tm_info->tm_min = 0;
    tm_info->tm_sec = 0;
    return mktime(tm_info);
}

// Helper: check if stats need daily reset
static void check_daily_reset(PlayerStats* ps)
{
    time_t today = get_today_midnight();
    time_t last = ps->last_reset;

    // If last_reset is from a different day, reset stats
    if (difftime(today, last) >= 86400) {  // 86400 seconds = 1 day
        ps->wins = 0;
        ps->losses = 0;
        ps->last_reset = today;
        l1(OUTPUT_TYPE_INFO, "Reset daily stats for player: %s", ps->nick);
    }
}

// Helper: create stats file directory if needed
static void ensure_stats_dir(void)
{
    if (!stats_file_path) return;

    // Find last slash to get directory
    char* path_copy = strdup(stats_file_path);
    char* last_slash = strrchr(path_copy, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir(path_copy, 0755);  // Create directory if it doesn't exist
    }
    free(path_copy);
}

// Initialize stats system
void stats_init(void)
{
    stats_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    // Determine stats file path
    const char* home = getenv("HOME");
    if (home) {
        stats_file_path = g_strdup_printf("%s/.fb-server/stats.dat", home);
    } else {
        stats_file_path = g_strdup("/var/lib/fb-server/stats.dat");
    }

    ensure_stats_dir();

    // Try to load existing stats
    FILE* f = fopen(stats_file_path, "r");
    if (!f) {
        l0(OUTPUT_TYPE_INFO, "No existing stats file, starting fresh");
        return;
    }

    time_t today = get_today_midnight();
    char line[1024];
    int loaded = 0;

    while (fgets(line, sizeof(line), f)) {
        // Format: nick wins losses last_reset
        char nick_buf[256];
        int wins, losses;
        time_t last_reset;

        if (sscanf(line, "%255s %d %d %ld", nick_buf, &wins, &losses, &last_reset) == 4) {
            PlayerStats* ps = g_new0(PlayerStats, 1);
            ps->nick = g_strdup(nick_buf);
            ps->wins = wins;
            ps->losses = losses;
            ps->last_reset = last_reset;

            // Check for daily reset on load
            check_daily_reset(ps);

            g_hash_table_insert(stats_table, g_strdup(nick_buf), ps);
            loaded++;
        }
    }

    fclose(f);
    l1(OUTPUT_TYPE_INFO, "Loaded stats for %d players", loaded);
}

// Save stats to file
void stats_save(void)
{
    if (!stats_table || !stats_file_path) return;

    FILE* f = fopen(stats_file_path, "w");
    if (!f) {
        l1(OUTPUT_TYPE_ERROR, "Failed to save stats file: %s", stats_file_path);
        return;
    }

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, stats_table);

    while (g_hash_table_iter_next(&iter, &key, &value)) {
        PlayerStats* ps = (PlayerStats*)value;
        fprintf(f, "%s %d %d %ld\n", ps->nick, ps->wins, ps->losses, ps->last_reset);
    }

    fclose(f);
    l1(OUTPUT_TYPE_INFO, "Saved stats to %s", stats_file_path);
}

// Record a win for a player
void stats_record_win(const char* nick)
{
    if (!stats_table || !nick) return;

    PlayerStats* ps = g_hash_table_lookup(stats_table, nick);
    if (!ps) {
        // Create new stats entry
        ps = g_new0(PlayerStats, 1);
        ps->nick = g_strdup(nick);
        ps->wins = 0;
        ps->losses = 0;
        ps->last_reset = get_today_midnight();
        g_hash_table_insert(stats_table, g_strdup(nick), ps);
    }

    check_daily_reset(ps);
    ps->wins++;

    l1(OUTPUT_TYPE_INFO, "Recorded win for %s", nick);

    stats_save();
}

// Record a loss for a player
void stats_record_loss(const char* nick)
{
    if (!stats_table || !nick) return;

    PlayerStats* ps = g_hash_table_lookup(stats_table, nick);
    if (!ps) {
        // Create new stats entry
        ps = g_new0(PlayerStats, 1);
        ps->nick = g_strdup(nick);
        ps->wins = 0;
        ps->losses = 0;
        ps->last_reset = get_today_midnight();
        g_hash_table_insert(stats_table, g_strdup(nick), ps);
    }

    check_daily_reset(ps);
    ps->losses++;

    l1(OUTPUT_TYPE_INFO, "Recorded loss for %s", nick);

    stats_save();
}

// Get stats for a player
PlayerStats* stats_get(const char* nick)
{
    if (!stats_table || !nick) return NULL;

    PlayerStats* ps = g_hash_table_lookup(stats_table, nick);
    if (ps) {
        check_daily_reset(ps);
    }
    return ps;
}

// Clean up stats system
void stats_cleanup(void)
{
    if (stats_table) {
        stats_save();
        g_hash_table_destroy(stats_table);
        stats_table = NULL;
    }
    if (stats_file_path) {
        g_free(stats_file_path);
        stats_file_path = NULL;
    }
}
