/* 
 * $Id$ 
 *
 * UNIXODBC module
 *
 * Copyright (C) 2005-2006 Marco Lorrai
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 * History:
 * --------
 *  2005-12-01  initial commit (chgen)
 */


#include "../../dprint.h"
#include "../../lib/kcore/strcommon.h"
#include "../../lib/srdb1/db_ut.h"
#include "db_unixodbc.h"
#include "val.h"
#include "connection.h"

/*
 * Used when converting the query to a result
 */
int db_unixodbc_str2val(const db_type_t _t, db_val_t* _v, const char* _s, const int _l,
		const unsigned int _cpy)
{
	/* db_unixodbc uses the NULL string for NULL SQL values */
	if (_v && _s && !strcmp(_s, "NULL")) {
		LM_DBG("converting NULL value");
		static str dummy_string = {"", 0};
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
	} else {
		return db_str2val(_t, _v, _s, _l, _cpy);
	}
}

/*
 * Used when converting a result from the query
 */
int db_unixodbc_val2str(const db1_con_t* _c, const db_val_t* _v, char* _s, int* _len)
{
	int l, tmp;
	char* old_s;

	/* db_unixodbc uses a custom escape function */
	tmp = db_val2str(_c, _v, _s, _len);
	if (tmp < 1)
		return tmp;

	switch(VAL_TYPE(_v))
	{
		case DB1_STRING:
			l = strlen(VAL_STRING(_v));
			if (*_len < (l * 2 + 3))
			{
				LM_ERR("destination buffer too short\n");
				return -6;
			}
			else
			{
				old_s = _s;
				*_s++ = '\'';
				if(use_escape_common)
				{
					_s += escape_common(_s, (char*)VAL_STRING(_v), l);
				} else {
					memcpy(_s, VAL_STRING(_v), l);
					_s += l;
				}
				*_s++ = '\'';
				*_s = '\0'; /* FIXME */
				*_len = _s - old_s;
				return 0;
			}
			break;

		case DB1_STR:
			l = VAL_STR(_v).len;
			if (*_len < (l * 2 + 3))
			{
				LM_ERR("destination buffer too short\n");
				return -7;
			}
			else
			{
				old_s = _s;
				*_s++ = '\'';
				if(use_escape_common)
				{
					_s += escape_common(_s, VAL_STR(_v).s, l);
				} else {
					memcpy(_s, VAL_STR(_v).s, l);
					_s += l;
				}
				*_s++ = '\'';
				*_s = '\0'; /* FIXME */
				*_len = _s - old_s;
				return 0;
			}
			break;

		case DB1_BLOB:
			l = VAL_BLOB(_v).len;
			if (*_len < (l * 2 + 3))
			{
				LM_ERR("destination buffer too short\n");
				return -9;
			}
			else
			{
				old_s = _s;
				*_s++ = '\'';
				if(use_escape_common)
				{
					_s += escape_common(_s, VAL_BLOB(_v).s, l);
				} else {
					memcpy(_s, VAL_BLOB(_v).s, l);
					_s += l;
				}
				*_s++ = '\'';
				*_s = '\0'; /* FIXME */
				*_len = _s - old_s;
				return 0;
			}
			break;

		default:
			LM_DBG("unknown data type\n");
			return -10;
	}
}
