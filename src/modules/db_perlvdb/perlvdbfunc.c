/* 
 * $Id: perlvdbfunc.c 816 2007-02-13 18:33:22Z bastian $
 *
 * Perl virtual database module interface
 *
 * Copyright (C) 2007 Collax GmbH
 *                    (Bastian Friedrich <bastian.friedrich@collax.com>)
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

#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "db_perlvdb.h"
#include "perlvdbfunc.h"
#include "../../core/str.h"

/*
 * Simple conversion IV -> int
 * including decreasing ref cnt
 */

long IV2int(SV *in) {
	int ret = -1;

	if (SvOK(in)) {
		if (SvIOK(in)) {
			ret = SvIV(in);
		}
		SvREFCNT_dec(in);
	}

	return ret;
}

/*
 * Returns the class part of the URI
 */
char *parseurl(const str* url) {
	char *cn;

	cn = strchr(url->s, ':') + 1;
	if (strlen(cn) > 0)
		return cn;
	else
		return NULL;
}


SV *newvdbobj(const char* cn) {
	SV* obj;
	SV *class;

	class = newSVpv(cn, 0);

	obj = perlvdb_perlmethod(class, PERL_CONSTRUCTOR_NAME,
			NULL, NULL, NULL, NULL);

	return obj;
}

SV *getobj(const db1_con_t *con) {
	return ((SV*)CON_TAIL(con));
}

/*
 * Checks whether the passed SV is a valid VDB object:
 * - not null
 * - not undef
 * - an object
 * - derived from Kamailio::VDB
 */
int checkobj(SV* obj) {
	if (obj != NULL) {
		if (obj != &PL_sv_undef) {
			if (sv_isobject(obj)) {
				if (sv_derived_from(obj, PERL_VDB_BASECLASS)) {
					return 1;
				}
			}
		}
	}

	return 0;
}

/*
 * Initialize database module
 * No function should be called before this
 */
db1_con_t* perlvdb_db_init(const str* url) {
	db1_con_t* res;

	char *cn;
	SV *obj = NULL;
	
	int consize = sizeof(db1_con_t) + sizeof(SV);
	
	if (!url) {
		LM_ERR("invalid parameter value\n");
		return NULL;
	}

	cn = parseurl(url);
	if (!cn) {
		LM_ERR("invalid perl vdb url.\n");
		return NULL;
	}

	obj = newvdbobj(cn);
	if (!checkobj(obj)) {
		LM_ERR("could not initialize module. Not inheriting from %s?\n",
				PERL_VDB_BASECLASS);
		return NULL;
	}

	res = pkg_malloc(consize);
	if (!res) {
		LM_ERR("no pkg memory left\n");
		return NULL;
	}
	memset(res, 0, consize);
	CON_TAIL(res) = (unsigned int)(unsigned long)obj;

	return res;
}


/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int perlvdb_use_table(db1_con_t* h, const str* t) {
	SV *ret;
	
	if (!h || !t || !t->s) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	ret = perlvdb_perlmethod(getobj(h), PERL_VDB_USETABLEMETHOD,
			sv_2mortal(newSVpv(t->s, t->len)), NULL, NULL, NULL);

	return IV2int(ret);
}


void perlvdb_db_close(db1_con_t* h) {
	if (!h) {
		LM_ERR("invalid parameter value\n");
		return;
	}

	pkg_free(h);
}


/*
 * Insert a row into specified table
 * h: structure representing database connection
 * k: key names
 * v: values of the keys
 * n: number of key=value pairs
 */
int perlvdb_db_insertreplace(const db1_con_t* h, const db_key_t* k, const db_val_t* v,
		const int n, char *insertreplace) {
	AV *arr;
	SV *arrref;
	SV *ret;

	arr = pairs2perlarray(k, v, n);
	arrref = newRV_noinc((SV*)arr);
	ret = perlvdb_perlmethod(getobj(h), insertreplace,
			arrref, NULL, NULL, NULL);

	av_undef(arr);

	return IV2int(ret);
}

int perlvdb_db_insert(const db1_con_t* h, const db_key_t* k, const db_val_t* v, const int n) {
	return perlvdb_db_insertreplace(h, k, v, n, PERL_VDB_INSERTMETHOD);
}

/*
 * Just like insert, but replace the row if it exists
 */
int perlvdb_db_replace(const db1_con_t* h, const db_key_t* k, const db_val_t* v,
		const int n, const int un, const int m) {
	return perlvdb_db_insertreplace(h, k, v, n, PERL_VDB_REPLACEMETHOD);
}

/*
 * Delete a row from the specified table
 * h: structure representing database connection
 * k: key names
 * o: operators
 * v: values of the keys that must match
 * n: number of key=value pairs
 */
int perlvdb_db_delete(const db1_con_t* h, const db_key_t* k, const db_op_t* o,
		const db_val_t* v, const int n) {
	AV *arr;
	SV *arrref;
	SV *ret;

	arr = conds2perlarray(k, o, v, n);
	arrref = newRV_noinc((SV*)arr);
	ret = perlvdb_perlmethod(getobj(h), PERL_VDB_DELETEMETHOD,
			arrref, NULL, NULL, NULL);

	av_undef(arr);

	return IV2int(ret);
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
int perlvdb_db_update(const db1_con_t* h, const db_key_t* k, const db_op_t* o,
		const db_val_t* v, const db_key_t* uk, const db_val_t* uv,
		const int n, const int un) {

	AV *condarr;
	AV *updatearr;

	SV *condarrref;
	SV *updatearrref;

	SV *ret;

	condarr = conds2perlarray(k, o, v, n);
	updatearr = pairs2perlarray(uk, uv, un);

	condarrref = newRV_noinc((SV*)condarr);
	updatearrref = newRV_noinc((SV*)updatearr);
	
	ret = perlvdb_perlmethod(getobj(h), PERL_VDB_UPDATEMETHOD,
			condarrref, updatearrref, NULL, NULL);

	av_undef(condarr);
	av_undef(updatearr);

	return IV2int(ret);
}


/*
 * Query table for specified rows
 * h: structure representing database connection
 * k: key names
 * op: operators
 * v: values of the keys that must match
 * c: column names to return
 * n: number of key=values pairs to compare
 * nc: number of columns to return
 * o: order by the specified column
 */
int perlvdb_db_query(const db1_con_t* h, const db_key_t* k, const db_op_t* op,
		const db_val_t* v, const db_key_t* c, const int n, const int nc,
		const db_key_t o, db1_res_t** r) {


	AV *condarr;
	AV *retkeysarr;
	SV *order;

	SV *condarrref;
	SV *retkeysref;

	SV *resultset;

	int retval = 0;

	/* Create parameter set */
	condarr = conds2perlarray(k, op, v, n);
	retkeysarr = keys2perlarray(c, nc);

	if (o) order = newSVpv(o->s, o->len);
	else order = &PL_sv_undef;


	condarrref = newRV_noinc((SV*)condarr);
	retkeysref = newRV_noinc((SV*)retkeysarr);

	/* Call perl method */
	resultset = perlvdb_perlmethod(getobj(h), PERL_VDB_QUERYMETHOD,
			condarrref, retkeysref, order, NULL);

	av_undef(condarr);
	av_undef(retkeysarr);

	/* Transform perl result set to Kamailio result set */
	if (!resultset) {
		/* No results. */
		LM_ERR("no perl result set.\n");
		retval = -1;
	} else {
		if (sv_isa(resultset, "Kamailio::VDB::Result")) {
			retval = perlresult2dbres(resultset, r);
		/* Nested refs are decreased/deleted inside the routine */
			SvREFCNT_dec(resultset);
		} else {
			LM_ERR("invalid result set retrieved from perl call.\n");
			retval = -1;
		}
	}

	return retval;
}


/*
 * Release a result set from memory
 */
int perlvdb_db_free_result(db1_con_t* _h, db1_res_t* _r) {
	int i;

	if (_r) {
		for (i = 0; i < _r->n; i++) {
			if (_r->rows[i].values)
				pkg_free(_r->rows[i].values);
		}

		if (_r->col.types)
			pkg_free(_r->col.types);
		if (_r->col.names)
			pkg_free(_r->col.names);
		if (_r->rows)
			pkg_free(_r->rows);
		pkg_free(_r);
	}
	return 0;
}
