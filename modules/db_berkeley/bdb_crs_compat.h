/*
 * BDB Compatibility layer for Kamailio
 *
 * Copyright (C) 2010 Marius Zbihlei marius.zbihlei at 1and1 dot ro
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Kamailio is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef BDB_CRS_COMPAT_H
#define BDB_CRS_COMPAT_H

#include <db.h>

/* this is a compatibility layer for cursor close function
 * Historically, the function was called c_close() but it became deprecated
 * starting with version 4.6
 */
#if DB_VERSION_MAJOR < 4
#	define CLOSE_CURSOR c_close
#else
#if (DB_VERSION_MAJOR == 4) && (DB_VERSION_MINOR < 6)
#	define CLOSE_CURSOR c_close
#else
#	define CLOSE_CURSOR close
#endif
#endif

#endif //BDB_CRS_COMPAT_H
