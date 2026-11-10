/**
 * Copyright (C) 2019-2026 Alexey Volokitin (didww.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _LH_WORKER_H_
#define _LH_WORKER_H_

#include <stdbool.h>
#include <curl/curl.h>

#include "../../core/str.h"
#include "lh_async.h"

struct curl_conn_handle
{
	struct lh_target *target;
	CURL *curl;
	/* target that started the failover round, 0 if none */
	struct lh_target *failover_target;
	lh_body_t body;
	int body_off; /* bytes of body already handed to curl */
	int body_state;
	struct lh_message *next; /* next queued entry to serialize */
	unsigned int left;		 /* entries left in the batch */
	struct curl_slist *headers;
};

void lh_worker_destroy(void);
int lh_worker_init_pair_sockets(void);
void lh_worker_close_sockets_parent(void);
void lh_worker_close_sockets_child(void);
void lh_worker_run(void);
bool lh_worker_send(void *entry);
int lh_conn_add(struct curl_conn_handle *conn);
void lh_conn_remove(struct curl_conn_handle *conn);

#endif
