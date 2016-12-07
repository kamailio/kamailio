/* $Id$
 *
 * Copyright (C) 2006-2007 Sippy Software, Inc. <sales@sippysoft.com>
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


#include "bdb.h"

#define BDB_ID          "bdb://"
#define BDB_ID_LEN      (sizeof(BDB_ID)-1)
#define BDB_PATH_LEN    MAXPATHLEN

/*
 * Initialize database connection
 */
db_con_t* bdb_init(const char* _sqlurl)
{
        db_con_t* _res;
        char *p;
        char bdb_path[BDB_PATH_LEN];
	int ret;

        if (!_sqlurl)
        {
#ifdef BDB_EXTRA_DEBUG
                LOG(L_ERR, "BDB:bdb_init: Invalid parameter value\n");
#endif
                return NULL;
        }
        p = (char *)_sqlurl;
        if(strncmp(p, BDB_ID, sizeof(BDB_ID) - 1))
        {
                LOG(L_ERR, "BDB:bdb_init: invalid database URL - should be:"
                        " <%s[/]path/to/directory>\n", BDB_ID);
                return NULL;
        }
        p += BDB_ID_LEN;
        if(p[0] != '/')
        {
                if(sizeof(CFG_DIR) + strlen(p) + 2 > BDB_PATH_LEN)
                {
                        LOG(L_ERR, "BDB:bdb_init: path to database is too long\n");
                        return NULL;
                }
                strcpy(bdb_path, CFG_DIR);
		bdb_path[sizeof(CFG_DIR) - 1] = '/';
                strcpy(&bdb_path[sizeof(CFG_DIR)], p);
                p = bdb_path;
        }
#ifdef BDB_EXTRA_DEBUG
	LOG(L_NOTICE, "BDB:bdb_init: bdb_path = %s\n", p);
#endif

        _res = pkg_malloc(sizeof(db_con_t) + sizeof(bdb_con_t));
        if (!_res)
        {
                LOG(L_ERR, "BDB:bdb_init: No memory left\n");
                return NULL;
        }
        memset(_res, 0, sizeof(db_con_t) + sizeof(bdb_con_t));
	_res->tail = (unsigned long)((char*)_res + sizeof(bdb_con_t));

	ret = db_env_create(&BDB_CON_DBENV(_res), 0);
	if (ret != 0) {
		LOG(L_ERR, "BDB:bdb_init: unable to db_env_create(): %s\n", db_strerror(ret));

		pkg_free(_res);
		return NULL;
	}

	ret = BDB_CON_DBENV(_res)->open(BDB_CON_DBENV(_res), p, DB_CREATE | DB_INIT_MPOOL | DB_INIT_CDB, 0);
	if (ret != 0) {
		LOG(L_ERR, "BDB:bdb_init: unable to open environment: %s\n", db_strerror(ret));

		pkg_free(_res);
		return NULL;
	}

	return _res;
}

/*
 * Close a database connection
 */
void bdb_close(db_con_t* _h)
{
        if (!_h)
        {
#ifdef DBT_EXTRA_DEBUG
                LOG(L_ERR, "BDB:bdb_close: Invalid parameter value\n");
#endif
                return;
        }

	if (BDB_CON_DB(_h) != NULL) {
		bdb_close_table(_h);
	}

	if (BDB_CON_DBENV(_h) != NULL) {
		BDB_CON_DBENV(_h)->close(BDB_CON_DBENV(_h), 0);
	}

        pkg_free(_h);

	return;
}


/*
 * Raw SQL query -- is not the case to have this method
 */
int bdb_raw_query(db_con_t* _h, char* _s, db_res_t** _r)
{
    *_r = NULL;
    LOG(L_ERR, "BDB:bdb_raw_query: method is not supported.\n");
    return -1;
}


/*
 * Query table for specified rows
 * _h: structure representing database connection
 * _k: key names
 * _op: operators
 * _v: values of the keys that must match
 * _c: column names to return
 * _n: number of key=values pairs to compare
 * _nc: number of columns to return
 * _o: order by the specified column
 */

int bdb_query(db_con_t* _h, db_key_t* _k, db_op_t* _op, db_val_t* _v,
		db_key_t* _c, int _n, int _nc, db_key_t _o, db_res_t** _r)
{
	bdb_srow_p	s_r;		/* search keys row */
	bdb_rrow_p	r_r;		/* return keys row */
	int		ret;

	*_r = NULL;
	s_r = NULL;
	r_r = NULL;

	if (_o) {
		LOG(L_ERR, "BDB:bdb_query: ORDER BY is not supported.\n");
		return -1;
	}

	if ((ret = bdb_srow_db2bdb(_h, _k, _op, _v, _n, &s_r)) < 0) {
		return -1;
	};

	if ((ret = bdb_rrow_db2bdb(_h, _c, _nc, &r_r)) < 0) {
		bdb_free_srow(s_r);
		return -1;
	};

	if ((ret = bdb_query_table(_h, s_r, r_r, _nc, _r)) < 0) {
		bdb_free_rrow(r_r);
		bdb_free_srow(s_r);
		return -1;
	};

	bdb_free_rrow(r_r);
	bdb_free_srow(s_r);

	return 0;
}


/*
 * Insert a row into table
 */
int bdb_insert(db_con_t* _h, db_key_t* _k, db_val_t* _v, int _n)
{
	bdb_row_p	r;
	DBC		*cursorp;
	int		ret;

	if ((ret = bdb_row_db2bdb(_h, _k, _v, _n, &r)) < 0) {
		return -1;
	};

	if (r->key.size == 0) {
		LOG(L_ERR, "BDB:bdb_insert: no primary key specified\n");
		bdb_free_row(r);
		return -1;
	}

	BDB_CON_DB(_h)->cursor(BDB_CON_DB(_h), NULL, &cursorp, DB_WRITECURSOR);
	ret = cursorp->c_put(cursorp, &(r->key), &(r->data), DB_KEYLAST);

	if (ret < 0) {
		LOG(L_ERR, "BDB:bdb_insert: unable to insert record: %s\n", db_strerror(ret));
	}

	if (cursorp != NULL) {
		cursorp->c_close(cursorp);
	}

	bdb_free_row(r);

	return ret;
}


/*
 * Delete a row from table
 */
int bdb_delete(db_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v, int _n)
{
	bdb_srow_p	s_r;		/* search keys row */
	int		ret;

	s_r = NULL;

	if ((ret = bdb_srow_db2bdb(_h, _k, _o, _v, _n, &s_r)) < 0) {
		return -1;
	};

	if ((ret = bdb_delete_table(_h, s_r)) < 0) {
		bdb_free_srow(s_r);
		return -1;
	};

	bdb_free_srow(s_r);

	return 0;
}

/*
 * Update a row in table
 */
int bdb_update(db_con_t* _h, db_key_t* _k, db_op_t* _o, db_val_t* _v,
              db_key_t* _uk, db_val_t* _uv, int _n, int _un)
{
	bdb_srow_p	s_r;		/* search keys row */
	bdb_urow_p	u_r;		/* update row */
	int		ret;

	s_r = NULL;
	u_r = NULL;

	if ((ret = bdb_srow_db2bdb(_h, _k, _o, _v, _n, &s_r)) < 0) {
		return -1;
	};

	if ((ret = bdb_urow_db2bdb(_h, _uk, _uv, _un, &u_r)) < 0) {
		bdb_free_srow(s_r);
		return -1;
	};

	if ((ret = bdb_update_table(_h, s_r, u_r)) < 0) {
		bdb_free_urow(u_r);
		bdb_free_srow(s_r);
		return -1;
	};

	bdb_free_urow(u_r);
	bdb_free_srow(s_r);

	return 0;
}

/*
 * Free all memory allocated by get_result
 */
int bdb_free_result(db_con_t* _h, db_res_t* _r)
{
	db_row_t* r;
	db_val_t* v;
	int i, j;

	if (!_r) {
#ifdef BDB_EXTRA_DEBUG
	LOG(L_NOTICE, "BDB:bdb_free_result: NULL pointer\n");
#endif
		return 0;
	}

	for (i = 0; i < RES_ROW_N(_r); i++) {
		r = &(RES_ROWS(_r)[i]);
		for (j = 0; j < RES_COL_N(_r); j++) {
			v = &(ROW_VALUES(r)[j]);
			if (VAL_TYPE(v) == DB_STRING || VAL_TYPE(v) == DB_STR || VAL_TYPE(v) == DB_BLOB) {
				free(VAL_STR(v).s);
			}
		}
		free(ROW_VALUES(r));
	}
	free(RES_ROWS(_r));

	for (i = 0; i < RES_COL_N(_r); i++) {
		pkg_free((void *)RES_NAMES(_r)[i]);
	}
	pkg_free(RES_NAMES(_r));
	pkg_free(RES_TYPES(_r));

	pkg_free(_r);

        return 0;
}
