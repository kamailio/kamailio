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
#include <time.h>

#include "types.h"
#include "../../core/ut.h"
#include "../../core/rand/kam_rand.h"

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

/**
 * Create and initialize a new queue in shared memory
 * @return pointer to newly created queue, or NULL on failure
 */
fo_queue_t *fo_queue_create(void)
{
	fo_queue_t *q = (fo_queue_t *)shm_malloc(sizeof(fo_queue_t));
	if(q == NULL) {
		LM_ERR("Failed to allocate memory for queue\n");
		return NULL;
	}

	q->front = NULL;
	q->rear = NULL;

	if(lock_init(&q->lock) == 0) {
		LM_ERR("Failed to initialize queue lock\n");
		shm_free(q);
		return NULL;
	}

	return q;
}

/**
 * Destroy and free a queue and all its contents
 * @param q queue to destroy
 */
void fo_queue_destroy(fo_queue_t *q)
{
	if(q == NULL) {
		return;
	}

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

	lock_destroy(&q->lock);
	shm_free(q);
}

/**
 * Calculate effective interval when a range is configured;
 * otherwise use the base interval
 * range semantics: picked from [interval * (1 - range_percent / 100), interval]
 * @param base_interval base interval in seconds
 * @param range_percent range as percentage (0-100), 0 means no change
 * @return effective interval in seconds
 */
int fo_calculate_effective_interval(int base_interval, int range_percent)
{
	if(base_interval <= 0) {
		return base_interval;
	}

	/* No change if range is 0 or negative */
	if(range_percent <= 0) {
		return base_interval;
	}

	/* Clamp range to 0-100 percent */
	if(range_percent > 100) {
		range_percent = 100;
	}

	/* Calculate min interval: interval * (1 - range% / 100) */
	int min_interval = base_interval * (100 - range_percent) / 100;
	if(min_interval <= 0) {
		min_interval = 1; /* ensure at least 1 second */
	}

	/* Return random value in [min_interval, base_interval]
	 * Using Kamailio's internal RNG */
	int random_offset = kam_rand() % (base_interval - min_interval + 1);
	return min_interval + random_offset;
}

int fo_output_properties_init(fo_output_properties_t *fp)
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
	fp->fo_base_interval_seconds = 0;
	fp->fo_interval_range = 0;

	/* Initialize multi-worker fields */
	fp->num_workers = 1; /* default: single worker mode */
	fp->files = NULL;

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
int fo_output_properties_destroy(fo_output_properties_t *fp)
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

	/* Free multi-worker files */
	if(fp->files != NULL) {
		fo_destroy_files(fp);
		fp->files = NULL;
	}

	return 1;
}

int fo_output_properties_print(const fo_output_properties_t file_prop)
{
	int i = 0;
	LM_INFO("base filename: %s, extension: %s, base interval: %d, "
			"interval_range: %d, "
			"workers: %d ",
			file_prop.fo_base_filename.s, file_prop.fo_extension.s,
			file_prop.fo_base_interval_seconds, file_prop.fo_interval_range,
			file_prop.num_workers);
	LM_DBG("Prefix: %s ", file_prop.fo_prefix.s);

	for(i = 0; i < file_prop.num_workers; i++) {
		fo_worker_file_t *sub = &file_prop.files[i];
		LM_INFO("worker %d: actual interval: %d, filename suffix: %s",
				sub->worker_id, sub->effective_interval_seconds,
				sub->filename_suffix.s);
	}
	return 1;
}

/**
 * Create multiple files for multi-worker configuration
 * Each worker gets its own file with a suffix (e.g., _0, _1, _2), its own queue
 * @param fp output properties to initialize files for
 * @param num_workers number of workers/files to create
 * @return 1 on success, -1 on failure
 */
int fo_create_files(fo_output_properties_t *fp, int num_workers)
{
	if(fp == NULL) {
		LM_ERR("File properties NULL\n");
		return -1;
	}

	if(num_workers <= 0) {
		LM_ERR("num_workers must be > 0\n");
		return -1;
	}

	if(num_workers > FO_MAX_WORKERS_PER_FILE) {
		LM_WARN("num_workers %d exceeds max %d, clamping to max\n", num_workers,
				FO_MAX_WORKERS_PER_FILE);
		num_workers = FO_MAX_WORKERS_PER_FILE;
	}

	/* Allocate files array */
	fp->files = (fo_worker_file_t *)shm_malloc(
			sizeof(fo_worker_file_t) * num_workers);
	if(fp->files == NULL) {
		LM_ERR("Failed to allocate files array for %d workers\n", num_workers);
		return -1;
	}

	/* Initialize each file */
	for(int i = 0; i < num_workers; i++) {
		fo_worker_file_t *sub = &fp->files[i];

		sub->worker_id = i;
		sub->file_output = NULL;
		sub->effective_interval_seconds = 0;
		sub->fo_stored_timestamp = time(NULL);

		/* Create queue for this file */
		sub->queue = fo_queue_create();
		if(sub->queue == NULL) {
			LM_ERR("Failed to create queue for worker %d\n", i);
			/* Cleanup previously created queues */
			for(int j = 0; j < i; j++) {
				fo_queue_destroy(fp->files[j].queue);
			}
			shm_free(fp->files);
			fp->files = NULL;
			return -1;
		}

		/* Create filename suffix string */
		char suffix_str[16];
		snprintf(suffix_str, sizeof(suffix_str), "_%d", i);

		sub->filename_suffix.s = (char *)shm_malloc(strlen(suffix_str) + 1);
		if(sub->filename_suffix.s == NULL) {
			LM_ERR("Failed to allocate suffix string for worker %d\n", i);
			/* Cleanup */
			for(int j = 0; j <= i; j++) {
				fo_queue_destroy(fp->files[j].queue);
				if(fp->files[j].filename_suffix.s != NULL) {
					shm_free(fp->files[j].filename_suffix.s);
				}
			}
			shm_free(fp->files);
			fp->files = NULL;
			return -1;
		}
		strcpy(sub->filename_suffix.s, suffix_str);
		sub->filename_suffix.len = strlen(suffix_str);

		LM_DBG("Created file for worker %d with suffix '%s'\n", i,
				sub->filename_suffix.s);
	}

	fp->num_workers = num_workers;

	LM_DBG("Created %d worker files for output '%s'\n", fp->num_workers,
			fp->fo_base_filename.s);

	return 1;
}

/**
 * Destroy all files and their queues
 * @param fp file properties with files to destroy
 * @return 1 on success, -1 on failure
 */
int fo_destroy_files(fo_output_properties_t *fp)
{
	if(fp == NULL || fp->files == NULL) {
		return 1;
	}

	for(int i = 0; i < fp->num_workers; i++) {
		fo_worker_file_t *sub = &fp->files[i];

		/* Close file if open */
		if(sub->file_output != NULL) {
			fclose(sub->file_output);
			sub->file_output = NULL;
		}

		/* Destroy queue */
		if(sub->queue != NULL) {
			fo_queue_destroy(sub->queue);
			sub->queue = NULL;
		}

		/* Free suffix string */
		if(sub->filename_suffix.s != NULL) {
			shm_free(sub->filename_suffix.s);
			sub->filename_suffix.s = NULL;
		}
	}

	shm_free(fp->files);
	fp->files = NULL;
	fp->num_workers = 1;

	return 1;
}
