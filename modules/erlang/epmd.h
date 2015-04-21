/**
 * Copyright (C) 2015 Bicom Systems Ltd, (bicomsystems.com)
 *
 * Author: Seudin Kasumovic (seudin.kasumovic@gmail.com)
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
 */

#ifndef EPMD_H_
#define EPMD_H_

#include <ei.h>

/*
 * @brief Handler for epmd connections.
 */
typedef struct epmd_handler_s
{
	/* d-linked list  */
	struct handler_common_s *prev;
	struct handler_common_s *next;

	/* if need to add new in i/o handler */
	struct handler_common_s *new;

	/*
	 * Handle connections from local epmd daemon.
	 *
	 * returns:
	 *  0 - ok to continue
	 * -1 - epmd lost (we die regards we can't recovery)
	 * sockfd - socket of new accepted connection to be added into
	 * poll
	 */
	int (*handle_f)(handler_common_t *handler);
	int (*wait_tmo_f)(handler_common_t *handler);
	int (*destroy_f)(handler_common_t *handler);
	int sockfd; /* socket to epmd */
	ei_cnode ec; /* erlang C node (actually it's me) */

} epmd_handler_t;

int epmd_init(epmd_handler_t *epmd);
int handle_epmd(handler_common_t *phandler);

#endif /* EPMD_H_ */
