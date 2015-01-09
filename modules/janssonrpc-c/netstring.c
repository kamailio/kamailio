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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <sys/socket.h>
#include "netstring.h"
#include "janssonrpc.h"

#ifdef TEST
#include "unit_tests/test.h"
#else
#include "../../mem/mem.h"
#endif


void free_netstring(netstring_t* netstring) {
	if(!netstring) return;
	if(netstring->buffer) pkg_free(netstring->buffer);
	pkg_free(netstring);
}

//TODO: refactor out common code in the following two functions

int netstring_read_evbuffer(struct bufferevent *bev, netstring_t **netstring)
{
	int bytes, offset;
	size_t read_len;
	char *temp_buffer;
	temp_buffer = NULL;
	offset = 0;
	struct evbuffer *ib = bufferevent_get_input(bev);

	if (*netstring == NULL) {
		/* No buffer yet. Peek at first 10 bytes, to get length and colon. */
		unsigned char *lenstr;
		int i, len;
		struct evbuffer_ptr *search_end = pkg_malloc(sizeof(struct evbuffer_ptr));
		CHECK_MALLOC(search_end);

		i = evbuffer_get_length(ib);
		len = ((NETSTRING_PEEKLEN <= i) ? (NETSTRING_PEEKLEN) : (i-1));
		evbuffer_ptr_set(ib, search_end, len, EVBUFFER_PTR_SET);
		struct evbuffer_ptr loc = evbuffer_search_range(ib, ":", 1, NULL, search_end);
		pkg_free(search_end);
		if (loc.pos < 0) {
			// no colon found
			if (i > NETSTRING_PEEKLEN)
				return NETSTRING_ERROR_TOO_LONG;
			// TODO: peek at what's available and return suitable errors
			return NETSTRING_INCOMPLETE;
		}


		lenstr = pkg_malloc(loc.pos+1);
		CHECK_MALLOC(lenstr);
		bytes = evbuffer_remove(ib, lenstr, loc.pos+1);

		/* First character must be a digit */
		if (!isdigit(lenstr[0]))
			return NETSTRING_ERROR_NO_LENGTH;

		/* No leading zeros allowed! */
		if (lenstr[0] == '0' && isdigit(lenstr[1]))
			return NETSTRING_ERROR_LEADING_ZERO;
		if (lenstr[loc.pos] != ':') {
			return NETSTRING_ERROR_NO_COLON;
		}
		len = i = 0;

		/* Read the number of bytes */
		for (i = 0; i < loc.pos; i++) {
			/* Accumulate each digit, assuming ASCII. */
			len = len*10 + (lenstr[i] - '0');
		}
		pkg_free(lenstr);
		/* alloc the memory needed for the whole netstring */
		read_len = len+1;
		temp_buffer = pkg_malloc(read_len);
		CHECK_MALLOC(temp_buffer);

		/* initialize the netstring struct */
		*netstring = pkg_malloc(sizeof(netstring_t));
		CHECK_MALLOC(netstring);
		(*netstring)->read = 0;
		(*netstring)->length = len;
		(*netstring)->buffer = temp_buffer;
		(*netstring)->string = NULL;
	} else {
		/* Continue reading into an existing buffer. */
		offset = (*netstring)->read;
		read_len = (*netstring)->length-offset+1;
		temp_buffer = (*netstring)->buffer + offset;
	}

	/* Read from the evbuffer */
	bytes = evbuffer_remove(ib, temp_buffer, read_len);
	int total = (*netstring)->read += bytes;

	/* See if we have the whole netstring yet */
	if (read_len > bytes) {
		return NETSTRING_INCOMPLETE;
	}

	/* Test for the trailing comma */
	if (((*netstring)->buffer)[total-1] != ',') {
		return NETSTRING_ERROR_NO_COMMA;
	}

	/* Replace the comma with \0 */
	(*netstring)->buffer[total-1] = '\0';

	/* Set the string pointer to the "body" of the netstring */
	(*netstring)->string = (*netstring)->buffer;
	return 0;
}

int netstring_read_fd(int fd, netstring_t **netstring)
{
	int bytes, offset;
	size_t read_len;
	char *temp_buffer;
	temp_buffer = NULL;
	offset = 0;

	if (*netstring == NULL) {
		/* No buffer yet. Peek at first 10 bytes, to get length and colon. */
		char peek[10]={0};
		bytes = recv(fd,peek,10,MSG_PEEK);

		if (bytes < 3) return NETSTRING_INCOMPLETE;

		/* No leading zeros allowed! */
		if (peek[0] == '0' && isdigit(peek[1]))
			return NETSTRING_ERROR_LEADING_ZERO;

		/* The netstring must start with a number */
		if (!isdigit(peek[0])) return NETSTRING_ERROR_NO_LENGTH;

		int i, len;
		len = i = 0;

		/* Read the number of bytes */
		for (i = 0; i < bytes && isdigit(peek[i]); i++) {
			/* Error if more than 9 digits */
			if (i >= 9) return NETSTRING_ERROR_TOO_LONG;
			/* Accumulate each digit, assuming ASCII. */
			len = len*10 + (peek[i] - '0');
		}

		/* Read the colon */
		if (peek[i++] != ':') return NETSTRING_ERROR_NO_COLON;

		/* alloc the memory needed for the whole netstring */
		read_len = len+i+1;
		temp_buffer = pkg_malloc(read_len);
		CHECK_MALLOC(temp_buffer);

		/* initialize the netstring struct */
		*netstring = pkg_malloc(sizeof(netstring_t));
		CHECK_MALLOC(netstring);
		(*netstring)->start = i;
		(*netstring)->read = 0;
		(*netstring)->length = len;
		(*netstring)->buffer = temp_buffer;
		(*netstring)->string = NULL;
	} else {
		/* Continue reading into an existing buffer. */
		offset = (*netstring)->read;
		read_len = (*netstring)->start+(*netstring)->length-offset+1;
		temp_buffer = (*netstring)->buffer + offset;
	}

	/* Read from the socket */
	bytes = recv(fd, temp_buffer, read_len, 0);
	int total = (*netstring)->read += bytes;

	/* See if we have the whole netstring yet */
	if (read_len > bytes) {
		return NETSTRING_INCOMPLETE;
	}

	/* Test for the trailing comma */
	if (((*netstring)->buffer)[total-1] != ',') {
		return NETSTRING_ERROR_NO_COMMA;
	}

	/* Replace the comma with \0 */
	(*netstring)->buffer[total-1] = '\0';

	/* Set the string pointer to the "body" of the netstring */
	(*netstring)->string = (*netstring)->buffer + (*netstring)->start;
	return 0;
}


/* Reads a netstring from a `buffer` of length `buffer_length`. Writes
   to `netstring_start` a pointer to the beginning of the string in
   the buffer, and to `netstring_length` the length of the
   string. Does not allocate any memory. If it reads successfully,
   then it returns 0. If there is an error, then the return value will
   be negative. The error values are:

   NETSTRING_ERROR_TOO_LONG      More than 999999999 bytes in a field
   NETSTRING_ERROR_NO_COLON      No colon was found after the number
   NETSTRING_ERROR_TOO_SHORT     Number of bytes greater than buffer length
   NETSTRING_ERROR_NO_COMMA      No comma was found at the end
   NETSTRING_ERROR_LEADING_ZERO  Leading zeros are not allowed
   NETSTRING_ERROR_NO_LENGTH     Length not given at start of netstring

   If you're sending messages with more than 999999999 bytes -- about
   1 GB -- then you probably should not be doing so in the form of a
   single netstring. This restriction is in place partially to protect
   from malicious or erroneous input, and partly to be compatible with
   D. J. Bernstein's reference implementation.

   Example:
	if (netstring_read("3:foo,", 6, &str, &len) < 0) explode_and_die();
*/

int netstring_read(char *buffer, size_t buffer_length,
			char **netstring_start, size_t *netstring_length)
{
	int i;
	size_t len = 0;

	/* Write default values for outputs */
	*netstring_start = NULL; *netstring_length = 0;

	/* Make sure buffer is big enough. Minimum size is 3. */
	if (buffer_length < 3) return NETSTRING_ERROR_TOO_SHORT;

	/* No leading zeros allowed! */
	if (buffer[0] == '0' && isdigit(buffer[1]))
		return NETSTRING_ERROR_LEADING_ZERO;

	/* The netstring must start with a number */
	if (!isdigit(buffer[0])) return NETSTRING_ERROR_NO_LENGTH;

	/* Read the number of bytes */
	for (i = 0; i < buffer_length && isdigit(buffer[i]); i++) {
		/* Error if more than 9 digits */
		if (i >= 9) return NETSTRING_ERROR_TOO_LONG;
		/* Accumulate each digit, assuming ASCII. */
		len = len*10 + (buffer[i] - '0');
	}

	/* Check buffer length once and for all. Specifically, we make sure
		 that the buffer is longer than the number we've read, the length
		 of the string itself, and the colon and comma. */
	if (i + len + 1 >= buffer_length) return NETSTRING_ERROR_TOO_SHORT;

	/* Read the colon */
	if (buffer[i++] != ':') return NETSTRING_ERROR_NO_COLON;

	/* Test for the trailing comma, and set the return values */
	if (buffer[i + len] != ',') return NETSTRING_ERROR_NO_COMMA;
	*netstring_start = &buffer[i]; *netstring_length = len;

	return 0;
}

/* Return the length, in ASCII characters, of a netstring containing
   `data_length` bytes. */
size_t netstring_buffer_size(size_t data_length)
{
	if (data_length == 0) return 3;
	return (size_t)ceil(log10((double)data_length + 1)) + data_length + 2;
}

/* Allocate and create a netstring containing the first `len` bytes of
   `data`. This must be manually freed by the client. If `len` is 0
   then no data will be read from `data`, and it may be NULL. */
size_t netstring_encode_new(char **netstring, char *data, size_t len)
{
	char *ns;
	size_t num_len = 1;

	*netstring = NULL;

	if (len == 0) {
		ns = pkg_malloc(3);
		if (!ns) {
			return JSONRPC_ERROR_NO_MEMORY;
		}
		ns[0] = '0';
		ns[1] = ':';
		ns[2] = ',';
	} else {
		num_len = (size_t)ceil(log10((double)len + 1));
		ns = pkg_malloc(num_len + len + 2);
		if (!ns) {
			return JSONRPC_ERROR_NO_MEMORY;
		}
		sprintf(ns, "%lu:", (unsigned long)len);
		memcpy(ns + num_len + 1, data, len);
		ns[num_len + len + 1] = ',';
	}

	*netstring = ns;
	return num_len + len + 2;
}
