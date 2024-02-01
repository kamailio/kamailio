/*
 * Copyright (C) 2024 GILAWA Ltd
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

#include <stddef.h>
#include <stdlib.h>

#include "types.h"

static fo_node_t *fo_new_node(fo_log_message_t data)
{
	fo_node_t *temp = (fo_node_t *)shm_malloc(sizeof(fo_node_t));
	temp->data = data;
	temp->next = NULL;
	return temp;
}


int fo_enqueue(fo_queue_t *q, fo_log_message_t data)
{
	/*
	Copy the contents of data.message
    */
	char *message_copy = (char *)shm_malloc(strlen(data.message) + 1);
	strcpy(message_copy, data.message);
	data.message = message_copy;
	fo_node_t *temp = fo_new_node(data);

	lock_get(&(q->lock));

	if(q->rear == NULL) {
		q->front = q->rear = temp;
		lock_release(&(q->lock));
		return 1;
	}

	q->rear->next = temp;
	q->rear = temp;

	lock_release(&(q->lock));
	return 1;
}

int fo_dequeue(fo_queue_t *q, fo_log_message_t *data)
{
	lock_get(&(q->lock));

	if(q->front == NULL) {
		lock_release(&(q->lock));
		return -1;
	}
	fo_node_t *temp = q->front;
	*data = temp->data;
	q->front = q->front->next;

	if(q->front == NULL)
		q->rear = NULL;


	if(temp != NULL) {
		if(temp->data.message != NULL) {
			shm_free(temp->data.message);
			temp->data.message = NULL;
		}
		shm_free(temp);
		temp = NULL;
	}
	lock_release(&(q->lock));

	return 1;
}

int fo_is_queue_empty(fo_queue_t *q)
{
	lock_get(&(q->lock));
	int result = (q->front == NULL);
	lock_release(&(q->lock));
	return result;
}

int fo_queue_size(fo_queue_t *q)
{
	lock_get(&(q->lock));
	int count = 0;
	fo_node_t *temp = q->front;
	while(temp != NULL) {
		count++;
		temp = temp->next;
	}
	lock_release(&(q->lock));
	return count;
}
