/* 
 * MySQL module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _MYSQL_MOD_H
#define _MYSQL_MOD_H

#include "../../counters.h"

/* counter struct
*/
struct mysql_counters_h {
    counter_handle_t driver_err;
};
/* defined in km_dbase.c */
extern struct mysql_counters_h mysql_cnts_h;

/** @defgroup mysql MySQL db driver
 *  @ingroup DB_API
 */
/** @{ */

extern int my_ping_interval;
extern unsigned int my_connect_to;
extern unsigned int my_send_to;
extern unsigned int my_recv_to;
extern unsigned long my_client_ver;
extern unsigned int my_retries;

/** @} */

#endif /* _MYSQL_MOD_H */
