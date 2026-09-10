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
// per-player push registrations: every room join posts a message to one
// Discord channel via a webhook, optionally carrying the joining player's
// self-reported geolocation.
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

// Call once per successful room join, from add_player(). geoloc may be NULL
// (nothing self-reported by that client yet -- see GEOLOC's own comment on
// why that's the normal case for a fast joiner). A no-op (and free) when the
// relay isn't configured.
void discordalert_fire_join_event(const char* nick, const char* ip, const char* geoloc);

// Close the UDP socket. Call once at shutdown, next to stats_cleanup().
void discordalert_cleanup(void);

#endif
