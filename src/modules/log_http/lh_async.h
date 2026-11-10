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

#ifndef _LH_ASYNC_H_
#define _LH_ASYNC_H_

#include <stdbool.h>
#include <stdint.h>

#include "../../core/str.h"
#include "../../core/list.h"
#include "../../core/ip_addr.h"
#include "log_http.h"

struct group_key;
struct lh_target;

typedef struct
{
	log_param_type_t type;
	union
	{
		str string;
		int num;
		float flp;
		struct ip_addr addr;
		bool cond;
		uint64_t date; /* unix time in milliseconds */
	} u;
} async_param_value_t;

/* one field of a log entry, packed into shm together with the entry */
typedef struct async_log_param
{
	SLIST_ENTRY(async_log_param) next;
	str name;
	async_param_value_t value;
} async_log_param_t;

/* log entry passed from the SIP workers to the sender process */
typedef struct
{
	SLIST_HEAD(, async_log_param) head;
	int size;
} async_log_entry_t;

/* serialized json line waiting in the sender queue */
struct lh_message
{
	STAILQ_ENTRY(lh_message) next;
	str msg_str;
};

STAILQ_HEAD(lh_queue_t, lh_message);
typedef struct lh_queue_t lh_queue_t;

/* growable pkg buffer the request body is assembled in */
typedef struct
{
	char *s;
	int len;
	int size;
} lh_body_t;

/* append data to the body, growing the buffer as needed */
int lh_body_append(lh_body_t *body, const char *data, int len);

/* called from SIP workers: pack the values and pass them to the sender */
int lh_async_send(struct group_key *key, async_param_value_t *values, int size);

/* called in the sender process: serialize the entry and queue it.
 * returns true when a send should be triggered */
bool lh_add_message_to_queue(async_log_entry_t *message);

void lh_free_queue(void);

/* start sending of the queued messages (if nothing is in flight) */
void lh_send_queue_messages(void);

/* finish the request in flight: drop the sent messages on success */
void lh_send_queue_messages_fin(bool success);

int lh_curl_send_message(
		struct lh_target *target, struct lh_target *failover_target);

#endif
