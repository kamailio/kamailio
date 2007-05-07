/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2007 1und1 Internet AG
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


#include "db_ut.h"

#include "../dprint.h"
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>


/*
 * Convert a string to integer
 */
inline int db_str2int(const char* _s, int* _v)
{
	long tmp;

	if (!_s || !_v) {
	       LOG(L_ERR, "ERROR:db_str2int: Invalid parameter value\n");
	       return -1;
	}

	tmp = strtoul(_s, 0, 10);
	if ((tmp == ULONG_MAX && errno == ERANGE) || 
	    (tmp < INT_MIN) || (tmp > UINT_MAX)) {
		LOG(L_ERR, "ERROR:db_str2int: Value out of range\n");
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
		LOG(L_ERR, "ERROR:db_str2double: Invalid parameter value\n");
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
		LOG(L_ERR, "ERROR:db_int2str: Invalid parameter value\n");
		return -1;
	}

	ret = snprintf(_s, *_l, "%-d", _v);
	if (ret < 0 || ret >= *_l) {
		LOG(L_ERR, "ERROR:db_int2str: error in sprintf\n");
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
		LOG(L_ERR, "ERROR:db_double2str: Invalid parameter value\n");
		return -1;
	}

	ret = snprintf(_s, *_l, "%-10.2f", _v);
	if (ret < 0 || ret >= *_l) {
		LOG(L_ERR, "ERROR:db_double2str: Error in snprintf\n");
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
	if ((!_s) || (!_v))
	{
		LOG(L_ERR, "ERROR:db_str2time: Invalid parameter value\n");
		return -1;
	}

	// Convert database time representation to time_t structure
	struct tm time;
	/* It is necessary to zero tm structure first */
	memset(&time, '\0', sizeof(struct tm));
	strptime(_s, "%Y-%m-%d %H:%M:%S", &time);

	/* Daylight saving information got lost in the database
	* so let mktime to guess it. This eliminates the bug when
	* contacts reloaded from the database have different time
	* of expiration by one hour when daylight saving is used
	*/ 
	time.tm_isdst = -1;
	*_v = mktime(&time);

	return 0;
}


/*
 * Convert time_t to string
 */
inline int db_time2str(time_t _v, char* _s, int* _l)
{
	int l;

	if ((!_s) || (!_l) || (*_l < 2))
	{
		LOG(L_ERR, "ERROR:db_time2str: Invalid parameter value\n");
		return -1;
	}

	*_s++ = '\'';

	// Convert time_t structure to format accepted by the database
	struct tm* t;
	t = localtime(&_v);
	l = strftime(_s, *_l -1, "%Y-%m-%d %H:%M:%S", t);

	*(_s + l) = '\'';
	*_l = l + 2;
	return 0;
}
