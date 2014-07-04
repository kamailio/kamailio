/* 
 * $Id$
 *
 * Flastore module connection structure
 *
 * Copyright (C) 2004 FhG Fokus
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

#include <string.h>
#include <errno.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "km_flatstore_mod.h"
#include "km_flat_con.h"

#define FILE_SUFFIX ".log"
#define FILE_SUFFIX_LEN (sizeof(FILE_SUFFIX) - 1)

/* returns a pkg_malloc'ed file name */
static char* get_name(struct flat_id* id)
{
	char* buf;
	int buf_len;
	char* num, *ptr;
	int num_len;
	int total_len;

	buf_len=pathmax();
	if (!id) {
		LM_ERR("invalid parameter value\n");
		return 0;
	}
	total_len=id->dir.len+1 /* / */+id->table.len+1 /* _ */+
				FILE_SUFFIX_LEN+1 /* \0 */; /* without pid*/
	if (buf_len<total_len){
		LM_ERR("the path is too long (%d and PATHMAX is %d)\n",
					total_len, buf_len);
		return 0;
	}
	
	buf=pkg_malloc(buf_len);
	if (buf==0){
		LM_ERR("pkg memory allocation failure\n");
		return 0;
	}

	ptr = buf;

	memcpy(ptr, id->dir.s, id->dir.len);
	ptr += id->dir.len;
	*ptr++ = '/';

	memcpy(ptr, id->table.s, id->table.len);
	ptr += id->table.len;

	*ptr++ = '_';
	
	num = int2str(km_flat_pid, &num_len);
	if (buf_len<(total_len+num_len)){
		LM_ERR("the path is too long (%d and PATHMAX is"
				" %d)\n", total_len+num_len, buf_len);
		pkg_free(buf);
		return 0;
	}
	memcpy(ptr, num, num_len);
	ptr += num_len;

	memcpy(ptr, FILE_SUFFIX, FILE_SUFFIX_LEN);
	ptr += FILE_SUFFIX_LEN;

	*ptr = '\0';
	return buf;
}


struct flat_con* flat_new_connection(struct flat_id* id)
{
	char* fn;

	struct flat_con* res;

	if (!id) {
		LM_ERR("invalid parameter value\n");
		return 0;
	}

	res = (struct flat_con*)pkg_malloc(sizeof(struct flat_con));
	if (!res) {
		LM_ERR("no pkg memory left\n");
		return 0;
	}

	memset(res, 0, sizeof(struct flat_con));
	res->ref = 1;
	
	res->id = id;

	fn = get_name(id);
	if (fn==0){
		LM_ERR("get_name() failed\n");
		pkg_free(res);
		return 0;
	}

	res->file = fopen(fn, "a");
	pkg_free(fn); /* we don't need fn anymore */
	if (!res->file) {
		LM_ERR(" %s\n", strerror(errno));
		pkg_free(res);
		return 0;
	}
	
	return res;
}


/*
 * Close the connection and release memory
 */
void flat_free_connection(struct flat_con* con)
{
	if (!con) return;
	if (con->id) free_flat_id(con->id);
	if (con->file) {
		fclose(con->file);
	}
	pkg_free(con);
}


/*
 * Reopen a connection
 */
int flat_reopen_connection(struct flat_con* con)
{
	char* fn;

	if (!con) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (con->file) {
		fclose(con->file);
		con->file = 0;

		fn = get_name(con->id);
		if (fn == 0) {
			LM_ERR("failed to get_name\n");
			return -1;
		}

		con->file = fopen(fn, "a");
		pkg_free(fn);

		if (!con->file) {
			LM_ERR("invalid parameter value\n");
			return -1;
		}
	}

	return 0;
}
