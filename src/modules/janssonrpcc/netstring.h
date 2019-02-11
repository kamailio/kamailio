/**
 * Copyright (C) 2013 Flowroute LLC (flowroute.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __NETSTRING_STREAM_H
#define __NETSTRING_STREAM_H

#include <string.h>
#include <event2/bufferevent.h>

typedef struct {
	char* buffer;
	char* string;
	unsigned int start, read, length;
} netstring_t;

void free_netstring(netstring_t* netstring);

int netstring_read_evbuffer(struct bufferevent *bev, netstring_t **netstring);

int netstring_read_fd(int fd, netstring_t **netstring);

int netstring_read(char *buffer, size_t buffer_length,
		   char **netstring_start, size_t *netstring_length);

size_t netstring_buffer_size(size_t data_length);

size_t netstring_encode_new(char **netstring, char *data, size_t len);

/* Errors that can occur during netstring parsing */
typedef enum {
	NETSTRING_ERROR_TOO_LONG = -1000,
	NETSTRING_ERROR_NO_COLON,
	NETSTRING_ERROR_TOO_SHORT,
	NETSTRING_ERROR_NO_COMMA,
	NETSTRING_ERROR_LEADING_ZERO,
	NETSTRING_ERROR_NO_LENGTH,
	NETSTRING_ERROR_BAD_FD,
	NETSTRING_INCOMPLETE
} netstring_errors;

#define NETSTRING_PEEKLEN 10

#endif
