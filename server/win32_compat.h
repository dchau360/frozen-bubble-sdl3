/*
 * win32_compat.h — Windows (MinGW64) compatibility shim for fb-server.
 *
 * Replaces POSIX headers and calls that are unavailable on Windows:
 *   - Winsock2 for BSD socket API
 *   - poll → WSAPoll
 *   - uname/utsname stub
 *   - syslog → stderr fallback
 *   - pwd/user-switching stubs (no-op)
 *   - daemonize → no-op (server runs in foreground on Windows)
 */
#pragma once
#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ── socket close ─────────────────────────────────────────────────────── */
#define SOCKET_CLOSE(s) closesocket(s)

/* ── ssize_t ──────────────────────────────────────────────────────────── */
#if !defined(_SSIZE_T_) && !defined(_SSIZE_T_DEFINED)
typedef int ssize_t;
#  define _SSIZE_T_DEFINED
#endif

/* ── socket error / EAGAIN ────────────────────────────────────────────── */
#ifndef EAGAIN
#  define EAGAIN WSAEWOULDBLOCK
#endif
#ifndef EWOULDBLOCK
#  define EWOULDBLOCK WSAEWOULDBLOCK
#endif
/* errno is not set by Winsock calls; use WSAGetLastError() */
#define SOCK_ERRNO WSAGetLastError()
#define SOCK_EAGAIN (SOCK_ERRNO == WSAEWOULDBLOCK)

/* ── MSG_* flags ──────────────────────────────────────────────────────── */
#ifndef MSG_DONTWAIT
#  define MSG_DONTWAIT 0
#endif
#ifndef MSG_NOSIGNAL
#  define MSG_NOSIGNAL 0
#endif

/* ── poll ─────────────────────────────────────────────────────────────── */
/* WSAPoll has the same signature as POSIX poll */
#include <winsock2.h>
#define pollfd  WSAPOLLFD
#define poll    WSAPoll
#define POLLIN  POLLRDNORM

/* ── utsname ──────────────────────────────────────────────────────────── */
struct utsname {
    char sysname[64];
    char machine[64];
    char nodename[64];
    char release[64];
    char version[64];
};
static inline int uname(struct utsname *u) {
    strncpy(u->sysname,  "Windows", sizeof(u->sysname)  - 1);
    strncpy(u->machine,  "x86_64",  sizeof(u->machine)  - 1);
    strncpy(u->nodename, "unknown", sizeof(u->nodename) - 1);
    strncpy(u->release,  "unknown", sizeof(u->release)  - 1);
    strncpy(u->version,  "unknown", sizeof(u->version)  - 1);
    return 0;
}

/* ── pwd / user-switching (no-op on Windows) ──────────────────────────── */
typedef int uid_t;
typedef int gid_t;
struct passwd { char *pw_name; uid_t pw_uid; gid_t pw_gid; };
static inline struct passwd *getpwnam(const char *n) { (void)n; return NULL; }
static inline int setuid(uid_t u) { (void)u; return 0; }
static inline int setgid(gid_t g) { (void)g; return 0; }
static inline int setsid(void)    { return 1; }
static inline int chdir(const char *p) { (void)p; return 0; }

/* ── syslog → stderr ──────────────────────────────────────────────────── */
#define LOG_PID     0
#define LOG_DAEMON  0
#define LOG_DEBUG   7
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_WARNING 4
#define LOG_ERR     3
#define LOG_CRIT    2

static inline void openlog(const char *ident, int opt, int fac) {
    (void)ident; (void)opt; (void)fac;
}
static inline void closelog(void) {}

/* syslog on Windows: print to stderr */
static inline void syslog(int prio, const char *fmt, ...) {
    (void)prio;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* ── WSA init helper ──────────────────────────────────────────────────── */
static inline void win32_socket_init(void) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
}

#else  /* !_WIN32 */

/* POSIX: SOCKET_CLOSE is just close() */
#define SOCKET_CLOSE(s) close(s)
#define SOCK_ERRNO errno
#define SOCK_EAGAIN (errno == EAGAIN || errno == EWOULDBLOCK)
static inline void win32_socket_init(void) {}

#endif /* _WIN32 */
