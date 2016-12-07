/*
 * Copyright (C) 2006 iptelorg GmbH
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
/*! \brief Kamailio core :: tcp various information
 * \ingroup core
 */

#ifndef _tcp_info_h
#define _tcp_info_h

struct tcp_gen_info{
	int tcp_readers;
	int tcp_max_connections; /* startup connection limit, cannot be exceeded*/
	int tls_max_connections; /* startup tls limit, cannot exceed tcp limit*/
	int tcp_connections_no; /* crt. connections number */
	int tls_connections_no; /* crt. tls connections number */
	int tcp_write_queued; /* total bytes queued for write, 0 if no
							 write queued support is enabled */
};




void tcp_get_info(struct tcp_gen_info* ti);

#endif
