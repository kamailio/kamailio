/* 
 * $Id$
 *
 * Flastore module connection structure
 *
 * Copyright (C) 2004 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include <errno.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "flatstore_mod.h"
#include "flat_con.h"

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
		LOG(L_ERR, "get_name: Invalid parameter value\n");
		return 0;
	}
	total_len=id->dir.len+1 /* / */+id->table.len+1 /* _ */+
				FILE_SUFFIX_LEN+1 /* \0 */; /* without pid*/
	if (buf_len<total_len){
		LOG(L_ERR, "get_name: the path is too long (%d and PATHMAX is %d)\n",
					total_len, buf_len);
		return 0;
	}
	
	buf=pkg_malloc(buf_len);
	if (buf==0){
		LOG(L_ERR, "ERROR: get_name: memory allocation failure\n");
		return 0;
	}

	ptr = buf;

	memcpy(ptr, id->dir.s, id->dir.len);
	ptr += id->dir.len;
	*ptr++ = '/';

	memcpy(ptr, id->table.s, id->table.len);
	ptr += id->table.len;

	*ptr++ = '_';
	
	num = int2str(flat_pid, &num_len);
	if (buf_len<(total_len+num_len)){
		LOG(L_ERR, "ERROR:  get_name: the path is too long (%d and PATHMAX is"
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
		LOG(L_ERR, "flat_new_connection: Invalid parameter value\n");
		return 0;
	}

	res = (struct flat_con*)pkg_malloc(sizeof(struct flat_con));
	if (!res) {
		LOG(L_ERR, "flat_new_connection: No memory left\n");
		return 0;
	}

	memset(res, 0, sizeof(struct flat_con));
	res->ref = 1;
	
	res->id = id;

	fn = get_name(id);
	if (fn==0){
		LOG(L_ERR, "flat_new_connection: get_name() failed\n");
		return 0;
	}

	res->file = fopen(fn, "a");
	pkg_free(fn); /* we don't need fn anymore */
	if (!res->file) {
		LOG(L_ERR, "flat_new_connection: %s\n", strerror(errno));
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
		LOG(L_ERR, "flat_reopen_connection: Invalid parameter value\n");
		return -1;
	}

	if (con->file) {
		fclose(con->file);

		fn = get_name(con->id);

		con->file = fopen(fn, "a");
		if (!con->file) {
			LOG(L_ERR, "flat_reopen_connection: Invalid parameter value\n");
			return -1;
		}
	}

	return 0;
}
