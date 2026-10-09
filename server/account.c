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

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "account.h"
#include "links.h"
#include "monocypher.h"

typedef struct {
        int pending;              /* challenge sent, AUTHSIG not yet seen */
        uint8_t pubkey[32];
        uint8_t nonce[32];
        char id[ACCOUNT_ID_HEX_LEN + 1];
} AccountState;

static AccountState state[256];

static int unhex(const char* s, uint8_t* out, size_t n)
{
        size_t i;
        if (!s || strlen(s) != n * 2) return 0;
        for (i = 0; i < n * 2; i++) {
                char c = s[i];
                int v = (c >= '0' && c <= '9') ? c - '0'
                      : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                      : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
                if (v < 0) return 0;
                if (i % 2 == 0) out[i / 2] = (uint8_t)(v << 4);
                else out[i / 2] |= (uint8_t)v;
        }
        return 1;
}

static void tohex(const uint8_t* in, size_t n, char* out)
{
        static const char digits[] = "0123456789abcdef";
        size_t i;
        for (i = 0; i < n; i++) {
                out[i * 2] = digits[in[i] >> 4];
                out[i * 2 + 1] = digits[in[i] & 15];
        }
        out[n * 2] = '\0';
}

static int random_bytes(uint8_t* out, size_t n)
{
        FILE* f = fopen("/dev/urandom", "rb");
        size_t got;
        if (!f) return 0;
        got = fread(out, 1, n, f);
        fclose(f);
        return got == n;
}

void account_reset(int fd)
{
        if (fd < 0 || fd >= 256) return;
        crypto_wipe(&state[fd], sizeof(state[fd]));
}

int account_begin(int fd, const char* pubkey_hex, char* reply, size_t replysz)
{
        AccountState* st;
        char nonce_hex[65];
        if (fd < 0 || fd >= 256) return 0;
        st = &state[fd];
        if (st->id[0]) {
                snprintf(reply, replysz, "ALREADY_AUTHENTICATED");
                return 0;
        }
        if (!unhex(pubkey_hex, st->pubkey, sizeof(st->pubkey))) {
                st->pending = 0;
                snprintf(reply, replysz, "INVALID_KEY");
                return 0;
        }
        if (!random_bytes(st->nonce, sizeof(st->nonce))) {
                st->pending = 0;
                snprintf(reply, replysz, "AUTH_UNAVAILABLE");
                return 0;
        }
        st->pending = 1;
        tohex(st->nonce, sizeof(st->nonce), nonce_hex);
        snprintf(reply, replysz, "CHALLENGE %s", nonce_hex);
        return 1;
}

int account_finish(int fd, const char* sig_hex)
{
        AccountState* st;
        uint8_t sig[64], hash[ACCOUNT_ID_HEX_LEN / 2];
        char msg[sizeof(ACCOUNT_SIGNED_PREFIX) + 64];
        char nonce_hex[65];
        int ok;
        if (fd < 0 || fd >= 256) return 0;
        st = &state[fd];
        if (!st->pending) return 0;
        st->pending = 0;
        if (!unhex(sig_hex, sig, sizeof(sig))) return 0;
        tohex(st->nonce, sizeof(st->nonce), nonce_hex);
        snprintf(msg, sizeof(msg), "%s%s", ACCOUNT_SIGNED_PREFIX, nonce_hex);
        ok = crypto_eddsa_check(sig, st->pubkey, (const uint8_t*)msg, strlen(msg)) == 0;
        crypto_wipe(st->nonce, sizeof(st->nonce));
        if (!ok) return 0;
        crypto_blake2b(hash, sizeof(hash), st->pubkey, sizeof(st->pubkey));
        tohex(hash, sizeof(hash), st->id);
        /* A device linked with a PIN signs in as the account it joined. */
        {
                const char* canon = links_resolve(st->id);
                if (canon != st->id && strlen(canon) == ACCOUNT_ID_HEX_LEN)
                        memcpy(st->id, canon, ACCOUNT_ID_HEX_LEN + 1);
        }
        return 1;
}

void account_set_id(int fd, const char* id)
{
        if (fd < 0 || fd >= 256 || !id || strlen(id) != ACCOUNT_ID_HEX_LEN) return;
        memcpy(state[fd].id, id, ACCOUNT_ID_HEX_LEN + 1);
}

const char* account_id(int fd)
{
        return (fd >= 0 && fd < 256) ? state[fd].id : "";
}

int account_pending(int fd)
{
        return fd >= 0 && fd < 256 && state[fd].pending;
}
