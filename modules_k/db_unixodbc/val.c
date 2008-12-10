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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * History:
 * --------
 *  2005-12-01  initial commit (chgen)
 */


#include "../../dprint.h"
#include "../../strcommon.h"
#include "../../db/db_ut.h"
#include "db_unixodbc.h"
#include "val.h"
#include "con.h"


#include <string.h>
#include <stdio.h>


/*
 * Used when converting result from a query
 */
int db_unixodbc_val2str(const db_con_t* _c, const db_val_t* _v, char* _s, int* _len)
{
	int l;
	char* old_s;

	if (!_c || !_v || !_s || !_len || !*_len)
	{
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (VAL_NULL(_v))
	{
		if (*_len < sizeof("NULL"))
		{
			LM_ERR("buffer too small\n");
			return -1;
		}
		*_len = snprintf(_s, *_len, "NULL");
		return 0;
	}

	switch(VAL_TYPE(_v))
	{
		case DB_INT:
			if (db_int2str(VAL_INT(_v), _s, _len) < 0)
			{
				LM_ERR("converting string to int failed\n");
				return -2;
			}
			else
			{
				return 0;
			}
			break;

		case DB_BIGINT:
			if (db_longlong2str(VAL_BIGINT(_v), _s, _len) < 0)
			{
				LM_ERR("converting string to big int failed\n");
				return -3;
			}
			else
			{
				return 0;
			}
			break;

		case DB_BITMAP:
			if (db_int2str(VAL_BITMAP(_v), _s, _len) < 0)
			{
				LM_ERR("converting string to int failed\n");
				return -4;
			}
			else
			{
				return 0;
			}
			break;

		case DB_DOUBLE:
			if (db_double2str(VAL_DOUBLE(_v), _s, _len) < 0)
			{
				LM_ERR("converting string to double failed\n");
				return -5;
			}
			else
			{
				return 0;
			}
			break;

		case DB_STRING:
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

		case DB_STR:
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

		case DB_DATETIME:
			if (db_time2str(VAL_TIME(_v), _s, _len) < 0)
			{
				LM_ERR("converting string to time_t failed\n");
				return -8;
			}
			else
			{
				return 0;
			}
			break;

		case DB_BLOB:
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
