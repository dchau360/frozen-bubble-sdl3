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

#ifndef DISCORDALERT_H
#define DISCORDALERT_H

// Discord join-alert relay hookup. Replaces the old "follow a server" push
// feature (removed) with a single operator-facing notification instead of
// per-player push registrations: every player arriving on the server posts a
// message to one Discord channel via a webhook.
//
// Same reasoning as the feature this replaces: fb-server has no TLS and runs
// a single-threaded blocking event loop, so it cannot POST to a Discord
// webhook itself without risking a stall for every connected player.
// Instead it fires a best-effort UDP datagram at a local sidecar
// (server/discord-relay/) that owns the actual HTTPS call and holds the
// webhook URL, which never needs to reach this process at all. If the
// sidecar is absent, slow, or misconfigured, the datagram is simply
// dropped -- this must never be able to affect gameplay.
//
// Unlike the feature this replaces there is no registry and nothing to
// persist: this is one operator's standing webhook, not per-player
// registrations that have to outlive a connection.

// Resolve FB_SERVER_DISCORD_RELAY ("host:port") and open the UDP socket
// toward it. A no-op (feature stays inert) if unset, unresolvable, or
// malformed -- this must never be fatal to server startup. Call once at
// startup, next to stats_init().
void discordalert_init(void);

// Call once per player arriving on the server, from the NICK handler's
// success path -- the point at which a connection acquires a name and
// becomes visible in the lobby to everyone else. Not from add_player(): a
// room join is a later, less useful moment (see the call site's comment).
//
// geoloc is almost always NULL here, since GEOLOC arrives after NICK and the
// client-side lookup behind it can take ~16s. That costs nothing: the relay
// discards the field. A no-op (and free) when the relay isn't configured.
void discordalert_fire_join_event(const char* nick, const char* ip, const char* geoloc);

// Call once per round-end, from the 'F' opcode sniffed in process_msg_prio_
// (game.c) -- a bare "F" is a draw, "F<nick>" is a win claim, and the
// server relays either verbatim to clients whether or not this fires.
//
// game_id is the room's g->game_id: an opaque, per-process-monotonic value
// assigned once at CREATE and never reused, carried along purely so the
// relay can group every round from the same room into one Discord thread.
// It means nothing to this file or to fb-server generally -- it is not a
// player-facing id and never touches the client wire protocol, only this
// UDP datagram.
//
// roster_csv is every current player's nick, comma-joined -- each one
// already passed is_nick_ok() when its owner connected or joined, unlike
// winner_nick, which is exactly what the reporting client's payload said
// and is not validated against anything here beyond a length cap and
// stripping '|' at the call site. A modified client can claim a win it did
// not earn; it cannot forge a false roster, since that comes from this
// server's own bookkeeping, not the wire message. Pass NULL for winner_nick
// on a draw. game_mode is the room's raw 0-3 GAMEMODE value (see
// src/gamemode.h) or 0 if the room never set one -- the relay maps it to a
// display name, not this file, so a value this build doesn't recognize
// degrades to "unlabelled" rather than needing a matching update here.
// A no-op (and free) when the relay isn't configured.
void discordalert_fire_result_event(int game_id, const char* roster_csv, const char* winner_nick, int game_mode);

// Close the UDP socket. Call once at shutdown, next to stats_cleanup().
void discordalert_cleanup(void);

#endif
