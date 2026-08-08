/*******************************************************************************
 *
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "win32_compat.h"
#ifndef _WIN32
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#endif

#include <glib.h>

#include "notify.h"
#include "net.h"
#include "log.h"

typedef struct {
    char* platform;
    char* token;
    time_t registered_at;
    time_t last_notified;   // 0 = never notified yet
} NotifyReg;

// token -> NotifyReg*
static GHashTable* notify_table = NULL;
static char* notify_file_path = NULL;

// Cooldown between two notifications to the same device, so a busy server
// doesn't turn into a banner-spam machine. Overridable for testing.
#define DEFAULT_COOLDOWN_SECONDS 600
static int notify_cooldown_seconds = DEFAULT_COOLDOWN_SECONDS;

// UDP transport to the relay sidecar. -1 / unset means the feature is
// registered-but-inert: tokens are still accepted and persisted (so nothing
// is lost by running without a relay configured), but no datagram is ever
// sent.
static int notify_relay_socket = -1;
static struct sockaddr_in notify_relay_addr;
static int notify_relay_configured = 0;

static void notify_ensure_dir(void)
{
    if (!notify_file_path) return;
    char* path_copy = strdup(notify_file_path);
    char* last_slash = strrchr(path_copy, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir(path_copy, 0755);
    }
    free(path_copy);
}

static void notify_reg_free(gpointer data)
{
    NotifyReg* reg = (NotifyReg*)data;
    if (!reg) return;
    free(reg->platform);
    free(reg->token);
    free(reg);
}

// Resolves FB_SERVER_NOTIFY_RELAY ("host:port") into notify_relay_addr and
// opens the UDP socket. Failure here (bad hostname, no relay configured)
// just leaves the feature inert -- registrations still work, sends are
// silently skipped. This must never be fatal to server startup.
static void notify_relay_init(void)
{
    const char* relay = getenv("FB_SERVER_NOTIFY_RELAY");
    if (!relay || !*relay) {
        l0(OUTPUT_TYPE_INFO, "FB_SERVER_NOTIFY_RELAY not set; follow notifications are registered but never sent");
        return;
    }

    char* relay_copy = strdup(relay);
    char* colon = strrchr(relay_copy, ':');
    if (!colon) {
        l1(OUTPUT_TYPE_ERROR, "FB_SERVER_NOTIFY_RELAY '%s' is not host:port; notifications disabled", relay);
        free(relay_copy);
        return;
    }
    *colon = '\0';
    const char* host = relay_copy;
    int relay_port = atoi(colon + 1);
    if (relay_port <= 0 || relay_port > 65535) {
        l1(OUTPUT_TYPE_ERROR, "FB_SERVER_NOTIFY_RELAY has an invalid port in '%s'; notifications disabled", relay);
        free(relay_copy);
        return;
    }

    struct hostent* h = gethostbyname(host);
    if (!h || !h->h_addr_list || !h->h_addr_list[0]) {
        l1(OUTPUT_TYPE_ERROR, "FB_SERVER_NOTIFY_RELAY host '%s' does not resolve; notifications disabled", host);
        free(relay_copy);
        return;
    }

    memset(&notify_relay_addr, 0, sizeof(notify_relay_addr));
    notify_relay_addr.sin_family = AF_INET;
    notify_relay_addr.sin_port = htons((unsigned short)relay_port);
    memcpy(&notify_relay_addr.sin_addr, h->h_addr_list[0], sizeof(notify_relay_addr.sin_addr));

    notify_relay_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (notify_relay_socket < 0) {
        l1(OUTPUT_TYPE_ERROR, "notify: socket() failed: %s", strerror(errno));
        free(relay_copy);
        return;
    }

    notify_relay_configured = 1;
    l2(OUTPUT_TYPE_INFO, "Follow notifications will be relayed to %s:%d", host, relay_port);
    free(relay_copy);
}

void notify_init(void)
{
    notify_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, notify_reg_free);

    const char* cooldown_env = getenv("FB_SERVER_NOTIFY_COOLDOWN_SECONDS");
    if (cooldown_env && *cooldown_env) {
        int v = atoi(cooldown_env);
        if (v >= 0) notify_cooldown_seconds = v;
    }

    // Same precedence as stats.c's FB_SERVER_STATS_FILE: an explicit path
    // wins, then $HOME, then a system-wide fallback.
    const char* explicit_path = getenv("FB_SERVER_NOTIFY_FILE");
    const char* home = getenv("HOME");
    if (explicit_path && *explicit_path) {
        notify_file_path = g_strdup(explicit_path);
    } else if (home) {
        notify_file_path = g_strdup_printf("%s/.fb-server/notify.dat", home);
    } else {
        notify_file_path = g_strdup("/var/lib/fb-server/notify.dat");
    }
    l1(OUTPUT_TYPE_INFO, "Notify registrations file: %s", notify_file_path);

    notify_ensure_dir();

    FILE* f = fopen(notify_file_path, "r");
    if (f) {
        char platform_buf[16];
        char token_buf[512];
        long registered_at, last_notified;
        int loaded = 0;
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "%15s %511s %ld %ld", platform_buf, token_buf, &registered_at, &last_notified) == 4) {
                NotifyReg* reg = g_new0(NotifyReg, 1);
                reg->platform = g_strdup(platform_buf);
                reg->token = g_strdup(token_buf);
                reg->registered_at = (time_t)registered_at;
                reg->last_notified = (time_t)last_notified;
                g_hash_table_insert(notify_table, g_strdup(token_buf), reg);
                loaded++;
            }
        }
        fclose(f);
        l1(OUTPUT_TYPE_INFO, "Loaded %d follow-notification registration(s)", loaded);
    } else {
        l0(OUTPUT_TYPE_INFO, "No existing notify file, starting fresh");
    }

    notify_relay_init();
}

void notify_save(void)
{
    if (!notify_table || !notify_file_path) return;

    FILE* f = fopen(notify_file_path, "w");
    if (!f) {
        l1(OUTPUT_TYPE_ERROR, "Failed to save notify file: %s", notify_file_path);
        return;
    }

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, notify_table);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        NotifyReg* reg = (NotifyReg*)value;
        fprintf(f, "%s %s %ld %ld\n", reg->platform, reg->token,
                (long)reg->registered_at, (long)reg->last_notified);
    }
    fclose(f);
}

void notify_register(const char* platform, const char* token)
{
    if (!notify_table || !platform || !token || !*token) return;

    NotifyReg* reg = g_hash_table_lookup(notify_table, token);
    if (reg) {
        // Refresh platform (harmless if unchanged) without disturbing the
        // cooldown clock -- re-registering (e.g. on every reconnect, to
        // cover token rotation) must not let a device dodge the cooldown by
        // repeatedly re-registering.
        free(reg->platform);
        reg->platform = strdup(platform);
    } else {
        reg = g_new0(NotifyReg, 1);
        reg->platform = strdup(platform);
        reg->token = strdup(token);
        reg->registered_at = time(NULL);
        reg->last_notified = 0;
        g_hash_table_insert(notify_table, g_strdup(token), reg);
    }
    notify_save();
}

void notify_unregister(const char* token)
{
    if (!notify_table || !token) return;
    if (g_hash_table_remove(notify_table, token)) {
        notify_save();
    }
}

static void notify_send_one(const NotifyReg* reg, const char* message)
{
    if (!notify_relay_configured) return;

    char datagram[1024];
    snprintf(datagram, sizeof(datagram), "NOTIFY|%s|%s|%s", reg->platform, reg->token, message);

    if (sendto(notify_relay_socket, datagram, strlen(datagram), 0,
               (struct sockaddr*)&notify_relay_addr, sizeof(notify_relay_addr)) < 0) {
        // Best-effort by design: log and move on, never block or retry on
        // the main event loop.
        l1(OUTPUT_TYPE_ERROR, "notify: sendto relay failed: %s", strerror(errno));
    }
}

void notify_fire_join_event(void)
{
    if (!notify_table || g_hash_table_size(notify_table) == 0) return;
    if (!notify_relay_configured) return;   // nothing to do without a relay

    char message[256];
    snprintf(message, sizeof(message), "A player just joined %s!", net_servername());

    time_t now = time(NULL);
    int fired = 0;
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, notify_table);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        NotifyReg* reg = (NotifyReg*)value;
        if (reg->last_notified != 0 && difftime(now, reg->last_notified) < notify_cooldown_seconds)
            continue;
        notify_send_one(reg, message);
        reg->last_notified = now;
        fired++;
    }
    if (fired > 0) {
        l1(OUTPUT_TYPE_DEBUG, "notify: fired join-event to %d device(s)", fired);
        notify_save();
    }
}

void notify_cleanup(void)
{
    if (notify_table) {
        notify_save();
        g_hash_table_destroy(notify_table);
        notify_table = NULL;
    }
    if (notify_file_path) {
        g_free(notify_file_path);
        notify_file_path = NULL;
    }
    if (notify_relay_socket != -1) {
        SOCKET_CLOSE(notify_relay_socket);
        notify_relay_socket = -1;
    }
    notify_relay_configured = 0;
}
