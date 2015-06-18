/*
 * db_berkeley module, portions of this code were templated using
 * the dbtext and postgres modules.

 * Copyright (C) 2007 Cisco Systems
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
 
/*! \file
 * Berkeley DB : 
 *
 * \ingroup database
 */


#include "../../lib/srdb1/db_val.h"
#include "../../lib/srdb1/db_ut.h"
#include "km_db_berkeley.h"
#include "km_bdb_res.h"
#include "km_bdb_val.h"
#include <string.h>

/**
 * A copy of db_ut::db_time2str EXCEPT does not wrap the date in single-quotes
 *
 * Convert a time_t value to string (w.o single-quote)
 * \param _v source value
 * \param _s target string
 * \param _l available length and target length
 * \return -1 on error, 0 on success
 * \todo This functions add quotes to the time value. This
 * should be done in the val2str function, as some databases
 * like db_berkeley don't need or like this at all.
 */
inline int km_bdb_time2str(time_t _v, char* _s, int* _l)
{
	struct tm* t;
	int l;

	if ((!_s) || (!_l) || (*_l < 2)) {
		LM_ERR("Invalid parameter value\n");
		return -1;
	}

//	*_s++ = '\'';

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

//	*(_s + l) = '\'';
//	*_l = l + 2;
	return 0;
}

/**
 * Does not copy strings
 */
int bdb_str2val(db_type_t _t, db_val_t* _v, char* _s, int _l)
{

	static str dummy_string = {"", 0};

	if(!_s)
	{
		memset(_v, 0, sizeof(db_val_t));
		/* Initialize the string pointers to a dummy empty
		 * string so that we do not crash when the NULL flag
		 * is set but the module does not check it properly
		 */
		VAL_STRING(_v) = dummy_string.s;
		VAL_STR(_v) = dummy_string;
		VAL_BLOB(_v) = dummy_string;
		VAL_TYPE(_v) = _t;
		VAL_NULL(_v) = 1;
		return 0;
	}
	VAL_NULL(_v) = 0;

	switch(_t) {
	case DB1_INT:
		if (db_str2int(_s, &VAL_INT(_v)) < 0) {
			LM_ERR("Error while converting INT value from string\n");
			return -2;
		} else {
			VAL_TYPE(_v) = DB1_INT;
			return 0;
		}
		break;

	case DB1_BIGINT:
			LM_ERR("BIGINT not supported");
			return -1;

	case DB1_BITMAP:
		if (db_str2int(_s, &VAL_INT(_v)) < 0) {
			LM_ERR("Error while converting BITMAP value from string\n");
			return -3;
		} else {
			VAL_TYPE(_v) = DB1_BITMAP;
			return 0;
		}
		break;

	case DB1_DOUBLE:
		if (db_str2double(_s, &VAL_DOUBLE(_v)) < 0) {
			LM_ERR("Error while converting DOUBLE value from string\n");
			return -4;
		} else {
			VAL_TYPE(_v) = DB1_DOUBLE;
			return 0;
		}
		break;

	case DB1_STRING:
		VAL_STRING(_v) = _s;
		VAL_TYPE(_v) = DB1_STRING;
		VAL_FREE(_v) = 1;
		
		if( strlen(_s)==4 && !strncasecmp(_s, "NULL", 4) )
			VAL_NULL(_v) = 1;
		
		return 0;

	case DB1_STR:
		VAL_STR(_v).s = (char*)_s;
		VAL_STR(_v).len = _l;
		VAL_TYPE(_v) = DB1_STR;
		VAL_FREE(_v) = 1;

		if( strlen(_s)==4 && !strncasecmp(_s, "NULL", 4) )
			VAL_NULL(_v) = 1;

		return 0;

	case DB1_DATETIME:
		if (db_str2time(_s, &VAL_TIME(_v)) < 0) {
			LM_ERR("Error converting datetime\n");
			return -5;
		} else {
			VAL_TYPE(_v) = DB1_DATETIME;
			return 0;
		}
		break;

	case DB1_BLOB:
		VAL_BLOB(_v).s = _s;
		VAL_TYPE(_v) = DB1_BLOB;
		LM_DBG("got blob len %d\n", _l);
		return 0;
	default:
		break;
	}

	return -6;
}


/*
 * Used when converting result from a query
 */
int km_bdb_val2str(db_val_t* _v, char* _s, int* _len)
{
	int l;

	if (VAL_NULL(_v)) 
	{
		*_len = snprintf(_s, *_len, "NULL");
		return 0;
	}
	
	switch(VAL_TYPE(_v)) {
	case DB1_INT:
		if (db_int2str(VAL_INT(_v), _s, _len) < 0) {
			LM_ERR("Error while converting int to string\n");
			return -2;
		} else {
			LM_DBG("Converted int to string\n");
			return 0;
		}
		break;

	case DB1_BITMAP:
		if (db_int2str(VAL_INT(_v), _s, _len) < 0) {
			LM_ERR("Error while converting bitmap to string\n");
			return -3;
		} else {
			LM_DBG("Converted bitmap to string\n");
			return 0;
		}
		break;

	case DB1_DOUBLE:
		if (db_double2str(VAL_DOUBLE(_v), _s, _len) < 0) {
			LM_ERR("Error while converting double  to string\n");
			return -3;
		} else {
			LM_DBG("Converted double to string\n");
			return 0;
		}
		break;

	case DB1_STRING:
		l = strlen(VAL_STRING(_v));
		if (*_len < l ) 
		{	LM_ERR("Destination buffer too short for string\n");
			return -4;
		} 
		else 
		{	LM_DBG("Converted string to string\n");
			strncpy(_s, VAL_STRING(_v) , l);
			_s[l] = 0;
			*_len = l;
			return 0;
		}
		break;

	case DB1_STR:
		l = VAL_STR(_v).len;
		if (*_len < l) 
		{
			LM_ERR("Destination buffer too short for str\n");
			return -5;
		} 
		else 
		{
			LM_DBG("Converted str to string\n");
			strncpy(_s, VAL_STR(_v).s , VAL_STR(_v).len);
			*_len = VAL_STR(_v).len;
			return 0;
		}
		break;

	case DB1_DATETIME:
		if (km_bdb_time2str(VAL_TIME(_v), _s, _len) < 0) {
			LM_ERR("Error while converting time_t to string\n");
			return -6;
		} else {
			LM_DBG("Converted time_t to string\n");
			return 0;
		}
		break;

	case DB1_BLOB:
		l = VAL_BLOB(_v).len;
		if (*_len < l) 
		{
			LM_ERR("Destination buffer too short for blob\n");
			return -7;
		} 
		else 
		{
			LM_DBG("Converting BLOB [%s]\n", _s);
			_s = VAL_BLOB(_v).s;
			*_len = 0;
			return -8;
		}
		break;

	default:
		LM_DBG("Unknown data type\n");
		return -8;
	}
}
