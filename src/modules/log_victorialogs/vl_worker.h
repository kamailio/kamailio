/**
 * Copyright (C) 2019-2026 Alexey Volokitin (didww.com)
 * Copyright (C) 2025-2026 Michael Furmur (didww.com)
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

#ifndef _VL_WORKER_H_
#define _VL_WORKER_H_

#include <stdbool.h>
#include <curl/curl.h>

#include "../../core/str.h"
#include "vl_async.h"

struct curl_conn_handle
{
	struct vl_target *target;
	CURL *curl;
	/* target that started the failover round, 0 if none */
	struct vl_target *failover_target;
	/* request body: newline-delimited json lines */
	str body;
	struct curl_slist *headers;
};

void vl_worker_destroy(void);
int vl_worker_init_pair_sockets(void);
void vl_worker_close_sockets_parent(void);
void vl_worker_close_sockets_child(void);
void vl_worker_run(void);
bool vl_worker_send(void *entry);
int vl_conn_add(struct curl_conn_handle *conn);
void vl_conn_remove(struct curl_conn_handle *conn);

#endif
