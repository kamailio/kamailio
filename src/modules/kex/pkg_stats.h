/*
 * Copyright (C) 2010 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

/*!
 * \file
 * \brief KEX :: Kamailio private memory pool statistics
 * \ingroup kex
 */


#ifndef _PKG_STATS_H_
#define _PKG_STATS_H_

int pkg_proc_stats_init(void);
int pkg_proc_stats_myinit(int rank);
int pkg_proc_stats_destroy(void);
int register_pkg_proc_stats(void);
int pkg_proc_stats_init_rpc(void);

#endif /*_PKG_STATS_H_*/
