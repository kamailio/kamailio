/* 
 * $Id$
 *
 *
 * Copyright (C) 2001-2004 iptel.org
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MY_POOL_H
#define MY_POOL_H

#include "my_con.h"

/*
 * Get a connection from the pool, reuse existing
 * if possible, otherwise create a new one
 */
struct my_con* get_connection(const char* url);


/*
 * Release a connection, the connection will be left
 * in the pool if ref count != 0, otherwise it
 * will be delete completely
 */
void release_connection(struct my_con* con);


#endif /* MY_POOL_H */
