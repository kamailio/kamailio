/*
 * $Id$
 *
 * Copyright (C) 2009 Henning Westerholt
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

#include <memcache.h>

#ifndef MEMCACHED_H
#define MEMCACHED_H

/*! server string */
extern char* db_memcached_srv_str;
/*! cache expire time in seconds */
extern unsigned int memcached_expire;
/*! cache storage mode, set or add */
extern unsigned int memcached_mode;
/*! server timeout */
extern int memcached_timeout;
/*! memcached handle */
extern struct memcache* memcached_h;


#endif
