/* 
 * Copyright (C) 2005 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __CDS_SYNC_H
#define __CDS_SYNC_H

#ifdef SER
#	include <locking.h>
#	define cds_mutex_t			gen_lock_t
#	define cds_mutex_init(m)	lock_init(m)
#	define cds_mutex_destroy(m)	lock_destroy(m)
#	define cds_mutex_lock(m)	lock_get(m)
#	define cds_mutex_unlock(m)	lock_release(m)
#else
#	include <pthread.h>
#	include <stdlib.h>
#	define cds_mutex_t				pthread_mutex_t
#	define cds_mutex_init(m)		pthread_mutex_init(m, NULL)
#	define cds_mutex_destroy(m)		pthread_mutex_destroy(m)
#	define cds_mutex_lock(m)		pthread_mutex_lock(m)
#	define cds_mutex_unlock(m)		pthread_mutex_unlock(m)
#endif

#endif
