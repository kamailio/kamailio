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

#include <sys/time.h>

#include "../../core/str.h"
#include "../../core/locking.h"

#define SIPDUMP_MODE_WTEXT (1 << 0)
#define SIPDUMP_MODE_EVROUTE (1 << 1)
#define SIPDUMP_MODE_WPCAP (1 << 2)
#define SIPDUMP_MODE_WPCAPEX (1 << 3)

typedef struct sipdump_data
{
	int pid;
	int procno;
	struct timeval tv;
	str data;
	str tag;
	int afid;
	int protoid;
	str src_ip;
	int src_port;
	str dst_ip;
	int dst_port;
	struct sipdump_data *next;
} sipdump_data_t;

typedef struct sipdump_list
{
	int count;
	int enable;
	gen_lock_t lock;
	struct sipdump_data *first;
	struct sipdump_data *last;
} sipdump_list_t;

int sipdump_list_init(int en);

int sipdump_list_destroy(void);

int sipdump_list_add(sipdump_data_t *sdd);

void sipdump_timer_exec(unsigned int ticks, void *param);

int sipdump_file_init(str *folder, str *fprefix);

void sipdump_init_pcap(FILE *fs);

void sipdump_write_pcap(FILE *fs, sipdump_data_t *sd);

int sipdump_data_print(sipdump_data_t *sd, str *obuf);

int sipdump_data_clone(sipdump_data_t *isd, sipdump_data_t **osd);

int sipdump_enabled(void);

int sipdump_rpc_init(void);

#endif
