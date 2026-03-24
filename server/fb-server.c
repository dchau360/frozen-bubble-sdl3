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
#ifndef _WIN32
#  include <signal.h>
#endif
#ifdef _WIN32
#  include "win32_compat.h"
#endif
#include "net.h"
#include "game.h"
#include "tools.h"
#include "stats.h"

static void cleanup_atexit(void)
{
        stats_cleanup();
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
        setup_signal_handlers();

        create_server(argc, argv);
        daemonize();
        connections_manager();

        return 0;
}
