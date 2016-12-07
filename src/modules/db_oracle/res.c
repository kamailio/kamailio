/*
 * $Id$
 *
 * Oracle module result related functions
 *
 * Copyright (C) 2007,2008 TRUNK MOBILE
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
#include <time.h>
#include <oci.h>
#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db_row.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "ora_con.h"
#include "dbase.h"
#include "asynch.h"
#include "res.h"


#define MAX_DEF_HANDLES 64

struct dmap {
    OCIDefine* defh[MAX_DEF_HANDLES];
    union {
	dvoid* v;
	double* f;
	int* i;
	char* c;
	OCIDate* o;
    }pv[MAX_DEF_HANDLES];
    dvoid* pval[MAX_DEF_HANDLES];
    ub2 ilen[MAX_DEF_HANDLES];
    sb2 ind[MAX_DEF_HANDLES];
    ub2 len[MAX_DEF_HANDLES];
};
typedef struct dmap dmap_t;


/*
 * Get and convert columns from a result. Define handlers and buffers
 */
static int get_columns(ora_con_t* con, db1_res_t* _r, OCIStmt* _c, dmap_t* _d)
{
	OCIParam *param;
	size_t tsz;
	ub4 i, n;
	sword status;

	status = OCIAttrGet(_c, OCI_HTYPE_STMT, &n, NULL, OCI_ATTR_PARAM_COUNT,
		con->errhp);

	if (status != OCI_SUCCESS) {
		LM_ERR("driver: %s\n", db_oracle_error(con, status));
		return -1;
	}

	if (!n) {
		LM_ERR("no columns\n");
		return -2;
	}

	if (n >= MAX_DEF_HANDLES) {
		LM_ERR("too many res. Rebuild with MAX_DEF_HANDLES >= %u\n", n);
		return -3;
	}

	if (db_allocate_columns(_r, n) != 0) {
		LM_ERR("could not allocate columns");
		return -4;
	}
	memset(RES_NAMES(_r), 0, sizeof(db_key_t) * n);

	RES_COL_N(_r) = n;

	tsz = 0;
	memset(_d->defh, 0, sizeof(_d->defh[0]) * n);
	for (i = 0; i < n; i++) {
		ub4 len;
		ub2 dtype;

		status = OCIParamGet(_c, OCI_HTYPE_STMT, con->errhp,
			(dvoid**)(dvoid*)&param, i+1);
		if (status != OCI_SUCCESS) goto ora_err;

		{
			text* name;
			str* sname;
			status = OCIAttrGet(param, OCI_DTYPE_PARAM,
				(dvoid**)(dvoid*)&name,	&len, OCI_ATTR_NAME,
				con->errhp);
			if (status != OCI_SUCCESS) goto ora_err;
			sname = (str*)pkg_malloc(sizeof(str)+len+1);
			if (!sname) {
				db_free_columns(_r);
				LM_ERR("no private memory left\n");
				return -5;
			}
			sname->len = len;
			sname->s = (char*)sname + sizeof(str);
			memcpy(sname->s, name, len);
			sname->s[len] = '\0';
			RES_NAMES(_r)[i] = sname;
		}

		status = OCIAttrGet(param, OCI_DTYPE_PARAM,
			(dvoid**)(dvoid*)&dtype, NULL, OCI_ATTR_DATA_TYPE,
			con->errhp);
		if (status != OCI_SUCCESS) goto ora_err;

		switch (dtype) {
		case SQLT_UIN:		/* unsigned integer */
set_bitmap:
			LM_DBG("use DB1_BITMAP type");
			RES_TYPES(_r)[i] = DB1_BITMAP;
			len = sizeof(VAL_BITMAP((db_val_t*)NULL));
			break;

		case SQLT_INT:		/* (ORANET TYPE) integer */
set_int:
			LM_DBG("use DB1_INT result type");
			RES_TYPES(_r)[i] = DB1_INT;
			len = sizeof(VAL_INT((db_val_t*)NULL));
			break;

//		case SQLT_LNG:		/* long */
		case SQLT_VNU:		/* NUM with preceding length byte */
		case SQLT_NUM:		/* (ORANET TYPE) oracle numeric */
			len = 0; /* PRECISION is ub1 */
			status = OCIAttrGet(param, OCI_DTYPE_PARAM,
				(dvoid**)(dvoid*)&len, NULL, OCI_ATTR_PRECISION,
				con->errhp);
			if (status != OCI_SUCCESS) goto ora_err;
			if (len <= 11) {
				sb1 sc;
				status = OCIAttrGet(param, OCI_DTYPE_PARAM,
					(dvoid**)(dvoid*)&sc, NULL,
					OCI_ATTR_SCALE, con->errhp);
				if (status != OCI_SUCCESS) goto ora_err;
				if (!sc) {
					dtype = SQLT_INT;
					if (len != 11) goto set_int;
					dtype = SQLT_UIN;
					goto set_bitmap;
				}
			}
		case SQLT_FLT:		/* (ORANET TYPE) Floating point number */
		case SQLT_BFLOAT:       /* Native Binary float*/
		case SQLT_BDOUBLE:	/* NAtive binary double */
		case SQLT_IBFLOAT:	/* binary float canonical */
		case SQLT_IBDOUBLE:	/* binary double canonical */
		case SQLT_PDN:		/* (ORANET TYPE) Packed Decimal Numeric */
			LM_DBG("use DB1_DOUBLE result type");
			RES_TYPES(_r)[i] = DB1_DOUBLE;
			len = sizeof(VAL_DOUBLE((db_val_t*)NULL));
			dtype = SQLT_FLT;
			break;

//		case SQLT_TIME:		/* TIME */
//		case SQLT_TIME_TZ:	/* TIME WITH TIME ZONE */
		case SQLT_DATE:		/* ANSI Date */
		case SQLT_DAT:		/* date in oracle format */
		case SQLT_ODT:		/* OCIDate type */
		case SQLT_TIMESTAMP:	/* TIMESTAMP */
		case SQLT_TIMESTAMP_TZ:	/* TIMESTAMP WITH TIME ZONE */
		case SQLT_TIMESTAMP_LTZ:/* TIMESTAMP WITH LOCAL TZ */
//		case SQLT_INTERVAL_YM:	/* INTERVAL YEAR TO MONTH */
//		case SQLT_INTERVAL_DS:	/* INTERVAL DAY TO SECOND */
			LM_DBG("use DB1_DATETIME result type");
			RES_TYPES(_r)[i] = DB1_DATETIME;
			len = sizeof(OCIDate);
			dtype = SQLT_ODT;
			break;

		case SQLT_CLOB:		/* character lob */
		case SQLT_BLOB:		/* binary lob */
//		case SQLT_BFILEE:	/* binary file lob */
//		case SQLT_CFILEE:	/* character file lob */
//		case SQLT_BIN:		/* binary data(DTYBIN) */
//		case SQLT_LBI:		/* long binary */
			LM_DBG("use DB1_BLOB result type");
			RES_TYPES(_r)[i] = DB1_BLOB;
			goto dyn_str;

		case SQLT_CHR:		/* (ORANET TYPE) character string */
		case SQLT_STR:		/* zero terminated string */
		case SQLT_VST:		/* OCIString type */
		case SQLT_VCS:		/* Variable character string */
		case SQLT_AFC:		/* Ansi fixed char */
		case SQLT_AVC:		/* Ansi Var char */
//		case SQLT_RID:		/* rowid */
			LM_DBG("use DB1_STR result type");
			RES_TYPES(_r)[i] = DB1_STR;
dyn_str:
			dtype = SQLT_CHR;
			len = 0; /* DATA_SIZE is ub2 */
			status = OCIAttrGet(param, OCI_DTYPE_PARAM,
				(dvoid**)(dvoid*)&len, NULL, OCI_ATTR_DATA_SIZE,
				con->errhp);
			if (status != OCI_SUCCESS) goto ora_err;
			if (len >= 4000) {
				LM_DBG("use DB1_BLOB result type");
				RES_TYPES(_r)[i] = DB1_BLOB;
			}
			++len;
			break;

		default:
			LM_ERR("unsupported datatype %d\n", dtype);
			goto stop_load;
		}
		_d->ilen[i] = (ub2)len;
		_d->pv[i].v = st_buf + tsz;
		tsz += len;
		status = OCIDefineByPos(_c, &_d->defh[i], con->errhp, i+1,
			_d->pv[i].v, len, dtype, &_d->ind[i],
			&_d->len[i], NULL, OCI_DEFAULT);
		if (status != OCI_SUCCESS) goto ora_err;
	}

#if STATIC_BUF_LEN < 65536
#error
#endif
	if (tsz > 65536) {
		LM_ERR("Row size exceed 65K. IOB's are not supported");
		goto stop_load;
	}
	return 0;

ora_err:
	LM_ERR("driver: %s\n", db_oracle_error(con, status));
stop_load:
	db_free_columns(_r);
	return -6;
}


/*
 * Convert data fron db format to internal format
 */
static int convert_row(db1_res_t* _res, db_row_t* _r, dmap_t* _d)
{
	unsigned i, n = RES_COL_N(_res);

	ROW_N(_r) = n;
	ROW_VALUES(_r) = (db_val_t*)pkg_malloc(sizeof(db_val_t) * n);
	if (!ROW_VALUES(_r)) {
nomem:
		LM_ERR("no private memory left\n");
		return -1;
	}
	memset(ROW_VALUES(_r), 0, sizeof(db_val_t) * n);

	for (i = 0; i < n; i++) {
		static const str dummy_string = {"", 0};

		db_val_t* v = &ROW_VALUES(_r)[i];
		db_type_t t = RES_TYPES(_res)[i];

		if (_d->ind[i] == -1) {
			/* Initialize the string pointers to a dummy empty
			 * string so that we do not crash when the NULL flag
			 * is set but the module does not check it properly
			 */
			VAL_STRING(v) = dummy_string.s;
			VAL_STR(v) = dummy_string;
			VAL_BLOB(v) = dummy_string;
			VAL_TYPE(v) = t;
			VAL_NULL(v) = 1;
			continue;
		}

		if (_d->ind[i])
			LM_WARN("truncated value in DB\n");

		VAL_TYPE(v) = t;
		switch (t) {
		case DB1_INT:
			VAL_INT(v) = *_d->pv[i].i;
			break;

		case DB1_BIGINT:
			LM_ERR("BIGINT not supported");
			return -1;

		case DB1_BITMAP:
			VAL_BITMAP(v) = *_d->pv[i].i;
			break;

		case DB1_DOUBLE:
			VAL_DOUBLE(v) = *_d->pv[i].f;
			break;

		case DB1_DATETIME:
			{
				struct tm tm;
				memset(&tm, 0, sizeof(tm));
				OCIDateGetTime(_d->pv[i].o, &tm.tm_hour,
					&tm.tm_min, &tm.tm_sec);
				OCIDateGetDate(_d->pv[i].o, &tm.tm_year,
					&tm.tm_mon, &tm.tm_mday);
				if (tm.tm_mon)
					--tm.tm_mon;
				if (tm.tm_year >= 1900)
					tm.tm_year -= 1900;
				VAL_TIME(v) = mktime(&tm);
			}
			break;

		case DB1_STR:
		case DB1_BLOB:
		case DB1_STRING:
			{
				size_t len = _d->len[i];
				char *pstr = pkg_malloc(len+1);
				if (!pstr) goto nomem;
				memcpy(pstr, _d->pv[i].c, len);
				pstr[len] = '\0';
				VAL_FREE(v) = 1;
				if (t == DB1_STR) {
					VAL_STR(v).s = pstr;
					VAL_STR(v).len = len;
				} else if (t == DB1_BLOB) {
					VAL_BLOB(v).s = pstr;
					VAL_BLOB(v).len = len;
				} else {
					VAL_STRING(v) = pstr;
				}
			}
			break;

		default:
			LM_ERR("unknown type mapping (%u)\n", t);
			return -2;
		}
	}

	return 0;
}


/*
 * Get rows and convert it from oracle to db API representation
 */
static int get_rows(ora_con_t* con, db1_res_t* _r, OCIStmt* _c, dmap_t* _d)
{
	ub4 rcnt;
	sword status;
	unsigned n = RES_COL_N(_r);

	memcpy(_d->len, _d->ilen, sizeof(_d->len[0]) * n);

	// timelimited operation
	status = begin_timelimit(con, 0);
	if (status != OCI_SUCCESS) goto ora_err;
	do status = OCIStmtFetch2(_c, con->errhp, 1, OCI_FETCH_LAST, 0,
		OCI_DEFAULT);
	while (wait_timelimit(con, status));
	if (done_timelimit(con, status)) goto stop_load;
	if (status != OCI_SUCCESS) {
		if (status != OCI_NO_DATA)
			goto ora_err;

		RES_ROW_N(_r) = 0;
		RES_ROWS(_r) = NULL;
		return 0;
	}

	status = OCIAttrGet(_c, OCI_HTYPE_STMT, &rcnt, NULL,
		OCI_ATTR_CURRENT_POSITION, con->errhp);
	if (status != OCI_SUCCESS) goto ora_err;
	if (!rcnt) {
		LM_ERR("lastpos==0\n");
		goto stop_load;
	}

	RES_ROW_N(_r) = rcnt;
	RES_ROWS(_r) = (db_row_t*)pkg_malloc(sizeof(db_row_t) * rcnt);
	if (!RES_ROWS(_r)) {
		LM_ERR("no private memory left\n");
		return -1;
	}
	memset(RES_ROWS(_r), 0, sizeof(db_row_t) * rcnt);

	while ( 1 ) {
		if (convert_row(_r, &RES_ROWS(_r)[--rcnt], _d) < 0) {
			LM_ERR("erroc convert row\n");
			goto stop_load;
		}

		if (!rcnt)
			return 0;

		memcpy(_d->len, _d->ilen, sizeof(_d->len[0]) * n);
		// timelimited operation
		status = begin_timelimit(con, 0);
		if (status != OCI_SUCCESS) goto ora_err;
		do status = OCIStmtFetch2(_c, con->errhp, 1, OCI_FETCH_PRIOR, 0,
			OCI_DEFAULT);
		while (wait_timelimit(con, status));
		if (done_timelimit(con, status)) goto stop_load;
		if (status != OCI_SUCCESS) break;
	}
ora_err:
	LM_ERR("driver: %s\n", db_oracle_error(con, status));
stop_load:
	db_free_rows(_r);
	RES_ROW_N(_r) = 0;	/* TODO: skipped in db_res.c :) */
	return -3;
}


/*
 * Read database answer and fill the structure
 */
int db_oracle_store_result(const db1_con_t* _h, db1_res_t** _r)
{
	dmap_t dmap;
	int rc;
	db1_res_t* r;
	ora_con_t* con;
	OCIStmt* hs;

	if (!_h || !_r) {
badparam:
		LM_ERR("invalid parameter\n");
		return -1;
	}

	con = CON_ORA(_h);
	{
	    query_data_t *pcb = con->pqdata;
	    

	    if (!pcb || !pcb->_rs)
		    goto badparam;
		    
	    hs = *pcb->_rs;
	    pcb->_rs = NULL; /* paranoid for next call */
	}	    
	
	rc = -1;
	if (_r)	*_r = NULL;	/* unification for all errors */

	r = db_new_result();
	if (!r) {
		LM_ERR("no memory left\n");
		goto done;
	}

	if (get_columns(con, r, hs, &dmap) < 0) {
		LM_ERR("error while getting column names\n");
		goto done;
	}

	if (get_rows(con, r, hs, &dmap) < 0) {
		LM_ERR("error while converting rows\n");
		db_free_columns(r);
		goto done;
	}

	rc = 0;
	*_r = r;
done:
	OCIHandleFree(hs, OCI_HTYPE_STMT);
	return rc;
}
