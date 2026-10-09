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

#ifndef LINKS_H
#define LINKS_H

/* Account PINs and links (protocol 1.8). An account is a key on one device
 * (account.h), and a browser that forgets it, or a second phone, comes back
 * as a new account. Instead of typing the 16-character code there, a player
 * sets a short PIN once, signed in on the device that has the account:
 *
 *   SETPIN <pin> <name>   -> SETPIN: OK | NOT_SIGNED_IN | INVALID | NAME_TAKEN
 *
 * and on the new device, signed in as whatever account it has, links it:
 *
 *   LINKPIN <name> <pin>  -> LINKPIN: OK <account id> | NOT_SIGNED_IN | INVALID
 *                            | WRONG_PIN | TOO_MANY_TRIES
 *
 * From then on the new device's key signs in as the PIN's account
 * (account_finish() resolves it here), so its runs, weekly line and Discord
 * tag are that account's; what it had already recorded under its own id is
 * folded in at the link. The device keeps its own code: nothing secret is
 * stored or sent back.
 *
 * Deliberately weak, and accepted as such (user decision): a PIN is 4-8
 * digits, sent in the clear like everything on port 1511, and kept as a
 * salted BLAKE2b hash a copy of the file could brute-force in moments. What
 * stands between a stranger and someone's line is the per-name limit on
 * wrong guesses (LINKS_MAX_TRIES an hour). The stakes are a scoreboard. */

#define LINKS_PIN_MIN 4
#define LINKS_PIN_MAX 8
#define LINKS_MAX_TRIES 5
#define LINKS_TRY_WINDOW_SECS 3600

enum links_result {
        LINKS_OK = 0,
        LINKS_INVALID,
        LINKS_NAME_TAKEN,
        LINKS_WRONG_PIN,
        LINKS_TOO_MANY_TRIES,
};

/* Loads the file (FB_SERVER_LINKS_FILE, else links.dat beside the hiscores
 * file, else ~/.fb-server/links.dat). */
void links_init(void);

/* The account `id` signs in as: the one it was linked to, or itself. Never
 * NULL; points at storage valid until the next links_* call. */
const char* links_resolve(const char* id);

/* SETPIN for the signed-in account `id` (already resolved). One PIN per
 * account: setting it again, under the same or a new name, replaces it. */
enum links_result links_set_pin(const char* id, const char* name, const char* pin);

/* LINKPIN: links `id` (the connection's own, resolved) to the account that
 * set `pin` under `name`, and writes that account's id to out (17 bytes).
 * Linking to itself is OK and changes nothing. */
enum links_result links_link(const char* id, const char* name, const char* pin, char* out);

/* DELETEACCOUNT: drops the account's PIN and every link to or from it. */
void links_forget(const char* id);

#endif
