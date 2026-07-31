/*
 * WebSocket upgrade and frame codec for fb-server.
 * Allows browser clients (WASM) to connect alongside native TCP clients.
 */

#pragma once

#include <sys/types.h>

int     ws_is_websocket(int fd);
void    ws_reset(int fd);

/* After accept(): sniff for HTTP upgrade; upgrade the fd if WS.
 * Returns 1 if upgraded, 0 if plain TCP (leaves fd untouched). */
int     ws_detect_and_upgrade(int fd);

/* Try to complete a WebSocket upgrade from already-received data.
 * data[0..len-1] is the data received so far from a newly accepted
 * connection. Returns:
 *   >0  bytes consumed (the HTTP request) — upgrade completed;
 *        caller must treat the fd as WebSocket and send greeting as WS frame
 *    0  definitely not a WebSocket request (at least 4 bytes seen
 *        and they don't start with "GET ") — classify as plain TCP
 *   -1  incomplete — the data so far is consistent with a WebSocket
 *        handshake (starts with "GET " or fewer than 4 bytes yet) but
 *        \r\n\r\n hasn't arrived; caller must keep accumulating */
int     ws_try_upgrade_from_data(int fd, const char* data, int len);

/* Send data wrapped in a WebSocket text frame. */
ssize_t ws_send(int fd, const char* data, int len);

/* Decode all complete WebSocket frames in buf[0..*len-1] in-place.
 * On return: buf[0..retval-1] = decoded payload bytes (game messages);
 *            buf[retval..*len-1] = raw partial frame bytes (if any).
 * Returns:  N>=0  number of decoded payload bytes (0 = only partial frame)
 *          -1     fatal protocol error (close frame, unsupported length)  */
int     ws_decode_inplace(char* buf, int* len);
