/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2007-2008 1&1 Internet AG
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file db/db_ut.c
 * \brief Utility functions for database drivers.
 *
 * This utility methods are used from the database SQL driver to convert
 * values and print SQL queries from the internal API representation.
 */

#include "db_ut.h"

#include "../mem/mem.h"
#include "../dprint.h"
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


inline int db_str2int(const char* _s, int* _v)
{
	long tmp;

	if (!_s || !_v) {
	       LM_ERR("Invalid parameter value\n");
	       return -1;
	}

	tmp = strtoul(_s, 0, 10);
	if ((tmp == ULONG_MAX && errno == ERANGE) || 
	    (tmp < INT_MIN) || (tmp > UINT_MAX)) {
		LM_ERR("Value out of range\n");
		return -1;
	}

	*_v = (int)tmp;
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
 * Convert a double to string
 */
inline int db_double2str(double _v, char* _s, int* _l)
{
	int ret;

	if ((!_s) || (!_l) || (!*_l)) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	ret = snprintf(_s, *_l, "%-10.2f", _v);
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
inline int db_print_columns(char* _b, const int _l, const db_key_t* _c, const int _n)
{
	int i, ret, len = 0;

	if ((!_c) || (!_n) || (!_b) || (!_l)) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	for(i = 0; i < _n; i++)	{
		if (i == (_n - 1)) {
			ret = snprintf(_b + len, _l - len, "%.*s ", _c[i]->len, _c[i]->s);
			if (ret < 0 || ret >= (_l - len)) goto error;
			len += ret;
		} else {
			ret = snprintf(_b + len, _l - len, "%.*s,", _c[i]->len, _c[i]->s);
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
int db_print_values(const db_con_t* _c, char* _b, const int _l, const db_val_t* _v,
	const int _n, int (*val2str)(const db_con_t*, const db_val_t*, char*, int*))
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
int db_print_where(const db_con_t* _c, char* _b, const int _l, const db_key_t* _k,
	const db_op_t* _o, const db_val_t* _v, const int _n, int (*val2str)
	(const 	db_con_t*, const db_val_t*, char*, int*))
{
	int i, l, ret, len = 0;

	if (!_c || !_b || !_l || !_k || !_v || !_n) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	for(i = 0; i < _n; i++) {
		if (_o) {
			ret = snprintf(_b + len, _l - len, "%.*s%s", _k[i]->len, _k[i]->s, _o[i]);
			if (ret < 0 || ret >= (_l - len)) goto error;
			len += ret;
		} else {
			ret = snprintf(_b + len, _l - len, "%.*s=", _k[i]->len, _k[i]->s);
			if (ret < 0 || ret >= (_l - len)) goto error;
			len += ret;
		}
		l = _l - len;
		if ( (*val2str)(_c, &(_v[i]), _b + len, &l) < 0) {
			LM_ERR("Error while converting value to string\n");
			return -1;
		}
		len += l;
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
int db_print_set(const db_con_t* _c, char* _b, const int _l, const db_key_t* _k,
	const db_val_t* _v, const int _n, int (*val2str)(const db_con_t*,
	const db_val_t*,char*, int*))
{
	int i, l, ret, len = 0;

	if (!_c || !_b || !_l || !_k || !_v || !_n) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

	for(i = 0; i < _n; i++) {
		ret = snprintf(_b + len, _l - len, "%.*s=", _k[i]->len, _k[i]->s);
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
