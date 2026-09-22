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
// discards the field.
//
// platform is the opposite case: a single char from platform_tag[fd] (see
// game.c's is_platform_tag_ok) that this client sent *before* its NICK
// precisely so it would be set by the time this fires, and which the relay
// does post -- an OS badge is not whereabouts, so it carries none of the
// reason geoloc is dropped. 0 when the client never said, which the relay
// renders as no badge. A no-op (and free) when the relay isn't configured.
//
// country is the ISO alpha-2 code from the COUNTRY command, or empty. It comes
// from the same client-side lookup as geoloc and so arrives on the same ~16s
// delay, which means it is usually empty here for exactly the reason geoloc
// is -- a round-result alert is where it normally first appears. It is posted
// where geoloc is not: a country is not a location fix on a person, and it is
// what someone reading the channel to decide whether to go play actually wants
// to know. The lat/lon still stops at this server.
void discordalert_fire_join_event(const char* nick, const char* ip, const char* geoloc,
                                  char platform, const char* country);

// Call once per round-end, from the 'F' opcode sniffed in process_msg_prio_
// (game.c) -- a bare "F" is a draw, "F<nick>" is a win claim, and the
// server relays either verbatim to clients whether or not this fires.
//
// game_id is the room's g->game_id: an opaque, per-process-monotonic value
// assigned once at CREATE and never reused, carried along purely so the
// relay can group every round from the same room into one Discord thread.
// It means nothing to this file or to fb-server generally -- it is not a
// player-facing id and never touches the client wire protocol, only this
// UDP datagram. round_number is g->round_number, 1-based and incremented
// once per posted result -- purely a display label, same as game_mode.
//
// roster_csv is every current player's nick, comma-joined -- each one
// already passed is_nick_ok() when its owner connected or joined, unlike
// winner_nick, which is exactly what the reporting client's payload said
// and is not validated against anything here beyond a length cap and
// stripping '|' at the call site. A modified client can claim a win it did
// not earn; it cannot forge a false roster, since that comes from this
// server's own bookkeeping, not the wire message. Pass NULL for winner_nick
// on a draw. wins_csv is g->players_wins[], comma-joined in the same order
// as roster_csv (build_wins_csv() in game.c) -- already updated for this
// round's winner by the time this fires, so the relay's win-count chart
// never lags a round behind. victories_limit is the room's own win-count
// target (g->victories_limit, see parse_victories_limit() in game.c), 0
// meaning "no limit was ever set" -- the relay scales the win-count chart's
// bars against it when positive ("first to N") and falls back to scaling
// against whoever currently leads when it's 0, same as before this field
// existed. game_mode is the room's raw 0-3 GAMEMODE value (see
// src/gamemode.h) or 0 if the room never set one -- the relay maps it to a
// display name, not this file, so a value this build doesn't recognize
// degrades to "unlabelled" rather than needing a matching update here.
//
// platforms_csv, inputs_csv and countries_csv are all index-aligned with
// roster_csv the same way wins_csv is (build_tags_csv()/build_country_csv() in
// game.c): one char per seat, or an empty
// field for a player whose client never reported one. platforms_csv comes from
// the PLATFORM command and so is fixed for a connection; inputs_csv comes from
// the 'i' opcode and is whatever device that player last actually shot with,
// so it can legitimately differ from one round to the next -- which is the
// point of carrying it per round rather than per room.
//
// popped_csv is index-aligned with roster_csv the same way, from
// build_popped_csv() (game.c): each seat's bubbles-popped count for the
// round, or an empty field if that seat's own 'S' opcode hadn't arrived by
// the time this fired -- unlike every field above, this one did NOT come
// from this server's own bookkeeping; it is self-reported by each client
// (the 'S' opcode) and only reaches this datagram after game.c clamps it
// against that seat's server-observed shot count (see MAX_POPS_PER_SHOT/
// POP_CEILING_GRACE, game.c), same reasoning as the winner_nick trust
// caveat above but with an actual ceiling behind it. A clamped field
// carries a trailing '!'. This is also *why* this whole event now fires up
// to PENDING_STATS_TIMEOUT_SECS (game.c) after the round's own 'F' instead
// of synchronously with it -- most seats' 'S' hasn't arrived yet at 'F'
// time, so the caller (process_msg_prio_'s maybe_fire_pending_result())
// waits for either every seat to report or that timeout, whichever comes
// first, before calling this. A no-op (and free) when the relay isn't
// configured, same as every fire function here.
void discordalert_fire_result_event(int game_id, int round_number, const char* roster_csv,
                                     const char* wins_csv, int victories_limit,
                                     const char* winner_nick, int game_mode,
                                     const char* platforms_csv, const char* inputs_csv,
                                     const char* countries_csv, const char* popped_csv);

// Call once per match-end, immediately after the round-end alert above,
// when that round's winner has just reached the room's own VICTORIESLIMIT
// (see game.c's parse_victories_limit()/g->victories_limit) -- i.e. the
// same call site, at most once per round, and only on a round that also
// decides the whole best-of-N match. game_id is the same room key the round
// alert above uses, so the relay threads this into the identical Discord
// thread as every round before it -- it always lands as a reply, never a
// fresh top-level message, since the round that triggered it already
// created (or reused) that thread a moment earlier. champion_nick carries
// the same trust posture as winner_nick above: exactly what the reporting
// client said, not validated against is_nick_ok. wins is the champion's
// final win count (>= the room's victories_limit); game_mode is the same
// raw 0-3 value as the round alert. A no-op (and free) when the relay isn't
// configured.
void discordalert_fire_match_event(int game_id, const char* champion_nick, int wins, int game_mode);

// Close the UDP socket. Call once at shutdown, next to stats_cleanup().
void discordalert_cleanup(void);

#endif
