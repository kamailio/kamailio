/*
 * pipelimit module
 *
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
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

/*! \file
 * \ingroup pipelimit
 * \brief pipelimit :: pl_db
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../lib/srdb1/db.h"
#include "../../lib/srdb1/db_res.h"
#include "../../str.h"

#include "pl_ht.h"

#if 0
INSERT INTO version (table_name, table_version) values ('pl_pipes','1');
CREATE TABLE pl_pipes (
  id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
  pipeid VARCHAR(64) DEFAULT '' NOT NULL,
  algorithm VARCHAR(32) DEFAULT '' NOT NULL,
  plimit INT DEFAULT 0 NOT NULL,
  CONSTRAINT pipeid_idx UNIQUE (pipeid)
) ENGINE=MyISAM;
#endif

#define RLP_PIPEID_COL			"pipeid"
#define RLP_LIMIT_COL			"plimit"
#define RLP_ALGORITHM_COL		"algorithm"
#define RLP_TABLE_NAME			"pl_pipes"


#define RLP_TABLE_VERSION	1
static int _rlp_table_version = RLP_TABLE_VERSION;
static db_func_t  pl_dbf;
static db1_con_t* pl_db_handle=0;

/*db */
str pl_db_url          = {NULL, 0};
str rlp_pipeid_col     = str_init(RLP_PIPEID_COL);
str rlp_limit_col      = str_init(RLP_LIMIT_COL);
str rlp_algorithm_col  = str_init(RLP_ALGORITHM_COL);
str rlp_table_name     = str_init(RLP_TABLE_NAME);

int pl_load_db(void);

int pl_connect_db(void)
{
	if(pl_db_url.s==NULL)
		return -1;

	if (pl_db_handle!=NULL)
	{
		LM_CRIT("BUG - db connection found already open\n");
		return -1;
	}

	if ((pl_db_handle = pl_dbf.init(&pl_db_url)) == 0){
		
			return -1;
	}
	return 0;
}

void pl_disconnect_db(void)
{
	if(pl_db_handle!=NULL)
	{
		pl_dbf.close(pl_db_handle);
		pl_db_handle = 0;
	}
}


/*! \brief Initialize and verify DB stuff*/
int pl_init_db(void)
{
	int ret;

	if(pl_db_url.s==NULL)
		return 1;

	if(rlp_table_name.len <= 0 || pl_db_url.len<=0)
	{
		LM_INFO("no table name or db url - skipping loading from db\n");
		return 0;
	}

	/* Find a database module */
	if (db_bind_mod(&pl_db_url, &pl_dbf) < 0)
	{
		LM_ERR("Unable to bind to a database driver\n");
		return -1;
	}
	
	if(pl_connect_db()!=0){
		
		LM_ERR("unable to connect to the database\n");
		return -1;
	}
	
	_rlp_table_version = db_table_version(&pl_dbf, pl_db_handle,
			&rlp_table_name);
	if (_rlp_table_version < 0) 
	{
		LM_ERR("failed to query pipes table version\n");
		return -1;
	} else if (_rlp_table_version != RLP_TABLE_VERSION) {
		LM_ERR("invalid table version (found %d , required %d)\n"
			"(use kamdbctl reinit)\n",
			_rlp_table_version, RLP_TABLE_VERSION);
		return -1;
	}

	ret = pl_load_db();

	pl_disconnect_db();

	return ret;
}

/*! \brief load pipe descriptions from DB*/
int pl_load_db(void)
{
	int i, nr_rows;
	int nrcols;
	str pipeid;
	int limit;
	str algorithm;
	db1_res_t * res;
	db_val_t * values;
	db_row_t * rows;
	
	db_key_t query_cols[3] = {&rlp_pipeid_col, &rlp_limit_col,
								&rlp_algorithm_col};
	
	nrcols = 3;

	if(pl_db_handle == NULL) {
		LM_ERR("invalid DB handler\n");
		return -1;
	}

	if (pl_dbf.use_table(pl_db_handle, &rlp_table_name) < 0)
	{
		LM_ERR("error in use_table\n");
		return -1;
	}

	if(pl_dbf.query(pl_db_handle,0,0,0,query_cols,0,nrcols,0,&res) < 0)
	{
		LM_ERR("error while querying database\n");
		return -1;
	}

	nr_rows = RES_ROW_N(res);
	rows 	= RES_ROWS(res);
	if(nr_rows == 0)
	{
		LM_WARN("no ratelimit pipes data in the db\n");
		pl_dbf.free_result(pl_db_handle, res);
		return 0;
	}

	for(i=0; i<nr_rows; i++)
	{
		values = ROW_VALUES(rows+i);

		pipeid.s      = VAL_STR(values).s;
		pipeid.len    = strlen(pipeid.s);
		limit         = VAL_INT(values+1);
		algorithm.s   = VAL_STR(values+2).s;
		algorithm.len = strlen(algorithm.s);

		if(pl_pipe_add(&pipeid, &algorithm, limit) != 0)
			goto error;

	}
	pl_dbf.free_result(pl_db_handle, res);

	pl_print_pipes();

	return 0;

error:
	pl_dbf.free_result(pl_db_handle, res);

	return -1;
}


