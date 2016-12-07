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

#ifndef WORKER_H_
#define WORKER_H_

#include <ei.h>
#include "erl_helpers.h"

typedef struct worker_handler_s
{
	/* d-linked list  */
	struct handler_common_s *prev;
	struct handler_common_s *next;

	/* if need to add new in i/o handler */
	struct handler_common_s *new;

	/*
	 * handler for socket pair requests
	 *
	 * if function return
	 * 0  - ok
	 * -1 - RPC error (we die)
	 * -2 - remote C Node disconnected (remove node from list)
	 */
	int (*handle_f)(handler_common_t *phandler_t);
	int (*wait_tmo_f)(handler_common_t *phandler_t);
	int (*destroy_f)(handler_common_t *handler);
	int sockfd; /* kamailio to cnode socket r/w */
	ei_cnode ec; /* erlang C node (actually it's me) */

} worker_handler_t;

int worker_init(worker_handler_t *phandler, int fd, const ei_cnode *ec);
int handle_worker(handler_common_t *phandler_t);
int wait_tmo_worker(handler_common_t *phandler_t);

#endif /* WORKER_H_ */
