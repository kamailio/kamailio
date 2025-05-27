/*
 * Copyright (C) 2024 GILAWA Ltd
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
#include "../../core/ut.h"

static fo_node_t *fo_new_node(fo_log_message_t data)
{
	fo_node_t *temp = (fo_node_t *)shm_malloc(sizeof(fo_node_t));
	temp->data = data;
	temp->next = NULL;
	return temp;
}


int fo_enqueue(fo_queue_t *q, fo_log_message_t data)
{
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

void fo_free_queue(fo_queue_t *q)
{
	fo_log_message_t data;
	while(fo_dequeue(q, &data) > 0) {
		if(data.prefix != NULL) {
			if(data.prefix->s != NULL) {
				shm_free(data.prefix->s);
			}
			shm_free(data.prefix);
		}

		if(data.message != NULL) {
			if(data.message->s != NULL) {
				shm_free(data.message->s);
			}
			shm_free(data.message);
		}
	}
	shm_free(q);
}

int fo_file_properties_init(fo_file_properties_t *fp)
{
	if(fp == NULL) {
		return -1;
	}

	/* Malloc member for each element required */
	fp->fo_base_filename.s = (char *)shm_malloc(FO_MAX_PATH_LEN * sizeof(char));
	if(fp->fo_base_filename.s == NULL) {
		LM_ERR("Failed to allocate memory for base filename\n");
		return -1;
	}
	fp->fo_extension.s = (char *)shm_malloc(10 * sizeof(char));
	if(fp->fo_extension.s == NULL) {
		LM_ERR("Failed to allocate memory for extension\n");
		return -1;
	}

	fp->fo_prefix.s = (char *)shm_malloc(4096 * sizeof(char));
	if(fp->fo_prefix.s == NULL) {
		LM_ERR("Failed to allocate memory for prefix\n");
		return -1;
	}
	fp->fo_stored_timestamp = time(NULL);
	fp->fo_interval_seconds = 0;

	fp->fo_prefix_pvs = (pv_elem_t *)shm_malloc(sizeof(pv_elem_t));
	if(fp->fo_prefix_pvs == NULL) {
		LM_ERR("Failed to allocate memory for prefix pvs\n");
		return -1;
	}

	return 1;
}

/* freeing the memory allocated for the members base_filename, extension, prefix
	lead to BUG bad pointer 0x7f6b50d57095 (out of memory block!) called from file_out: types.c: fo_file_properties_destroy(167) - ignoring
 */
int fo_file_properties_destroy(fo_file_properties_t *fp)
{
	if(fp == NULL) {
		return 1;
	}

	if(fp->fo_prefix_pvs != NULL) {
		if(pv_elem_free_all(fp->fo_prefix_pvs) < 0) {
			LM_ERR("Failed to free prefix pvs\n");
			return -1;
		}
	}
	if(fp->fo_file_output != NULL) {
		if(fclose(fp->fo_file_output) != 0) {
			LM_ERR("Failed to close file\n");
			return -1;
		}
	}
	return 1;
}

int fo_file_properties_print(const fo_file_properties_t file_prop)
{
	LM_DBG("Base filename: %s\n", file_prop.fo_base_filename.s);
	LM_DBG("Prefix: %s\n", file_prop.fo_prefix.s);
	LM_DBG("Extension: %s\n", file_prop.fo_extension.s);
	LM_DBG("Interval: %d\n", file_prop.fo_interval_seconds);
	LM_DBG("Stored timestamp: %ld\n", file_prop.fo_stored_timestamp);
	return 1;
}
