/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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

#include "../mysql/dbase.h"
#include "../../dprint.h"
#include "cpl_db.h"


char *DB_URL;
char *DB_TABLE;

int write_to_db( char *usr, char *bin_s, int bin_len, char *xml_s, int xml_len)
{
	db_key_t   keys[] = {"username","cpl","status","cpl_bin"};
	db_val_t   vals[4];
	db_con_t   *db_con;
	db_res_t   *res;


	/* open connexion to db */
/*
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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

	db_con = db_init(DB_URL);
	if (!db_con) {
		LOG(L_ERR,"ERROR:cpl:write_to_db: cannot connect to mysql db.\n");
		goto error;
	}

	/* set the table */
	if (use_table(db_con, DB_TABLE)<0) {
		LOG(L_ERR,"ERROR:cpl:write_to_db: db_use_table failed\n");
		goto error;
	}

	/* lets see if the user is already in database */
	vals[0].type = DB_STRING;
	vals[0].nul  = 0;
	vals[0].val.string_val = usr;
	if (db_query(db_con, keys, 0, vals, keys, 1, 1, NULL, &res) < 0) {
		LOG(L_ERR,"ERROR:cpl:write_to_db: db_query failed\n");
		goto error;
	}
	if (res->n>1) {
		LOG(L_ERR,"ERROR:cpl:write_to_db: Incosistent CPL database:"
			" %d records for user %s\n",res->n,usr);
		goto error;
	}

	/* username */
	vals[0].type = DB_STRING;
	vals[0].nul  = 0;
	vals[0].val.string_val = usr;
	/* cpl text */
	vals[1].type = DB_BLOB;
	vals[1].nul  = 0;
	vals[1].val.blob_val.s = xml_s;
	vals[1].val.blob_val.len = xml_len;
	/* status */
	vals[2].type = DB_INT;
	vals[2].nul  = 0;
	vals[2].val.int_val = 1;
	/* cpl bin */
	vals[3].type = DB_BLOB;
	vals[3].nul  = 0;
	vals[3].val.blob_val.s = bin_s;
	vals[3].val.blob_val.len = bin_len;
	/* insert or update ? */
	if (res->n==0) {
		DBG("DEBUG:cpl:write_to_db:No user %s in CPL databse->insert\n",usr);
		if (db_insert(db_con, keys, vals, 4) < 0) {
			LOG(L_ERR,"ERROR:cpl:write_to_db: insert failed !\n");
			goto error;
		}
	} else {
		DBG("DEBUG:cpl:write_to_db:User %s already in CPL database ->"
			" update\n",usr);
		if (db_update(db_con, keys, vals, keys+1, vals+1, 1, 3) < 0) {
			LOG(L_ERR,"ERROR:cpl:write_to_db: updare failed !\n");
			goto error;
		}
	}

	return 0;
error:
	return -1;
}

