/*
 * $Id$
 *
 * POSTGRES module, portions of this code were templated using
 * the mysql module, thus it's similarity.
 *
 * Copyright (C) 2003 August.Net Services, LLC
 * Copyright (C) 2008 1&1 Internet AG
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
#include "pg_con.h"

#include "../../mem/mem.h"
#include "val.h"

#include <string.h>
#include <time.h>


/*
 * Convert a str to a db value, does not copy strings
 * The postgresql module uses a custom escape function for BLOBs.
 * If the _s is linked in the db_val result, it will be returned zero
 */
int db_postgres_str2val(const db_type_t _t, db_val_t* _v, const char* _s, const int _l)
{
	static str dummy_string = {"", 0};

	if (!_v) {
		LM_ERR("invalid parameter value\n");
	}

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
		LM_DBG("converting INT [%s]\n", _s);
		if (db_str2int(_s, &VAL_INT(_v)) < 0) {
			LM_ERR("failed to convert INT value from string\n");
			return -2;
		} else {
			VAL_TYPE(_v) = DB_INT;
			return 0;
		}
		break;

	case DB_BITMAP:
		LM_DBG("converting BITMAP [%s]\n", _s);
		if (db_str2int(_s, &VAL_INT(_v)) < 0) {
			LM_ERR("failed to convert BITMAP value from string\n");
			return -3;
		} else {
			VAL_TYPE(_v) = DB_BITMAP;
			return 0;
		}
		break;
	
	case DB_DOUBLE:
		LM_DBG("converting DOUBLE [%s]\n", _s);
		if (db_str2double(_s, &VAL_DOUBLE(_v)) < 0) {
			LM_ERR("failed to convert DOUBLE value from string\n");
			return -4;
		} else {
			VAL_TYPE(_v) = DB_DOUBLE;
			return 0;
		}
		break;

	case DB_STRING:
		LM_DBG("converting STRING [%s]\n", _s);
		VAL_STRING(_v) = _s;
		VAL_TYPE(_v) = DB_STRING;
		VAL_FREE(_v) = 1;
		return 0;

	case DB_STR:
		LM_DBG("converting STR [%.*s]\n", _l, _s);
		VAL_STR(_v).s = (char*)_s;
		VAL_STR(_v).len = _l;
		VAL_TYPE(_v) = DB_STR;
		VAL_FREE(_v) = 1;
		_s = 0;
		return 0;

	case DB_DATETIME:
		LM_DBG("converting DATETIME [%s]\n", _s);
		if (db_str2time(_s, &VAL_TIME(_v)) < 0) {
			LM_ERR("failed to convert datetime\n");
			return -5;
		} else {
			VAL_TYPE(_v) = DB_DATETIME;
			return 0;
		}
		break;

	case DB_BLOB:
		LM_DBG("converting BLOB [%.*s]\n", _l, _s);
		/* PQunescapeBytea:  Converts a string representation of binary data
		 * into binary data - the reverse of PQescapeBytea.
		 * This is needed when retrieving bytea data in text format,
		 * but not when retrieving it in binary format.
		 */
		VAL_BLOB(_v).s = (char*)PQunescapeBytea((unsigned char*)_s, 
			(size_t*)(void*)&(VAL_BLOB(_v).len) );
		VAL_TYPE(_v) = DB_BLOB;
		VAL_FREE(_v) = 1;
		LM_DBG("got blob len %d\n", _l);
		return 0;
	}
	return -6;
}


/*
 * Used when converting result from a query
 */
int db_postgres_val2str(const db_con_t* _con, const db_val_t* _v, char* _s, int* _len)
{
	int l, ret;
	int pgret;
	char *tmp_s;
	size_t tmp_len;
	char* old_s;

	if ((!_v) || (!_s) || (!_len) || (!*_len)) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (VAL_NULL(_v)) {
		*_len = snprintf(_s, *_len, "NULL");
		return 0;
	}
	
	switch(VAL_TYPE(_v)) {
	case DB_INT:
		if (db_int2str(VAL_INT(_v), _s, _len) < 0) {
			LM_ERR("failed to convert string to int\n");
			return -2;
		} else {
			return 0;
		}
		break;

	case DB_BITMAP:
		if (db_int2str(VAL_BITMAP(_v), _s, _len) < 0) {
			LM_ERR("failed to convert string to int\n");
			return -3;
		} else {
			return 0;
		}
		break;

	case DB_DOUBLE:
		if (db_double2str(VAL_DOUBLE(_v), _s, _len) < 0) {
			LM_ERR("failed to convert string to double\n");
			return -3;
		} else {
			return 0;
		}
		break;

	case DB_STRING:
		l = strlen(VAL_STRING(_v));
		if (*_len < (l * 2 + 3)) {
			LM_ERR("destination buffer too short for string\n");
			return -4;
		} else {
			old_s = _s;
			*_s++ = '\'';
			ret = PQescapeStringConn(CON_CONNECTION(_con), _s, VAL_STRING(_v),
					l, &pgret);
			if(pgret!=0)
			{
				LM_ERR("PQescapeStringConn failed\n");
				return -4;
			}
			LM_DBG("PQescapeStringConn: in: %d chars,"
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
			LM_ERR("destination buffer too short for str\n");
			return -5;
		} else {
			old_s = _s;
			*_s++ = '\'';
			ret = PQescapeStringConn(CON_CONNECTION(_con), _s, VAL_STRING(_v),
					l, &pgret);
			if(pgret!=0)
			{
				LM_ERR("PQescapeStringConn failed \n");
				return -5;
			}
	        LM_DBG("PQescapeStringConn: in: %d chars, out: %d chars\n", l, ret);
			_s += ret;
			*_s++ = '\'';
			*_s = '\0'; /* FIXME */
			*_len = _s - old_s;
			return 0;
		}
		break;

	case DB_DATETIME:
		if (db_time2str(VAL_TIME(_v), _s, _len) < 0) {
			LM_ERR("failed to convert string to time_t\n");
			return -6;
		} else {
			return 0;
		}
		break;

	case DB_BLOB:
		l = VAL_BLOB(_v).len;
		/* this estimation is not always correct, thus we need to check later again */
		if (*_len < (l * 2 + 3)) {
			LM_ERR("destination buffer too short for blob\n");
			return -7;
		} else {
			*_s++ = '\'';
			tmp_s = (char*)PQescapeByteaConn(CON_CONNECTION(_con), (unsigned char*)VAL_STRING(_v),
					(size_t)l, (size_t*)&tmp_len);
			if(tmp_s==NULL)
			{
				LM_ERR("PQescapeBytea failed\n");
				return -7;
			}
			if (tmp_len > *_len) {
				LM_ERR("escaped result too long\n");
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
		LM_DBG("unknown data type\n");
		return -7;
	}
	return -8;
}
