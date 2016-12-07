/*
 * Copyright (C) 2009, 2013 Henning Westerholt
 * Copyright (C) 2013 Charles Chance, sipcentric.com
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*!
 * \file
 * \brief memcached module
 */

#include <libmemcached/memcached.h>

#ifndef MEMCACHED_H
#define MEMCACHED_H

/*! server string */
extern char* mcd_srv_str;
/*! cache expire time in seconds */
extern unsigned int mcd_expire;
/*! cache storage mode, set or add */
extern unsigned int mcd_mode;
/*! server timeout */
extern unsigned int mcd_timeout;
/*! Internal or system memory manager */
extern unsigned int mcd_memory;
/*! stringify all values retrieved from memcached */
extern unsigned int mcd_stringify;
/*! memcached handle */
extern struct memcached_st* memcached_h;
/*! memcached server list */
extern struct memcached_server_st *servers;

#endif
