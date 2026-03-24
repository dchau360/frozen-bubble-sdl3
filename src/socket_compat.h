/*
 * socket_compat.h — Cross-platform socket include shim.
 * Maps POSIX socket API to Winsock2 on Windows (MinGW64).
 */
#pragma once

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
// Map POSIX socket close → Winsock closesocket
#  define close(s) closesocket(s)
// Map POSIX EAGAIN/EWOULDBLOCK to Winsock equivalent
#  ifndef EAGAIN
#    define EAGAIN WSAEWOULDBLOCK
#  endif
#  ifndef EWOULDBLOCK
#    define EWOULDBLOCK WSAEWOULDBLOCK
#  endif
// socklen_t is not defined in older MinGW
typedef int socklen_t;
// ssize_t is POSIX-only
typedef int ssize_t;
// MSG_DONTWAIT is POSIX-only; on Windows use non-blocking mode via ioctlsocket
#  ifndef MSG_DONTWAIT
#    define MSG_DONTWAIT 0
#  endif
// setsockopt on Windows requires (const char*) optval
#  define SETSOCKOPT_OPTVAL(v) (reinterpret_cast<const char*>(&(v)))
// Helper: retrieve last socket error (errno doesn't work for Winsock calls)
#  define SOCK_ERRNO WSAGetLastError()
#  define SOCK_WOULD_BLOCK(e) ((e) == WSAEWOULDBLOCK)
// INVALID_SOCKET already defined by winsock2
// Inline WSA init helper — call once before first socket use
#include <cstdio>
inline bool socket_init() {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2,2), &wsa) == 0;
}
inline void socket_cleanup() { WSACleanup(); }
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#  include <fcntl.h>
#  define SETSOCKOPT_OPTVAL(v) (&(v))
#  define SOCK_ERRNO errno
#  define SOCK_WOULD_BLOCK(e) ((e) == EAGAIN || (e) == EWOULDBLOCK)
#  ifndef INVALID_SOCKET
#    define INVALID_SOCKET (-1)
#  endif
inline bool socket_init() { return true; }
inline void socket_cleanup() {}
#endif
