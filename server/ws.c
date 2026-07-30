/*
 * WebSocket upgrade and frame codec for fb-server.
 *
 * Allows browser clients (WASM, using emscripten WebSocket API) to connect to
 * fb-server on the same port as native TCP clients.  After accept() the server
 * briefly peeks at the first bytes; if it sees an HTTP GET it performs the RFC
 * 6455 WebSocket handshake.  Thereafter send/recv are wrapped transparently.
 */

#include "ws.h"
#include "win32_compat.h"

#ifndef _WIN32
#  include <unistd.h>
#  include <sys/socket.h>
#  include <sys/select.h>
#  include <arpa/inet.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/* ── Per-connection WebSocket flag ───────────────────────────────────────── */

static int is_ws[256];

int ws_is_websocket(int fd) { return (fd >= 0 && fd < 256) ? is_ws[fd] : 0; }
void ws_reset(int fd)       { if (fd >= 0 && fd < 256) is_ws[fd] = 0; }

/* ── SHA-1 (FIPS 180-4) ─────────────────────────────────────────────────────
 * Minimal public-domain implementation, sufficient for WebSocket handshake.  */

#define ROL32(v,n) (((v)<<(n))|((uint32_t)(v)>>(32-(n))))

typedef struct { uint32_t s[5]; uint64_t bc; uint8_t buf[64]; int bi; } SHA1_CTX;

static void sha1_block(SHA1_CTX* c)
{
    uint32_t w[80], a, b, cc, d, e, f, k, t;
    int i;
    for (i = 0; i < 16; i++) {
        uint8_t* p = c->buf + i*4;
        w[i] = ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
    }
    for (; i < 80; i++) w[i] = ROL32(w[i-3]^w[i-8]^w[i-14]^w[i-16], 1);
    a=c->s[0]; b=c->s[1]; cc=c->s[2]; d=c->s[3]; e=c->s[4];
    for (i = 0; i < 80; i++) {
        if      (i < 20) { f=(b&cc)|((~b)&d); k=0x5A827999U; }
        else if (i < 40) { f=b^cc^d;           k=0x6ED9EBA1U; }
        else if (i < 60) { f=(b&cc)|(b&d)|(cc&d); k=0x8F1BBCDCU; }
        else             { f=b^cc^d;           k=0xCA62C1D6U; }
        t=ROL32(a,5)+f+e+k+w[i]; e=d; d=cc; cc=ROL32(b,30); b=a; a=t;
    }
    c->s[0]+=a; c->s[1]+=b; c->s[2]+=cc; c->s[3]+=d; c->s[4]+=e;
}

static void sha1_init(SHA1_CTX* c) {
    c->s[0]=0x67452301U; c->s[1]=0xEFCDAB89U; c->s[2]=0x98BADCFEU;
    c->s[3]=0x10325476U; c->s[4]=0xC3D2E1F0U; c->bc=0; c->bi=0;
}
static void sha1_update(SHA1_CTX* c, const uint8_t* d, size_t n) {
    c->bc += (uint64_t)n * 8;
    for (size_t i = 0; i < n; i++) {
        c->buf[c->bi++] = d[i];
        if (c->bi == 64) { sha1_block(c); c->bi = 0; }
    }
}
static void sha1_final(SHA1_CTX* c, uint8_t out[20]) {
    c->buf[c->bi++] = 0x80;
    if (c->bi > 56) { while (c->bi < 64) c->buf[c->bi++]=0; sha1_block(c); c->bi=0; }
    while (c->bi < 56) c->buf[c->bi++] = 0;
    for (int i = 7; i >= 0; i--) c->buf[56+(7-i)] = (uint8_t)(c->bc >> (i*8));
    sha1_block(c);
    for (int i = 0; i < 5; i++) {
        out[i*4+0]=(uint8_t)(c->s[i]>>24); out[i*4+1]=(uint8_t)(c->s[i]>>16);
        out[i*4+2]=(uint8_t)(c->s[i]>>8);  out[i*4+3]=(uint8_t) c->s[i];
    }
}
static void sha1(const uint8_t* data, size_t len, uint8_t out[20])
{ SHA1_CTX c; sha1_init(&c); sha1_update(&c, data, len); sha1_final(&c, out); }

/* ── Base64 encoder ──────────────────────────────────────────────────────── */

static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode(const uint8_t* in, int n, char* out)
{
    int i = 0, j = 0;
    for (; i < n - 2; i += 3) {
        out[j++] = B64[ in[i]>>2 ];
        out[j++] = B64[((in[i]&3)<<4)|(in[i+1]>>4)];
        out[j++] = B64[((in[i+1]&15)<<2)|(in[i+2]>>6)];
        out[j++] = B64[ in[i+2]&63 ];
    }
    if (n%3 == 1) {
        out[j++]=B64[in[i]>>2]; out[j++]=B64[(in[i]&3)<<4];
        out[j++]='=';           out[j++]='=';
    } else if (n%3 == 2) {
        out[j++]=B64[in[i]>>2]; out[j++]=B64[((in[i]&3)<<4)|(in[i+1]>>4)];
        out[j++]=B64[(in[i+1]&15)<<2]; out[j++]='=';
    }
    out[j]='\0';
}

/* ── WebSocket handshake ─────────────────────────────────────────────────── */

#define WS_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

int ws_detect_and_upgrade(int fd)
{
    /* Wait up to 50 ms for the client to send the first bytes.
     * Native TCP clients wait for the server greeting before sending anything,
     * so a 50 ms window cleanly distinguishes them from browser WebSocket
     * clients, which send the HTTP upgrade request immediately on connect.
     * 50 ms is intentionally short so the server greeting reaches native
     * clients well within their SERVER_READY receive window. */
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    tv.tv_sec = 0; tv.tv_usec = 50000;
    if (select(fd+1, &rfds, NULL, NULL, &tv) <= 0)
        return 0;   /* timeout — plain TCP client */

    /* Peek at first 4 bytes without consuming them */
    char peek[4];
    if (recv(fd, peek, 4, MSG_PEEK) < 4 || strncmp(peek, "GET ", 4) != 0)
        return 0;   /* not an HTTP request — plain TCP client */

    /* Consume the full HTTP upgrade request */
    char req[4096];
    ssize_t n = recv(fd, req, sizeof(req)-1, 0);
    if (n <= 0) return 0;
    req[n] = '\0';

    /* Locate Sec-WebSocket-Key header */
    const char* key_hdr = strstr(req, "Sec-WebSocket-Key:");
    if (!key_hdr) key_hdr = strstr(req, "Sec-Websocket-Key:"); /* fallback */
    if (!key_hdr) return 0;
    key_hdr += 18;
    while (*key_hdr == ' ') key_hdr++;
    const char* key_end = key_hdr;
    while (*key_end && *key_end != '\r' && *key_end != '\n') key_end++;
    int klen = (int)(key_end - key_hdr);
    if (klen <= 0 || klen > 64) return 0;

    /* Compute SHA-1(key + WS_MAGIC) and Base64-encode */
    char combined[128];
    memcpy(combined, key_hdr, (size_t)klen);
    memcpy(combined + klen, WS_MAGIC, sizeof(WS_MAGIC)-1);

    uint8_t digest[20];
    sha1((uint8_t*)combined, (size_t)(klen + (int)(sizeof(WS_MAGIC)-1)), digest);

    char accept_key[32];
    base64_encode(digest, 20, accept_key);

    /* Send HTTP 101 Switching Protocols */
    char response[256];
    int rlen = snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n",
        accept_key);
    send(fd, response, (size_t)rlen, MSG_NOSIGNAL);

    is_ws[fd] = 1;
    return 1;
}

/* ── WebSocket frame send (server → client, no masking) ─────────────────── */

/* Returns len when the whole frame reached the socket, -1 otherwise. Callers
   treat any nonnegative result as "sent it all", so a partial result must never
   be reported as success: the peer would be left mid-frame and every later
   frame would be parsed at the wrong offset. */
ssize_t ws_send(int fd, const char* data, int len)
{
    /* Max game protocol message is well under 16 KB */
    unsigned char frame[16400];
    int hdr;
    ssize_t sent;

    if (len < 0)
        return -1;
    if (len <= 125)
        hdr = 2;
    else if (len <= 65535)
        hdr = 4;
    else
        return -1;  /* shouldn't happen */
    /* The header states the payload length, so the payload has to be all here.
       Clamping it to fit while the header still advertises the original length
       desynchronizes the peer's parser for the rest of the connection; refuse
       to emit the frame at all instead. */
    if ((size_t)len > sizeof(frame) - (size_t)hdr)
        return -1;

    frame[0] = 0x82;            /* FIN=1, opcode=2 (binary) — avoids UTF-8 rejection
                                 * of GAME_CAN_START which embeds raw binary player IDs */
    if (hdr == 2) {
        frame[1] = (unsigned char)len;
    } else {
        frame[1] = 126;
        frame[2] = (unsigned char)(len >> 8);
        frame[3] = (unsigned char)(len & 0xFF);
    }
    memcpy(frame + hdr, data, (size_t)len);

    for (sent = 0; sent < (ssize_t)(hdr + len); ) {
        ssize_t n = send(fd, frame + sent, (size_t)((ssize_t)(hdr + len) - sent), MSG_NOSIGNAL);
        if (n > 0) {
            sent += n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        return -1;
    }
    return len;
}

/* ── WebSocket frame decode (in-place, client → server) ─────────────────── *
 *                                                                            *
 * Decodes all complete WebSocket frames in buf[0..*len-1] in-place.         *
 *                                                                            *
 * On return:                                                                 *
 *   buf[0..retval-1]        = decoded payload bytes (game messages)         *
 *   buf[retval..*len-1]     = raw partial frame bytes (if any)              *
 *   *len                    = retval + raw_partial_bytes                     *
 *                                                                            *
 * Returns:  N >= 0  number of decoded payload bytes (0 = only partial frame)*
 *          -1       fatal error: close frame or unsupported 64-bit length   *
 *                                                                            *
 * Caller must:                                                               *
 *   1. Process buf[0..retval-1] as game protocol messages                   *
 *   2. Save buf[retval..*len-1] to the fd's incoming buffer (raw WS bytes)  *
 *      to be prepended on the next recv and re-decoded then                 */

int ws_decode_inplace(char* buf, int* len)
{
    int total = *len;
    int out = 0;    /* write cursor into decoded-payload region */
    int pos = 0;    /* read cursor through raw frame bytes      */

    while (pos < total) {
        /* Need at least 2 bytes for the minimal frame header */
        if (total - pos < 2) goto incomplete;

        unsigned char* b = (unsigned char*)buf + pos;
        int opcode = b[0] & 0x0F;
        int masked  = (b[1] >> 7) & 1;
        int plen    = b[1] & 0x7F;
        int hdr_len = 2;

        if (opcode == 8) return -1;     /* close frame */
        if (plen == 127) return -1;     /* 64-bit length — not used here */

        if (plen == 126) {
            if (total - pos < 4) goto incomplete;
            plen = ((int)b[2] << 8) | (int)b[3];
            hdr_len = 4;
        }
        if (masked) hdr_len += 4;

        /* Ensure the full frame is present */
        if (total - pos < hdr_len + plen) goto incomplete;

        unsigned char* payload = b + hdr_len;
        if (masked) {
            const unsigned char* mask = b + hdr_len - 4;
            for (int i = 0; i < plen; i++)
                payload[i] ^= mask[i & 3];
        }

        /* Append decoded payload.  out <= pos always, so this is safe. */
        memmove(buf + out, payload, (size_t)plen);
        out += plen;
        pos += hdr_len + plen;

        /* Ping frames (opcode 9) carry no game payload — skip them */
        if (opcode == 9) { out -= plen; continue; }
    }

    *len = out;     /* no partial frame: *len == retval */
    return out;

incomplete:
    /* buf[0..out-1]           = decoded payloads (game messages)
     * buf[out..out+partial-1] = raw partial frame bytes (must be re-decoded) */
    memmove(buf + out, buf + pos, (size_t)(total - pos));
    *len = out + (total - pos);
    return out;     /* caller saves buf[out..*len-1] as raw frame bytes */
}
