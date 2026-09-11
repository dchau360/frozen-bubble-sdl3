/*******************************************************************************
 *
 * Copyright (c) 2004-2012 Guillaume Cottenceau
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

/*
 * this file holds game operations: create, join, list etc.
 * it should be as far away as possible from network operations
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <regex.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <glib.h>

#include "net.h"
#include "ws.h"
#include "tools.h"
#include "log.h"
#include "game.h"
#include "stats.h"
#include "discordalert.h"
#include "tournament.h"

enum game_status { GAME_STATUS_OPEN, GAME_STATUS_CLOSED, GAME_STATUS_PLAYING };

#define MAX_PLAYERS_PER_GAME 20
struct game
{
        enum game_status status;
        int tournament_id, tournament_match, tournament_round;
        int players_number;
        int max_players;
        int players_conn[MAX_PLAYERS_PER_GAME];
        char* players_nick[MAX_PLAYERS_PER_GAME];
        int players_started[MAX_PLAYERS_PER_GAME];
        /* Per-game wire id for each player: 'A' + join order, monotonically
         * assigned and never reused within a game. Replaces the raw fd in
         * GAME_CAN_START and the synthesized leave message -- fds 10/13/44
         * collide with the \n/\r/, framing and corrupt the client's parse. */
        int players_id[MAX_PLAYERS_PER_GAME];
        int next_player_id;

        /* For the Discord match-result alert (discordalert_fire_result_event,
         * fired from a sniffed 'F' in process_msg_prio_). None of these
         * change anything about how the game is played -- all three are
         * read back, never acted on. */
        int game_mode;      /* raw GAMEMODE:%d from the room's last SETOPTIONS,
                              * or 0 (Classic) if none was ever set -- see
                              * setoptions() below. */
        int result_posted;  /* guards against posting the same round twice;
                              * reset on 'n' (ready for next round). */

        /* Rounds won so far this room, and each seat's team -- both indexed
         * the same as players_nick[]/players_id[] and shifted the same way
         * on departure (see player_part_game_). Used by report_round_result()
         * below to announce win-counts, top scorers and team wins to the
         * lobby; never read by anything that affects actual gameplay. */
        int players_wins[MAX_PLAYERS_PER_GAME];
        int team_count;               /* TEAMCOUNT from the room's last SETOPTIONS,
                                        * or 0 if none was ever set (no teams). */
        int players_team[MAX_PLAYERS_PER_GAME]; /* kNoTeam(0) or 1..5; only slots
                                        * 0-4 are ever set by parse_teams() below --
                                        * SETOPTIONS has no PLAYERTEAM_Pn field past
                                        * P5, same cap NUMCOLORS_Pn/AIMGUIDE_Pn already
                                        * have (src/mainmenu_teampanel.cpp). */

        int game_id;        /* opaque, monotonically-assigned key identifying
                              * this room to the relay across every round it
                              * plays -- see next_game_id() below. Lets the
                              * relay group a room's round-result alerts into
                              * one Discord thread without relying on
                              * players_nick[0], which is not a stable room
                              * name: it is CREATE's argument, but a departure
                              * shifts the whole players_nick[] array down
                              * (see player leave handling), so it can change
                              * mid-room. */
};

static GList * games = NULL;
static GList * open_players = NULL;

static ssize_t amount_transmitted = 0;

static char ok_pong[] = "PONG";
static char ok_player_joined[] = "JOINED: %s";
static char ok_player_parted[] = "PARTED: %s";
static char ok_player_kicked[] = "KICKED: %s";
static char ok_talk[] = "TALK: %s: %s";
static char ok_can_start[] = "GAME_CAN_START: %s";

static char wn_unknown_command[] = "UNKNOWN_COMMAND";
static char wn_missing_arguments[] = "MISSING_ARGUMENTS";
static char wn_nick_invalid[] = "INVALID_NICK";
static char wn_nick_in_use[] = "NICK_IN_USE";
static char wn_no_such_game[] = "NO_SUCH_GAME";
static char wn_game_full[] = "GAME_FULL";
static char wn_already_in_game[] = "ALREADY_IN_GAME";
static char wn_max_open_games[] = "ALREADY_MAX_OPEN_GAMES";
static char wn_not_started[] = "NOT_STARTED";
static char wn_already_ok_started[] = "ALREADY_OK_STARTED";
static char wn_not_in_game[] = "NOT_IN_GAME";
static char wn_alone_in_the_dark[] = "ALONE_IN_THE_DARK";
static char wn_not_creator[] = "NOT_CREATOR";
static char wn_no_such_player[] = "NO_SUCH_PLAYER";
static char wn_denied[] = "DENIED";
static char wn_flooding[] = "FLOODING";
static char wn_others_not_ready[] = "OTHERS_NOT_READY";
static char wn_report_failed[] = "REPORT_FAILED";
static char wn_bot_limit_reached[] = "BOT_LIMIT_REACHED";

/* Absolute path for the abuse-report log. The server daemonizes with cwd="/"
 * (see net.c), so a bare relative fopen() lands in the filesystem root and
 * fails on any sane install -- which would silently drop every report while
 * still telling the reporter it was filed. Same precedence as notify.c and
 * stats.c: explicit env var, then $HOME, then a system-wide path. Resolved
 * once and cached; the directory is created by whichever of those modules
 * gets there first, and fopen() failing is handled by the caller either way. */
static const char* report_file_path(void)
{
        static char* path = NULL;
        if (path) return path;

        const char* explicit_path = getenv("FB_SERVER_REPORT_FILE");
        const char* home = getenv("HOME");
        if (explicit_path && *explicit_path) {
                path = g_strdup(explicit_path);
        } else if (home) {
                path = g_strdup_printf("%s/.fb-server/reports.log", home);
        } else {
                path = g_strdup("/var/lib/fb-server/reports.log");
        }

        /* mkdir the containing directory rather than assuming notify_init()
         * already did: reports must work on a server with the follow feature
         * entirely unconfigured. */
        char* dir = g_strdup(path);
        char* last_slash = strrchr(dir, '/');
        if (last_slash && last_slash != dir) {
                *last_slash = '\0';
                mkdir(dir, 0755);
        }
        g_free(dir);

        l1(OUTPUT_TYPE_INFO, "Abuse report file: %s", path);
        return path;
}

/* reports.log is one record per line and is read by a human deciding whether
 * to act on a player. Both the reported nick and the reason are attacker-
 * chosen -- this is a raw line protocol, so nothing stops a hand-rolled client
 * from putting a newline in either and forging extra entries that frame an
 * innocent player or invent a reporter. Fold anything non-printable to '.' so
 * one report can only ever be one line. */
static void sanitize_report_field(const char* in, char* out, size_t outsz)
{
        size_t i = 0;
        for (; in[i] && i + 1 < outsz; i++) {
                unsigned char c = (unsigned char)in[i];
                out[i] = (c < 0x20 || c == 0x7f) ? '.' : in[i];
        }
        out[i] = '\0';
}

static char fl_line_unrecognized[] = "MISSING_FB_PROTOCOL_TAG";
static char fl_proto_mismatch[] = "INCOMPATIBLE_PROTOCOL";

char* nick[256];
char* geoloc[256];
char* IP[256];
int remote_proto_minor[256];
int admin_authorized[256];
// Set by the BOT command; cleared and counted back down in conn_terminated
// (net.c), the same place nick[]/geoloc[]/IP[] are torn down for this fd.
int is_bot[256];
int bots_connected = 0;

// calculate the list of players for a given game
static char* list_game(const struct game * g)
{
        char list_game_str[8192] = "";
        int i;
        for (i = 0; i < g->players_number; i++) {
                strconcat(list_game_str, g->players_nick[i], sizeof(list_game_str));
                if (i < g->players_number - 1)
                        strconcat(list_game_str, ",", sizeof(list_game_str));
        }
        return memdup(list_game_str, strlen(list_game_str) + 1);
}

// calculate the list of players for a given game with geolocation
static char* list_game_with_geolocation(const struct game * g)
{
        char list_game_str[8192] = "";
        int i;
        char* n;
        for (i = 0; i < g->players_number; i++) {
                strconcat(list_game_str, g->players_nick[i], sizeof(list_game_str));
                n = geoloc[g->players_conn[i]];
                if (n != NULL) {
                        strconcat(list_game_str, ":", sizeof(list_game_str));
                        strconcat(list_game_str, n, sizeof(list_game_str));
                }
                if (i < g->players_number - 1)
                        strconcat(list_game_str, ",", sizeof(list_game_str));
        }
        return memdup(list_game_str, strlen(list_game_str) + 1);
}

static char list_games_str[16384] __attribute__((aligned(4096))) = "";
static char list_playing_geolocs_str[16384] __attribute__((aligned(4096))) = "";
static int players_in_game;
/* Players seated in rooms that have not started. Counted separately from
 * players_in_game, which feeds the `playing:` field and must keep meaning
 * "in a running game" — but both have to come off the `free:` count, which
 * previously subtracted only players_in_game and so reported everyone waiting
 * in an open room as free, contradicting the open-player list in the same
 * message (audit finding BUG-050). */
static int players_seated_open;
static int games_open;
static int games_running;
static void list_open_nicks_aux(gpointer data, gpointer user_data)
{
        (void)user_data;
        char* n = nick[GPOINTER_TO_INT(data)];
        if (n == NULL)
                return;
        strconcat(list_games_str, n, sizeof(list_games_str));
        n = geoloc[GPOINTER_TO_INT(data)];
        if (n != NULL) {
                strconcat(list_games_str, ":", sizeof(list_games_str));
                strconcat(list_games_str, n, sizeof(list_games_str));
        }
        strconcat(list_games_str, ",", sizeof(list_games_str));
}
static void list_games_aux(gpointer data, gpointer user_data)
{
        (void)user_data;
        const struct game* g = data;
        if (g->status == GAME_STATUS_OPEN) {
                char* game;
                char* cap;
                games_open++;
                players_seated_open += g->players_number;
                strconcat(list_games_str, "[", sizeof(list_games_str));
                game = list_game(g);
                strconcat(list_games_str, game, sizeof(list_games_str));
                free(game);
                strconcat(list_games_str, "]", sizeof(list_games_str));
                /* Room cap after the bracket: "[nick,nick]:20". Sits in the
                 * inter-bracket gap every known LIST parser (Perl regex, C++
                 * bracket scan) already skips, so legacy clients ignore it. */
                cap = asprintf_(":%d", g->max_players);
                strconcat(list_games_str, cap, sizeof(list_games_str));
                free(cap);
        } else {
                int i;
                char* geo;
                players_in_game += g->players_number;
                games_running++;
                for (i = 0; i < g->players_number; i++) {
                        geo = geoloc[g->players_conn[i]];
                        if (geo != NULL) {
                                strconcat(list_playing_geolocs_str, geo, sizeof(list_playing_geolocs_str));
                                strconcat(list_playing_geolocs_str, ",", sizeof(list_playing_geolocs_str));
                        }
                }
                return;
        }
}
/* Game list is of the following scheme:
 * 1.1 protocol:
 * <list-of-open-players format="NICK|NICK:GEOLOC"> [<list-of-open-games format=<list-of-players format="NICK">>] free:%d games:%d playing:%d at:<list-of-playing-geolocs>
 * 1.0 protocol:
 * <list-of-open-players format="NICK|NICK:GEOLOC"> [<list-of-open-games format=<list-of-players format="NICK">>] free:%d games:%d playing:%d
 */
void calculate_list_games(void)
{
        char * free_players;
        list_games_str[0] = '\0';
        list_playing_geolocs_str[0] = '\0';
        players_in_game = 0;
        players_seated_open = 0;
        games_open = 0;
        games_running = 0;
        g_list_foreach(open_players, list_open_nicks_aux, NULL);
        strconcat(list_games_str, " ", sizeof(list_games_str));
        g_list_foreach(games, list_games_aux, NULL);
        free_players = asprintf_(" free:%d games:%d playing:%d at:%s", conns_nb() - players_in_game - players_seated_open - 1, games_running, players_in_game, list_playing_geolocs_str);  // 1: don't count myself
        strconcat(list_games_str, free_players, sizeof(list_games_str));
        free(free_players);
}

/* Monotonic across the process's whole lifetime, never reused -- see
 * g->game_id's comment. A plain counter is fine here, unlike next_seat_id()'s
 * wraparound dance: this value never touches the wire protocol (it only ever
 * leaves the process in a RESULT datagram to the Discord relay), so it has
 * no framing bytes to avoid and no per-game cap to wrap within. */
static int next_game_id = 1;

static void create_game(int fd, char* nick, int max_players)
{
        struct game * g = malloc_(sizeof(struct game));
        g->tournament_id = g->tournament_match = g->tournament_round = 0;
        g->players_number = 1;
        g->players_conn[0] = fd;
        g->players_id[0] = 'A';
        g->next_player_id = 'A' + 1;
        g->players_nick[0] = nick;
        g->status = GAME_STATUS_OPEN;
        g->max_players = max_players;
        g->game_mode = 0;      /* Classic, until/unless SETOPTIONS says otherwise */
        g->result_posted = 0;
        {
                int k;
                for (k = 0; k < MAX_PLAYERS_PER_GAME; k++) {
                        g->players_wins[k] = 0;
                        g->players_team[k] = 0;
                }
        }
        g->team_count = 0;
        g->game_id = next_game_id++;
        games = g_list_append(games, g);
        open_players = g_list_remove(open_players, GINT_TO_POINTER(fd));
        calculate_list_games();
}

/* Allocate the next per-game seat id, wrapping within the printable
 * single-byte window ['A', 'z'] (65-122) and skipping any id still held
 * by a currently-seated player. g->next_player_id is monotonic over the
 * whole lifetime of the game object, which persists as long as >=1
 * player remains -- a long-lived room with a revolving cast of joiners
 * can rack up hundreds of cumulative joins, so a raw counter would
 * eventually wrap into a wire framing byte (0/'\n'/'\r'/',') or collide
 * with a still-present player's live id. This window structurally
 * excludes those framing bytes and never exceeds signed-char range, and
 * it holds 58 values versus the MAX_PLAYERS_PER_GAME cap of 20, so the
 * skip-scan below is always guaranteed to find a free id.
 */
static int next_seat_id(struct game * g)
{
        int id = g->next_player_id;
        int i;
        for (;;) {
                if (id > 'z')
                        id = 'A';
                for (i = 0; i < g->players_number; i++)
                        if (g->players_id[i] == id)
                                break;
                if (i == g->players_number)
                        break;   /* id is free */
                id++;
        }
        g->next_player_id = id + 1;
        return id;
}

static int add_player(struct game * g, int fd, char* nick)
{
        char joined_msg[1000];
        int i;
        if (g->max_players > 5 && remote_proto_minor[fd] < 3) {
                /* Legacy (pre-1.3) clients can't render a room shaped for >5
                 * players. Reject them here and let the JOIN handler reply with
                 * the existing wn_game_full ("GAME_FULL") string -- deliberate
                 * reuse, not a new wire message, since legacy clients only
                 * understand the warning strings that already existed in their
                 * build. */
                free(nick);
                return 0;   /* JOIN handler already replies wn_game_full */
        }
        if (g->players_number < g->max_players) {
                /* inform other players */
                snprintf(joined_msg, sizeof(joined_msg), ok_player_joined, nick);
                for (i = 0; i < g->players_number; i++)
                        send_line_log_push(g->players_conn[i], joined_msg);

                g->players_conn[g->players_number] = fd;
                g->players_id[g->players_number] = next_seat_id(g);
                g->players_nick[g->players_number] = nick;
                g->players_number++;
                open_players = g_list_remove(open_players, GINT_TO_POINTER(fd));
                calculate_list_games();
                return 1;
        } else {
                free(nick);
                return 0;
        }
}

static int find_game_by_nick_aux(gconstpointer game, gconstpointer nick)
{
        const struct game * g = game;
        if (g->status == GAME_STATUS_OPEN
            && streq(g->players_nick[0], (char *) nick))
                return 0;
        else
                return 1;
}
static struct game* find_game_by_nick(char* nick)
{
        return GListp2data(g_list_find_custom(games, nick, find_game_by_nick_aux));
}

static int find_game_by_fd_aux(gconstpointer game, gconstpointer fd)
{
        const struct game* g = game;
        int fd_ = GPOINTER_TO_INT(fd);
        int i;
        for (i = 0; i < g->players_number; i++)
                if (g->players_conn[i] == fd_)
                        return 0;
        return 1;
}
static struct game* find_game_by_fd(int fd)
{
        return GListp2data(g_list_find_custom(games, GINT_TO_POINTER(fd), find_game_by_fd_aux));
}

/* Is this game still in the games list, or has a re-entrant teardown freed it?
   Every site that frees a game removes it from `games` immediately beforehand,
   so list membership is the liveness test. Callers that relay to other players
   must use this before touching a game pointer again: a failed send terminates
   the destination connection, which re-enters player_part_game_ on the same
   game and can free it underneath the caller. */
static int game_is_live(const struct game *g)
{
        return g != NULL && g_list_find(games, g) != NULL;
}

/* Seat of this connection, or -1 if it does not hold one. Callers that can
   legitimately ask about a connection already removed from the game -- the
   departure relay does exactly that -- must use this rather than
   find_player_number, which treats absence as fatal. */
static int find_player_slot(const struct game *g, int fd)
{
        int i;
        for (i = 0; i < g->players_number; i++)
                if (g->players_conn[i] == fd)
                        return i;
        return -1;
}

int find_player_number(struct game *g, int fd)
{
        int i = find_player_slot(g, fd);
        if (i < 0) {
                l0(OUTPUT_TYPE_ERROR, "Internal error");
                exit(EXIT_FAILURE);
        }
        return i;
}

/* Comma-joined roster for the Discord match-result alert. Every name in
 * g->players_nick[] already passed through is_nick_ok() when its owner
 * connected or joined -- unlike the winner claim in a sniffed 'F' payload
 * (see process_msg_prio_), which is not validated at all. Bounded the same
 * way mapping_str is below: MAX_PLAYERS_PER_GAME nicks of at most 10 chars
 * each comfortably fit 512 bytes with room for the commas. */
static void build_roster_csv(struct game* g, char* out, size_t outsz)
{
        int i;
        out[0] = '\0';
        for (i = 0; i < g->players_number; i++) {
                if (i > 0)
                        strconcat(out, ",", outsz);
                strconcat(out, g->players_nick[i], outsz);
        }
}

static void real_start_game(struct game* g)
{
        int i;
        char mapping_str[4096] = "";
        char can_start_msg[1000];
        for (i = 0; i < g->players_number; i++) {
                size_t len = strlen(mapping_str);
                if (len >= sizeof(mapping_str)-1)
                        return;
                mapping_str[len] = g->players_id[i];
                mapping_str[len+1] = '\0';
                strconcat(mapping_str, g->players_nick[i], sizeof(mapping_str));
                if (i < g->players_number - 1)
                        strconcat(mapping_str, ",", sizeof(mapping_str));
        }
        snprintf(can_start_msg, sizeof(can_start_msg), ok_can_start, mapping_str);
        for (i = 0; i < g->players_number; i++) {
                send_line_log_push_binary(g->players_conn[i], can_start_msg, ok_can_start);
                g->players_started[i] = 0;
        }
        g->status = GAME_STATUS_PLAYING;
}

static void start_game(int fd)
{
        struct game * g = find_game_by_fd(fd);
        if (g) {
                if (g->players_conn[0] == fd) {
                        if (g->players_number == 1) {
                                send_line_log(fd, wn_alone_in_the_dark, "START");
                                return;
                        }
                        send_ok(fd, "START");
                        real_start_game(g);
                        calculate_list_games();
                        l2(OUTPUT_TYPE_INFO, "running games increments to: %d (%d players)", games_running, players_in_game);
                } else {
                        send_line_log(fd, wn_not_creator, "START");
                }

        } else {
                l0(OUTPUT_TYPE_ERROR, "Internal error");
                exit(EXIT_FAILURE);
        }
}

static void close_game(int fd)
{
        struct game * g = find_game_by_fd(fd);
        if (g) {
                if (g->players_conn[0] == fd) {
                        if (g->players_number == 1) {
                                send_line_log(fd, wn_alone_in_the_dark, "CLOSE");
                                return;
                        }
                        send_ok(fd, "CLOSE");
                        g->status = GAME_STATUS_CLOSED;
                        calculate_list_games();
                } else {
                        send_line_log(fd, wn_not_creator, "CLOSE");
                }

        } else {
                l0(OUTPUT_TYPE_ERROR, "Internal error");
                exit(EXIT_FAILURE);
        }
}

static int min_protocol_level(struct game* g)
{
        int i;
        int minor = remote_proto_minor[g->players_conn[0]];
        for (i = 1; i < g->players_number; i++)
                minor = MIN(minor, remote_proto_minor[g->players_conn[i]]);
        return minor;
}

/* Pulls GAMEMODE:%d out of a SETOPTIONS string, purely for labelling the
 * Discord match-result alert -- the server has no other use for game mode
 * and does not otherwise interpret this string at all (setoptions() below
 * relays it to clients verbatim, same as always). Missing key or an
 * out-of-range value both fall back to 0 (Classic): a value this server
 * doesn't recognize should degrade to "unlabelled", not crash or read out
 * of the mode-name table's bounds. See src/gamemode.h for the 0-3 mapping. */
static int parse_game_mode(const char* options)
{
        const char* key = strstr(options, "GAMEMODE:");
        if (!key) return 0;
        int mode = atoi(key + strlen("GAMEMODE:"));
        return (mode >= 0 && mode <= 3) ? mode : 0;
}

/* Extract TEAMCOUNT and PLAYERTEAM_P1..P5 from the room's SETOPTIONS string
 * into g->team_count / g->players_team[0..4], the same way parse_game_mode()
 * above pulls out GAMEMODE. Both keys already ride along on every SETOPTIONS
 * (src/networkclient.cpp's SendOptions) -- the server has just never read
 * them before now, only relayed them opaquely to other clients, same as it
 * still does. A missing key or an out-of-range value both fall back to "no
 * teams"/"no team" (0/kNoTeam): a value this server doesn't recognize should
 * degrade to untracked, not crash or index players_team[] out of range. */
static void parse_teams(struct game* g, const char* options)
{
        const char* key = strstr(options, "TEAMCOUNT:");
        int tc = key ? atoi(key + strlen("TEAMCOUNT:")) : 0;
        g->team_count = (tc >= 0 && tc <= 5) ? tc : 0;

        int p;
        for (p = 0; p < 5; p++) {
                char name[16];
                snprintf(name, sizeof(name), "PLAYERTEAM_P%d:", p + 1);
                key = strstr(options, name);
                int t = key ? atoi(key + strlen(name)) : 0;
                g->players_team[p] = (t >= 0 && t <= 5) ? t : 0;
        }
}

static void setoptions(int fd, char* options)
{
        struct game * g = find_game_by_fd(fd);
        if (g) {
                if (g->players_conn[0] == fd) {
                        int i;
                        char* msg;
                        g->game_mode = parse_game_mode(options);
                        parse_teams(g, options);
                        send_ok(fd, "SETOPTIONS");
                        msg = asprintf_("OPTIONS: %s,PROTOCOLLEVEL:%d", options, min_protocol_level(g));
                        for (i = 0; i < g->players_number; i++)
                                if (remote_proto_minor[g->players_conn[i]] >= 1)
                                        send_line_log_push(g->players_conn[i], msg);
                        free(msg);
                } else {
                        send_line_log(fd, wn_not_creator, "SETOPTIONS");
                }

        } else {
                l0(OUTPUT_TYPE_ERROR, "Internal error");
                exit(EXIT_FAILURE);
        }
}

static void leader_check_game_start(int fd)
{
        struct game * g = find_game_by_fd(fd);
        if (g) {
                if (g->status == GAME_STATUS_PLAYING) {
                        int i;
                        for (i = 0; i < g->players_number; i++) {
                                if (fd != g->players_conn[i]) {
                                        if (!g->players_started[i]) {
                                                send_line_log(fd, wn_others_not_ready, "LEADER_CHECK_GAME_START");
                                                return;
                                        }
                                }
                        }
                        send_ok(fd, "LEADER_CHECK_GAME_START");
                } else {
                        send_line_log(fd, wn_not_started, "LEADER_CHECK_GAME_START");
                }
        } else {
                l0(OUTPUT_TYPE_ERROR, "Internal error");
                exit(EXIT_FAILURE);
        }
}

static void ok_start_game(int fd)
{
        struct game * g = find_game_by_fd(fd);
        if (g) {
                if (g->status == GAME_STATUS_PLAYING) {
                        int i;
                        for (i = 0; i < g->players_number; i++) {
                                if (g->players_conn[i] == fd) {
                                        if (!g->players_started[i]) {
                                                if (remote_proto_minor[g->players_conn[i]] >= 1)
                                                        send_ok(fd, "OK_GAME_START");
                                                g->players_started[i] = 1;
                                                l1(OUTPUT_TYPE_DEBUG, "[%d] entering prio mode", g->players_conn[i]);
                                                add_prio(g->players_conn[i]);
                                        } else {
                                                send_line_log(fd, wn_already_ok_started, "OK_GAME_START");
                                        }
                                }
                        }
                } else {
                        send_line_log(fd, wn_not_started, "OK_GAME_START");
                }
        } else {
                l0(OUTPUT_TYPE_ERROR, "Internal error");
                exit(EXIT_FAILURE);
        }
}

static void kick_player(int fd, struct game * g, char * nick)
{
        int i;
        for (i = 0; i < g->players_number; i++) {
                if (g->players_conn[i] != fd && streq(g->players_nick[i], nick)) {
                        send_ok(fd, "KICK");
                        send_line_log_push(g->players_conn[i], "KICKED");
                        player_part_game_(g->players_conn[i], ok_player_kicked);
                        return;
                }
        }
        send_line_log(fd, wn_no_such_player, "KICK");
}

void player_connects(int fd)
{
        tournament_connect(fd);
        open_players = g_list_append(open_players, GINT_TO_POINTER(fd));
}

void player_disconnects(int fd)
{
        tournament_disconnect(fd);
        open_players = g_list_remove(open_players, GINT_TO_POINTER(fd));
}

static void talk_serverwide_aux(gpointer data, gpointer user_data)
{
        send_line_log_push(GPOINTER_TO_INT(data), user_data);
}

static gboolean check_match_alert_words(gconstpointer data, gconstpointer user_data)
{
        const regex_t* preg = data;
        const char* msg = user_data;
        if (regexec(preg, msg, 0, NULL, 0) == 0) {
                return TRUE;
        }
        return FALSE;
}

static void talk(int fd, char* msg)
{
        struct game * g = find_game_by_fd(fd);
        char talk_msg[1000];

        if (g_list_any(alert_words, check_match_alert_words, msg))
                l2(OUTPUT_TYPE_INFO, "message '%s' from %s matches alert words!", msg, IP[fd]);

        amount_talk_flood[fd]++;
        if (amount_talk_flood[fd] == 15) {
                l1(OUTPUT_TYPE_INFO, "'%s' is flooding!", IP[fd]);
                send_line_log(fd, wn_flooding, msg);
                conn_terminated(fd, "flooding");
                return;
        }

        snprintf(talk_msg, sizeof(talk_msg), ok_talk, nick[fd] ? nick[fd] : "?", msg);
        if (g) {
                // player is in a game, it's a game-only chat
                int i;
                for (i = 0; i < g->players_number; i++)
                        send_line_log_push(g->players_conn[i], talk_msg);
        } else {
                // player is not in a game, it's a server-wide chat
                g_list_foreach(open_players, talk_serverwide_aux, talk_msg);
        }
}

static void status(int fd, char* msg)
{
        struct game * g = find_game_by_fd(fd);
        if (g) {
                char* game = list_game(g);
                send_line_log(fd, game, msg);
                free(game);
        } else {
                send_line_log(fd, wn_not_in_game, msg);
        }
}

static void status_geo(int fd, char* msg)
{
        struct game * g = find_game_by_fd(fd);
        if (g) {
                char* game = list_game_with_geolocation(g);
                send_line_log(fd, game, msg);
                free(game);
        } else {
                send_line_log(fd, wn_not_in_game, msg);
        }
}

static void protocol_level(int fd, char* msg)
{
        // Find the smallest minor protocol level among players in game
        struct game * g = find_game_by_fd(fd);
        if (g) {
                char* response;
                int level = min_protocol_level(g);
                response = asprintf_("%d", level);
                send_line_log(fd, response, msg);
                free(response);
        } else {
                send_line_log(fd, wn_not_in_game, msg);
        }
}

static gboolean nick_available_aux(gconstpointer data, gconstpointer user_data)
{
        const struct game* g = data;
        const char* nick = user_data;
        int i;
        for (i = 0; i < g->players_number; i++)
                if (streq(g->players_nick[i], nick))
                        return TRUE;
        return FALSE;
}
static int nick_available(char* nick)
{
        return !g_list_any(games, nick_available_aux, nick);
}

static gboolean already_in_game_aux(gconstpointer data, gconstpointer user_data)
{
        const struct game* g = data;
        int fd = GPOINTER_TO_INT(user_data);
        int i;
        for (i = 0; i < g->players_number; i++)
                if (g->players_conn[i] == fd)
                        return TRUE;
        return FALSE;
}
static int already_in_game(int fd)
{
        return g_list_any(games, already_in_game_aux, GINT_TO_POINTER(fd));
}

static int is_nick_ok(char* nick)
{
        size_t i;
        /* An empty nick passed: the character loop below runs zero times and
         * falls through to success. `CREATE ` therefore allocated a room with an
         * empty name, which known LIST parsers cannot enumerate — the room
         * exists and occupies one of the 16 open slots but no client can see or
         * join it (audit finding BUG-009). Applies to NICK as well, which shares
         * this validator. */
        if (nick[0] == '\0')
                return 0;
        if (strlen(nick) > 10)
                return 0;
        for (i = 0; i < strlen(nick); i++) {
                if (!((nick[i] >= 'a' && nick[i] <= 'z')
                      || (nick[i] >= 'A' && nick[i] <= 'Z')
                      || (nick[i] >= '0' && nick[i] <= '9')
                      || nick[i] == '-' || nick[i] == '_')) {
                        return 0;
                }
        }
        return 1;
}

/* true return value indicates that connection must be closed */
int process_msg(int fd, char* msg)
{
        int client_proto_major;
        int client_proto_minor;
        char * args;
        char * ptr, * ptr2;
        char * msg_orig;

        /* check for leading protocol tag */
        if (!str_begins_static_str(msg, "FB/")
            || strlen(msg) < 8) {  // 8 stands for "FB/M.m f"(oo)
                send_line_log(fd, fl_line_unrecognized, msg);
                return 1;
        }
    
        /* check if client protocol is compatible; for simplicity, we don't support client protocol more recent
         * than server protocol, we suppose that our servers are upgraded when a new release appears (but of
         * course client protocol older is supported within the major protocol) */
        client_proto_major = charstar_to_int(msg + 3);
        client_proto_minor = charstar_to_int(msg + 5);
        if (client_proto_major != proto_major
            || client_proto_minor > proto_minor) {
                send_line_log(fd, fl_proto_mismatch, msg);
                return 1;
        }

        if (remote_proto_minor[fd] == -1)
                remote_proto_minor[fd] = client_proto_minor;

        msg_orig = strdup(msg);

        /* after protocol, first word is command, then possible args */
        current_command = msg + 7; // 7 stands for "FB/M.m "
        if ((ptr = strchr(current_command, ' '))) {
                *ptr = '\0';
                args = current_command + strlen(current_command) + 1;
        } else
                args = NULL;

        if (streq(current_command, "TOUR")) {
                tournament_command(fd, args);
        } else if (tournament_active(fd) &&
            (streq(current_command, "CREATE") || streq(current_command, "JOIN") ||
             streq(current_command, "NICK") || streq(current_command, "BOT") ||
             streq(current_command, "SETOPTIONS") || streq(current_command, "START") ||
             streq(current_command, "CLOSE") || streq(current_command, "KICK"))) {
                send_line_log(fd, "TOURNAMENT_ACTIVE", msg_orig);
        } else if (streq(current_command, "PART") && tournament_active(fd)) {
                tournament_withdraw(fd);
                send_ok(fd, msg_orig);
        } else if (streq(current_command, "PING")) {
                send_line_log(fd, ok_pong, msg_orig);
        } else if (streq(current_command, "BOT")) {
                // Self-declared: a bot is otherwise an ordinary connection,
                // indistinguishable from a person at the protocol level, so
                // there is no way to enforce this against a client that lies
                // -- it exists so an honest client (this project's own) can
                // be capped, not as a security boundary. Idempotent: a
                // second BOT from the same fd is a no-op rather than
                // double-counting it against the cap.
                if (is_bot[fd]) {
                        send_ok(fd, msg_orig);
                } else if (bots_connected >= max_bots) {
                        send_line_log(fd, wn_bot_limit_reached, msg_orig);
                } else {
                        is_bot[fd] = 1;
                        bots_connected++;
                        send_ok(fd, msg_orig);
                }
        } else if (streq(current_command, "NICK")) {
                if (!args) {
                        send_line_log(fd, wn_missing_arguments, msg_orig);
                } else {
                        if ((ptr = strchr(args, ' ')))
                                *ptr = '\0';
                        if (strlen(args) > 10)
                                args[10] = '\0';
                        if (!is_nick_ok(args)) {
                                send_line_log(fd, wn_nick_invalid, msg_orig);
                        } else {
                                // Refuse the nick if another connection currently holds it AND is
                                // still actively talking to us (two genuinely live clients — e.g.
                                // two instances on the same machine defaulting to the same OS
                                // username — must not be able to kill each other just by
                                // connecting). Only a same-nick connection that's gone quiet is
                                // treated as a stale ghost and evicted below; see the "ghost
                                // player fix" comment there for why eviction exists at all.
                                int live_collision_fd = -1;
                                {
                                        GList *iter = open_players;
                                        while (iter) {
                                                int other_fd = GPOINTER_TO_INT(iter->data);
                                                iter = iter->next;
                                                if (other_fd != fd && nick[other_fd] != NULL && streq(nick[other_fd], args)
                                                    && conn_recently_active(other_fd)) {
                                                        live_collision_fd = other_fd;
                                                        break;
                                                }
                                        }
                                }
                                if (live_collision_fd != -1) {
                                        send_line_log(fd, wn_nick_in_use, msg_orig);
                                } else {
                                        /* A connection's *first* accepted NICK is the moment a
                                         * player arrives: they now have a name and show up in
                                         * LIST for everyone in the lobby. A later NICK on the
                                         * same fd is a rename, which is not an arrival -- see
                                         * the Discord alert below. */
                                        int first_nick = (nick[fd] == NULL);
                                        int replaced_ghost = 0;
                                        if (nick[fd] != NULL) {
                                                free(nick[fd]);
                                        }
                                        nick[fd] = strdup(args);
                                        // Log nick + IP + timestamp to joiners file
                                        {
                                                FILE *jf = fopen("joiners.log", "a");
                                                if (jf) {
                                                        time_t now = time(NULL);
                                                        struct tm *tm_info = gmtime(&now);
                                                        char tbuf[32];
                                                        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S UTC", tm_info);
                                                        fprintf(jf, "%s  %-15s  %s\n", tbuf, IP[fd] ? IP[fd] : "unknown", nick[fd]);
                                                        fclose(jf);
                                                }
                                        }
                                        // Evict any stale open_players entry with the same nick (ghost player fix).
                                        // When a player reconnects after a silent TCP drop, the old fd stays in
                                        // open_players until gracetime fires, causing duplicate entries in LIST.
                                        // Only genuinely-stale collisions reach here — a live collision already
                                        // branched away above.
                                        {
                                                GList *iter = open_players;
                                                while (iter) {
                                                        int other_fd = GPOINTER_TO_INT(iter->data);
                                                        iter = iter->next;
                                                        if (other_fd != fd && nick[other_fd] != NULL && streq(nick[other_fd], args)) {
                                                                conn_terminated(other_fd, "replaced by new connection with same nick");
                                                                replaced_ghost = 1;
                                                        }
                                                }
                                        }
                                        calculate_list_games();
                                        send_ok(fd, msg_orig);

                                        /* Tell the operator's Discord channel somebody showed up.
                                         *
                                         * This fires on arrival at the *server*, not on a room
                                         * join: a player alone in a room they just created is
                                         * exactly the person an alert should summon company for,
                                         * and by the time a second player has joined their room
                                         * the two have already found each other. The client's own
                                         * copy for the feature promises the same thing --
                                         * "alerts when online players connect".
                                         *
                                         * Two arrivals that aren't: a rename (same connection,
                                         * already announced) and a reconnect after a silent TCP
                                         * drop, which the eviction above has just identified by
                                         * finding this nick still seated on a dead fd. Without
                                         * that second guard a player on a flapping connection
                                         * redials the channel every time their link blinks.
                                         *
                                         * geoloc[fd] is essentially always NULL here -- GEOLOC
                                         * arrives after NICK, and the lookup behind it can take
                                         * ~16s. It stays in the datagram because the wire format
                                         * has the field and the relay discards it either way. */
                                        if (first_nick && !replaced_ghost)
                                                discordalert_fire_join_event(nick[fd], IP[fd], geoloc[fd]);
                                }
                        }
                }
        } else if (streq(current_command, "GEOLOC")) {
                if (!args) {
                        send_line_log(fd, wn_missing_arguments, msg_orig);
                } else {
                        if ((ptr = strchr(args, ' ')))
                                *ptr = '\0';
                        if (strlen(args) > 13)  // sign, 4 digits, dot, colon, sign, 4 digits, dot
                                args[13] = '\0';
                        if (geoloc[fd] != NULL) {
                                free(geoloc[fd]);
                        }
                        geoloc[fd] = strdup(args);
                        calculate_list_games();
                        send_ok(fd, msg_orig);
                }
        } else if (streq(current_command, "CREATE")) {
                if (!args) {
                        send_line_log(fd, wn_missing_arguments, msg_orig);
                } else {
                        int max_players = 5;  // legacy default: clients that don't ask get classic rooms
                        if ((ptr = strchr(args, ' '))) {
                                int mp = charstar_to_int(ptr + 1);
                                if (mp >= 2 && mp <= MAX_PLAYERS_PER_GAME)
                                        max_players = mp;
                                else if (mp > MAX_PLAYERS_PER_GAME) {
                                        max_players = MAX_PLAYERS_PER_GAME;
                                        l2(OUTPUT_TYPE_INFO, "CREATE room cap %d clamped to %d", mp, MAX_PLAYERS_PER_GAME);
                                }
                                else
                                        l1(OUTPUT_TYPE_INFO, "CREATE room cap %d ignored (minimum 2)", mp);
                                *ptr = '\0';
                        }
                        if (strlen(args) > 10)
                                args[10] = '\0';
                        if (!is_nick_ok(args)) {
                                send_line_log(fd, wn_nick_invalid, msg_orig);
                        } else if (!nick_available(args)) {
                                send_line_log(fd, wn_nick_in_use, msg_orig);
                        } else if (already_in_game(fd)) {
                                send_line_log(fd, wn_already_in_game, msg_orig);
                        } else if (games_open == 16) {  // FB client can display 16 max
                                send_line_log(fd, wn_max_open_games, msg_orig);
                        } else {
                                create_game(fd, strdup(args), max_players);
                                send_ok(fd, msg_orig);
                        }
                }
        } else if (streq(current_command, "JOIN")) {
                if (!args || !(ptr = strchr(args, ' '))) {
                        send_line_log(fd, wn_missing_arguments, msg_orig);
                } else {
                        struct game * g;
                        char* joinnick = ptr + 1;
                        *ptr = '\0';
                        if ((ptr2 = strchr(ptr, ' ')))
                                *ptr2 = '\0';
                        if (strlen(joinnick) > 10)
                                joinnick[10] = '\0';
                        if (!is_nick_ok(joinnick)) {
                                send_line_log(fd, wn_nick_invalid, msg_orig);
                        } else if (!nick_available(joinnick)) {
                                send_line_log(fd, wn_nick_in_use, msg_orig);
                        } else if (already_in_game(fd)) {
                                send_line_log(fd, wn_already_in_game, msg_orig);
                        } else if (!(g = find_game_by_nick(args))) {
                                send_line_log(fd, wn_no_such_game, msg_orig);
                        } else {
                                if (add_player(g, fd, strdup(joinnick))) {
                                        /* Update the GLOBAL nick[fd] table so TALK uses the in-game
                                         * nick. The local was named `nick`, which shadowed the
                                         * global char* nick[256] table — so nick[fd] indexed the
                                         * local join-string (a char), not the table. Renamed to
                                         * joinnick so nick[fd] resolves to the global table. */
                                        if (nick[fd] != NULL)
                                                free(nick[fd]);
                                        nick[fd] = strdup(joinnick);
                                        send_ok(fd, msg_orig);
                                } else
                                        send_line_log(fd, wn_game_full, msg_orig);
                        }
                }
        } else if (streq(current_command, "KICK")) {
                if (!args) {
                        send_line_log(fd, wn_missing_arguments, msg_orig);
                } else {
                        if ((ptr = strchr(args, ' ')))
                                *ptr = '\0';
                        if (strlen(args) > 10)
                                args[10] = '\0';
                        if (!already_in_game(fd)) {
                                send_line_log(fd, wn_not_in_game, msg_orig);
                        } else {
                                struct game * g = find_game_by_fd(fd);
                                if (g->players_conn[0] != fd) {
                                        send_line_log(fd, wn_not_creator, msg_orig);
                                } else {
                                        kick_player(fd, g, args);
                                }
                        }
                }
        } else if (streq(current_command, "PART")) {
                if (!already_in_game(fd)) {
                        send_line_log(fd, wn_not_in_game, msg_orig);
                } else {
                        player_part_game(fd);
                        remove_prio(fd);  /* return fd to lobby mode; prevents 5-sec prio gracetime and duplicate add_prio on rejoin */
                        send_ok(fd, msg_orig);
                }
        } else if (streq(current_command, "REPORT")) {
                /* Player-abuse report: "REPORT <nick> <reason>". Appended to a
                 * flat file for the operator to read, exactly like the
                 * joiners.log written on NICK above -- this server has no
                 * moderation tooling and inventing one here would be a much
                 * larger feature than the report path itself.
                 *
                 * Deliberately never acted on automatically. A nick is not an
                 * identity here (no accounts, chosen fresh every connect), so
                 * auto-kicking on report would hand every player a way to
                 * remove anyone they liked. The client-side block list is what
                 * gives the reporter immediate relief; this is the channel for
                 * an operator to see a pattern and act out of band. */
                if (!args || !(ptr = strchr(args, ' '))) {
                        send_line_log(fd, wn_missing_arguments, msg_orig);
                } else {
                        char* reported = args;
                        *ptr = '\0';
                        char* reason = ptr + 1;
                        if (!*reported || !*reason) {
                                send_line_log(fd, wn_missing_arguments, msg_orig);
                        } else {
                                FILE* rf = fopen(report_file_path(), "a");
                                if (rf) {
                                        time_t now = time(NULL);
                                        struct tm* tm_info = gmtime(&now);
                                        char tbuf[32];
                                        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S UTC", tm_info);
                                        char safe_reporter[64];
                                        char safe_reported[64];
                                        char safe_reason[256];
                                        /* The reporter's own nick is chosen via NICK, so it is
                                         * no more trustworthy than the reported one. */
                                        sanitize_report_field(nick[fd] ? nick[fd] : "?",
                                                              safe_reporter, sizeof(safe_reporter));
                                        sanitize_report_field(reported, safe_reported, sizeof(safe_reported));
                                        sanitize_report_field(reason, safe_reason, sizeof(safe_reason));
                                        fprintf(rf, "%s  reporter=%-12s reporter_ip=%-15s reported=%-12s reason=%.200s\n",
                                                tbuf,
                                                safe_reporter,
                                                IP[fd] ? IP[fd] : "unknown",
                                                safe_reported, safe_reason);
                                        fclose(rf);
                                        l2(OUTPUT_TYPE_INFO, "player report: '%s' reported '%s'",
                                           nick[fd] ? nick[fd] : "?", safe_reported);
                                        /* OK means "we recorded it", not "we
                                         * acted on it" -- the client's wording
                                         * says so too. */
                                        send_ok(fd, msg_orig);
                                } else {
                                        /* Telling the player their report was
                                         * filed when it went nowhere is worse
                                         * than having no report feature at
                                         * all, so this failure is loud on both
                                         * ends rather than swallowed. */
                                        l2(OUTPUT_TYPE_ERROR,
                                           "could not write report file %s: %s",
                                           report_file_path(), strerror(errno));
                                        send_line_log(fd, wn_report_failed, msg_orig);
                                }
                        }
                }
        } else if (streq(current_command, "LIST")) {
                send_line_log(fd, list_games_str, msg_orig);
        } else if (streq(current_command, "STATUS")) {  // 1.0 command
                if (!already_in_game(fd)) {
                        send_line_log(fd, wn_not_in_game, msg_orig);
                } else {
                        status(fd, msg_orig);
                }
        } else if (streq(current_command, "STATUSGEO")) {
                if (!already_in_game(fd)) {
                        send_line_log(fd, wn_not_in_game, msg_orig);
                } else {
                        status_geo(fd, msg_orig);
                }
        } else if (streq(current_command, "PROTOCOL_LEVEL")) {
                if (!already_in_game(fd)) {
                        send_line_log(fd, wn_not_in_game, msg_orig);
                } else {
                        protocol_level(fd, msg_orig);
                }
        } else if (streq(current_command, "TALK")) {
                if (!args) {
                        send_line_log(fd, wn_missing_arguments, msg_orig);
                } else {
                        talk(fd, args);
                }
        } else if (streq(current_command, "START")) {
                if (!already_in_game(fd)) {
                        send_line_log(fd, wn_not_in_game, msg_orig);
                } else {
                        start_game(fd);
                }
        } else if (streq(current_command, "CLOSE")) {
                if (!already_in_game(fd)) {
                        send_line_log(fd, wn_not_in_game, msg_orig);
                } else {
                        close_game(fd);
                }
        } else if (streq(current_command, "SETOPTIONS")) {
                if (!args) {
                        send_line_log(fd, wn_missing_arguments, msg_orig);
                } else if (!already_in_game(fd)) {
                        send_line_log(fd, wn_not_in_game, msg_orig);
                } else {
                        setoptions(fd, args);
                }
        } else if (streq(current_command, "LEADER_CHECK_GAME_START")) {
                if (!already_in_game(fd)) {
                        send_line_log(fd, wn_not_in_game, msg_orig);
                } else {
                        leader_check_game_start(fd);
                }
        } else if (streq(current_command, "OK_GAME_START")) {
                if (!already_in_game(fd)) {
                        send_line_log(fd, wn_not_in_game, msg_orig);
                } else {
                        ok_start_game(fd);
                }
        } else if (streq(current_command, "ADMIN_REREAD")) {
                if (!admin_authorized[fd]) {
                        send_line_log(fd, wn_denied, msg_orig);
                } else {
                        reread();
                        send_ok(fd, "ADMIN_REREAD");
                }
        } else {
                send_line_log(fd, wn_unknown_command, msg);
        }

        free(msg_orig);
        current_command = NULL;

        return 0;
}


ssize_t get_reset_amount_transmitted(void)
{
        ssize_t ret = amount_transmitted;
        amount_transmitted = 0;
        return ret;
}

static void conn_to_terminate_helper(gpointer data, gpointer user_data)
{
        (void)user_data;
        conn_terminated(GPOINTER_TO_INT(data), "system error on send (probably peer shutdown or try again)");
}

/* Seat holding this exact nick, or -1. Unlike find_game_by_nick_aux (which
 * only ever checks slot 0, the room's creator), this scans every seat --
 * report_round_result() below needs to resolve an arbitrary winner claim to
 * a seat, not just ask whether the creator holds a name. */
static int find_player_slot_by_nick(const struct game* g, const char* nick)
{
        int i;
        for (i = 0; i < g->players_number; i++)
                if (streq(g->players_nick[i], nick))
                        return i;
        return -1;
}

/* "Server: <text>" pushed to every connection sitting in the lobby right
 * now (open_players) -- the same scope talk()'s own server-wide branch uses
 * when its sender isn't seated in a game. Reuses ok_talk purely so this
 * renders identically to a real chat line on every client; "Server" is not
 * a nick anyone can hold (is_nick_ok forbids the ':' TALK's own parser
 * would need to see here, but nothing stops a player literally naming
 * themselves "Server" -- same ambiguity join/leave system messages already
 * carry under a real nick, not a new one this introduces). */
static void broadcast_serverwide(const char* text)
{
        char buf[1000];
        snprintf(buf, sizeof(buf), ok_talk, "Server", text);
        g_list_foreach(open_players, talk_serverwide_aux, buf);
}

/* Up to 5 players with at least one win this match, most wins first, as
 * "nick (n), nick (n)". Ties break by room-slot order (join order) rather
 * than needing a stable sort -- deterministic without extra bookkeeping.
 * Win-count, not bubbles popped or any other per-round tally, is this
 * server's only notion of "score": it already tracks it (players_wins[]
 * below) from the 'F' opcode it was sniffing anyway, where popped-bubble
 * counts exist only in the client-to-client 'S' stats opcode this server
 * has never parsed and still doesn't. */
static void top_scorers_line(const struct game* g, char* out, size_t outsz)
{
        int order[MAX_PLAYERS_PER_GAME];
        int i, j, n = g->players_number;
        int shown = 0;
        for (i = 0; i < n; i++) order[i] = i;
        for (i = 0; i < n - 1; i++)
                for (j = i + 1; j < n; j++)
                        if (g->players_wins[order[j]] > g->players_wins[order[i]]) {
                                int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
                        }
        out[0] = '\0';
        for (i = 0; i < n && shown < 5; i++) {
                char entry[80];
                if (g->players_wins[order[i]] <= 0)
                        break;  /* sorted descending -- the rest are 0 too */
                snprintf(entry, sizeof(entry), "%s%s (%d)", shown > 0 ? ", " : "",
                         g->players_nick[order[i]], g->players_wins[order[i]]);
                strconcat(out, entry, outsz);
                shown++;
        }
}

/* Announces one round's outcome to the lobby: who won (or that it was a
 * draw), their new win-count, the team and its roster on a team win, and
 * the match's top scorers so far. Called once per round from the same 'F'
 * sniff that already fires the Discord alert (process_msg_prio_ below),
 * under the same g->result_posted guard -- this never doubles up with it,
 * it just also happens to fire from the same trigger. winner_nick carries
 * the same trust posture discordalert.h documents for that field: it is
 * exactly what the reporting client's payload said, checked here only
 * against the room's actual roster (find_player_slot_by_nick), never
 * against is_nick_ok. A claim that matches no seated player still gets
 * announced -- just without a win-count or team, since there is nothing
 * real to attach one to. */
static void report_round_result(struct game* g, const char* winner_nick)
{
        char headline[256];
        char scorers[256];
        int wslot = winner_nick ? find_player_slot_by_nick(g, winner_nick) : -1;

        if (wslot >= 0)
                g->players_wins[wslot]++;

        if (!winner_nick) {
                snprintf(headline, sizeof(headline), "Round over: a draw.");
        } else {
                int wteam = (wslot >= 0) ? g->players_team[wslot] : 0;
                if (g->team_count > 0 && wteam > 0) {
                        /* Name every player who shares the winner's team, not just
                         * the reporting client -- "the team" means all of them.
                         * Only slots 0-4 ever carry a team number (parse_teams()),
                         * so a >5-player room's remaining seats are left out of the
                         * roster the same way they are left out of team assignment
                         * itself. */
                        char teammates[256] = "";
                        int i, first = 1;
                        for (i = 0; i < g->players_number && i < 5; i++) {
                                if (g->players_team[i] != wteam) continue;
                                if (!first) strconcat(teammates, ", ", sizeof(teammates));
                                strconcat(teammates, g->players_nick[i], sizeof(teammates));
                                first = 0;
                        }
                        snprintf(headline, sizeof(headline),
                                 "Round over: Team %d wins (%s)!", wteam, teammates);
                } else if (wslot >= 0) {
                        snprintf(headline, sizeof(headline),
                                 "Round over: %s wins! (win #%d this match)",
                                 winner_nick, g->players_wins[wslot]);
                } else {
                        snprintf(headline, sizeof(headline), "Round over: %s wins!", winner_nick);
                }
        }

        broadcast_serverwide(headline);

        top_scorers_line(g, scorers, sizeof(scorers));
        if (scorers[0]) {
                char line[300];
                snprintf(line, sizeof(line), "Top scorers: %s", scorers);
                broadcast_serverwide(line);
        }
}

void process_msg_prio_(int fd, char* msg, ssize_t len, struct game* g)
{
        GList * conn_to_terminate = NULL;
        if (!g)
                g = find_game_by_fd(fd);
        if (g) {
                if (g->tournament_id && len >= 2 && msg[1] == 'n') return;
                int i;
                /* Stamp the sender byte with the seat this server assigned, so a
                   client cannot claim to be another player. Peers read msg[0] as
                   the originating seat and act on it, and everything below this
                   point relays msg verbatim.

                   A negative slot means the sender no longer holds one: the
                   departure relay announces a player the caller has already
                   removed from the game, and supplies the correct id itself.
                   Leave that message alone. */
                {
                        int sender_slot = find_player_slot(g, fd);
                        if (sender_slot >= 0 && len > 0)
                                msg[0] = g->players_id[sender_slot];
                }

                /* Sniff round-end ('F') and ready-for-next-round ('n') for the
                 * Discord match-result alert. Read-only with respect to
                 * everything below -- it never changes what gets relayed, only
                 * whether a datagram also goes to discord-relay.
                 *
                 * 'F' means round over: bare "F" is a draw, "F<nick>" is a win
                 * claim (src/bubblegame_state.cpp). Multiple clients can send
                 * their own 'F' for the same round -- the client's own
                 * CommitRoundWin ignores every one after the first, and
                 * result_posted is this function's equivalent, cleared on the
                 * next round's 'n' rather than reset by any explicit
                 * round-boundary signal, since there isn't one on the wire.
                 *
                 * The winner name is exactly what the reporting client's
                 * payload said and is NOT checked against is_nick_ok -- a
                 * modified client could claim a win it did not earn, or embed
                 * something adversarial. Stripping '|' below only protects
                 * this datagram's own field boundaries; discord-relay applies
                 * its usual markdown/mention escaping on top of that, same
                 * distrust as the join alert's nick and servername fields. */
                if (!g->tournament_id && len >= 3 && msg[1] == 'F' && !g->result_posted) {
                        char winner[32] = "";
                        size_t wlen = (size_t)len - 3;  /* id + 'F' + '\n' */
                        if (wlen > 0 && wlen < sizeof(winner)) {
                                size_t j, w = 0;
                                for (j = 0; j < wlen; j++)
                                        winner[w++] = (msg[2 + j] == '|') ? ' ' : msg[2 + j];
                                winner[w] = '\0';
                        }
                        {
                                char roster[512];
                                build_roster_csv(g, roster, sizeof(roster));
                                discordalert_fire_result_event(g->game_id, roster, winner[0] ? winner : NULL, g->game_mode);
                        }
                        report_round_result(g, winner[0] ? winner : NULL);
                        g->result_posted = 1;
                } else if (!g->tournament_id && len >= 2 && msg[1] == 'n') {
                        g->result_posted = 0;
                }

                for (i = 0; i < g->players_number; i++) {
                        // Pings are for the server only. Don't broadcast them to save bandwidth.
                        if (len == 3 && msg[1] == 'p') {
                                // nada

                        // Emitter wants to receive synchro message as well
                        } else if (g->players_conn[i] == fd && len > 2 && msg[1] == '!') {
                                char synchro4self[] = "?!\n";
                                ssize_t retval;
                                int dest = g->players_conn[i];
                                synchro4self[0] = g->players_id[i];
                                l1(OUTPUT_TYPE_DEBUG, "[%d] sending self synchro", dest);
                                // net_queue_send() (BUG-007) on both branches now -- ws_send()
                                // already routes through it internally. A short/blocked send is
                                // no longer possible here: it's either queued (reported as if
                                // fully sent, same contract send() itself gave this caller) or a
                                // hard -1. The dead-code "short send" branch below is left in
                                // place rather than removed, in case that contract ever changes.
                                if (ws_is_websocket(dest))
                                        retval = (ws_send(dest, synchro4self, sizeof(synchro4self) - 1) < 0) ? -1 : (ssize_t)(sizeof(synchro4self) - 1);
                                else
                                        retval = net_queue_send(dest, synchro4self, sizeof(synchro4self) - 1);
                                if (retval != (ssize_t)(sizeof(synchro4self) - 1)) {
                                        if (retval != -1) {
                                                l4(OUTPUT_TYPE_INFO, "[%d] short send of %zd instead of %zd bytes from %d - destination is not reading data "
                                                                     "(illegal FB client) or our upload bandwidth is saturated - sorry, cannot continue serving "
                                                                     "this client in this situation, closing connection",
                                                                     dest, retval, sizeof(synchro4self) - 1, fd);
                                        }
                                        conn_to_terminate = g_list_append(conn_to_terminate, GINT_TO_POINTER(dest));
                                }

                        } else if (g->players_conn[i] != fd) {
                                ssize_t retval;
                                int dest = g->players_conn[i];
                                l3(OUTPUT_TYPE_DEBUG, "[%d] sending %zd bytes to %d", fd, len, dest);
                                if (ws_is_websocket(dest))
                                        retval = (ws_send(dest, msg, (int)len) < 0) ? -1 : len;
                                else
                                        retval = net_queue_send(dest, msg, (size_t)len);
                                if (retval != len) {
                                        if (retval != -1) {
                                                l4(OUTPUT_TYPE_INFO, "[%d] short send of %zd instead of %zd bytes from %d - destination is not reading data "
                                                                     "(illegal FB client) or our upload bandwidth is saturated - sorry, cannot continue serving "
                                                                     "this client in this situation, closing connection",
                                                                     dest, retval, len, fd);
                                        }
                                        conn_to_terminate = g_list_append(conn_to_terminate, GINT_TO_POINTER(dest));
                                }
                        }
                }
                if (conn_to_terminate) {
                        g_list_foreach(conn_to_terminate, conn_to_terminate_helper, NULL);
                        g_list_free(conn_to_terminate);
                }
        } else {
                /* A connection can sit in priority mode with no game behind it:
                   being kicked from a playing room, or the room closing after
                   the round started, both leave the fd live while its game is
                   gone. Its next binary line used to land here and take the
                   whole server -- every other room included -- down with it.
                   Close just this connection instead. */
                l1(OUTPUT_TYPE_ERROR, "[%d] priority message with no game behind it; closing this connection", fd);
                conn_terminated(fd, "in-game message from a connection that is no longer in a game");
        }
}

void process_msg_prio(int fd, char* msg, ssize_t len)
{
        process_msg_prio_(fd, msg, len, NULL);
}

void player_part_game(int fd)
{
        player_part_game_(fd, NULL);
}

void player_part_game_(int fd, char* reason)
{
        /* A detected disconnect has already cleared nick[fd]; never turn its
         * reserved-room teardown into the ordinary departure-win path. */
        if (tournament_active(fd)) {
                if (!nick[fd]) tournament_disconnect(fd);
                else tournament_withdraw(fd);
        }
        struct game * g = find_game_by_fd(fd);
        if (g) {
                char * save_nick;
                int j;
                int i = find_player_number(g, fd);
                int was_playing = (g->status == GAME_STATUS_PLAYING);
                int leaving_player_index = i;
                int save_id = g->players_id[i];

                // remove parting player from game
                save_nick = g->players_nick[i];
                for (j = i; j < g->players_number - 1; j++) {
                        g->players_conn[j] = g->players_conn[j + 1];
                        g->players_nick[j] = g->players_nick[j + 1];
                        g->players_started[j] = g->players_started[j + 1];
                        g->players_id[j] = g->players_id[j + 1];
                        g->players_wins[j] = g->players_wins[j + 1];
                        g->players_team[j] = g->players_team[j + 1];
                }
                g->players_number--;

                // completely remove game if empty
                if (g->players_number == 0) {
                        // Record win for the last remaining player (if game was in progress)
                        if (was_playing && g->players_number == 0 && leaving_player_index >= 0) {
                                // This was the last player - they win by default (others already left)
                                stats_record_win(save_nick);
                                l1(OUTPUT_TYPE_INFO, "Game ended: %s wins (last player remaining)", save_nick);
                        }

                        games = g_list_remove(games, g);
                        free(g);
                        calculate_list_games();
                        if (was_playing)
                                l2(OUTPUT_TYPE_INFO, "running games decrements to: %d (%d players)", games_running, players_in_game);

                } else {
                        if (was_playing) {
                                // inform other players, playing state
                                char leave_player_prio_msg[] = "?l\n";
                                leave_player_prio_msg[0] = save_id;
                                process_msg_prio_(fd, leave_player_prio_msg, strlen(leave_player_prio_msg), g);

                                // Record loss for the leaving player (they left during gameplay)
                                stats_record_loss(save_nick);
                                l1(OUTPUT_TYPE_INFO, "Player %s left during game - recorded as loss", save_nick);

                                // The relay above can fail to send (ordinary EPIPE when several
                                // clients drop at once), which appends the destination to
                                // conn_to_terminate and runs conn_terminated -> player_part_game_
                                // re-entrantly on this same game. That nested call can take the
                                // players_number == 0 branch and free g, so g may be dangling
                                // here. Re-validate against the games list before reading it; if
                                // it is gone, the nested call already recorded the last player's
                                // win and there is nothing left for this frame to do.
                                if (game_is_live(g) && g->players_number == 1) {
                                        char* winner_nick = g->players_nick[0];
                                        stats_record_win(winner_nick);
                                        l1(OUTPUT_TYPE_INFO, "Game ended: %s wins (last player remaining)", winner_nick);
                                }
                        } else if (leaving_player_index == 0) {
                                // Creator/leader left before the round started. The creator is
                                // the only client wired up to be authoritative for level
                                // generation (SyncNetworkLevel's b|/N/T sync); silently
                                // promoting players_nick[1] to slot 0 would make some other
                                // client (often a bot with no leader logic at all) the new
                                // "creator" with no way to actually host. Close the whole room
                                // instead, so the remaining players return to the lobby rather
                                // than being stuck in a room with a leader who can't lead.
                                char room_closed_msg[1000];
                                snprintf(room_closed_msg, sizeof(room_closed_msg), "ROOM_CLOSED: %s", save_nick);
                                for (j = 0; j < g->players_number; j++) {
                                        send_line_log_push(g->players_conn[j], room_closed_msg);
                                        open_players = g_list_append(open_players, GINT_TO_POINTER(g->players_conn[j]));
                                        /* Each seat owns its own strdup'd nick (create_game and
                                         * the join path both hand one over). Freeing the game
                                         * without these leaked one allocation per surviving
                                         * player on every creator-led room closure (audit
                                         * finding BUG-008). The departing player's nick is
                                         * save_nick, freed below — do not free it twice. */
                                        free(g->players_nick[j]);
                                        g->players_nick[j] = NULL;
                                }
                                games = g_list_remove(games, g);
                                free(g);
                                g = NULL;
                        } else {
                                char parted_msg[1000];
                                // inform other players, non-playing state
                                snprintf(parted_msg, sizeof(parted_msg), reason ? reason : ok_player_parted, save_nick);
                                for (j = 0; j < g->players_number; j++)
                                        send_line_log_push(g->players_conn[j], parted_msg);
                        }
                        calculate_list_games();
                }
                free(save_nick);

                open_players = g_list_append(open_players, GINT_TO_POINTER(fd));
                calculate_list_games();  // recalculate now that player is back in open_players
        }
}


int game_has_room(int fd) { return already_in_game(fd); }

int game_tournament_start(int tid, int mid, int round, int a, int fd_a, int b, int fd_b, const char *options) {
        if (already_in_game(fd_a) || already_in_game(fd_b) || !nick[fd_a] || !nick[fd_b]) return 0;
        create_game(fd_a, strdup(nick[fd_a]), 2);
        struct game *g = find_game_by_fd(fd_a);
        g->tournament_id = tid; g->tournament_match = mid; g->tournament_round = round;
        /* Seat directly: JOINED assumes the client's room already exists. */
        g->players_conn[1] = fd_b; g->players_id[1] = 'B';
        g->players_nick[1] = strdup(nick[fd_b]); g->players_number = 2;
        g->next_player_id = 'C';
        open_players = g_list_remove(open_players, GINT_TO_POINTER(fd_b));
        g->status = GAME_STATUS_CLOSED;
        char line[256];
        snprintf(line, sizeof(line), "TOUR_ASSIGN: %d %d %d %d %s %d %s", tid, mid, round, a, nick[fd_a], b, nick[fd_b]);
        send_line_log_push_binary(fd_a, line, line);
        send_line_log_push_binary(fd_b, line, line);
        /* Apply the organizer's ruleset (TOUR CREATE's options blob, stored
         * verbatim on the tournament) to this match's freshly created room,
         * exactly as setoptions() above does for a live room's own
         * SETOPTIONS -- same game_mode/team bookkeeping, same OPTIONS push
         * to both seats, sent before real_start_game() so each side's own
         * pendingOptions/GetAndClearPendingOptions() has already applied it
         * (see src/mainmenu_netpanel.cpp) by the time gameplay begins. Every
         * match in the bracket gets the identical ruleset this way, so
         * nothing can drift partway through. A bare "TOUR CREATE" (no
         * options) leaves this string empty and the room simply keeps the
         * server's zero-initialized game defaults, same as before this
         * feature existed. */
        if (options && *options) {
                g->game_mode = parse_game_mode(options);
                parse_teams(g, options);
                char *msg = asprintf_("OPTIONS: %s,PROTOCOLLEVEL:%d", options, min_protocol_level(g));
                send_line_log_push(fd_a, msg);
                send_line_log_push(fd_b, msg);
                free(msg);
        }
        real_start_game(g);
        calculate_list_games();
        return 1;
}

void game_tournament_retire(int tid, int mid, int round) {
        struct game *g = NULL;
        for (GList *item = games; item; item = item->next) {
                struct game *candidate = item->data;
                if (candidate->tournament_id == tid && candidate->tournament_match == mid && candidate->tournament_round == round) { g = candidate; break; }
        }
        if (!g) return;
        games = g_list_remove(games, g);
        char line[128];
        snprintf(line, sizeof(line), "TOUR_RETURN: %d %d %d", tid, mid, round);
        for (int i = 0; i < g->players_number; ++i) {
                int fd = g->players_conn[i];
                if (nick[fd]) {
                        send_line_log_push_binary(fd, line, line);
                        remove_prio(fd);
                        if (!g_list_find(open_players, GINT_TO_POINTER(fd))) open_players = g_list_append(open_players, GINT_TO_POINTER(fd));
                }
                free(g->players_nick[i]);
        }
        free(g);
        calculate_list_games();
}
