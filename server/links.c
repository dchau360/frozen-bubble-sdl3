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

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <glib.h>
#include "links.h"
#include "account.h"
#include "log.h"
#include "monocypher.h"

#define SALT_LEN 16
#define HASH_LEN 32

typedef struct {
        char name[16];                      /* lower-cased, is_nick_ok() shape */
        char id[ACCOUNT_ID_HEX_LEN + 1];    /* the account that set it */
        uint8_t salt[SALT_LEN];
        uint8_t hash[HASH_LEN];             /* BLAKE2b keyed with salt, over the PIN */
} Pin;

typedef struct {
        int fails;
        time_t window_start;
} Tries;

static GHashTable* pins = NULL;    /* lower-cased name -> Pin* */
static GHashTable* links = NULL;   /* alias id -> canonical id (g_strdup) */
static GHashTable* tries = NULL;   /* lower-cased name -> Tries*, memory only */
static char* file_path = NULL;

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

static int is_id(const char* s)
{
        size_t i;
        if (!s || strlen(s) != ACCOUNT_ID_HEX_LEN) return 0;
        for (i = 0; i < ACCOUNT_ID_HEX_LEN; i++)
                if (!isxdigit((unsigned char)s[i])) return 0;
        return 1;
}

/* is_nick_ok()'s shape, lower-cased into out (16 bytes). */
static int name_key(const char* name, char* out)
{
        size_t i, n = name ? strlen(name) : 0;
        if (n < 1 || n > 10) return 0;
        for (i = 0; i < n; i++) {
                char c = name[i];
                if (!isalnum((unsigned char)c) && c != '_' && c != '-') return 0;
                out[i] = (char)tolower((unsigned char)c);
        }
        out[n] = '\0';
        return 1;
}

static int pin_ok(const char* pin)
{
        size_t i, n = pin ? strlen(pin) : 0;
        if (n < LINKS_PIN_MIN || n > LINKS_PIN_MAX) return 0;
        for (i = 0; i < n; i++)
                if (!isdigit((unsigned char)pin[i])) return 0;
        return 1;
}

static void hash_pin(const uint8_t* salt, const char* pin, uint8_t* out)
{
        crypto_blake2b_keyed(out, HASH_LEN, salt, SALT_LEN, (const uint8_t*)pin, strlen(pin));
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

static void save(void)
{
        GHashTableIter it;
        gpointer k, v;
        char* tmp;
        FILE* f;
        if (!file_path) return;
        /* Write-then-rename, like the other stores. */
        tmp = g_strdup_printf("%s.tmp", file_path);
        f = fopen(tmp, "w");
        if (!f) {
                l2(OUTPUT_TYPE_ERROR, "Failed to save links to %s: %s", tmp, strerror(errno));
                g_free(tmp);
                return;
        }
        fprintf(f, "v1\n");
        g_hash_table_iter_init(&it, pins);
        while (g_hash_table_iter_next(&it, &k, &v)) {
                Pin* p = v;
                char salt[SALT_LEN * 2 + 1], hash[HASH_LEN * 2 + 1];
                tohex(p->salt, SALT_LEN, salt);
                tohex(p->hash, HASH_LEN, hash);
                fprintf(f, "pin %s %s %s %s\n", p->name, p->id, salt, hash);
        }
        g_hash_table_iter_init(&it, links);
        while (g_hash_table_iter_next(&it, &k, &v))
                fprintf(f, "link %s %s\n", (char*)k, (char*)v);
        if (fclose(f) != 0 || rename(tmp, file_path) != 0)
                l2(OUTPUT_TYPE_ERROR, "Failed to save links to %s: %s", file_path, strerror(errno));
        g_free(tmp);
}

void links_init(void)
{
        const char* explicit_path = getenv("FB_SERVER_LINKS_FILE");
        const char* hiscores = getenv("FB_SERVER_HISCORE_FILE");
        const char* home = getenv("HOME");
        FILE* f;
        char buf[512];

        pins = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        links = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        tries = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        if (explicit_path && *explicit_path) {
                file_path = g_strdup(explicit_path);
        } else if (hiscores && *hiscores) {
                char* dir = g_path_get_dirname(hiscores);
                file_path = g_build_filename(dir, "links.dat", NULL);
                g_free(dir);
        } else if (home) {
                file_path = g_strdup_printf("%s/.fb-server/links.dat", home);
        } else {
                file_path = g_strdup("/var/lib/fb-server/links.dat");
        }
        {
                char* dir = g_path_get_dirname(file_path);
                mkdir(dir, 0755);
                g_free(dir);
        }
        l1(OUTPUT_TYPE_INFO, "Account links file: %s", file_path);

        f = fopen(file_path, "r");
        if (!f) return;
        while (fgets(buf, sizeof(buf), f)) {
                char a[64], b[64], c[64], d[96];
                if (sscanf(buf, "pin %63s %63s %63s %95s", a, b, c, d) == 4) {
                        Pin* p = g_new0(Pin, 1);
                        if (!name_key(a, p->name) || !is_id(b)
                            || !unhex(c, p->salt, SALT_LEN) || !unhex(d, p->hash, HASH_LEN)) {
                                g_free(p);
                                continue;
                        }
                        g_strlcpy(p->id, b, sizeof(p->id));
                        g_hash_table_replace(pins, g_strdup(p->name), p);
                } else if (sscanf(buf, "link %63s %63s", a, b) == 2 && is_id(a) && is_id(b)
                           && strcmp(a, b) != 0) {
                        g_hash_table_replace(links, g_strdup(a), g_strdup(b));
                }
        }
        fclose(f);
        l2(OUTPUT_TYPE_INFO, "Account links: %d PINs, %d links", g_hash_table_size(pins),
           g_hash_table_size(links));
}

const char* links_resolve(const char* id)
{
        const char* cur = id;
        int hops;
        if (!links || !id) return id ? id : "";
        /* A hand-edited file could hold a loop; stop rather than spin. */
        for (hops = 0; hops < 16; hops++) {
                const char* next = g_hash_table_lookup(links, cur);
                if (!next) break;
                cur = next;
        }
        return cur;
}

/* The PIN entry `id` owns, if any. */
static const char* pin_name_of(const char* id)
{
        GHashTableIter it;
        gpointer k, v;
        g_hash_table_iter_init(&it, pins);
        while (g_hash_table_iter_next(&it, &k, &v))
                if (strcmp(((Pin*)v)->id, id) == 0)
                        return k;
        return NULL;
}

enum links_result links_set_pin(const char* id, const char* name, const char* pin)
{
        char key[16];
        Pin* p;
        const char* old;
        if (!pins || !is_id(id) || !name_key(name, key) || !pin_ok(pin))
                return LINKS_INVALID;
        p = g_hash_table_lookup(pins, key);
        /* A name whose PIN belongs to an account that now signs in as this
         * one (it linked here) is ours to take over. */
        if (p && strcmp(links_resolve(p->id), id) != 0)
                return LINKS_NAME_TAKEN;
        while ((old = pin_name_of(id)))
                g_hash_table_remove(pins, old);
        p = g_new0(Pin, 1);
        g_strlcpy(p->name, key, sizeof(p->name));
        g_strlcpy(p->id, id, sizeof(p->id));
        if (!random_bytes(p->salt, SALT_LEN)) {
                g_free(p);
                return LINKS_INVALID;
        }
        hash_pin(p->salt, pin, p->hash);
        g_hash_table_replace(pins, g_strdup(key), p);
        g_hash_table_remove(tries, key);
        save();
        return LINKS_OK;
}

enum links_result links_link(const char* id, const char* name, const char* pin, char* out)
{
        char key[16];
        Pin* p;
        Tries* t;
        uint8_t hash[HASH_LEN];
        time_t now = time(NULL);
        const char* canon;
        const char* old;
        int match;
        if (!pins || !is_id(id) || !name_key(name, key) || !pin_ok(pin))
                return LINKS_INVALID;
        t = g_hash_table_lookup(tries, key);
        if (t && now - t->window_start >= LINKS_TRY_WINDOW_SECS) {
                g_hash_table_remove(tries, key);
                t = NULL;
        }
        if (t && t->fails >= LINKS_MAX_TRIES)
                return LINKS_TOO_MANY_TRIES;
        p = g_hash_table_lookup(pins, key);
        match = 0;
        if (p) {
                hash_pin(p->salt, pin, hash);
                match = crypto_verify32(hash, p->hash) == 0;
                crypto_wipe(hash, sizeof(hash));
        }
        /* An unknown name counts and answers like a wrong PIN, so the reply
         * never says which names have one. */
        if (!match) {
                if (!t) {
                        t = g_new0(Tries, 1);
                        t->window_start = now;
                        g_hash_table_replace(tries, g_strdup(key), t);
                }
                t->fails++;
                return LINKS_WRONG_PIN;
        }
        g_hash_table_remove(tries, key);
        canon = links_resolve(p->id);
        g_strlcpy(out, canon, ACCOUNT_ID_HEX_LEN + 1);
        if (strcmp(canon, id) == 0)
                return LINKS_OK;
        g_hash_table_replace(links, g_strdup(id), g_strdup(canon));
        /* This id no longer signs in as itself, so a PIN it set would point
         * at a line nobody can reach; the account it joined keeps its own. */
        while ((old = pin_name_of(id)))
                g_hash_table_remove(pins, old);
        save();
        return LINKS_OK;
}

void links_forget(const char* id)
{
        GHashTableIter it;
        gpointer k, v;
        const char* old;
        int changed = 0;
        if (!pins || !id || !*id) return;
        while ((old = pin_name_of(id))) {
                g_hash_table_remove(pins, old);
                changed = 1;
        }
        /* Every device that signed in as this account starts over as itself.
         * Resolve them all before removing any, since removing one link of a
         * chain would change what the others resolve to. */
        {
                GPtrArray* drop = g_ptr_array_new_with_free_func(g_free);
                guint i;
                g_hash_table_iter_init(&it, links);
                while (g_hash_table_iter_next(&it, &k, &v))
                        if (strcmp(k, id) == 0 || strcmp(links_resolve(v), id) == 0)
                                g_ptr_array_add(drop, g_strdup(k));
                for (i = 0; i < drop->len; i++)
                        g_hash_table_remove(links, g_ptr_array_index(drop, i));
                if (drop->len) changed = 1;
                g_ptr_array_free(drop, TRUE);
        }
        if (changed) save();
}
