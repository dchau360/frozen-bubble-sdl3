/* See geoip.h. */
#include "geoip.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* An address is kept as 16 bytes: IPv6 as is, IPv4 as ::ffff:a.b.c.d, so one
 * ordering covers both families and a plain memcmp compares them. */
struct range {
        unsigned char start[16];
        unsigned char end[16];
        char cc[2];
};

static struct range* table;
static size_t table_len;

#define GEOIP_MAX_RANGES 4000000

static int parse_addr(const char* text, unsigned char out[16])
{
        struct in_addr a4;
        struct in6_addr a6;
        if (inet_pton(AF_INET, text, &a4) == 1) {
                memset(out, 0, 10);
                out[10] = out[11] = 0xff;
                memcpy(out + 12, &a4, 4);
                return 1;
        }
        if (inet_pton(AF_INET6, text, &a6) == 1) {
                memcpy(out, &a6, 16);
                return 1;
        }
        return 0;
}

static int cmp_range(const void* a, const void* b)
{
        return memcmp(((const struct range*)a)->start, ((const struct range*)b)->start, 16);
}

/* Copies one comma-separated field into buf without its quotes. */
static const char* next_field(const char* p, char* buf, size_t bufsz)
{
        size_t n = 0;
        while (*p == ' ' || *p == '"') p++;
        while (*p && *p != ',' && *p != '\n' && *p != '\r' && *p != '"') {
                if (n + 1 < bufsz) buf[n++] = *p;
                p++;
        }
        buf[n] = '\0';
        while (*p == '"') p++;
        if (*p == ',') p++;
        return p;
}

int geoip_load(const char* path)
{
        free(table);
        table = NULL;
        table_len = 0;
        if (!path || !*path) return 0;
        FILE* f = fopen(path, "r");
        if (!f) return 0;

        size_t cap = 1024;
        struct range* t = malloc(cap * sizeof(*t));
        size_t n = 0;
        char line[256];
        while (t && fgets(line, sizeof(line), f)) {
                char a[64], b[64], cc[16];
                const char* p = next_field(line, a, sizeof(a));
                p = next_field(p, b, sizeof(b));
                next_field(p, cc, sizeof(cc));
                struct range r;
                if (!parse_addr(a, r.start) || !parse_addr(b, r.end)) continue;
                if (strlen(cc) != 2 || !isalpha((unsigned char)cc[0]) || !isalpha((unsigned char)cc[1]))
                        continue;
                r.cc[0] = (char)toupper((unsigned char)cc[0]);
                r.cc[1] = (char)toupper((unsigned char)cc[1]);
                if (memcmp(r.start, r.end, 16) > 0) continue;
                if (n >= GEOIP_MAX_RANGES) break;
                if (n == cap) {
                        struct range* grown = realloc(t, cap * 2 * sizeof(*t));
                        if (!grown) break;
                        t = grown;
                        cap *= 2;
                }
                t[n++] = r;
        }
        fclose(f);
        if (!t || n == 0) {
                free(t);
                return 0;
        }
        qsort(t, n, sizeof(*t), cmp_range);
        table = t;
        table_len = n;
        return (int)n;
}

int geoip_lookup(const char* ip, char out[3])
{
        unsigned char key[16];
        if (!table_len || !ip || !parse_addr(ip, key)) return 0;
        /* The last range starting at or before the address... */
        size_t lo = 0, hi = table_len;
        while (lo < hi) {
                size_t mid = lo + (hi - lo) / 2;
                if (memcmp(table[mid].start, key, 16) <= 0) lo = mid + 1;
                else hi = mid;
        }
        if (lo == 0) return 0;
        const struct range* r = &table[lo - 1];
        /* ...and the address has to fall inside it, not in a gap after it. */
        if (memcmp(key, r->end, 16) > 0) return 0;
        out[0] = r->cc[0];
        out[1] = r->cc[1];
        out[2] = '\0';
        return 1;
}
