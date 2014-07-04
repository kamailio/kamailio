/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 /*
  * History:
  * --------
  *  2004-06-06  updated to the new DB api (andrei)
  */

#include "../../mem/shm_mem.h"
#include "../../lib/srdb2/db.h"
#include "../../dprint.h"
#include "cpl_db.h"

static db_ctx_t* ctx = NULL;
static db_cmd_t* get_script;
static db_cmd_t* write_script;
static db_cmd_t* delete_user;


void cpl_db_close()
{
	if (delete_user) db_cmd_free(delete_user);
	delete_user = NULL;

	if (write_script) db_cmd_free(write_script);
	write_script = NULL;

	if (get_script) db_cmd_free(get_script);
	get_script = NULL;

	if (ctx) {
		db_disconnect(ctx);
		db_ctx_free(ctx);
		ctx = NULL;
	}
}


int cpl_db_init(char* db_url, char* db_table)
{
	db_fld_t cols[] = {
		{.name = "cpl_bin", .type = DB_BLOB},
		{.name = "cpl_xml", .type = DB_STR},
		{.name = 0}
	};

	db_fld_t match[] = {
		{.name = "uid", .type = DB_CSTR},
		{.name = 0}
	};

	db_fld_t vals[] = {
		{.name = "uid",     .type = DB_CSTR},
		{.name = "cpl_bin", .type = DB_BLOB},
		{.name = "cpl_xml", .type = DB_STR },
		{.name = 0}
	};

	ctx = db_ctx("cpl-c");
	if (ctx == NULL) goto error;

	if (db_add_db(ctx, db_url) < 0) goto error;
	if (db_connect(ctx) < 0) goto error;

	get_script = db_cmd(DB_GET, ctx, db_table, cols, match, NULL);
	if (!get_script) goto error;

	write_script = db_cmd(DB_PUT, ctx, db_table, NULL, NULL, vals);
	if (!write_script) goto error;

	delete_user = db_cmd(DB_DEL, ctx, db_table, NULL, match, NULL);
	if (!delete_user) goto error;

	return 0;
error:
	ERR("cpl-c: Error while initializing db layer\n");
	cpl_db_close();
	return -1;
}


/* gets from database the cpl script in binary format; the returned script is
 * allocated in shared memory
 * Returns:  1 - success
 *          -1 - error
 */
int get_user_script(str *user, str *script, int bin)
{
	db_res_t* res = 0;
	db_rec_t* rec;
	int i;

	if (bin) i = 0;
	else i = 1;

	get_script->match[0].v.cstr = user->s;

	DBG("DEBUG:get_user_script: fetching script for user <%s>\n",user->s);
	if (db_exec(&res, get_script) < 0) {
		LOG(L_ERR,"ERROR:cpl-c:get_user_script: db_query failed\n");
		goto error;
	}

	if (!res || !(rec = db_first(res))) {
		DBG("DEBUG:get_user_script: user <%.*s> not found in db -> probably "
			"he has no script\n",user->len, user->s);
		script->s = 0;
		script->len = 0;
	} else {
		if (rec->fld[i].flags & DB_NULL) {
			DBG("DEBUG:get_user_script: user <%.*s> has a NULL script\n",
				user->len, user->s);
			script->s = 0;
			script->len = 0;
		} else {
			DBG("DEBUG:get_user_script: we got the script len=%d\n",
				rec->fld[i].v.blob.len);
			script->len = rec->fld[i].v.blob.len;
			script->s = shm_malloc( script->len );
			if (!script->s) {
				LOG(L_ERR,"ERROR:cpl-c:get_user_script: no free sh_mem\n");
				goto error;
			}
			memcpy( script->s, rec->fld[i].v.blob.s,
				script->len);
		}
	}

	if (res) db_res_free(res);
	return 1;
error:
	if (res)
		db_res_free(res);
	script->s = 0;
	script->len = 0;
	return -1;
}



/* inserts into database a cpl script in XML format(xml) along with its binary
 * format (bin)
 * Returns:  1 - success
 *          -1 - error
 */
int write_to_db(char *usr, str *xml, str *bin)
{
	write_script->vals[0].v.cstr = usr;
	write_script->vals[1].v.blob = *bin;
	write_script->vals[2].v.lstr = *xml;

	/* No need to do update/insert here, the db layer does that
	 * automatically without the need to query the db
	 */
	if (db_exec(NULL, write_script) < 0) {
		ERR("cpl-c: Error while writing script into database\n");
		return -1;
	}
	return 0;
}



/* delete from database the entity record for a given user - if a user has no
 * script, he will be removed completely from db; users without script are not
 * allowed into db ;-)
 * Returns:  1 - success
 *          -1 - error
 */
int rmv_from_db(char *usr)
{
	delete_user->match[0].v.cstr = usr;
	
	if (db_exec(NULL, delete_user) < 0) {
		LOG(L_ERR,"ERROR:cpl-c:rmv_from_db: error when deleting script for "
			"user \"%s\"\n",usr);
		return -1;
	}

	return 1;
}

