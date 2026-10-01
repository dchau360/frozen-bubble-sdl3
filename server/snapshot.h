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

#ifndef _SNAPSHOT_H_
#define _SNAPSHOT_H_

/* Weekly backups of the score files, taken at the Monday rollover just
 * before a finished week is cleared: copies `path` to
 * "<path>.<YYYY-MM-DD>", named after that week's Monday (UTC day index
 * `week_start_day`), then deletes all but the newest snapshots of `path`.
 * FB_SERVER_SNAPSHOTS sets how many are kept (default 12, 0 = none taken).
 * Restoring one is an operator job: stop the server, copy it over `path`,
 * start it again (see SetupServer.md). */
void snapshot_week(const char* path, long week_start_day);

#endif
