/*
 * Copyright (C) 2005-2006 iptelorg GmbH
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
 *
 */

/*!
 * \file
 * \brief Kamailio core :: static buffer for select results (mma)
 *	            each process owns a separate space
 *	            each request starts using the buffer from the start
 * \ingroup core
 * Module: \ref core
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "dprint.h"
#include "mem/mem.h"
#include "str.h"
#include "ut.h"

/*
 * Placeholder for the buffer
 *
 * two buffers are actually used to cover the different size requests
 * assuming that resize can move the result to newly allocated space
 * and comparing two selects from the script could require two static buffers
 *
 * if more static buffers need to be valid at the same time change
 * the following constant
 */

#define MAX_BUFFERS 2
#define BUFFER_GRANULARITY 256

typedef struct stat_buffer_ {
	char *b;
	int size;
	int offset;
} stat_buffer_t;

static stat_buffer_t buffer[MAX_BUFFERS];
static int active_buffer=-1;

#define ALLOC_SIZE(req_size) (((req_size/BUFFER_GRANULARITY)+1)*BUFFER_GRANULARITY)

static int allocate_buffer(int req_size) {
	void *b;
	int size=ALLOC_SIZE(req_size);
	
	if (buffer[active_buffer].b == NULL) {
		if ((buffer[active_buffer].b=pkg_malloc(size))==NULL)
			return 0;
		buffer[active_buffer].size=size;
		buffer[active_buffer].offset=0;
		return 1;
	}
	
	active_buffer = (active_buffer?active_buffer:MAX_BUFFERS)-1;
	if (buffer[active_buffer].size >= req_size) {
		buffer[active_buffer].offset = 0;
		return 1;
	}
	
	if ((b=pkg_realloc(buffer[active_buffer].b,size))) {
		buffer[active_buffer].b=b;
		buffer[active_buffer].size=size;
		buffer[active_buffer].offset=0;
		return 1;
	}
	
	return 0;
}

/*
 * Request for space from buffer
 *
 * Returns:  NULL  memory allocation failure (no more space)
 *           pointer to the space on success
 */

char* get_static_buffer(int req_size) {
	char *p = NULL;

#ifdef EXTRA_DEBUG
	if ((active_buffer < 0) || (active_buffer > MAX_BUFFERS-1)) {
		LM_CRIT("buffers have not been initialized yet. "
			"Call reset_static_buffer() before executing "
			"a route block.\n");
		abort();
	}
#endif
	if ((buffer[active_buffer].size >= buffer[active_buffer].offset + req_size)
			|| (allocate_buffer(req_size))) {
		/* enough space in current buffer or allocation successful */
		p = buffer[active_buffer].b+buffer[active_buffer].offset;
		buffer[active_buffer].offset += req_size;	
		return p;
	}
	return NULL;
}

/* Internal function - called before request is going to be processed
 *
 * Reset offset to unused space
 */

int reset_static_buffer(void) {
	int i;

	if (active_buffer == -1) {
		memset(buffer, 0, sizeof(buffer));
	} else {
		for (i=0; i<MAX_BUFFERS; i++)
			buffer[i].offset=0;
	}
	active_buffer=0;
	return 0;
}

int str_to_static_buffer(str* res, str* s)
{
	res->s = get_static_buffer(s->len);
	if (!res->s) return -1;
	memcpy(res->s, s->s, s->len);
	res->len = s->len;
	return 0;
}

int int_to_static_buffer(str* res, int val)
{
	char *c;
	c = int2str(abs(val), &res->len);
	res->s = get_static_buffer(res->len+((val<0)?1:0));
	if (!res->s) return -1;
	if (val < 0) {
		res->s[0] = '-';	
		memcpy(res->s+1, c, res->len);
		res->len++;
	}
	else {
		memcpy(res->s, c, res->len);
	}
	return 0;
}

int uint_to_static_buffer(str* res, unsigned int val)
{
	char *c;
	c = int2str(val, &res->len);
	res->s = get_static_buffer(res->len);
	if (!res->s) return -1;
	memcpy(res->s, c, res->len);
	return 0;
}

int uint_to_static_buffer_ex(str* res, unsigned int val, int base, int pad)
{
	char *c;
	c = int2str_base_0pad(val, &res->len, base, pad); 
	res->s = get_static_buffer(res->len);
	if (!res->s) return -1;
	memcpy(res->s, c, res->len);
	return 0;
}

