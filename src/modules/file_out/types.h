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

#include "../../core/locking.h"

typedef struct log_message
{
	char *message;
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

int fo_enqueue(fo_queue_t *q, fo_log_message_t data);
int fo_dequeue(fo_queue_t *q, fo_log_message_t *data);
int fo_is_queue_empty(fo_queue_t *q);
int fo_queue_size(fo_queue_t *q);
