/*
 * Copyright (C) 2009 1&1 Internet AG
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * sip-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "dtm.h"
#include "carrier.h"
#include "log.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>




struct dtm_node_t *dtm_load(char *filename) {
	struct dtm_node_t *mroot;
	int fd;
	int len;
	int nodes;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		LERR("cannot open file '%s'\n", filename);
		return NULL;
	}

	len=lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	nodes=len/sizeof(struct dtm_node_t);
	LINFO("file contains %ld nodes (size=%ld, rest=%ld)\n", (long int)nodes, (long int)len, (long int)len%sizeof(struct dtm_node_t));

	mroot=mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
	if (mroot==MAP_FAILED) {
		LERR("cannot mmap file '%s', error=%d (%s)\n", filename, errno, strerror(errno));
		close(fd);
		return NULL;
	}

	return mroot;
}




int dtm_longest_match(struct dtm_node_t *mroot, const char *number, int numberlen, carrier_t *carrier)
{
	dtm_node_index_t node = 0;

  int nmatch = -1;
	int i=0;
	unsigned int digit;

	if (mroot[node].carrier > 0) {
		nmatch=0;
		*carrier = mroot[node].carrier;
	}
	while (i<numberlen) {
		digit = number[i] - '0';
		if (digit>9) return nmatch;
		node = mroot[node].child[digit];
		if (node == NULL_CARRIERID) return nmatch;
		i++;
		if (node<0) {
			nmatch=i;
			*carrier = -node;
			return nmatch;
		}
		if (mroot[node].carrier > 0) {
			nmatch=i;
			*carrier = mroot[node].carrier;
		}
	}

	return nmatch;
}
