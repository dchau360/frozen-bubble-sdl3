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

#ifndef NOTIFY_H
#define NOTIFY_H

// "Follow a server" push notifications.
//
// A registration here is deliberately NOT tied to a connection's fd -- every
// other piece of per-connection state in net.c/game.c is destroyed the instant
// the socket closes (see conn_terminated()), but the whole point of this
// feature is to reach a device *after* it has disconnected. So, like
// stats.c, this is a flat-file-backed table that outlives the server
// process's individual connections and survives a restart.
//
// Registrations are keyed by the device's push token rather than by nick:
// there is no account system in this game, nicks are chosen fresh every
// connection, but a token is the one thing that identifies "this device"
// across sessions.
//
// Delivery is intentionally NOT done here. fb-server has no TLS and runs a
// single-threaded blocking event loop (see net.c's connections_manager());
// hand-rolling APNs's HTTP/2+JWT or FCM's OAuth2 in C would be a stall risk
// for the one loop that runs the whole game. Instead notify_fire_join_event()
// fires a best-effort, non-blocking UDP datagram at a local sidecar
// (server/notify-relay/) that owns the actual APNs/FCM calls. If the sidecar
// is absent, slow, or misconfigured, the datagram is simply dropped -- this
// must never be able to affect gameplay.

// Initialize the registry: load the flat file (if any) and open the UDP
// socket toward the relay (if FB_SERVER_NOTIFY_RELAY is set). Call once at
// startup, next to stats_init().
void notify_init(void);

// Rewrite the flat file from the in-memory table.
void notify_save(void);

// Register (or refresh) a device's push token. platform must be "ios" or
// "android" -- callers should validate that before calling this. Upserts by
// token; does not reset the per-token notification cooldown.
void notify_register(const char* platform, const char* token);

// Remove a device's registration (e.g. the player un-followed).
void notify_unregister(const char* token);

// Call once per successful join, from add_player(). Notifies every
// registered device whose cooldown has elapsed with a best-effort UDP
// datagram to the relay; updates last_notified and persists the table for
// the ones that fired. A no-op (and free) when no devices are registered or
// the relay is unconfigured.
void notify_fire_join_event(void);

// Free the table and close the UDP socket. Call once at shutdown, next to
// stats_cleanup().
void notify_cleanup(void);

#endif
