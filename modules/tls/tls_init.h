/*
 * $Id$
 * 
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2004,2005 Free Software Foundation, Inc.
 * Copyright (C) 2005,2006 iptelorg GmbH
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

#ifndef _TLS_INIT_H
#define _TLS_INIT_H

#include "../../ip_addr.h"


/*
 * just once, initialize the tls subsystem 
 */
int init_tls(void);


/*
 * Called from child_init, once the total number of
 * SER processes is known
 */
int init_tls_child(void);


/*
 * just once before cleanup 
 */
void destroy_tls(void);


/*
 * for each socket 
 */
int tls_init(struct socket_info *si);


#endif /* _TLS_INIT_H */
