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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __MSG_QUEUE_H
#define __MSG_QUEUE_H

#include <cds/sync.h>
#include <cds/ref_cntr.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*destroy_function_f)(void *);

typedef struct _mq_message_t {
	void *data;
	int data_len;
	struct _mq_message_t *next;
	destroy_function_f destroy_function; /* see doc */
	enum { 
		message_allocated_with_data, 
		message_holding_data_ptr
	} allocation_style;
	char data_buf[1];
} mq_message_t;

typedef struct msg_queue {
	reference_counter_data_t ref;
	mq_message_t *first;
	mq_message_t *last;
	cds_mutex_t q_mutex;
	int use_mutex;
} msg_queue_t;

#define get_message_data(msg)		(msg ? msg->data: NULL)
#define get_message_data_len(msg)	(msg ? msg->data_len: 0)

/** the space for data is allocated in messages data
 * (they are automaticaly freed!)! Pointer to allocated
 * data bytes is in data variable in the message structure. */
mq_message_t *create_message_ex(int data_len);

/** data must be allocated using cds_malloc and they
 * are automacicaly freed! */
mq_message_t *create_message(void *data, int data_len);

void set_data_destroy_function(mq_message_t *msg, destroy_function_f func);

/** initializes message, 
 * if auto_free set, data must be allocated using cds_malloc and are automaticaly freed by free_message 
 * (and if msg_queue_destroy called) */
void init_message_ex(mq_message_t *m, void *data, int data_len, destroy_function_f func);

/** frees the message and data holding by the message !!!! */
void free_message(mq_message_t *msg);

int push_message(msg_queue_t *q, mq_message_t *m);
mq_message_t *pop_message(msg_queue_t *q);

/** 1 ... empty, 0 ...  NOT empty !! */
int is_msg_queue_empty(msg_queue_t *q);

/** initializes synchronized message queue */
int msg_queue_init(msg_queue_t *q);
int msg_queue_init_ex(msg_queue_t *q, int synchronize);
void msg_queue_destroy(msg_queue_t *q);

/* removes reference to message queue and frees it if no other references exist */
void msg_queue_free(msg_queue_t *q);

#ifdef __cplusplus
}
#endif

#endif

