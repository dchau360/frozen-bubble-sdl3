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

#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <stddef.h>

/* Player accounts (protocol 1.6). An account is an EdDSA key pair (Monocypher,
 * third_party/monocypher) that lives only on the player's device; the server
 * never sees the secret half and keeps no registry. A connection proves it
 * holds an account with a challenge-response:
 *
 *   client: AUTH <public key, 64 hex>
 *   server: AUTH: CHALLENGE <nonce, 64 hex>          (32 fresh random bytes)
 *   client: AUTHSIG <signature, 128 hex>              over ACCOUNT_SIGNED_PREFIX + nonce hex
 *   server: AUTHSIG: OK <account id, 16 hex>          or INVALID_SIGNATURE
 *
 * The account id is the first 8 bytes of BLAKE2b(public key), so the same key
 * is the same account on every server with nothing to register. Nothing that
 * crosses the wire can be replayed: each nonce is used for one signature on
 * one connection. What this does not stop, since port 1511 is plaintext, is
 * someone on the path taking over a connection *after* it authenticated, or a
 * malicious server relaying another server's challenge to its own players;
 * the stakes (a weekly ranking) do not justify a TLS stack in fb-server. */

#define ACCOUNT_SIGNED_PREFIX "frozen-bubble account v1 "
#define ACCOUNT_ID_HEX_LEN 16

/* Clear fd's account state; called when a connection closes. */
void account_reset(int fd);

/* AUTH: remember the public key and write "CHALLENGE <nonce>" to reply.
 * Returns 0 and writes an error word to reply on a malformed key, when fd has
 * already authenticated, or when no randomness is available. */
int account_begin(int fd, const char* pubkey_hex, char* reply, size_t replysz);

/* AUTHSIG: check the signature against the outstanding challenge. The
 * challenge is spent either way, so a wrong guess cannot be retried against
 * it. Returns 1 on success, after which account_id(fd) is set. */
int account_finish(int fd, const char* sig_hex);

/* The connection's verified account id (16 hex), or "" when it has none. */
const char* account_id(int fd);

/* True between a successful AUTH and its AUTHSIG. */
int account_pending(int fd);

#endif
