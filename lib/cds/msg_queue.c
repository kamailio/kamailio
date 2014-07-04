/* 
 * Copyright (C) 2005 iptelorg GmbH
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <cds/msg_queue.h>
#include <cds/memory.h>
#include <cds/ref_cntr.h>
#include <cds/logger.h>

mq_message_t *create_message_ex(int data_len)
{
	mq_message_t *m;
	if (data_len < 0) data_len = 0;
	m = cds_malloc(data_len + sizeof(mq_message_t));
	if (!m) return NULL;
	m->data_len = data_len;
	m->data = (((char *)m) + sizeof(mq_message_t));
	m->next = NULL;
	m->allocation_style = message_allocated_with_data;
	m->destroy_function = NULL;
	return m;
}

mq_message_t *create_message(void *data, int data_len)
{
	mq_message_t *m;
	/* if (data_len < 0) data_len = 0; */
	m = cds_malloc(sizeof(mq_message_t));
	if (!m) return NULL;
	m->data_len = data_len;
	m->data = data;
	m->next = NULL;
	m->allocation_style = message_holding_data_ptr;
	m->destroy_function = cds_free_ptr;
	return m;
}

void init_message_ex(mq_message_t *m, void *data, int data_len, destroy_function_f func)
{
	/* if (data_len < 0) data_len = 0; */
	if (!m) return;
	
	m->data_len = data_len;
	m->data = data;
	m->next = NULL;
	m->destroy_function = func;
	m->allocation_style = message_holding_data_ptr;
}

void set_data_destroy_function(mq_message_t *msg, destroy_function_f func)
{
	if (msg) msg->destroy_function = func;
}

void free_message(mq_message_t *msg)
{
	if (msg->destroy_function && msg->data) 
		msg->destroy_function(msg->data);
	switch (msg->allocation_style) {
		case message_allocated_with_data: 
				break;
		case message_holding_data_ptr: 
				/* if (msg->data) cds_free(msg->data); */
				break;
	}
	cds_free(msg);
}

int push_message(msg_queue_t *q, mq_message_t *m)
{
	if ((!q) || (!m)) return -1;
	m->next = NULL;
	
	if (q->flags & MQ_USE_MUTEX) cds_mutex_lock(&q->q_mutex);
	if (q->last) q->last->next = m;
	else {
		q->first = m;
		q->last = m;
	}
	q->last = m;
	if (q->flags & MQ_USE_MUTEX) cds_mutex_unlock(&q->q_mutex);
	
	return 0;
}

int mq_add_to_top(msg_queue_t *q, mq_message_t *m)
{
	if ((!q) || (!m)) return -1;
	m->next = NULL;
	
	if (q->flags & MQ_USE_MUTEX) cds_mutex_lock(&q->q_mutex);
	m->next = q->first;
	q->first = m;
	if (!q->last) q->last = m;
	if (q->flags & MQ_USE_MUTEX) cds_mutex_unlock(&q->q_mutex);
	
	return 0;
}

mq_message_t *pop_message(msg_queue_t *q)
{
	mq_message_t *m;
	if (!q) return NULL;

	if (q->flags & MQ_USE_MUTEX) cds_mutex_lock(&q->q_mutex);
	m = q->first;
	if (m) {
		if (q->first == q->last) {
			q->first = NULL;
			q->last = NULL;
		}
		else q->first = m->next;
		m->next = NULL;
	}
	if (q->flags & MQ_USE_MUTEX) cds_mutex_unlock(&q->q_mutex);
		
	return m;
}

int is_msg_queue_empty(msg_queue_t *q)
{
	int res = 1;
	if (q->flags & MQ_USE_MUTEX) cds_mutex_lock(&q->q_mutex);
	if (q->first) res = 0;
	if (q->flags & MQ_USE_MUTEX) cds_mutex_unlock(&q->q_mutex);
	return res;
}

int msg_queue_init(msg_queue_t *q)
{
	return msg_queue_init_ex(q, 1);
}

int msg_queue_init_ex(msg_queue_t *q, int synchronize)
{
	if (synchronize) {
		cds_mutex_init(&q->q_mutex);
		q->flags = MQ_USE_MUTEX;
	}
	else q->flags = 0;
	q->first = NULL;
	q->last = NULL;
	return 0;
}

/** \internal Destroys all internal data of message queue and 
 * optionaly frees it if no more references exist. */
static inline void msg_queue_destroy_and_free(msg_queue_t *q, int do_free)
{
	mq_message_t *m,*n;
	if (!q) return;
	
	if (q->flags & MQ_USE_REF_CNTR) {
		if (!remove_reference(&q->ref)) {
			/* this was NOT the last reference */
			return;
		}
	}

	if (q->flags & MQ_USE_MUTEX) cds_mutex_lock(&q->q_mutex);
	m = q->first;
	while (m) {
		n = m->next;
		free_message(m);
		m = n;
	}
	q->first = NULL;
	q->last = NULL;
	if (q->flags & MQ_USE_MUTEX) {
		cds_mutex_unlock(&q->q_mutex);
		cds_mutex_destroy(&q->q_mutex);
	}
	if (do_free) cds_free(q);
}

void msg_queue_destroy(msg_queue_t *q)
{
	msg_queue_destroy_and_free(q, 0);
}

void msg_queue_free(msg_queue_t *q)
{
	msg_queue_destroy_and_free(q, 1);
}

void msg_queue_init_ref_cnt(msg_queue_t *q, reference_counter_group_t *grp)
{
	if (grp) {
		init_reference_counter(grp, &q->ref);
		q->flags |= MQ_USE_REF_CNTR;
	}
	else q->flags &= ~MQ_USE_REF_CNTR; /* don't use reference counter */
}

