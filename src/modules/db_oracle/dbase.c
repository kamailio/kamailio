/*
 * $Id$
 *
 * Oracle module core functions
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <oci.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../lib/srdb1/db_pool.h"
#include "../../lib/srdb1/db_ut.h"
#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db_query.h"
#include "val.h"
#include "ora_con.h"
#include "res.h"
#include "asynch.h"
#include "dbase.h"


#define	MAX_BIND_HANDLES 128

char st_buf[STATIC_BUF_LEN];


/*
 * Make error message. Always return negative value
 */
int sql_buf_small(void)
{
	LM_ERR("static buffer too small\n");
	return -11;
}

/*
 * Decode error
 */
static char errbuf[512];

static const char* db_oracle_errorinfo(ora_con_t* con)
{
	sword errcd;
	if (OCIErrorGet(con->errhp, 1, NULL, &errcd,
			(OraText*)errbuf, sizeof(errbuf),
			OCI_HTYPE_ERROR) != OCI_SUCCESS) errbuf[0] = '\0';
	else switch (errcd) {
	case 28:	/* your session has been killed */
	case 30:	/* session ID does not exists */
	case 31:	/* session marked for kill */
	case 41:	/* active time limit exceeded session terminated */
	case 107:	/* failed to connect to oracle listener */
	case 115:	/* connection refused; dispatcher table is full */
	case 1033:	/* init/shutdown in progress */
	case 1034:	/* not available (startup) */
	case 1089:	/* server shutdown */
	case 1090:	/* shutdown wait after command */
	case 1092:	/* oracle instance terminated. Disconnection forced */
	case 1573:	/* shutdown instance, no futher change allowed */
	case 2049:	/* timeout: distributed transaction waiting lock */
	case 3113:	/* EOF on communication channel */
	case 3114:	/* not connected */
	case 3135:	/* lost connection */
	case 6033:	/* connect failed, partner rejected connection */
	case 6034:	/* connect failed, partner exited unexpectedly */
	case 6037:	/* connect failed, node unrecheable */
	case 6039:	/* connect failed */
	case 6042:	/* msgrcv failure (DNT) */
	case 6043:	/* msgsend failure (DNT) */
	case 6107:	/* network server not found */
	case 6108:	/* connect to host failed */
	case 6109:	/* msgrcv failure (TCP) */
	case 6110:	/* msgsend failure (TCP) */
	case 6114:	/* SID lookup failure */
	case 6124:	/* TCP timeout */
	case 6135:	/* connect rejected; server is stopping (TCP) */
	case 6144:	/* SID unavaliable (TCP) */
	case 6413:	/* connection not open */
	case 12150:	/* tns can't send data, probably disconnect */
	case 12152:	/* tns unable to send break message */
	case 12153:	/* tns not connected */
	case 12161:	/* tns internal error */
	case 12170:	/* tns connect timeout */
	case 12224:	/* tns no listener */
	case 12225:	/* tns destination host unrecheable */
	case 12230:	/* tns network error */
	case 12525:	/* tns (internal) timeout */
	case 12521:	/* tns can't resolve db name */
	case 12537:	/* tns connection cloed */
	case 12541:	/* tns not running */
	case 12543:	/* tns destination host unrecheable */
	case 12547:	/* tns lost contact */
	case 12560:	/* tns protocol(transport) error */
	case 12561:	/* tns unknown error */
	case 12608:	/* tns send timeount */
	case 12609:	/* tns receive timeount */
	    LM_ERR("conneciom dropped\n");
	    db_oracle_disconnect(con);
	default:
		break;
	}

	return errbuf;
}

const char* db_oracle_error(ora_con_t* con, sword status)
{
	switch (status) {
		case OCI_SUCCESS:
			return "internal (success)";

		case OCI_SUCCESS_WITH_INFO:
		case OCI_ERROR:
			return db_oracle_errorinfo(con);

		case OCI_NEED_DATA:
			return "need data";

		case OCI_NO_DATA:
			return "no data";

		case OCI_INVALID_HANDLE:
			return "invalid handle";

		case OCI_STILL_EXECUTING:	// ORA-3123
			return "executing (logic)";

		case OCI_CONTINUE:
			return "continue (library)";

		default:
			snprintf(errbuf, sizeof(errbuf),
				"unknown status %u", status);
			return errbuf;
	}
}


/*
 * Initialize database module
 * No function should be called before this
 */
db1_con_t* db_oracle_init(const str* _url)
{
	return db_do_init(_url, (void *)db_oracle_new_connection);
}


/*
 * Shut down database module
 * No function should be called after this
 */
void db_oracle_close(db1_con_t* _h)
{
	db_do_close(_h, db_oracle_free_connection);
}


/*
 * Release a result set from memory
 */
int db_oracle_free_result(db1_con_t* _h, db1_res_t* _r)
{
	if (!_h || !_r) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (db_free_result(_r) < 0)
	{
		LM_ERR("failed to free result structure\n");
		return -1;
	}
	return 0;
}


/*
 * Send an SQL query to the server
 */
static int db_oracle_submit_query(const db1_con_t* _h, const str* _s)
{
	OCIBind* bind[MAX_BIND_HANDLES];
	OCIDate odt[sizeof(bind)/sizeof(bind[0])];
	str tmps;
	sword status;
	int pass;
	ora_con_t* con = CON_ORA(_h);
	query_data_t* pqd = con->pqdata;
	size_t hc = pqd->_n + pqd->_nw;
	OCIStmt *stmthp;

	if (hc >= sizeof(bind)/sizeof(bind[0])) {
		LM_ERR("too many bound. Rebuild with MAX_BIND_HANDLES >= %u\n",
			(unsigned)hc);
		return -1;
	}
	
	if (!pqd->_rs) {
		/*
		 * This method is at ~25% faster as set OCI_COMMIT_ON_SUCCESS
		 * in StmtExecute
		 */
		tmps.len = snprintf(st_buf, sizeof(st_buf),
			"begin %.*s; commit write batch nowait; end;",
			_s->len, _s->s);
		if ((unsigned)tmps.len >= sizeof(st_buf))
			return sql_buf_small();
		tmps.s = st_buf;
		_s = &tmps;
	}

	pass = 1;
	if (!con->connected) {
		status = db_oracle_reconnect(con);
		if (status != OCI_SUCCESS) {
			LM_ERR("can't restore connection: %s\n", db_oracle_error(con, status));
			return -2;
		}
		LM_INFO("connection restored\n");
		--pass;
	}
repeat:
	stmthp = NULL;
	status = OCIHandleAlloc(con->envhp, (dvoid**)(dvoid*)&stmthp,
		    OCI_HTYPE_STMT, 0, NULL);
	if (status != OCI_SUCCESS)
		goto ora_err;
	status = OCIStmtPrepare(stmthp, con->errhp, (text*)_s->s, _s->len,
		OCI_NTV_SYNTAX, OCI_DEFAULT);
	if (status != OCI_SUCCESS)
		goto ora_err;

	if (hc) {
		bmap_t bmap;
		size_t pos = 1;
		int i;

		memset(bind, 0, hc*sizeof(bind[0]));
		for (i = 0; i < pqd->_n; i++) {
			if (db_oracle_val2bind(&bmap, &pqd->_v[i], &odt[pos]) < 0)
				goto bind_err;
			status = OCIBindByPos(stmthp, &bind[pos], con->errhp,
				pos, bmap.addr, bmap.size, bmap.type,
				NULL, NULL, NULL, 0, NULL, OCI_DEFAULT);
			if (status != OCI_SUCCESS)
				goto ora_err;
			++pos;
		}
		for (i = 0; i < pqd->_nw; i++) {
			if (db_oracle_val2bind(&bmap, &pqd->_w[i], &odt[pos]) < 0) {
bind_err:
				OCIHandleFree(stmthp, OCI_HTYPE_STMT);
				LM_ERR("can't map values\n");
				return -3;
			}
			status = OCIBindByPos(stmthp, &bind[pos], con->errhp,
				pos, bmap.addr, bmap.size, bmap.type,
				NULL, NULL, NULL, 0, NULL, OCI_DEFAULT);
			if (status != OCI_SUCCESS)
				goto ora_err;
			++pos;
		}
	}

	// timelimited operation
	status = begin_timelimit(con, 0);
	if (status != OCI_SUCCESS) goto ora_err;
	do status = OCIStmtExecute(con->svchp, stmthp, con->errhp,
		!pqd->_rs, 0, NULL, NULL,
		pqd->_rs ? OCI_STMT_SCROLLABLE_READONLY : OCI_DEFAULT);
	while (wait_timelimit(con, status));
	if (done_timelimit(con, status)) goto stop_exec;
	switch (status)	{
	case OCI_SUCCESS_WITH_INFO:
		LM_WARN("driver: %s\n", db_oracle_errorinfo(con));
		//PASS THRU
	case OCI_SUCCESS:
		if (pqd->_rs)
			*pqd->_rs = stmthp;
		else
			OCIHandleFree(stmthp, OCI_HTYPE_STMT);
		return 0;
	default:
	    pass = -pass;
	    break;
	}

ora_err:
	LM_ERR("driver: %s\n", db_oracle_error(con, status));
stop_exec:
	if (stmthp)
		OCIHandleFree(stmthp, OCI_HTYPE_STMT);
	if (pass == -1 && !con->connected) {
		/* Attemtp to reconnect */
		if (db_oracle_reconnect(con) == OCI_SUCCESS) {
			LM_NOTICE("attempt repeat after reconnect\n");
			pass = 0;
			goto repeat;
		}
		LM_ERR("connection loss\n");
	}
	return -4;
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
int db_oracle_query(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _op,
		const db_val_t* _v, const db_key_t* _c, int _n, int _nc,
		const db_key_t _o, db1_res_t** _r)
{
	query_data_t cb;
	OCIStmt* reshp;
	int rc;

	if (!_h || !CON_TABLE(_h) || !_r) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	cb._rs = &reshp;
	cb._v  = _v;
	cb._n  = _n;
	cb._w  = NULL;
	cb._nw = 0;
	CON_ORA(_h)->pqdata = &cb;
	CON_ORA(_h)->bindpos = 0;
	rc = db_do_query(_h, _k, _op, _v, _c, _n, _nc, _o, _r,
		db_oracle_val2str, db_oracle_submit_query, db_oracle_store_result);
	CON_ORA(_h)->pqdata = NULL;	/* paranoid for next call */
	return rc;
}


/*
 * Execute a raw SQL query
 */
int db_oracle_raw_query(const db1_con_t* _h, const str* _s, db1_res_t** _r)
{
	query_data_t cb;
	OCIStmt* reshp;
	int len;
	const char *p;

	if (!_h || !_s || !_s->s) {
badparam:
		LM_ERR("invalid parameter value\n");
		return -1;
	}


	memset(&cb, 0, sizeof(cb));

	p = _s->s;
	len = _s->len;
	while (len && *p == ' ') ++p, --len;
#define _S_DIFF(p, l, S) (l <= sizeof(S)-1 || strncasecmp(p, S, sizeof(S)-1))
	if (!_S_DIFF(p, len, "select ")) {
		if (!_r) goto badparam;
		cb._rs = &reshp;
	} else {
		if (	_S_DIFF(p, len, "insert ")
		    && 	_S_DIFF(p, len, "delete ")
		    && 	_S_DIFF(p, len, "update "))
		{
			LM_ERR("unknown raw_query: '%.*s'\n", _s->len, _s->s);
			return -2;
		}
#undef _S_DIFF
		if (_r) goto badparam;
		cb._rs = NULL;
	}

	len = db_do_raw_query(_h, _s, _r, db_oracle_submit_query, db_oracle_store_result);
	CON_ORA(_h)->pqdata = NULL;	/* paranoid for next call */
	return len;
}


/*
 * Insert a row into specified table
 * _h: structure representing database connection
 * _k: key names
 * _v: values of the keys
 * _n: number of key=value pairs
 */
int db_oracle_insert(const db1_con_t* _h, const db_key_t* _k, const db_val_t* _v,
		int _n)
{
	query_data_t cb;
	int rc;

	if (!_h || !CON_TABLE(_h)) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	cb._rs = NULL;
	cb._v  = _v;
	cb._n  = _n;
	cb._w  = NULL;
	cb._nw = 0;
	CON_ORA(_h)->pqdata = &cb;
	CON_ORA(_h)->bindpos = 0;
	rc = db_do_insert(_h, _k, _v, _n, db_oracle_val2str, db_oracle_submit_query);
	CON_ORA(_h)->pqdata = NULL;	/* paranoid for next call */
	return rc;
}


/*
 * Delete a row from the specified table
 * _h: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _n: number of key=value pairs
 */
int db_oracle_delete(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, int _n)
{
	query_data_t cb;
	int rc;

	if (!_h || !CON_TABLE(_h)) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	cb._rs = NULL;
	cb._v  = _v;
	cb._n  = _n;
	cb._w  = NULL;
	cb._nw = 0;
	CON_ORA(_h)->pqdata = &cb;
	CON_ORA(_h)->bindpos = 0;
	rc = db_do_delete(_h, _k, _o, _v, _n, db_oracle_val2str, db_oracle_submit_query);
	CON_ORA(_h)->pqdata = NULL;	/* paranoid for next call */
	return rc;
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
int db_oracle_update(const db1_con_t* _h, const db_key_t* _k, const db_op_t* _o,
		const db_val_t* _v, const db_key_t* _uk, const db_val_t* _uv,
		int _n, int _un)
{
	query_data_t cb;
	int rc;
	
	if (!_h || !CON_TABLE(_h)) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	cb._rs = NULL;
	cb._v  = _uv;
	cb._n  = _un;
	cb._w  = _v;
	cb._nw = _n;
	CON_ORA(_h)->pqdata = &cb;
	CON_ORA(_h)->bindpos = 0;
	rc = db_do_update(_h, _k, _o, _v, _uk, _uv, _n, _un,
			db_oracle_val2str, db_oracle_submit_query);
	CON_ORA(_h)->pqdata = NULL;	/* paranoid for next call */
	return rc;
}


/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_oracle_use_table(db1_con_t* _h, const str* _t)
{
	return db_use_table(_h, _t);
}
