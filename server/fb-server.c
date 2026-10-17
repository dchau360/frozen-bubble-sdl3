/*******************************************************************************
 *
 * Copyright (c) 2004-2012 Guillaume Cottenceau
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
#include "win32_compat.h"
#ifndef _WIN32
#  include <signal.h>
#endif
#include "net.h"
#include "game.h"
#include "tools.h"
#include "stats.h"
#include "weeklystats.h"
#include "hiscores.h"
#include "links.h"
#include "discordalert.h"
#include "geoip.h"
#include "log.h"

static void cleanup_atexit(void)
{
        stats_cleanup();
        discordalert_cleanup();
}

static void setup_signal_handlers(void)
{
        atexit(cleanup_atexit);
}

int main(int argc, char **argv)
{
        printf("Frozen-Bubble server version %s (protocol version %d.%d)\n", VERSION, proto_major, proto_minor);
        printf("\n");
        printf("Copyright (c) 2004-2012 Guillaume Cottenceau.\n");
        printf("This is free software; see the source for copying conditions.  There is NO\n");
        printf("warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
        printf("\n");

        win32_socket_init();  // No-op on POSIX
        // Initialize stats system
        stats_init();
        weekly_init();
        hiscore_init();
        links_init();
        setup_signal_handlers();

        create_server(argc, argv);
        daemonize();
        // Resolve the Discord alert relay's address.
        //
        // Deliberately after create_server() and daemonize(), unlike
        // stats_init() above. create_server() is what parses -d/-o and calls
        // logging_init(), so anything logged before it -- including whether a
        // relay is configured at all -- is silently discarded, and this
        // module's startup diagnostic is the operator's only feedback that
        // the feature is wired up.
        discordalert_init();
        // Country for browser players, from the address nginx forwards (geoip.h).
        // Optional, like the Discord relay: unset means no country for them.
        {
                const char* geoip_file = getenv("FB_SERVER_GEOIP_FILE");
                if (geoip_file && *geoip_file) {
                        int ranges = geoip_load(geoip_file);
                        if (ranges > 0)
                                l1(OUTPUT_TYPE_INFO, "GeoIP: %d address ranges loaded; browser players get a country", ranges);
                        else
                                l1(OUTPUT_TYPE_ERROR, "GeoIP: could not read %s; browser players get no country", geoip_file);
                } else {
                        l0(OUTPUT_TYPE_INFO, "FB_SERVER_GEOIP_FILE not set; browser players get no country");
                }
        }
        connections_manager();

        return 0;
}
