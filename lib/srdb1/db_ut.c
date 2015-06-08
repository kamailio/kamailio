/*
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2007-2008 1&1 Internet AG
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

/**
 * \file lib/srdb1/db_ut.c
 * \brief Utility functions for database drivers.
 *
 * This utility methods are used from the database SQL driver to convert
 * values and print SQL queries from the internal API representation.
 * \ingroup db1
 */


#if defined (__OS_darwin) || defined (__OS_freebsd)
#include "../../pvar.h"
#endif

/**
 * make strptime available
 * use 600 for 'Single UNIX Specification, Version 3'
 * _XOPEN_SOURCE creates conflict in swab definition in Solaris
 */
#ifndef __OS_solaris
	#define _XOPEN_SOURCE 600          /* glibc2 on linux, bsd */
	#define _BSD_SOURCE 1              /* needed on linux to "fix" the effect
										 of the above define on 
										 features.h/unistd.h syscall() */
#else
	#define _XOPEN_SOURCE_EXTENDED 1   /* solaris */
#endif

#include <time.h>

#ifndef __OS_solaris
	#undef _XOPEN_SOURCE
	#undef _XOPEN_SOURCE_EXTENDED
#else
	#undef _XOPEN_SOURCE_EXTENDED 1   /* solaris */
#endif

#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../mem/mem.h"
#include "../../dprint.h"

#include "db_ut.h"


inline int db_str2int(const char* _s, int* _v)
{
	long tmp;
	char* p = NULL;

	if (!_s || !_v) {
	       LM_ERR("Invalid parameter value\n");
	       return -1;
	}

	tmp = strtoul(_s, &p, 10);
	if ((tmp == ULONG_MAX && errno == ERANGE) || 
	    (tmp < INT_MIN) || (tmp > UINT_MAX)) {
		LM_ERR("Value out of range\n");
		return -1;
	}
	if (p && *p != '\0') {
		LM_ERR("Unexpected characters: [%s]\n", p);
		return -2;
	}

	*_v = (int)tmp;
	return 0;
}


inline int db_str2longlong(const char* _s, long long * _v)
{
	long long tmp;
	char* p = NULL;

	if (!_s || !_v) {
	       LM_ERR("Invalid parameter value\n");
	       return -1;
	}

	tmp = strtoll(_s, &p, 10);
	if (errno == ERANGE) {
		LM_ERR("Value out of range\n");
		return -1;
	}
	if (p && *p != '\0') {
		LM_ERR("Unexpected characters: [%s]\n", p);
		return -2;
	}

	*_v = tmp;
	return 0;
}


/*
 * Convert a string to double
 */
inline int db_str2double(const char* _s, double* _v)
{
	if ((!_s) || (!_v)) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	*_v = atof(_s);
	return 0;
}



/*
 * Convert an integer to string
 */
inline int db_int2str(int _v, char* _s, int* _l)
{
	int ret;

	if ((!_s) || (!_l) || (!*_l)) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	ret = snprintf(_s, *_l, "%-d", _v);
	if (ret < 0 || ret >= *_l) {
		LM_ERR("Error in snprintf\n");
		return -1;
	}
	*_l = ret;

	return 0;
}


/*
 * Convert an long long to string
 */
inline int db_longlong2str(long long _v, char* _s, int* _l)
{
	int ret;

	if ((!_s) || (!_l) || (!*_l)) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	ret = snprintf(_s, *_l, "%-lld", _v);
	if (ret < 0 || ret >= *_l) {
		LM_ERR("Error in snprintf\n");
		return -1;
	}
	*_l = ret;

	return 0;
}


/*
 * Convert a double to string
 */
inline int db_double2str(double _v, char* _s, int* _l)
{
	int ret;

	if ((!_s) || (!_l) || (!*_l)) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	ret = snprintf(_s, *_l, "%-10.6f", _v);
	if (ret < 0 || ret >= *_l) {
		LM_ERR("Error in snprintf\n");
		return -1;
	}
	*_l = ret;

	return 0;
}


/* 
 * Convert a string to time_t
 */
inline int db_str2time(const char* _s, time_t* _v)
{
	struct tm time;

	if ((!_s) || (!_v)) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	/* Convert database time representation to time_t structure
	   It is necessary to zero tm structure first */
	memset(&time, '\0', sizeof(struct tm));
	if (strptime(_s, "%Y-%m-%d %H:%M:%S", &time) == NULL) {
		LM_ERR("Error during time conversion\n");
		return -1;
	}

	/* Daylight saving information got lost in the database
	* so let mktime to guess it. This eliminates the bug when
	* contacts reloaded from the database have different time
	* of expiration by one hour when daylight saving is used
	*/ 
	time.tm_isdst = -1;
	*_v = mktime(&time);

	return 0;
}


inline int db_time2str(time_t _v, char* _s, int* _l)
{
	struct tm* t;
	int l;

	if ((!_s) || (!_l) || (*_l < 2)) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	*_s++ = '\'';

	/* Convert time_t structure to format accepted by the database */
	t = localtime(&_v);
	l = strftime(_s, *_l -1, "%Y-%m-%d %H:%M:%S", t);

	if (l == 0) {
		LM_ERR("Error during time conversion\n");
		/* the value of _s is now unspecified */
		_s = NULL;
		_l = 0;
		return -1;
	}
	*_l = l;

	*(_s + l) = '\'';
	*_l = l + 2;
	return 0;
}


/*
 * Print list of columns separated by comma
 */
int db_print_columns(char* _b, const int _l, const db_key_t* _c, const int _n, const char *_tq)
{
	int i, ret, len = 0;

	if ((!_c) || (!_n) || (!_b) || (!_l)) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	for(i = 0; i < _n; i++)	{
		if (i == (_n - 1)) {
			ret = snprintf(_b + len, _l - len, "%s%.*s%s ", _tq, _c[i]->len, _c[i]->s, _tq);
			if (ret < 0 || ret >= (_l - len)) goto error;
			len += ret;
		} else {
			ret = snprintf(_b + len, _l - len, "%s%.*s%s,", _tq, _c[i]->len, _c[i]->s, _tq);
			if (ret < 0 || ret >= (_l - len)) goto error;
			len += ret;
		}
	}
	return len;

	error:
	LM_ERR("Error in snprintf\n");
	return -1;
}


/*
 * Print values of SQL statement
 */
int db_print_values(const db1_con_t* _c, char* _b, const int _l, const db_val_t* _v,
	const int _n, int (*val2str)(const db1_con_t*, const db_val_t*, char*, int*))
{
	int i, l, len = 0;

	if (!_c || !_b || !_l || !_v || !_n) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	for(i = 0; i < _n; i++) {
		l = _l - len;
		if ( (*val2str)(_c, _v + i, _b + len, &l) < 0) {
			LM_ERR("Error while converting value to string\n");
			return -1;
		}
		len += l;
		if (i != (_n - 1)) {
			*(_b + len) = ',';
			len++;
		}
	}
	return len;
}


/*
 * Print where clause of SQL statement
 */
int db_print_where(const db1_con_t* _c, char* _b, const int _l, const db_key_t* _k,
	const db_op_t* _o, const db_val_t* _v, const int _n, int (*val2str)
	(const 	db1_con_t*, const db_val_t*, char*, int*))
{
	int i, l, ret, len = 0;

	if (!_c || !_b || !_l || !_k || !_v || !_n) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	for(i = 0; i < _n; i++) {
		if (_o && strncmp(_o[i], OP_BITWISE_AND, 1) == 0) {
			char tmp_buf[16];
			int tmp_len = 15;
			memset(tmp_buf, '0', 16);
			if ((*val2str)(_c, &(_v[i]), tmp_buf, &tmp_len) < 0) {
				LM_ERR("Error while converting value to string\n");
				return -1;
			}
			ret = snprintf(_b + len, _l - len, "%s%.*s%s&%.*s=%.*s", CON_TQUOTESZ(_c),
					_k[i]->len, _k[i]->s, CON_TQUOTESZ(_c), tmp_len, tmp_buf, tmp_len, tmp_buf);
			if (ret < 0 || ret >= (_l - len)) goto error;
			len += ret;
		} else {
			if (_o) {
				ret = snprintf(_b + len, _l - len, "%s%.*s%s%s", CON_TQUOTESZ(_c),
						_k[i]->len, _k[i]->s, CON_TQUOTESZ(_c), _o[i]);
				if (ret < 0 || ret >= (_l - len)) goto error;
				len += ret;
			} else {
				ret = snprintf(_b + len, _l - len, "%s%.*s%s=", CON_TQUOTESZ(_c),
						_k[i]->len, _k[i]->s, CON_TQUOTESZ(_c));
				if (ret < 0 || ret >= (_l - len)) goto error;
				len += ret;
			}
			l = _l - len;
			if ( (*val2str)(_c, &(_v[i]), _b + len, &l) < 0) {
				LM_ERR("Error while converting value to string\n");
				return -1;
			}
			len += l;
		}

		if (i != (_n - 1)) {
			ret = snprintf(_b + len, _l - len, " AND ");
			if (ret < 0 || ret >= (_l - len)) goto error;
			len += ret;
		}
	}
	return len;

 error:
	LM_ERR("Error in snprintf\n");
	return -1;
}


/*
 * Print set clause of update SQL statement
 */
int db_print_set(const db1_con_t* _c, char* _b, const int _l, const db_key_t* _k,
	const db_val_t* _v, const int _n, int (*val2str)(const db1_con_t*,
	const db_val_t*,char*, int*))
{
	int i, l, ret, len = 0;

	if (!_c || !_b || !_l || !_k || !_v || !_n) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	for(i = 0; i < _n; i++) {
		ret = snprintf(_b + len, _l - len, "%s%.*s%s=",
				CON_TQUOTESZ(_c), _k[i]->len, _k[i]->s, CON_TQUOTESZ(_c));
		if (ret < 0 || ret >= (_l - len)) goto error;
		len += ret;

		l = _l - len;
		if ( (*val2str)(_c, &(_v[i]), _b + len, &l) < 0) {
			LM_ERR("Error while converting value to string\n");
			return -1;
		}
		len += l;
		if (i != (_n - 1)) {
			if ((_l - len) >= 1) {
				*(_b + len++) = ',';
			}
		}
	}
	return len;

 error:
	LM_ERR("Error in snprintf\n");
	return -1;
}

/*
 * Convert db_val to pv_spec
 */
int db_val2pv_spec(struct sip_msg* msg, db_val_t *dbval, pv_spec_t *pvs)
{
	pv_value_t pv;
#define LL_LEN 21   /* sign, 19 digits and \0 */
	static char ll_buf[LL_LEN];

	if(dbval->nul)
	{
		pv.flags = PV_VAL_NULL;
	} else
	{
		switch(dbval->type)
		{
			case DB1_STRING:
				pv.flags = PV_VAL_STR;
				pv.rs.s = (char*)dbval->val.string_val;
				pv.rs.len = strlen(pv.rs.s);
			break;
			case DB1_STR:
				pv.flags = PV_VAL_STR;
				pv.rs.s = (char*)dbval->val.str_val.s;
				pv.rs.len = dbval->val.str_val.len;
			break;
			case DB1_BLOB:
				pv.flags = PV_VAL_STR;
				pv.rs.s = (char*)dbval->val.blob_val.s;
				pv.rs.len = dbval->val.blob_val.len;
			break;
			case DB1_INT:
				pv.flags = PV_VAL_INT | PV_TYPE_INT;
				pv.ri = (int)dbval->val.int_val;
			break;
			case DB1_DATETIME:
				pv.flags = PV_VAL_INT | PV_TYPE_INT;
				pv.ri = (int)dbval->val.time_val;
			break;
			case DB1_BITMAP:
				pv.flags = PV_VAL_INT | PV_TYPE_INT;
				pv.ri = (int)dbval->val.bitmap_val;
			break;
			case DB1_BIGINT:
				/* BIGINT is stored as string */
				pv.flags = PV_VAL_STR;
				pv.rs.len = LL_LEN;
				db_longlong2str(dbval->val.ll_val, ll_buf, &pv.rs.len);
				pv.rs.s = ll_buf;
				/* if it fits, also store as 32 bit integer*/
				if (! ((unsigned long long)dbval->val.ll_val & 0xffffffff00000000ULL)) {
					pv.flags |= PV_VAL_INT | PV_TYPE_INT;
					pv.ri = (int)dbval->val.ll_val;
				}
			break;
			default:
				LM_NOTICE("unknown field type: %d, setting value to null\n",
				          dbval->type);
				pv.flags = PV_VAL_NULL;
		}
	}

	/* null values are ignored for avp type PV */
	if (pv.flags == PV_VAL_NULL && pvs->type == PVT_AVP)
		return 0;

	/* add value to result pv */
	if (pv_set_spec_value(msg, pvs, 0, &pv) != 0)
	{
		LM_ERR("Failed to add value to spec\n");
		return -1;
	}

	return 0;
}
