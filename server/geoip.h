/*
 * IP address -> ISO 3166-1 alpha-2 country, from a DB-IP "country lite" CSV.
 *
 * Used for one thing: a browser (WebSocket) player reaches fb-server through
 * nginx, so the connection's own address is nginx's, and the player's
 * country never reached the high scores (the game only reports one after the
 * online lobby's own lookup, which a browser or a solo-only player never runs).
 * nginx now passes the visitor's address in X-Forwarded-For; ws.c reads it
 * from a trusted proxy, and this turns it into a two-letter code. Only the
 * code is kept (country_tag[]); the address itself is neither stored nor
 * logged by this.
 *
 * Optional: with no file configured (FB_SERVER_GEOIP_FILE unset) or an
 * unreadable one, geoip_lookup() finds nothing and everything works as before.
 */
#ifndef FB_GEOIP_H
#define FB_GEOIP_H

/* Loads a CSV of "start_ip,end_ip,CC" lines (IPv4 and IPv6, optionally
 * quoted). Returns the number of ranges loaded, 0 on failure or an empty file;
 * a second call replaces the table. */
int geoip_load(const char* path);

/* Looks `ip` (text, IPv4 or IPv6) up. Writes the two-letter upper-case code
 * and a NUL to out[0..2] and returns 1, or returns 0 and leaves out alone. */
int geoip_lookup(const char* ip, char out[3]);

#endif
