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

#ifndef _FO_TYPES_H
#define _FO_TYPES_H

#include "../../core/locking.h"
#include "../../core/pvar.h"

#define FO_MAX_FILES 10			   /* Maximum number of files */
#define FO_MAX_WORKERS_PER_FILE 16 /* Maximum workers per file */
#define FO_MAX_PATH_LEN 2048

typedef struct log_message
{
	str *prefix;
	str *message;
	int dest_file;
} fo_log_message_t;

typedef struct node
{
	struct log_message data;
	struct node *next;
} fo_node_t;

typedef struct queue
{
	struct node *front;
	struct node *rear;
	gen_lock_t lock;
} fo_queue_t;

/* Queue creation and destruction */
fo_queue_t *fo_queue_create(void);
void fo_queue_destroy(fo_queue_t *q);

/* Queue operations */
int fo_enqueue(fo_queue_t *q, fo_log_message_t data);
int fo_dequeue(fo_queue_t *q, fo_log_message_t *data);
int fo_is_queue_empty(fo_queue_t *q);
int fo_queue_size(fo_queue_t *q);
void fo_free_queue(fo_queue_t *q);

/**
 * Per-worker file structure for multi-worker file configurations
 * When a file has multiple workers, each worker gets its own file queue
 */
typedef struct fo_worker_file
{
	int worker_id;					/* 0-based worker index */
	str filename_suffix;			/* e.g., "_0", "_1", "_2" */
	FILE *file_output;				/* File handle for this file */
	fo_queue_t *queue;				/* Queue for this file */
	int effective_interval_seconds; /* worker-specific interval */
	time_t fo_stored_timestamp;		/* Track rotation time per file */
} fo_worker_file_t;

typedef struct fo_output_properties
{
	str fo_base_filename;
	str fo_extension;
	str fo_prefix;
	int fo_base_interval_seconds;
	int fo_interval_range; /* percentage (0-100) */
	pv_elem_t *fo_prefix_pvs;

	/* Multi-worker configuration */
	int num_workers; /* number of workers for this file (1+ means multi-worker mode) */
	fo_worker_file_t *files; /* array of files, one per worker */
} fo_output_properties_t;

int fo_output_properties_init(fo_output_properties_t *fp);
int fo_output_properties_destroy(fo_output_properties_t *fp);
int fo_output_properties_print(const fo_output_properties_t file_prop);

/* Multi-worker file management */
int fo_create_files(fo_output_properties_t *fp, int num_workers);
int fo_destroy_files(fo_output_properties_t *fp);

/* Effective interval calculation */
int fo_calculate_effective_interval(int base_interval, int range_percent);

#endif
