/**
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
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

#ifndef _SIPDUMP_WRITE_H_
#define _SIPDUMP_WRITE_H_

#include "../../core/str.h"
#include "../../core/locking.h"

typedef struct sipdump_data {
	str data;
	struct sipdump_data *next;
} sipdump_data_t;

typedef struct sipdump_list {
	int count;
	int enable;
	gen_lock_t lock;
	struct sipdump_data *first;
	struct sipdump_data *last;
} sipdump_list_t;

int sipdump_list_init(int en);

int sipdump_list_destroy(void);

int sipdump_list_add(str *data);

void sipdump_timer_exec(unsigned int ticks, void *param);

int sipdump_file_init(str *folder, str *fprefix);

int sipdump_enabled(void);

int sipdump_rpc_init(void);

#endif
