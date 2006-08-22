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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "permissions.h"
#include "im_hash.h"
#include "im_locks.h"
#include "im_db.h"

#define VAL_NULL_STRING(dv)	((dv)->val.string_val[0] == '\0')

/*
 * reads the table from SQL database, and inserts the IP addresses into
 * the hash table
 * returns -1 in case of error, -2 if the sql table is empty
 */
static int load_db(im_entry_t **hash)
{
	db_res_t	*res;
	db_val_t	*val;
	int		i, row_num, found;
	char		*ip, *avp_val;
	unsigned int	flags, mark;

	if (!hash || !db_handle) return -1;

	if (perm_dbf.use_table(db_handle, ipmatch_table) < 0) {
                LOG(L_ERR, "ERROR: load_db(): Error while trying to use ipmatch table\n");
		return -1;
	}

        if (perm_dbf.query(db_handle, 0, 0, 0, 0, 0, 0, 0, &res) < 0) {
                LOG(L_ERR, "ERROR: load_db(): Error while querying database\n");
                return -1;
        }

	row_num = RES_ROW_N(res);
	LOG(L_DBG, "DEBUG: load_db(): number of rows in ipmatch table: %d\n", row_num); 

	if (!row_num) {
                LOG(L_WARN, "WARNING: load_db(): ipmatch table is empty!\n");
		perm_dbf.free_result(db_handle, res);
	        return -2;
	}

	found = 0;
	for (i = 0; i < row_num; i++) {

		/* get every value of the row */
		/* start with flags */
		flags = RES_ROWS(res)[i].values[3].val.int_val;
		if ((flags & DB_DISABLED)
		|| ((flags & DB_LOAD_SER) == 0)) continue;

		found = 1;
		val = &(RES_ROWS(res)[i].values[0]); /* get IP address */
		if (VAL_NULL(val) || VAL_NULL_STRING(val)) {
			LOG(L_ERR, "ERROR: load_db(): ip address can not be NULL!\n");
			goto error;
		}
		ip = (char *)val->val.string_val;

		val = &(RES_ROWS(res)[i].values[1]);	/* output AVP value */
		if (!VAL_NULL(val) && !VAL_NULL_STRING(val)) avp_val = (char *)val->val.string_val;
		else avp_val = NULL;

		mark = RES_ROWS(res)[i].values[2].val.int_val;	/* get mark */

		/* create a new entry and insert it into the hash table */
		if (insert_im_hash(ip, avp_val, mark, hash)) {
			LOG(L_ERR, "ERROR: load_db(): could not insert entry into the hash table\n");
			goto error;
		}

	}

	perm_dbf.free_result(db_handle, res);
	if (found) {
		return 0;
	} else {
		LOG(L_WARN, "WARNING: load_db(): there is no active row in ipmatch table!\n");
		return -2;
	}

error:
	perm_dbf.free_result(db_handle, res);
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
