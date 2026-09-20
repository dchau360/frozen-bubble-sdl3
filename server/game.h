/*******************************************************************************
 *
 * Copyright (c) 2004 Guillaume Cottenceau
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

#include <stdlib.h>
#include <stdarg.h>

void player_connects(int fd);
void player_disconnects(int fd);

void calculate_list_games(void);

void player_part_game(int fd);
void player_part_game_(int fd, char* reason);

int process_msg(int fd, char* msg);

ssize_t get_reset_amount_transmitted(void);
void process_msg_prio(int fd, char* msg, ssize_t len);

extern char* nick[256];
extern char* geoloc[256];
extern char* IP[256];
extern int remote_proto_minor[256];
extern int admin_authorized[256];
extern int is_bot[256];
extern int bots_connected;
/* Single-char self-reported tags, 0 when the client never said. Plain chars
 * rather than the char* nick[]/geoloc[] use, since both are one byte from a
 * fixed set -- nothing to allocate, nothing to free. See the PLATFORM command
 * handler and the 'i' opcode sniff (both game.c) for the accepted values. */
extern char platform_tag[256];
extern char input_tag[256];
/* ISO 3166-1 alpha-2 from the COUNTRY command, empty when never sent. Two
 * chars plus a terminator rather than the bare char the other two use. */
extern char country_tag[256][3];

int game_has_room(int fd);
int game_tournament_start(int tid, int mid, int round, int a, int fd_a, int b, int fd_b, const char *options);
void game_tournament_retire(int tid, int mid, int round);
