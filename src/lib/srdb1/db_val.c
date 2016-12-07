/*
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2008-2009 1&1 Internet AG
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

#include "db_ut.h"

#include <stdio.h>
#include <time.h>

/*!
 * \brief Convert a str to a db value
 *
 * Convert a str to a db value, copy strings if _cpy is not zero.
 * Copying is not necessary if the result from the database client library
 * is freed after the result inside the server is processed. If the result
 * is freed earlier, e.g. because its saved inside some temporary storage,
 * then it must be copied in order to be use it reliable.
 *
 * \param _t destination value type
 * \param _v destination value
 * \param _s source string
 * \param _l string length
 * \param _cpy when set to zero does not copy strings, otherwise copy strings
 * \return 0 on success, negative on error
 */
int db_str2val(const db_type_t _t, db_val_t* _v, const char* _s, const int _l,
		const unsigned int _cpy)
{
	static str dummy_string = {"", 0};
	static char dummy_string_buf[2];
	
	if (!_v) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}
	/* A NULL string is a SQL NULL value, otherwise its an empty value */
	if (!_s) {
		LM_DBG("converting NULL value\n");
		memset(_v, 0, sizeof(db_val_t));
			/* Initialize the string pointers to a dummy empty
			 * string so that we do not crash when the NULL flag
			 * is set but the module does not check it properly
			 */
		dummy_string_buf[0] = '\0';
		dummy_string.s = dummy_string_buf;
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
		LM_DBG("converting INT [%s]\n", _s);
		if (db_str2int(_s, &VAL_INT(_v)) < 0) {
			LM_ERR("error while converting integer value from string\n");
			return -2;
		} else {
			VAL_TYPE(_v) = DB1_INT;
			return 0;
		}
		break;

	case DB1_BIGINT:
		LM_DBG("converting BIGINT [%s]\n", _s);
		if (db_str2longlong(_s, &VAL_BIGINT(_v)) < 0) {
			LM_ERR("error while converting big integer value from string\n");
			return -3;
		} else {
			VAL_TYPE(_v) = DB1_BIGINT;
			return 0;
		}
		break;

	case DB1_BITMAP:
		LM_DBG("converting BITMAP [%s]\n", _s);
		if (db_str2int(_s, &VAL_INT(_v)) < 0) {
			LM_ERR("error while converting bitmap value from string\n");
			return -4;
		} else {
			VAL_TYPE(_v) = DB1_BITMAP;
			return 0;
		}
		break;
	
	case DB1_DOUBLE:
		LM_DBG("converting DOUBLE [%s]\n", _s);
		if (db_str2double(_s, &VAL_DOUBLE(_v)) < 0) {
			LM_ERR("error while converting double value from string\n");
			return -5;
		} else {
			VAL_TYPE(_v) = DB1_DOUBLE;
			return 0;
		}
		break;

	case DB1_STRING:
		LM_DBG("converting STRING [%s]\n", _s);

		if (_cpy == 0) {
			VAL_STRING(_v) = _s;
		} else {
			VAL_STRING(_v) = pkg_malloc(_l + 1);
			if (VAL_STRING(_v) == NULL) {
				LM_ERR("no private memory left\n");
				return -6;
			}
			LM_DBG("allocate %d bytes memory for STRING at %p\n", _l + 1, VAL_STRING(_v));
			strncpy((char*)VAL_STRING(_v), _s, _l);
			((char*)VAL_STRING(_v))[_l] = '\0';
			VAL_FREE(_v) = 1;
		}

		VAL_TYPE(_v) = DB1_STRING;
		return 0;

	case DB1_STR:
		LM_DBG("converting STR [%.*s]\n", _l, _s);

		if (_cpy == 0) {
			VAL_STR(_v).s = (char*) _s;
		} else {
			VAL_STR(_v).s = pkg_malloc(_l);
			if (VAL_STR(_v).s == NULL) {
				LM_ERR("no private memory left\n");
				return -7;
			}
			LM_DBG("allocate %d bytes memory for STR at %p\n", _l, VAL_STR(_v).s);
			strncpy(VAL_STR(_v).s, _s, _l);
			VAL_FREE(_v) = 1;
		}

		VAL_STR(_v).len = _l;
		VAL_TYPE(_v) = DB1_STR;
		return 0;

	case DB1_DATETIME:
		LM_DBG("converting DATETIME [%s]\n", _s);
		if (db_str2time(_s, &VAL_TIME(_v)) < 0) {
			LM_ERR("error while converting datetime value from string\n");
			return -8;
		} else {
			VAL_TYPE(_v) = DB1_DATETIME;
			return 0;
		}
		break;

	case DB1_BLOB:
		LM_DBG("converting BLOB [%.*s]\n", _l, _s);

		if (_cpy == 0) {
			VAL_BLOB(_v).s = (char*) _s;
		} else {
			VAL_BLOB(_v).s = pkg_malloc(_l);
			if (VAL_BLOB(_v).s == NULL) {
				LM_ERR("no private memory left\n");
				return -9;
			}
			LM_DBG("allocate %d bytes memory for BLOB at %p\n", _l, VAL_BLOB(_v).s);
			strncpy(VAL_BLOB(_v).s, _s, _l);
			VAL_FREE(_v) = 1;
		}

		VAL_BLOB(_v).len = _l;
		VAL_TYPE(_v) = DB1_BLOB;
		return 0;

	default:
		return -10;
	}
	return -10;
}


/*!
 * \brief Convert a numerical value to a string
 *
 * Convert a numerical value to a string, used when converting result from a query.
 * Implement common functionality needed from the databases, does parameter checking.
 * \param _c database connection
 * \param _v source value
 * \param _s target string
 * \param _len target string length
 * \return 0 on success, negative on error, 1 if value must be converted by other means
 */
int db_val2str(const db1_con_t* _c, const db_val_t* _v, char* _s, int* _len)
{
	if (!_c || !_v || !_s || !_len || !*_len) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (VAL_NULL(_v)) {
		if (*_len < sizeof("NULL")) {
			LM_ERR("buffer too small\n");
			return -1;
		}
		*_len = snprintf(_s, *_len, "NULL");
		return 0;
	}
	
	switch(VAL_TYPE(_v)) {
	case DB1_INT:
		if (db_int2str(VAL_INT(_v), _s, _len) < 0) {
			LM_ERR("error while converting string to int\n");
			return -2;
		} else {
			return 0;
		}
		break;

	case DB1_BIGINT:
		if (db_longlong2str(VAL_BIGINT(_v), _s, _len) < 0) {
			LM_ERR("error while converting string to big int\n");
			return -3;
		} else {
			return 0;
		}
		break;

	case DB1_BITMAP:
		if (db_int2str(VAL_BITMAP(_v), _s, _len) < 0) {
			LM_ERR("error while converting string to int\n");
			return -4;
		} else {
			return 0;
		}
		break;

	case DB1_DOUBLE:
		if (db_double2str(VAL_DOUBLE(_v), _s, _len) < 0) {
			LM_ERR("error while converting string to double\n");
			return -5;
		} else {
			return 0;
		}
		break;

	case DB1_DATETIME:
		if (db_time2str(VAL_TIME(_v), _s, _len) < 0) {
			LM_ERR("failed to convert string to time_t\n");
			return -8;
		} else {
			return 0;
		}
		break;

	default:
		return 1;
	}
}
