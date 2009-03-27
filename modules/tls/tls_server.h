/*
 * $Id$
 *
 * TLS module - main server part
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

#ifndef _TLS_SERVER_H
#define _TLS_SERVER_H

#include <stdio.h>
#include "../../tcp_conn.h"
#include "tls_domain.h"

struct tls_extra_data {
	tls_cfg_t* cfg; /* Configuration used for this connection */
	SSL* ssl;       /* SSL context used for the connection */
};

/*
 * dump ssl error stack 
 */
void tls_print_errstack(void);

/*
 * Called when new tcp connection is accepted 
 */
int tls_h_tcpconn_init(struct tcp_connection *c, int sock);

/*
 * clean the extra data upon connection shut down 
 */
void tls_h_tcpconn_clean(struct tcp_connection *c);

/*
 * shut down the TLS connection 
 */
void tls_h_close(struct tcp_connection *c, int fd);

int tls_h_blocking_write(struct tcp_connection *c, int fd,
			  const char *buf, unsigned int len);

int tls_h_read(struct tcp_connection *c);

int tls_h_fix_read_conn(struct tcp_connection *c);

#endif /* _TLS_SERVER_H */
