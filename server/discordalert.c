/*******************************************************************************
 *
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
#include <errno.h>

#include "win32_compat.h"
#ifndef _WIN32
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#endif

#include "discordalert.h"
#include "net.h"
#include "log.h"

// UDP transport to the relay sidecar. -1 / unset means the feature is
// unconfigured: discordalert_fire_join_event() is a cheap no-op, nothing is
// ever sent. Same shape as notify.c's now-removed relay socket.
static int relay_socket = -1;
static struct sockaddr_in relay_addr;
static int relay_configured = 0;

// Resolves FB_SERVER_DISCORD_RELAY ("host:port") into relay_addr and opens
// the UDP socket. Failure here (bad hostname, no relay configured) just
// leaves the feature inert -- this must never be fatal to server startup.
void discordalert_init(void)
{
    const char* relay = getenv("FB_SERVER_DISCORD_RELAY");
    if (!relay || !*relay) {
        l0(OUTPUT_TYPE_INFO, "FB_SERVER_DISCORD_RELAY not set; Discord join alerts disabled");
        return;
    }

    char* relay_copy = strdup(relay);
    char* colon = strrchr(relay_copy, ':');
    if (!colon) {
        l1(OUTPUT_TYPE_ERROR, "FB_SERVER_DISCORD_RELAY '%s' is not host:port; Discord alerts disabled", relay);
        free(relay_copy);
        return;
    }
    *colon = '\0';
    const char* host = relay_copy;
    int relay_port = atoi(colon + 1);
    if (relay_port <= 0 || relay_port > 65535) {
        l1(OUTPUT_TYPE_ERROR, "FB_SERVER_DISCORD_RELAY has an invalid port in '%s'; Discord alerts disabled", relay);
        free(relay_copy);
        return;
    }

    struct hostent* h = gethostbyname(host);
    if (!h || !h->h_addr_list || !h->h_addr_list[0]) {
        l1(OUTPUT_TYPE_ERROR, "FB_SERVER_DISCORD_RELAY host '%s' does not resolve; Discord alerts disabled", host);
        free(relay_copy);
        return;
    }

    memset(&relay_addr, 0, sizeof(relay_addr));
    relay_addr.sin_family = AF_INET;
    relay_addr.sin_port = htons((unsigned short)relay_port);
    memcpy(&relay_addr.sin_addr, h->h_addr_list[0], sizeof(relay_addr.sin_addr));

    relay_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (relay_socket < 0) {
        l1(OUTPUT_TYPE_ERROR, "discordalert: socket() failed: %s", strerror(errno));
        free(relay_copy);
        return;
    }

    relay_configured = 1;
    l2(OUTPUT_TYPE_INFO, "Discord join alerts will be relayed to %s:%d", host, relay_port);
    free(relay_copy);
}

void discordalert_fire_join_event(const char* nick, const char* ip, const char* geoloc,
                                  char platform, const char* country)
{
    if (!relay_configured) return;

    // nick and ip are already validated space-free by their own command
    // handlers (NICK/JOIN truncate at the first space; IP[] comes straight
    // from inet_ntoa()); geoloc is validated the same way by GEOLOC. platform
    // is one char from a closed set (is_platform_tag_ok, game.c) or 0, which
    // prints as an empty field. Only the trailing servername field is allowed
    // to contain anything at all, so the relay reads it as everything
    // remaining rather than splitting further -- same idiom the old notify.c
    // datagram used, and the reason platform is inserted *before* it rather
    // than appended.
    char platform_str[2] = { platform, '\0' };
    char datagram[1024];
    snprintf(datagram, sizeof(datagram), "JOIN|%s|%s|%s|%s|%s|%s",
             nick ? nick : "", ip ? ip : "", geoloc ? geoloc : "",
             platform ? platform_str : "", country ? country : "",
             net_servername());

    if (sendto(relay_socket, datagram, strlen(datagram), 0,
               (struct sockaddr*)&relay_addr, sizeof(relay_addr)) < 0) {
        // Best-effort by design: log and move on, never block or retry on
        // the main event loop.
        l1(OUTPUT_TYPE_ERROR, "discordalert: sendto relay failed: %s", strerror(errno));
    }
}

void discordalert_fire_result_event(int game_id, int round_number, const char* roster_csv,
                                     const char* wins_csv, int victories_limit,
                                     const char* winner_nick, int game_mode,
                                     const char* platforms_csv, const char* inputs_csv,
                                     const char* countries_csv, const char* popped_csv)
{
    if (!relay_configured) return;

    // roster_csv came from build_roster_csv() (game.c), joining nicks that
    // already passed is_nick_ok() -- [A-Za-z0-9_-]{1,10} -- so it can never
    // contain '|'. winner_nick had its own '|' stripped at the call site
    // (game.c) for the same reason, since it is not validated against
    // is_nick_ok at all. wins_csv (build_wins_csv(), game.c) is plain
    // digits and commas, same no-escaping-needed posture as game_id/
    // round_number/victories_limit below -- the last of those is g->
    // victories_limit verbatim (0 meaning "no limit"), which the relay uses
    // to scale the win-count chart's bars against a real target instead of
    // just whoever's currently ahead. Only net_servername() is genuinely
    // unbounded, same as for JOIN, which is why it stays last rather than
    // split further. game_id and round_number are both plain ints (see the
    // doc comments in discordalert.h), so neither needs such treatment --
    // they go right after RESULT rather than at the end, since they are the
    // fields every consumer needs before it can even start parsing the
    // rest. platforms_csv and inputs_csv (build_tags_csv(), game.c) are single
    // chars from closed sets and commas, and countries_csv is uppercase ASCII
    // letter pairs and commas (is_country_tag_ok, game.c), so none of the
    // three needs more escaping than wins_csv does; they sit just before the
    // servername for the same reason every other field does -- that one is
    // the unbounded remainder. popped_csv (build_popped_csv(), game.c) is
    // digits, commas and an occasional trailing '!' -- see this function's
    // header comment for its very different trust posture from every field
    // before it -- and needs no more escaping than wins_csv either, so it
    // sits right before the servername too.
    char datagram[1024];
    snprintf(datagram, sizeof(datagram), "RESULT|%d|%d|%d|%s|%s|%s|%d|%s|%s|%s|%s|%s",
             game_id, round_number, game_mode, winner_nick ? winner_nick : "",
             roster_csv ? roster_csv : "", wins_csv ? wins_csv : "", victories_limit,
             platforms_csv ? platforms_csv : "", inputs_csv ? inputs_csv : "",
             countries_csv ? countries_csv : "", popped_csv ? popped_csv : "",
             net_servername());

    if (sendto(relay_socket, datagram, strlen(datagram), 0,
               (struct sockaddr*)&relay_addr, sizeof(relay_addr)) < 0) {
        // Best-effort by design: log and move on, never block or retry on
        // the main event loop.
        l1(OUTPUT_TYPE_ERROR, "discordalert: sendto relay failed: %s", strerror(errno));
    }
}

void discordalert_fire_match_event(int game_id, const char* champion_nick, int wins, int game_mode)
{
    if (!relay_configured) return;

    // Same trust/escaping posture as discordalert_fire_result_event above:
    // champion_nick is untrusted free text (see discordalert.h), so it goes
    // last, before only the also-unbounded servername. wins is a plain int
    // fb-server computed itself (game.c's report_round_result() return
    // value), needing no such treatment.
    char datagram[1024];
    snprintf(datagram, sizeof(datagram), "MATCH|%d|%d|%d|%s|%s",
             game_id, wins, game_mode, champion_nick ? champion_nick : "", net_servername());

    if (sendto(relay_socket, datagram, strlen(datagram), 0,
               (struct sockaddr*)&relay_addr, sizeof(relay_addr)) < 0) {
        // Best-effort by design: log and move on, never block or retry on
        // the main event loop.
        l1(OUTPUT_TYPE_ERROR, "discordalert: sendto relay failed: %s", strerror(errno));
    }
}

void discordalert_cleanup(void)
{
    if (relay_socket != -1) {
        SOCKET_CLOSE(relay_socket);
        relay_socket = -1;
    }
    relay_configured = 0;
}
