/*
 * $Id$
 *
 * POSTGRES module, portions of this code were templated using
 * the mysql module, thus it's similarity.
 *
 * Copyright (C) 2003 August.Net Services, LLC
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
 *
 * ---
 *
 * History
 * -------
 * 2003-04-06 initial code written (Greg Fausak/Andy Fullford)
 * 2003-04-14 gmtime changed to localtime because mktime later
 *            expects localtime, changed daylight saving bug
 *            previously found in mysql module (janakj)
 *
 */


#include "../../db/db_val.h"
#include "../../db/db_ut.h"
#include "../../dprint.h"
#include "defs.h"
#include "db_utils.h"
#include "pg_con.h"

#include "../../mem/mem.h"
#include "dbase.h"

#include <string.h>
#include <time.h>


/*
 * Convert time_t to string
 * postgresql stores dates with the timezone attached, so we can't use the
 * converter function from db/db_ut.h
 */
static inline int time2str(time_t _v, char* _s, int* _l)
{
	struct tm *t;
	int bl;
#ifdef PARANOID
	if ((!_s) || (!_l) || (*_l < 2))  {
		LOG(L_ERR, "PG[time2str]: Invalid parameter value\n");
		return -1;
	}
#endif

	t = localtime(&_v);

	if((bl=strftime(_s,(size_t)(*_l)-1,"'%Y-%m-%d %H:%M:%S %z'",t))>0)
		*_l = bl;
	
	return 0;
}

/*
 * Does not copy strings
 */
int pg_str2val(db_type_t _t, db_val_t* _v, const char* _s, int _l)
{

	static str dummy_string = {"", 0};

#ifdef PARANOID
	if (!_v) {
		LOG(L_ERR, "PG[str2val]: db_val_t parameter cannot be NULL\n");
	}
#endif

	if (!_s) {
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
	case DB_INT:
		LOG(L_DBG, "PG[str2val]: Converting INT [%s]\n", _s);
		if (db_str2int(_s, &VAL_INT(_v)) < 0) {
			LOG(L_ERR, "PG[str2val]: Error while converting INT value from string\n");
			return -2;
		} else {
			VAL_TYPE(_v) = DB_INT;
			return 0;
		}
		break;

	case DB_BITMAP:
		LOG(L_DBG, "PG[str2val]: Converting BITMAP [%s]\n", _s);
		if (db_str2int(_s, &VAL_INT(_v)) < 0) {
			LOG(L_ERR, "PG[str2val]: Error while converting BITMAP value from string\n");
			return -3;
		} else {
			VAL_TYPE(_v) = DB_BITMAP;
			return 0;
		}
		break;
	
	case DB_DOUBLE:
		LOG(L_DBG, "PG[str2val]: Converting DOUBLE [%s]\n", _s);
		if (db_str2double(_s, &VAL_DOUBLE(_v)) < 0) {
			LOG(L_ERR, "PG[str2val]: Error while converting DOUBLE value from string\n");
			return -4;
		} else {
			VAL_TYPE(_v) = DB_DOUBLE;
			return 0;
		}
		break;

	case DB_STRING:
		LOG(L_DBG, "PG[str2val]: Converting STRING [%s]\n", _s);	
		VAL_STRING(_v) = _s;
		VAL_TYPE(_v) = DB_STRING;
		return 0;

	case DB_STR:
		LOG(L_DBG, "PG[str2val]: Convertingg STR [%s]\n", _s);
		VAL_STR(_v).s = (char*)_s;
		VAL_STR(_v).len = _l;
		VAL_TYPE(_v) = DB_STR;
		return 0;

	case DB_DATETIME:
		LOG(L_DBG, "PG[str2val]: Converting DATETIME [%s]\n", _s);
		if (db_str2time(_s, &VAL_TIME(_v)) < 0) {
			LOG(L_ERR, "PG[str2val]: Error converting datetime\n");
			return -5;
		} else {
			VAL_TYPE(_v) = DB_DATETIME;
			return 0;
		}
		break;

	case DB_BLOB:
		LOG(L_DBG, "PG[str2val]: Converting BLOB [%s]\n", _s);
		/* PQunescapeBytea:  Converts a string representation of binary data into binary data — the reverse of PQescapeBytea.
		 * This is needed when retrieving bytea data in text format, but not when retrieving it in binary format.
		 */
		VAL_BLOB(_v).s = (char*)PQunescapeBytea((unsigned char*)_s, (size_t*)&(VAL_BLOB(_v).len) );
		VAL_TYPE(_v) = DB_BLOB;
		LOG(L_DBG, "PG[str2val]: got blob len %d\n", _l);
		return 0;
	}
	return -6;
}


/*
 * Used when converting result from a query
 */
int val2str(db_con_t* _con, db_val_t* _v, char* _s, int* _len)
{
	int l, ret;
	int pgret;
	char *tmp_s;
	size_t tmp_len;
	char* old_s;

#ifdef PARANOID
	if ((!_v) || (!_s) || (!_len) || (!*_len)) {
		LOG(L_ERR, "PG[val2str]: Invalid parameter value\n");
		return -1;
	}
#endif
	if (VAL_NULL(_v)) {
		*_len = snprintf(_s, *_len, "NULL");
		return 0;
	}
	
	switch(VAL_TYPE(_v)) {
	case DB_INT:
		if (db_int2str(VAL_INT(_v), _s, _len) < 0) {
			LOG(L_ERR, "PG[val2str]: Error while converting string to int\n");
			return -2;
		} else {
			return 0;
		}
		break;

	case DB_BITMAP:
		if (db_int2str(VAL_BITMAP(_v), _s, _len) < 0) {
			LOG(L_ERR, "PG[val2str]: Error while converting string to int\n");
			return -3;
		} else {
			return 0;
		}
		break;

	case DB_DOUBLE:
		if (db_double2str(VAL_DOUBLE(_v), _s, _len) < 0) {
			LOG(L_ERR,
					"PG[val2str]: Error while converting string to double\n");
			return -3;
		} else {
			return 0;
		}
		break;

	case DB_STRING:
		l = strlen(VAL_STRING(_v));
		if (*_len < (l * 2 + 3)) {
			LOG(L_ERR,
					"PG[val2str]: Destination buffer too short for string\n");
			return -4;
		} else {
			old_s = _s;
			*_s++ = '\'';
			ret = PQescapeStringConn(CON_CONNECTION(_con), _s, VAL_STRING(_v),
					l, &pgret);
			if(pgret!=0)
			{
				LOG(L_ERR,
					"PG[val2str]: error PQescapeStringConn\n");
				return -4;
			}
			LOG(L_DBG,
				"PG[val2str:DB_STRING]: PQescapeStringConn: in: %d chars,"
				" out: %d chars\n", l, ret);
			_s += ret;
			*_s++ = '\'';
			*_s = '\0'; /* FIXME */
			*_len = _s - old_s;
			return 0;
		}
		break;

	case DB_STR:
		l = VAL_STR(_v).len;
		if (*_len < (l * 2 + 3)) {
			LOG(L_ERR, "PG[val2str]: Destination buffer too short for str\n");
			return -5;
		} else {
			old_s = _s;
			*_s++ = '\'';
			ret = PQescapeStringConn(CON_CONNECTION(_con), _s, VAL_STRING(_v),
					l, &pgret);
			if(pgret!=0)
			{
				LOG(L_ERR,
					"PG[val2str]: error PQescapeStringConn\n");
				return -5;
			}
	        LOG(L_DBG,
				"PG[val2str:DB_STR]: PQescapeStringConn: in: %d chars,"
				" out: %d chars\n", l, ret);
			_s += ret;
			*_s++ = '\'';
			*_s = '\0'; /* FIXME */
			*_len = _s - old_s;
			return 0;
		}
		break;

	case DB_DATETIME:
		if (time2str(VAL_TIME(_v), _s, _len) < 0) {
			LOG(L_ERR,
				"PG[val2str]: Error while converting string to time_t\n");
			return -6;
		} else {
			return 0;
		}
		break;

	case DB_BLOB:
		l = VAL_BLOB(_v).len;
		if (*_len < (l * 2 + 3)) {
			LOG(L_ERR, "PG[val2str]: Destination buffer too short for blob\n");
			return -7;
		} else {
			*_s++ = '\'';
			tmp_s = (char*)PQescapeBytea((unsigned char*)VAL_STRING(_v),
					(size_t)l, (size_t*)&tmp_len);
			if(tmp_s==NULL)
			{
				LOG(L_ERR,
					"PG[val2str]: error PQescapeBytea\n");
				return -7;
			}
			memcpy(_s, tmp_s, tmp_len);
			PQfreemem(tmp_s);
			tmp_len = strlen(_s);
			*(_s + tmp_len) = '\'';
			*(_s + tmp_len + 1) = '\0';
			*_len = tmp_len + 2;
			return 0;
		}
		break;

	default:
		LOG(L_DBG, "PG[val2str]: Unknown data type\n");
		return -7;
	}
	return -8;
}
