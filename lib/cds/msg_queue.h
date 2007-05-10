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

/** \ingroup cds
 * @{ 
 *
 * \defgroup cds_msg_queue Message Queue
 *
 * Message queue is a structure useful for sending data between processes.
 * It can be synchronized via its own mutex or the synchronization can
 * be left on caller. It can use reference counter which is useful
 * when accessing dynamicaly allocated queue destroyed by its last user.
 *
 * @{
 * */

typedef void (*destroy_function_f)(void *);

/** Structure holding message which can be put
 * into message queue.
 *
 * There is a need to allow destroing the message without knowing its
 * internals (destroying message queue with non-processed messages) and thus
 * the destroy_function able to fully destroy whole data hold by message must
 * be given. It is mostly needed to choose the function manually only for
 * complex data with pointers which content need to be freed too.
 * For simple structures it is set automaticaly during the message creation.
 */
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


/** Message queue flag meaning that internal queue mutex is used for
 * synchronization. It is set during message queue initialization via \ref
 * msg_queue_init or \ref msg_queue_init_ex. */
#define MQ_USE_MUTEX	1

/** Message queue flag meaning that reference counters are used. 
 * To set this flag is needed to call \ref msg_queue_init_ref_cnt with
 * non-NULL group parameter. */ 
#define MQ_USE_REF_CNTR	2

typedef struct msg_queue {
	reference_counter_data_t ref;
	mq_message_t *first;
	mq_message_t *last;
	cds_mutex_t q_mutex;
	unsigned int flags;
} msg_queue_t;

#define get_message_data(msg)		(msg ? msg->data: NULL)
#define get_message_data_len(msg)	(msg ? msg->data_len: 0)

/** The space for data is allocated in messages data
 * (they are automaticaly freed!)! Pointer to allocated
 * data bytes is in data variable in the message structure. */
mq_message_t *create_message_ex(int data_len);

/** Creates message holding data allocated using cds_malloc.
 * Warning: data must be allocated using cds_malloc and they
 * are automacicaly freed by free_message! */
mq_message_t *create_message(void *data, int data_len);

/** Sets function which will be called by free_message to destroy data. 
 *
 * This function may be useful when a complex structure with pointers is added
 * as data parameter.  */
void set_data_destroy_function(mq_message_t *msg, destroy_function_f func);

/** Initializes message.
 * If auto_free set, data must be allocated using cds_malloc and are
 * automaticaly freed by free_message (and if msg_queue_destroy called) */
void init_message_ex(mq_message_t *m, void *data, int data_len, destroy_function_f func);

/** Frees the message and data hold by the message. */
void free_message(mq_message_t *msg);

/** Put message into queue. */
int push_message(msg_queue_t *q, mq_message_t *m);

/** Remove message from queue. */
mq_message_t *pop_message(msg_queue_t *q);

/** Tests if message queue holds a message.
 * \retval 1 if empty
 * \retval 0 if NOT empty. */
int is_msg_queue_empty(msg_queue_t *q);

/** Initializes message queue with a mutex guarding queue operations. */
int msg_queue_init(msg_queue_t *q);

/** Initializes message queue. If synchronize is set it initializes
 * a mutex guarding queue operations otherwise the message queue remains
 * unsynchronized. */
int msg_queue_init_ex(msg_queue_t *q, int synchronize);

/** Initializes reference counter for given message queue
 * \param grp specifies group of reference counters to use. The message
 * queue will stop using the reference counter if NULL. 
 * \param q specifies the message queue */
void msg_queue_init_ref_cnt(msg_queue_t *q, reference_counter_group_t *grp);

/** Destroys message queue if no more references exist. 
 * This function destroys all message queue internal data but doesn't free
 * the message queue itself. It can be useful for staticaly allocated queues
 * or when allocated not using cds_malloc. */
void msg_queue_destroy(msg_queue_t *q);

/** Destroys and frees the message queue if no more references exist.
 * It uses cds_free for freeing the memory. */
void msg_queue_free(msg_queue_t *q);

/** @} 
 @} */

#ifdef __cplusplus
}
#endif

#endif

