/*
 * Copyright (C) 2014 Daniel-Constantin Mierla (asipto.com)
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
 *
 */


#include "../../core/mem/mem.h"
#include "../../core/dprint.h"
#include "../../lib/srdb1/db_ut.h"
#include "../../lib/srdb1/db_query.h"
#include "mongodb_connection.h"
#include "mongodb_dbase.h"

#define DB_MONGODB_ROWS_STEP	1000

typedef struct db_mongodb_result {
	mongoc_collection_t *collection;   /*!< Collection link */
	mongoc_cursor_t *cursor;           /*!< Cursor link */
	bson_t *rdoc;
	int idx;
	bson_t *colsdoc;
	int nrcols;
	int maxrows;
} db_mongodb_result_t;

/*
 * Initialize database module
 * No function should be called before this
 */
db1_con_t* db_mongodb_init(const str* _url)
{
	return db_do_init(_url, (void *)db_mongodb_new_connection);
}

/*
 * Shut down database module
 * No function should be called after this
 */
void db_mongodb_close(db1_con_t* _h)
{
	db_do_close(_h, db_mongodb_free_connection);
}

/*
 * Add key-op-value to a bson filter document
 */
int db_mongodb_bson_filter_add(bson_t *doc, const db_key_t* _k, const db_op_t* _op,
		const db_val_t* _v, int idx)
{
	bson_t mdoc;
	db_key_t tkey;
	const db_val_t *tval;
	int vtype;
	str ocmp;

	tkey = _k[idx];
	tval = _v + idx;
	vtype = VAL_TYPE(tval);

	/* OP_EQ is handled separately */
	if(!strcmp(_op[idx], OP_LT)) {
		ocmp.s = "$lt";
		ocmp.len = 3;
	} else if(!strcmp(_op[idx], OP_LEQ)) {
		ocmp.s = "$lte";
		ocmp.len = 4;
	} else if(!strcmp(_op[idx], OP_GT)) {
		ocmp.s = "$gt";
		ocmp.len = 3;
	} else if(!strcmp(_op[idx], OP_GEQ)) {
		ocmp.s = "$gte";
		ocmp.len = 4;
	} else if(!strcmp(_op[idx], OP_NEQ)
			|| !strcmp(_op[idx], "!=")) {
		ocmp.s = "$ne";
		ocmp.len = 3;
	} else {
		LM_ERR("unsupported match operator: %s\n", _op[idx]);
		goto error;
	}

	if(!bson_append_document_begin(doc, tkey->s, tkey->len, &mdoc)) {
		LM_ERR("failed to append start to bson doc %.*s %s ... [%d]\n",
					tkey->len, tkey->s, ocmp.s, idx);
		goto error;
	}

	if(VAL_NULL(tval)) {
		if(!bson_append_null(&mdoc, ocmp.s, ocmp.len)) {
			LM_ERR("failed to append null to bson doc %.*s %s null [%d]\n",
					tkey->len, tkey->s, ocmp.s, idx);
			goto error;
		}
		goto done;
	}
	switch(vtype) {
		case DB1_INT:
			if(!bson_append_int32(&mdoc, ocmp.s, ocmp.len,
						VAL_INT(tval))) {
				LM_ERR("failed to append int to bson doc %.*s %s %d [%d]\n",
						tkey->len, tkey->s, ocmp.s, VAL_INT(tval), idx);
				goto error;
			}
			break;

		case DB1_BIGINT:
			if(!bson_append_int64(&mdoc, ocmp.s, ocmp.len,
						VAL_BIGINT(tval ))) {
				LM_ERR("failed to append bigint to bson doc %.*s %s %lld [%d]\n",
						tkey->len, tkey->s, ocmp.s, VAL_BIGINT(tval), idx);
				goto error;
			}
			return -1;

		case DB1_DOUBLE:
			if(!bson_append_double(&mdoc, ocmp.s, ocmp.len,
						VAL_DOUBLE(tval))) {
				LM_ERR("failed to append double to bson doc %.*s %s %f [%d]\n",
						tkey->len, tkey->s, ocmp.s, VAL_DOUBLE(tval), idx);
				goto error;
			}
			break;

		case DB1_STRING:
			if(!bson_append_utf8(&mdoc, ocmp.s, ocmp.len,
						VAL_STRING(tval), strlen(VAL_STRING(tval))) ) {
				LM_ERR("failed to append string to bson doc %.*s %s %s [%d]\n",
						tkey->len, tkey->s, ocmp.s, VAL_STRING(tval), idx);
				goto error;
			}
			break;

		case DB1_STR:

			if(!bson_append_utf8(&mdoc, ocmp.s, ocmp.len,
						VAL_STR(tval).s, VAL_STR(tval).len) ) {
				LM_ERR("failed to append str to bson doc %.*s %s %.*s [%d]\n",
						tkey->len, tkey->s, ocmp.s, VAL_STR(tval).len, VAL_STR(tval).s, idx);
				goto error;
			}
			break;

		case DB1_DATETIME:
			if(!bson_append_time_t(&mdoc, ocmp.s, ocmp.len,
						VAL_TIME(tval))) {
				LM_ERR("failed to append time to bson doc %.*s %s %ld [%d]\n",
						tkey->len, tkey->s, ocmp.s, VAL_TIME(tval), idx);
				goto error;
			}
			break;

		case DB1_BLOB:
			if(!bson_append_binary(&mdoc, ocmp.s, ocmp.len,
						BSON_SUBTYPE_BINARY,
						(const uint8_t *)VAL_BLOB(tval).s, VAL_BLOB(tval).len) ) {
				LM_ERR("failed to append blob to bson doc %.*s %s [bin] [%d]\n",
						tkey->len, tkey->s, ocmp.s, idx);
				goto error;
			}
			break;

		case DB1_BITMAP:
			if(!bson_append_int32(&mdoc, ocmp.s, ocmp.len,
						VAL_INT(tval))) {
				LM_ERR("failed to append bitmap to bson doc %.*s %s %d [%d]\n",
						tkey->len, tkey->s, ocmp.s, VAL_INT(tval), idx);
				goto error;
			}
			break;

		default:
			LM_ERR("val type [%d] not supported\n", vtype);
			goto error;
	}

done:
	if(!bson_append_document_end(doc, &mdoc)) {
		LM_ERR("failed to append end to bson doc %.*s %s ... [%d]\n",
					tkey->len, tkey->s, ocmp.s, idx);
		goto error;
	}
	return 0;
error:
	return -1;
}

/*
 * Add key-value to a bson document
 */
int db_mongodb_bson_add(bson_t *doc, const db_key_t _k, const db_val_t *_v, int idx)
{
	int vtype;

	vtype = VAL_TYPE(_v);
	if(VAL_NULL(_v)) {
		if(!bson_append_null(doc, _k->s, _k->len)) {
			LM_ERR("failed to append int to bson doc %.*s = %d [%d]\n",
					_k->len, _k->s, VAL_INT(_v), idx);
			goto error;
		}
		goto done;
	}
	switch(vtype) {
		case DB1_INT:
			if(!bson_append_int32(doc, _k->s, _k->len,
						VAL_INT(_v))) {
				LM_ERR("failed to append int to bson doc %.*s = %d [%d]\n",
						_k->len, _k->s, VAL_INT(_v), idx);
				goto error;
			}
			break;

		case DB1_BIGINT:
			if(!bson_append_int64(doc, _k->s, _k->len,
						VAL_BIGINT(_v ))) {
				LM_ERR("failed to append bigint to bson doc %.*s = %lld [%d]\n",
						_k->len, _k->s, VAL_BIGINT(_v), idx);
				goto error;
			}
			return -1;

		case DB1_DOUBLE:
			if(!bson_append_double(doc, _k->s, _k->len,
						VAL_DOUBLE(_v))) {
				LM_ERR("failed to append double to bson doc %.*s = %f [%d]\n",
						_k->len, _k->s, VAL_DOUBLE(_v), idx);
				goto error;
			}
			break;

		case DB1_STRING:
			if(!bson_append_utf8(doc, _k->s, _k->len,
						VAL_STRING(_v), strlen(VAL_STRING(_v))) ) {
				LM_ERR("failed to append string to bson doc %.*s = %s [%d]\n",
						_k->len, _k->s, VAL_STRING(_v), idx);
				goto error;
			}
			break;

		case DB1_STR:

			if(!bson_append_utf8(doc, _k->s, _k->len,
						VAL_STR(_v).s, VAL_STR(_v).len) ) {
				LM_ERR("failed to append str to bson doc %.*s = %.*s [%d]\n",
						_k->len, _k->s, VAL_STR(_v).len, VAL_STR(_v).s, idx);
				goto error;
			}
			break;

		case DB1_DATETIME:
			if(!bson_append_time_t(doc, _k->s, _k->len,
						VAL_TIME(_v))) {
				LM_ERR("failed to append time to bson doc %.*s = %ld [%d]\n",
						_k->len, _k->s, VAL_TIME(_v), idx);
				goto error;
			}
			break;

		case DB1_BLOB:
			if(!bson_append_binary(doc, _k->s, _k->len,
						BSON_SUBTYPE_BINARY,
						(const uint8_t *)VAL_BLOB(_v).s, VAL_BLOB(_v).len) ) {
				LM_ERR("failed to append blob to bson doc %.*s = [bin] [%d]\n",
						_k->len, _k->s, idx);
				goto error;
			}
			break;

		case DB1_BITMAP:
			if(!bson_append_int32(doc, _k->s, _k->len,
						VAL_INT(_v))) {
				LM_ERR("failed to append bitmap to bson doc %.*s = %d [%d]\n",
						_k->len, _k->s, VAL_INT(_v), idx);
				goto error;
			}
			break;

		default:
			LM_ERR("val type [%d] not supported\n", vtype);
			return -1;
	}

done:
	return 0;
error:
	return -1;
}

/*!
 * \brief Get and convert columns from a result
 *
 * Get and convert columns from a result, fills the result structure
 * with data from the database.
 * \param _h database connection
 * \param _r database result set
 * \return 0 on success, negative on failure
 */
int db_mongodb_get_columns(const db1_con_t* _h, db1_res_t* _r)
{
	int col;
	db_mongodb_result_t *mgres;
	bson_iter_t riter;
#if MONGOC_CHECK_VERSION(1, 5, 0)
	bson_iter_t titer;
#endif
	bson_iter_t citer;
	bson_t *cdoc;
	const char *colname;
	bson_type_t coltype;
	int cdocproj;

	if ((!_h) || (!_r)) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	mgres = (db_mongodb_result_t*)RES_PTR(_r);
	if(!mgres->rdoc) {
		mgres->nrcols = 0;
		return 0;
	}
	if(mgres->nrcols==0 || mgres->colsdoc==NULL) {
		mgres->nrcols = (int)bson_count_keys(mgres->rdoc);
		if(mgres->nrcols==0) {
			LM_ERR("no keys in bson document\n");
			return -1;
		}
		cdoc = mgres->rdoc;
		cdocproj = 0;
	} else {
		cdoc = mgres->colsdoc;
		cdocproj = 1;
	}
	RES_COL_N(_r) = mgres->nrcols;
	if (!RES_COL_N(_r)) {
		LM_ERR("no columns returned from the query\n");
		return -2;
	} else {
		LM_DBG("%d columns returned from the query\n", RES_COL_N(_r));
	}

	if (db_allocate_columns(_r, RES_COL_N(_r)) != 0) {
		RES_COL_N(_r) = 0;
		LM_ERR("could not allocate columns\n");
		return -3;
	}

	if(cdocproj == 1) {
#if MONGOC_CHECK_VERSION(1, 5, 0)
		if (!bson_iter_init (&titer, cdoc)) {
			LM_ERR("failed to initialize columns iterator\n");
			return -3;
		}
		if(!bson_iter_find(&titer, "projection")
				|| !BSON_ITER_HOLDS_DOCUMENT (&titer)) {
			LM_ERR("failed to find projection field\n");
			return -3;
		}
		if(!bson_iter_recurse (&titer, &citer)) {
			LM_ERR("failed to init projection iterator\n");
			return -3;
		}
#else
		if (!bson_iter_init (&citer, cdoc)) {
			LM_ERR("failed to initialize columns iterator\n");
			return -3;
		}
#endif
	} else {
		if (!bson_iter_init (&citer, cdoc)) {
			LM_ERR("failed to initialize columns iterator\n");
			return -3;
		}
	}
	if(mgres->colsdoc) {
		if (!bson_iter_init (&riter, mgres->rdoc)) {
			LM_ERR("failed to initialize result iterator\n");
			return -3;
		}
	}

	col = 0;
	while (bson_iter_next (&citer)) {
		if(col >= RES_COL_N(_r)) {
			LM_ERR("invalid number of columns (%d/%d)\n", col, RES_COL_N(_r));
			return -4;
		}

		colname = bson_iter_key (&citer);
		LM_DBG("Found a field[%d] named: %s\n", col, colname);
		if(mgres->colsdoc) {
			if(!bson_iter_find(&riter, colname)) {
				/* re-init the iterator */
				if (!bson_iter_init (&riter, mgres->rdoc)) {
					LM_ERR("failed to initialize result iterator\n");
					return -3;
				}
				if(!bson_iter_find(&riter, colname)) {
					LM_ERR("field [%s] not found in result iterator\n",
							colname);
					return -4;
				}
			}
			coltype = bson_iter_type(&riter);
		} else {
			coltype = bson_iter_type(&citer);
		}

		RES_NAMES(_r)[col] = (str*)pkg_malloc(sizeof(str));
		if (! RES_NAMES(_r)[col]) {
			LM_ERR("no private memory left\n");
			db_free_columns(_r);
			return -4;
		}
		LM_DBG("allocate %lu bytes for RES_NAMES[%d] at %p\n",
				(unsigned long)sizeof(str), col, RES_NAMES(_r)[col]);

		/* pointer linked here is part of the result structure */
		RES_NAMES(_r)[col]->s = (char*)colname;
		RES_NAMES(_r)[col]->len = strlen(colname);

		switch(coltype) {
			case BSON_TYPE_BOOL:
			case BSON_TYPE_INT32:
			case BSON_TYPE_TIMESTAMP:
				LM_DBG("use DB1_INT result type\n");
				RES_TYPES(_r)[col] = DB1_INT;
				break;

			case BSON_TYPE_INT64:
				LM_DBG("use DB1_BIGINT result type\n");
				RES_TYPES(_r)[col] = DB1_BIGINT;
				break;

			case BSON_TYPE_DOUBLE:
				LM_DBG("use DB1_DOUBLE result type\n");
				RES_TYPES(_r)[col] = DB1_DOUBLE;
				break;

			case BSON_TYPE_DATE_TIME:
				LM_DBG("use DB1_DATETIME result type\n");
				RES_TYPES(_r)[col] = DB1_DATETIME;
				break;

			case BSON_TYPE_BINARY:
				LM_DBG("use DB1_BLOB result type\n");
				RES_TYPES(_r)[col] = DB1_BLOB;
				break;

			case BSON_TYPE_UTF8:
				LM_DBG("use DB1_STRING result type\n");
				RES_TYPES(_r)[col] = DB1_STRING;
				break;

#if 0
			case BSON_TYPE_EOD:
			case BSON_TYPE_DOCUMENT:
			case BSON_TYPE_ARRAY:
			case BSON_TYPE_UNDEFINED:
			case BSON_TYPE_OID:
			case BSON_TYPE_NULL:
			case BSON_TYPE_REGEX:
			case BSON_TYPE_DBPOINTER:
			case BSON_TYPE_CODE:
			case BSON_TYPE_SYMBOL:
			case BSON_TYPE_CODEWSCOPE:
			case BSON_TYPE_MAXKEY:
			case BSON_TYPE_MINKEY:
#endif

			default:
				LM_INFO("unhandled data type column (%.*s) type id (%d), "
						"use DB1_STRING as default\n", RES_NAMES(_r)[col]->len,
						RES_NAMES(_r)[col]->s, coltype);
				RES_TYPES(_r)[col] = DB1_STRING;
				break;
		}

		LM_DBG("RES_NAMES(%p)[%d]=[%.*s] (%d)\n", RES_NAMES(_r)[col], col,
				RES_NAMES(_r)[col]->len, RES_NAMES(_r)[col]->s, coltype);
		col++;
	}
	return 0;
}


/*!
 * \brief Convert rows from mongodb to db API representation
 * \param _h database connection
 * \param _r database result set
 * \return 0 on success, negative on failure
 */
static int db_mongodb_convert_bson(const db1_con_t* _h, db1_res_t* _r,
		int _row, const bson_t *_rdoc)
{
	static str dummy_string = {"", 0};
	int col;
	db_mongodb_result_t *mgres;
	const char *colname;
	bson_type_t coltype;
	bson_iter_t riter;
#if MONGOC_CHECK_VERSION(1, 5, 0)
	bson_iter_t titer;
#endif
	bson_iter_t citer;
	bson_iter_t *piter;
	db_val_t* dval;
	uint32_t i32tmp;
    bson_subtype_t subtype;
	bson_t *cdoc;

	mgres = (db_mongodb_result_t*)RES_PTR(_r);
	if(mgres->nrcols==0) {
		LM_ERR("no fields to convert\n");
		return -1;
	}
	if(mgres->colsdoc==NULL) {
		cdoc = (bson_t*)_rdoc;
		if (!bson_iter_init (&citer, cdoc)) {
			LM_ERR("failed to initialize columns iterator\n");
			return -3;
		}
	} else {
		cdoc = (bson_t*)mgres->colsdoc;
#if MONGOC_CHECK_VERSION(1, 5, 0)
		if (!bson_iter_init (&titer, cdoc)) {
			LM_ERR("failed to initialize columns iterator\n");
			return -3;
		}
		if(!bson_iter_find(&titer, "projection")
				|| !BSON_ITER_HOLDS_DOCUMENT (&titer)) {
			LM_ERR("failed to find projection field\n");
			return -3;
		}
		if(!bson_iter_recurse (&titer, &citer)) {
			LM_ERR("failed to init projection iterator\n");
			return -3;
		}
#else
		if (!bson_iter_init (&citer, cdoc)) {
			LM_ERR("failed to initialize columns iterator\n");
			return -3;
		}
#endif
	}

	if(mgres->colsdoc) {
		if (!bson_iter_init (&riter, _rdoc)) {
			LM_ERR("failed to initialize result iterator\n");
			return -3;
		}
	}
	if (db_allocate_row(_r, &(RES_ROWS(_r)[_row])) != 0) {
		LM_ERR("could not allocate row: %d\n", _row);
		return -2;
	}
	col = 0;
	while (bson_iter_next (&citer)) {
		if(col >= RES_COL_N(_r)) {
			LM_ERR("invalid number of columns (%d/%d)\n", col, RES_COL_N(_r));
			return -4;
		}

		colname = bson_iter_key (&citer);
		LM_DBG("looking for field[%d] named: %s\n", col, colname);
		if(mgres->colsdoc) {
			if(!bson_iter_find(&riter, colname)) {
				if (!bson_iter_init (&riter, _rdoc)) {
					LM_ERR("failed to initialize result iterator\n");
					return -3;
				}
				if(!bson_iter_find(&riter, colname)) {
					LM_ERR("field [%s] not found in result iterator\n",
						colname);
					return -4;
				}
			}
			piter = &riter;
		} else {
			piter = &citer;
		}
		coltype = bson_iter_type(piter);

		dval = &(ROW_VALUES(&(RES_ROWS(_r)[_row]))[col]);
		VAL_TYPE(dval) = RES_TYPES(_r)[col];

		switch(coltype) {
			case BSON_TYPE_BOOL:
				VAL_INT(dval) = (int)bson_iter_bool (piter);
				break;
			case BSON_TYPE_INT32:
				VAL_INT(dval) = bson_iter_int32 (piter);
				break;
			case BSON_TYPE_TIMESTAMP:
				bson_iter_timestamp (piter,
						(uint32_t*)&VAL_INT(dval), &i32tmp);
				break;

			case BSON_TYPE_INT64:
				VAL_BIGINT(dval) = bson_iter_int64 (piter);
				break;

			case BSON_TYPE_DOUBLE:
				VAL_DOUBLE(dval) = bson_iter_double (piter);
				break;

			case BSON_TYPE_DATE_TIME:
				VAL_TIME(dval) = (time_t)(bson_iter_date_time (piter)/1000);
				break;

			case BSON_TYPE_BINARY:
				bson_iter_binary (piter, &subtype,
                  (uint32_t*)&VAL_BLOB(dval).len, (const uint8_t**)&VAL_BLOB(dval).s);
				break;

			case BSON_TYPE_UTF8:
				{
					char* rstring = (char*)bson_iter_utf8 (piter, &i32tmp);
					if(db_str2val(DB1_STRING, dval, rstring, i32tmp, 1)<0) {
						LM_ERR("failed to convert utf8 value\n");
						return -5;
					}
				}
				break;

			case BSON_TYPE_OID:
				break;

			case BSON_TYPE_NULL:
				memset(dval, 0, sizeof(db_val_t));
				/* Initialize the string pointers to a dummy empty
				 * string so that we do not crash when the NULL flag
				 * is set but the module does not check it properly
				 */
				VAL_STRING(dval) = dummy_string.s;
				VAL_STR(dval) = dummy_string;
				VAL_BLOB(dval) = dummy_string;
				VAL_TYPE(dval) = RES_TYPES(_r)[col];
				VAL_NULL(dval) = 1;
				break;

#if 0
			case BSON_TYPE_EOD:
			case BSON_TYPE_DOCUMENT:
			case BSON_TYPE_ARRAY:
			case BSON_TYPE_UNDEFINED:
			case BSON_TYPE_REGEX:
			case BSON_TYPE_DBPOINTER:
			case BSON_TYPE_CODE:
			case BSON_TYPE_SYMBOL:
			case BSON_TYPE_CODEWSCOPE:
			case BSON_TYPE_MAXKEY:
			case BSON_TYPE_MINKEY:
#endif

			default:
				LM_WARN("unhandled data type column (%.*s) type id (%d), "
						"use DB1_STRING as default\n", RES_NAMES(_r)[col]->len,
						RES_NAMES(_r)[col]->s, coltype);
				RES_TYPES(_r)[col] = DB1_STRING;
				break;
		}

		LM_DBG("RES_NAMES(%p)[%d]=[%.*s] (%d)\n", RES_NAMES(_r)[col], col,
				RES_NAMES(_r)[col]->len, RES_NAMES(_r)[col]->s, coltype);
		col++;
	}
	return 0;
}

/*!
 * \brief Convert rows from mongodb to db API representation
 * \param _h database connection
 * \param _r database result set
 * \return 0 on success, negative on failure
 */
static int db_mongodb_convert_result(const db1_con_t* _h, db1_res_t* _r)
{
	int row;
	db_mongodb_result_t *mgres;
	const bson_t *itdoc;
	char *jstr;

	if ((!_h) || (!_r)) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	mgres = (db_mongodb_result_t*)RES_PTR(_r);
	if(!mgres->rdoc) {
		mgres->nrcols = 0;
		return 0;
	}
	if(mgres->nrcols==0) {
		LM_DBG("no fields to return\n");
		return 0;
	}

	if(!mongoc_cursor_more (mgres->cursor)) {
		RES_ROW_N(_r) = 1;
		mgres->maxrows = 1;
	} else {
		RES_ROW_N(_r) = DB_MONGODB_ROWS_STEP;
		mgres->maxrows = DB_MONGODB_ROWS_STEP;
	}

	if (db_allocate_rows(_r) < 0) {
		LM_ERR("could not allocate rows\n");
		RES_ROW_N(_r) = 0;
		return -2;
	}

	itdoc = mgres->rdoc;
	row = 0;
	do {
		if(row >= RES_ROW_N(_r)) {
			if (db_reallocate_rows(_r,
						RES_ROW_N(_r)+DB_MONGODB_ROWS_STEP) < 0) {
				LM_ERR("could not reallocate rows\n");
				return -2;
			}
			mgres->maxrows = RES_ROW_N(_r);
		}
		if(is_printable(L_DBG)) {
			jstr = bson_as_json (itdoc, NULL);
			LM_DBG("selected document: %s\n", jstr);
			bson_free (jstr);
		}
		if(db_mongodb_convert_bson(_h, _r, row, itdoc)) {
			LM_ERR("failed to convert bson at pos %d\n", row);
			return -1;
		}
		row++;
	} while (mongoc_cursor_more (mgres->cursor)
			&& mongoc_cursor_next (mgres->cursor, &itdoc));
	RES_ROW_N(_r) = row;
	LM_DBG("retrieved number of rows: %d\n", row);
	return 0;
}

db1_res_t* db_mongodb_new_result(void)
{
	db1_res_t* obj;

	obj = db_new_result();
	if (!obj)
		return NULL;
	RES_PTR(obj) = pkg_malloc(sizeof(db_mongodb_result_t));
	if (!RES_PTR(obj)) {
		db_free_result(obj);
		return NULL;
	}
	memset(RES_PTR(obj), 0, sizeof(db_mongodb_result_t));
	return obj;
}
/*
 * Retrieve result set
 */
static int db_mongodb_store_result(const db1_con_t* _h, db1_res_t** _r)
{
	km_mongodb_con_t *mgcon;
	db_mongodb_result_t *mgres;
	const bson_t *itdoc;
	bson_error_t error;

	mgcon = MONGODB_CON(_h);
	if(!_r) {
		LM_ERR("invalid result parameter\n");
		return -1;
	}

	*_r = db_mongodb_new_result();
	if (!*_r)  {
		LM_ERR("no memory left for result \n");
		goto error;
	}
	mgres = (db_mongodb_result_t*)RES_PTR(*_r);
	mgres->collection = mgcon->collection;
	mgcon->collection = NULL;
	mgres->cursor = mgcon->cursor;
	mgcon->cursor = NULL;
	mgres->colsdoc = mgcon->colsdoc;
	mgcon->colsdoc = NULL;
	mgres->nrcols = mgcon->nrcols;
	mgcon->nrcols = 0;
	if(!mongoc_cursor_more (mgres->cursor)
			|| !mongoc_cursor_next (mgres->cursor, &itdoc)
			|| !itdoc) {
		if (mongoc_cursor_error (mgres->cursor, &error)) {
			LM_DBG("An error occurred: %s\n", error.message);
		} else {
			LM_DBG("no result from mongodb\n");
		}
		return 0;
	}
	/* first document linked internally in result to get columns */
	mgres->rdoc = (bson_t*)itdoc;
	if(db_mongodb_get_columns(_h, *_r)<0) {
		LM_ERR("failed to set the columns\n");
		goto error;
	}
	if(db_mongodb_convert_result(_h, *_r)<0) {
		LM_ERR("failed to set the rows in result\n");
		goto error;
	}
	return 0;

error:
	if(mgcon->colsdoc) {
		bson_destroy (mgcon->colsdoc);
		mgcon->colsdoc = NULL;
	}
	mgcon->nrcols = 0;
	if(mgcon->cursor) {
		mongoc_cursor_destroy (mgcon->cursor);
		mgcon->cursor = NULL;
	}
	if(mgcon->collection) {
		mongoc_collection_destroy (mgcon->collection);
		mgcon->collection = NULL;
	}
	return -1;
}

/*
 * Release a result set from memory
 */
int db_mongodb_free_result(db1_con_t* _h, db1_res_t* _r)
{
	if(!_r)
		return -1;
	if(RES_PTR(_r)) {
		if(((db_mongodb_result_t*)RES_PTR(_r))->rdoc) {
			bson_destroy(((db_mongodb_result_t*)RES_PTR(_r))->rdoc);
			((db_mongodb_result_t*)RES_PTR(_r))->rdoc = NULL;
		}
		if(((db_mongodb_result_t*)RES_PTR(_r))->colsdoc) {
			bson_destroy (((db_mongodb_result_t*)RES_PTR(_r))->colsdoc);
			((db_mongodb_result_t*)RES_PTR(_r))->colsdoc = NULL;
		}
		((db_mongodb_result_t*)RES_PTR(_r))->nrcols = 0;
		if(((db_mongodb_result_t*)RES_PTR(_r))->cursor) {
			mongoc_cursor_destroy (((db_mongodb_result_t*)RES_PTR(_r))->cursor);
			((db_mongodb_result_t*)RES_PTR(_r))->cursor = NULL;
		}
		if(((db_mongodb_result_t*)RES_PTR(_r))->collection) {
			mongoc_collection_destroy (((db_mongodb_result_t*)RES_PTR(_r))->collection);
			((db_mongodb_result_t*)RES_PTR(_r))->collection = NULL;
		}
		pkg_free(RES_PTR(_r));
	}
	db_free_result(_r);
	return 0;
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
int db_mongodb_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
		const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,
		const db_key_t _o, db1_res_t** _r)
{
	int i;
	km_mongodb_con_t *mgcon;
	mongoc_client_t *client;
	bson_t *seldoc = NULL;
#if MONGOC_CHECK_VERSION(1, 5, 0)
	bson_t bcols;
#endif
	char *cname;
	char b1;
	char *jstr;

	mgcon = MONGODB_CON(_h);
	if(mgcon==NULL || mgcon->id== NULL || mgcon->con==NULL) {
		LM_ERR("connection to server is null\n");
		return -1;
	}
	if(mgcon->collection) {
		mongoc_collection_destroy (mgcon->collection);
		mgcon->collection = NULL;
	}
	if(mgcon->cursor) {
		mongoc_cursor_destroy (mgcon->cursor);
		mgcon->cursor = NULL;
	}
	if(mgcon->colsdoc) {
		bson_destroy (mgcon->colsdoc);
		mgcon->colsdoc = NULL;
	}
	mgcon->nrcols = 0;
	client = mgcon->con;
	if(CON_TABLE(_h)->s==NULL) {
		LM_ERR("collection (table) name not set\n");
		return -1;
	}

	if(_r) *_r = NULL;

	b1 = '\0';
	if(CON_TABLE(_h)->s[CON_TABLE(_h)->len]!='\0') {
		b1 = CON_TABLE(_h)->s[CON_TABLE(_h)->len];
		CON_TABLE(_h)->s[CON_TABLE(_h)->len] = '\0';
	}
	cname = CON_TABLE(_h)->s;
	LM_DBG("query to collection [%s]\n", cname);
	mgcon->collection = mongoc_client_get_collection(client, mgcon->id->database, cname);
	if(mgcon->collection==NULL) {
		LM_ERR("cannot get collection (table): %s\n", cname);
		if(b1 != '\0') CON_TABLE(_h)->s[CON_TABLE(_h)->len] = b1;
		return -1;
	}
	if(b1 != '\0') CON_TABLE(_h)->s[CON_TABLE(_h)->len] = b1;

	seldoc = bson_new();
	if(seldoc==NULL) {
		LM_ERR("cannot initialize query bson document\n");
		goto error;
	}

	if(_op==NULL) {
		for(i = 0; i < _n; i++) {
			if(db_mongodb_bson_add(seldoc, _k[i], _v+i, i)<0)
				goto error;
		}
	} else {
		for(i = 0; i < _n; i++) {
			if(!strcmp(_op[i], OP_EQ)) {
				if(db_mongodb_bson_add(seldoc, _k[i], _v+i, i)<0)
					goto error;
			} else {
				if(db_mongodb_bson_filter_add(seldoc, _k, _op, _v, i)<0)
					goto error;
			}
		}
	}

	if(is_printable(L_DBG)) {
		jstr = bson_as_json (seldoc, NULL);
		LM_DBG("query filter: %s\n", jstr);
		bson_free (jstr);
	}
	if(_nc > 0) {
		mgcon->colsdoc = bson_new();
		if(mgcon->colsdoc==NULL) {
			LM_ERR("cannot initialize columns bson document\n");
			goto error;
		}
#if MONGOC_CHECK_VERSION(1, 5, 0)
		if(!bson_append_document_begin (mgcon->colsdoc, "projection", 10,
					&bcols)) {
			LM_ERR("failed to start projection of fields\n");
			goto error;
		}
#endif
		for(i = 0; i < _nc; i++) {
#if MONGOC_CHECK_VERSION(1, 5, 0)
			if(!bson_append_int32(&bcols, _c[i]->s, _c[i]->len, 1))
#else
			if(!bson_append_int32(mgcon->colsdoc, _c[i]->s, _c[i]->len, 1))
#endif
			{
				LM_ERR("failed to append int to columns bson %.*s = %d [%d]\n",
						_c[i]->len, _c[i]->s, 1, i);
				goto error;
			}
		}
#if MONGOC_CHECK_VERSION(1, 5, 0)
		if(!bson_append_document_end (mgcon->colsdoc, &bcols)) {
			LM_ERR("failed to end projection of fields\n");
			goto error;
		}
#endif
		if(is_printable(L_DBG)) {
			jstr = bson_as_json (mgcon->colsdoc, NULL);
			LM_DBG("columns filter: %s\n", jstr);
			bson_free (jstr);
		}
		mgcon->nrcols = _nc;
	}
	#if MONGOC_CHECK_VERSION(1, 5, 0)
	mgcon->cursor = mongoc_collection_find_with_opts (mgcon->collection,
						seldoc, mgcon->colsdoc, NULL);
	#else
	mgcon->cursor = mongoc_collection_find (mgcon->collection,
						MONGOC_QUERY_NONE, 0, 0, 0,
						seldoc, mgcon->colsdoc, NULL);
	#endif
	if(!_r) {
		goto done;
	}

	if(db_mongodb_store_result(_h, _r)<0) {
		LM_ERR("failed to store result\n");
		goto error;
	}

done:
	bson_destroy (seldoc);
	return 0;

error:
	LM_ERR("failed to do the query\n");
	if(seldoc) bson_destroy (seldoc);
	if(mgcon->colsdoc) {
		bson_destroy (mgcon->colsdoc);
		mgcon->colsdoc = NULL;
	}
	mgcon->nrcols = 0;
	if(mgcon->collection) {
		mongoc_collection_destroy (mgcon->collection);
		mgcon->collection = NULL;
	}
	if(mgcon->cursor) {
		mongoc_cursor_destroy (mgcon->cursor);
		mgcon->cursor = NULL;
	}
	if(_r && *_r) { db_mongodb_free_result((db1_con_t*)_h, *_r); *_r = NULL; }
	return -1;
}

/*!
 * \brief Gets a partial result set, fetch rows from a result
 *
 * Gets a partial result set, fetch a number of rows from a databae result.
 * This function initialize the given result structure on the first run, and
 * fetches the nrows number of rows. On subsequenting runs, it uses the
 * existing result and fetches more rows, until it reaches the end of the
 * result set. Because of this the result needs to be null in the first
 * invocation of the function. If the number of wanted rows is zero, the
 * function returns anything with a result of zero.
 * \param _h structure representing the database connection
 * \param _r pointer to a structure representing the result
 * \param nrows number of fetched rows
 * \return return zero on success, negative value on failure
 */
int db_mongodb_fetch_result(const db1_con_t* _h, db1_res_t** _r, const int nrows)
{
	return -1;
}


/*
 * Execute a raw SQL query
 */
int db_mongodb_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r)
{
	return -1;
}

/*
 * Insert a row into specified table
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys
 * _n: number of key=value pairs
 */
int db_mongodb_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v, const int _n)
{
	int i;
	km_mongodb_con_t *mgcon;
	mongoc_client_t *client;
	mongoc_collection_t *collection = NULL;
	bson_error_t error;
	bson_t *doc = NULL;
	char *cname;
	char *jstr;
	char b1;

	mgcon = MONGODB_CON(_h);
	if(mgcon==NULL || mgcon->id== NULL || mgcon->con==NULL) {
		LM_ERR("connection to server is null\n");
		return -1;
	}
	client = mgcon->con;
	if(CON_TABLE(_h)->s==NULL) {
		LM_ERR("collection (table) name not set\n");
		return -1;
	}
	b1 = '\0';
	if(CON_TABLE(_h)->s[CON_TABLE(_h)->len]!='\0') {
		b1 = CON_TABLE(_h)->s[CON_TABLE(_h)->len];
		CON_TABLE(_h)->s[CON_TABLE(_h)->len] = '\0';
	}
	cname = CON_TABLE(_h)->s;
	collection = mongoc_client_get_collection(client, mgcon->id->database, cname);
	if(collection==NULL) {
		LM_ERR("cannot get collection (table): %s\n", cname);
		if(b1 != '\0') CON_TABLE(_h)->s[CON_TABLE(_h)->len] = b1;
		return -1;
	}
	if(b1 != '\0') CON_TABLE(_h)->s[CON_TABLE(_h)->len] = b1;

	doc = bson_new();
	if(doc==NULL) {
		LM_ERR("cannot initialize bson document\n");
		goto error;
	}

	for(i = 0; i < _n; i++) {
		if(db_mongodb_bson_add(doc, _k[i], _v+i, i)<0)
			goto error;
	}
	if(is_printable(L_DBG)) {
		jstr = bson_as_json (doc, NULL);
		LM_DBG("insert document: %s\n", jstr);
		bson_free (jstr);
	}

	if (!mongoc_collection_insert (collection, MONGOC_INSERT_NONE, doc, NULL, &error)) {
		LM_ERR("failed to insert in collection: %s\n", error.message);
		goto error;
	}
	bson_destroy (doc);
	mongoc_collection_destroy (collection);
	
	return 0;
error:
	if(doc) bson_destroy (doc);
	if(collection) mongoc_collection_destroy (collection);
	return -1;
}

/*
 * Delete a row from the specified table
 * _h: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _n: number of key=value pairs
 */
int db_mongodb_delete(const db1_con_t* _h, const db_key_t* _k,
		const db_op_t* _o, const db_val_t* _v, const int _n)
{
	int i;
	km_mongodb_con_t *mgcon;
	mongoc_client_t *client;
	mongoc_collection_t *collection = NULL;
	bson_error_t error;
	bson_t *doc = NULL;
	char *cname;
	char *jstr;
	char b1;

	mgcon = MONGODB_CON(_h);
	if(mgcon==NULL || mgcon->id== NULL || mgcon->con==NULL) {
		LM_ERR("connection to server is null\n");
		return -1;
	}
	client = mgcon->con;
	if(CON_TABLE(_h)->s==NULL) {
		LM_ERR("collection (table) name not set\n");
		return -1;
	}
	b1 = '\0';
	if(CON_TABLE(_h)->s[CON_TABLE(_h)->len]!='\0') {
		b1 = CON_TABLE(_h)->s[CON_TABLE(_h)->len];
		CON_TABLE(_h)->s[CON_TABLE(_h)->len] = '\0';
	}
	cname = CON_TABLE(_h)->s;
	collection = mongoc_client_get_collection(client, mgcon->id->database,
			cname);
	if(collection==NULL) {
		LM_ERR("cannot get collection (table): %s\n", cname);
		if(b1 != '\0') CON_TABLE(_h)->s[CON_TABLE(_h)->len] = b1;
		return -1;
	}
	if(b1 != '\0') CON_TABLE(_h)->s[CON_TABLE(_h)->len] = b1;

	doc = bson_new();
	if(doc==NULL) {
		LM_ERR("cannot initialize bson document\n");
		goto error;
	}

	if(_o==NULL) {
		for(i = 0; i < _n; i++) {
			if(db_mongodb_bson_add(doc, _k[i], _v+i, i)<0)
				goto error;
		}
	} else {
		for(i = 0; i < _n; i++) {
			if(!strcmp(_o[i], OP_EQ)) {
				if(db_mongodb_bson_add(doc, _k[i], _v+i, i)<0)
					goto error;
			} else {
				if(db_mongodb_bson_filter_add(doc, _k, _o, _v, i)<0)
					goto error;
			}
		}
	}

	if(is_printable(L_DBG)) {
		jstr = bson_as_json (doc, NULL);
		LM_DBG("delete filter document: %s\n", jstr);
		bson_free (jstr);
	}
	if (!mongoc_collection_remove (collection, MONGOC_REMOVE_NONE,
				doc, NULL, &error)) {
		LM_ERR("failed to delete in collection: %s\n", error.message);
		goto error;
	}
	bson_destroy (doc);
	mongoc_collection_destroy (collection);
	
	return 0;
error:
	if(doc) bson_destroy (doc);
	if(collection) mongoc_collection_destroy (collection);
	return -1;
}

/*
 * Update some rows in the specified table
 * _h: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _uk: updated columns
 * _uv: updated values of the columns
 * _n: number of key=value pairs
 * _un: number of columns to update
 */
int db_mongodb_update(const db1_con_t* _h, const db_key_t* _k,
		const db_op_t* _o, const db_val_t* _v, const db_key_t* _uk,
		const db_val_t* _uv, const int _n, const int _un)
{
	int i;
	km_mongodb_con_t *mgcon;
	mongoc_client_t *client;
	mongoc_collection_t *collection = NULL;
	bson_error_t error;
	bson_t *mdoc = NULL;
	bson_t *udoc = NULL, *sdoc = NULL;
	char *cname;
	char b1;

	mgcon = MONGODB_CON(_h);
	if(mgcon==NULL || mgcon->id== NULL || mgcon->con==NULL) {
		LM_ERR("connection to server is null\n");
		return -1;
	}
	client = mgcon->con;
	if(CON_TABLE(_h)->s==NULL) {
		LM_ERR("collection (table) name not set\n");
		return -1;
	}
	b1 = '\0';
	if(CON_TABLE(_h)->s[CON_TABLE(_h)->len]!='\0') {
		b1 = CON_TABLE(_h)->s[CON_TABLE(_h)->len];
		CON_TABLE(_h)->s[CON_TABLE(_h)->len] = '\0';
	}
	cname = CON_TABLE(_h)->s;
	collection = mongoc_client_get_collection(client, mgcon->id->database,
			cname);
	if(collection==NULL) {
		LM_ERR("cannot get collection (table): %s\n", cname);
		if(b1 != '\0') CON_TABLE(_h)->s[CON_TABLE(_h)->len] = b1;
		return -1;
	}
	if(b1 != '\0') CON_TABLE(_h)->s[CON_TABLE(_h)->len] = b1;

	udoc = bson_new();
	if(udoc==NULL) {
		LM_ERR("cannot initialize update bson document\n");
		goto error;
	}
	sdoc = bson_new();
	if(sdoc==NULL) {
		LM_ERR("cannot initialize update bson document\n");
		goto error;
	}
	mdoc = bson_new();
	if(mdoc==NULL) {
		LM_ERR("cannot initialize match bson document\n");
		goto error;
	}

	for(i = 0; i < _un; i++) {
		if(db_mongodb_bson_add(sdoc, _uk[i], _uv+i, i)<0)
			goto error;
	}
	if(!bson_append_document(udoc, "$set", 4, sdoc)) {
                LM_ERR("failed to append document to bson document\n");
                goto error;
        }

	if(_o==NULL) {
		for(i = 0; i < _n; i++) {
			if(db_mongodb_bson_add(mdoc, _k[i], _v+i, i)<0)
				goto error;
		}
	} else {
		for(i = 0; i < _n; i++) {
			if(!strcmp(_o[i], OP_EQ)) {
				if(db_mongodb_bson_add(mdoc, _k[i], _v+i, i)<0)
					goto error;
			} else {
				if(db_mongodb_bson_filter_add(mdoc, _k, _o, _v, i)<0)
					goto error;
			}
		}
	}

	if (!mongoc_collection_update (collection, MONGOC_UPDATE_NONE, mdoc,
				udoc, NULL, &error)) {
		LM_ERR("failed to update in collection: %s\n", error.message);
		goto error;
	}
	bson_destroy (mdoc);
	bson_destroy (udoc);
	bson_destroy (sdoc);
	mongoc_collection_destroy (collection);
	
	return 0;
error:
	if(mdoc) bson_destroy (mdoc);
	if(udoc) bson_destroy (udoc);
	if(sdoc) bson_destroy (sdoc);
	if(collection) mongoc_collection_destroy (collection);
	return -1;
}

/*
 * Just like insert, but replace the row if it exists
 */
int db_mongodb_replace(const db1_con_t* _h, const db_key_t* _k,
		const db_val_t* _v, const int _n, const int _un, const int _m)
{
	return -1;
}

/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_mongodb_use_table(db1_con_t* _h, const str* _t)
{
	return db_use_table(_h, _t);
}
