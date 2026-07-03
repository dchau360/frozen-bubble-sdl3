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

#ifndef STATS_H
#define STATS_H

#include <time.h>

typedef struct {
    char* nick;
    int wins;
    int losses;
    time_t last_reset;  // Date when stats were last reset (for daily rollover)
} PlayerStats;

// Initialize stats system (load from file, set up hash table)
void stats_init(void);

// Save stats to file
void stats_save(void);

// Record a win for a player (by nick)
void stats_record_win(const char* nick);

// Record a loss for a player (by nick)
void stats_record_loss(const char* nick);

// Get stats for a player (returns NULL if not found)
PlayerStats* stats_get(const char* nick);

// Clean up stats system (free memory)
void stats_cleanup(void);

#endif
