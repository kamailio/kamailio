/*
 * $Id$
 *
 * Oracle module interface
 *
 * Copyright (C) 2007,2008 TRUNK MOBILE
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
 *
 * History:
 * --------
 */

#ifndef ASYNCH_H
#define ASYNCH_H

#include <oci.h>
#include "ora_con.h"


/*
 * parse timeout value for operation in syntax: nnn.mmm (sec/ms)
 */
int set_timeout(unsigned type, const char* val);

/*
 * parse timeout value for reconnect in syntax: nnn.mmm (sec/ms)
 */
int set_reconnect(unsigned type, const char* val);


/*
 * start timelimited operation (if work in synch mode return SUCCESS)
 */
sword begin_timelimit(ora_con_t* con, int connect);

/*
 * check completion of timelimited operation (if work in synch mode return 0)
 */
int wait_timelimit(ora_con_t* con, sword status);

/*
 * close current timelimited operation and disconnect if timeout occured
 * return true only if work in asynch mode and timeout detect
 */
int done_timelimit(ora_con_t* con, sword status);

#endif /* ASYNCH_H */
