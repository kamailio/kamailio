/*
 * $Id$
 *
 * Copyright (C) 2006 iptelorg GmbH
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
 *
 */

#include "permissions.h"
#include "im_hash.h"
#include "im_locks.h"
#include "im_db.h"

/* DB commands to load the table */
static db_cmd_t	*cmd_load_im = NULL;

/* prepare the DB cmds */
int init_im_db(void)
{
	db_fld_t load_res_cols[] = {
		{.name = "ip",		.type = DB_CSTR},
		{.name = "avp_val",	.type = DB_CSTR},
		{.name = "mark",	.type = DB_BITMAP},
		{.name = "flags",	.type = DB_BITMAP},
		{.name = NULL}
	};

	if (db_mode != ENABLE_CACHE) return 0; /* nothing to do */
	if (!db_conn) return -1;

	cmd_load_im =
		db_cmd(DB_GET, db_conn, ipmatch_table, load_res_cols, NULL, NULL);

	if (!cmd_load_im) {
		LOG(L_ERR, "init_im_db(): failed to prepare DB commands\n");
		return -1;
	}
	return 0;
}

/* destroy the DB cmds */
void destroy_im_db(void)
{
	if (cmd_load_im) {
		db_cmd_free(cmd_load_im);
		cmd_load_im = NULL;
	}
}

#define VAL_NULL_STR(fld) ( \
		((fld).flags & DB_NULL) \
		|| (((fld).type == DB_CSTR) && ((fld).v.cstr[0] == '\0')) \
		|| (((fld).type == DB_STR) && \
			(((fld).v.lstr.len == 0) || ((fld).v.lstr.s[0] == '\0'))) \
	)

/*
 * reads the table from SQL database, and inserts the IP addresses into
 * the hash table
 * returns -1 in case of error, -2 if the sql table is empty
 */
static int load_db(im_entry_t **hash)
{
	db_res_t	*res = NULL;
	db_rec_t	*rec;
	int		found;
	char		*ip, *avp_val;
	unsigned int	flags, mark;

	if (!hash || !cmd_load_im) return -1;

	if (db_exec(&res, cmd_load_im) < 0) {
                LOG(L_ERR, "ERROR: load_db(): Error while querying database\n");
                return -1;
        }

	found = 0;
	rec = db_first(res);
	while (rec) {
		/* get every value of the row */
		/* start with flags */
		if (rec->fld[3].flags & DB_NULL) goto skip;
		flags = rec->fld[3].v.bitmap;
		if ((flags & SRDB_DISABLED)
		|| ((flags & SRDB_LOAD_SER) == 0)) goto skip;

		found++;
		/* get IP address */
		if (VAL_NULL_STR(rec->fld[0])) {
			LOG(L_ERR, "ERROR: load_db(): ip address can not be NULL!\n");
			goto error;
		}
		ip = rec->fld[0].v.cstr;

		/* output AVP value */
		if (!VAL_NULL_STR(rec->fld[1]))
			avp_val = rec->fld[1].v.cstr;
		else
			avp_val = NULL;

		if (rec->fld[2].flags & DB_NULL)
			mark = (unsigned int)-1;	/* will match eveything */
		else
			mark = rec->fld[2].v.bitmap;	/* get mark */

		/* create a new entry and insert it into the hash table */
		if (insert_im_hash(ip, avp_val, mark, hash)) {
			LOG(L_ERR, "ERROR: load_db(): could not insert entry into the hash table\n");
			goto error;
		}
skip:
		rec = db_next(res);
	}

	if (res) db_res_free(res);
	if (found) {
		LOG(L_DBG, "DEBUG: load_db(): number of rows in ipmatch table: %d\n", found); 
		return 0;
	} else {
		LOG(L_WARN, "WARNING: load_db(): there is no active row in ipmatch table!\n");
		return -2;
	}

error:
	if (res) db_res_free(res);
	return -1;
}

/* reload DB cache
 * return value
 *   0: success
 *  -1: error
 */
int reload_im_cache(void)
{
	im_entry_t	**hash, **old_hash;
	int	ret;


	if (!IM_HASH) {
		LOG(L_CRIT, "ERROR: reload_im_cache(): ipmatch hash table is not initialied. "
			"Have you set the database url?\n");
		return -1;
	}

	/* make sure that there is no other writer process */
	writer_lock_imhash();

	if (!(hash = new_im_hash())) {
		writer_release_imhash();
		return -1;
	}
	ret = load_db(hash);

	if (ret == -1) {
		/* error occured */
		LOG(L_ERR, "ERROR: reload_im_cache(): could not reload cache\n");
		free_im_hash(hash); /* there can be data in the hash already */
		writer_release_imhash();
		return -1;

	} else if (ret == -2) {
		/* SQL table was empty -- drop hash table */
		delete_im_hash(hash);
		hash = NULL;
	}

	old_hash = IM_HASH->entries;

	/* ask reader processes to stop reading */
	set_wd_imhash();
	IM_HASH->entries = hash;
	del_wd_imhash();

	if (old_hash) free_im_hash(old_hash);

	writer_release_imhash();
	return 0;
}
