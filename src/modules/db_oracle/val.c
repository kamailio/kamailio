/*
 * $Id$
 *
 * Copyright (C) 2007,2008 TRUNK MOBILE
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


#include <stdio.h>
#include <string.h>
#include <time.h>
#include <oci.h>
#include "../../dprint.h"
#include "ora_con.h"
#include "dbase.h"
#include "val.h"


/*
 * Convert value to sql-string as db bind index
 */
int db_oracle_val2str(const db1_con_t* _c, const db_val_t* _v, char* _s, int* _len)
{
	int ret;

	if (!_c || !_v || !_s || !_len || *_len <= 0) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	ret = snprintf(_s, *_len, ":%u", ++CON_ORA(_c)->bindpos);
	if ((unsigned)ret >= (unsigned)*_len)
		return sql_buf_small();
	*_len = ret;
	return 0;
}

/*
 * Called after val2str to realy binding
 */
int db_oracle_val2bind(bmap_t* _m, const db_val_t* _v, OCIDate* _o)
{
	if (VAL_NULL(_v)) {
		_m->addr = NULL;
		_m->size = 0;
		_m->type = SQLT_NON;
		return 0;
	}

	switch (VAL_TYPE(_v)) {
	case DB1_INT:
		_m->addr = (int*)&VAL_INT(_v);
		_m->size = sizeof(VAL_INT(_v));
		_m->type = SQLT_INT;
		break;

	case DB1_BIGINT:
		LM_ERR("BIGINT not supported");
		return -1;

	case DB1_BITMAP:
		_m->addr = (unsigned*)&VAL_BITMAP(_v);
		_m->size = sizeof(VAL_BITMAP(_v));
		_m->type = SQLT_UIN;
		break;

	case DB1_DOUBLE:
		_m->addr = (double*)&VAL_DOUBLE(_v);
		_m->size = sizeof(VAL_DOUBLE(_v));
		_m->type = SQLT_FLT;
		break;

	case DB1_STRING:
		_m->addr = (char*)VAL_STRING(_v);
		_m->size = strlen(VAL_STRING(_v))+1;
		_m->type = SQLT_STR;
		break;

	case DB1_STR:
		{
			unsigned len = VAL_STR(_v).len;
			char *estr, *pstr = VAL_STR(_v).s;

			estr = (char*)memchr(pstr, 0, len);
			if (estr) {
				LM_WARN("truncate STR len from %u to: '%s'\n",
					len, pstr);
				len = (unsigned)(estr - pstr) + 1;
			}
			_m->size = len;
			_m->addr = pstr;
			_m->type = SQLT_CHR;
		}
		break;

	case DB1_DATETIME:
		{
			struct tm* tm = localtime(&VAL_TIME(_v));
			if (tm->tm_sec == 60)
				--tm->tm_sec;
			OCIDateSetDate(_o, (ub2)(tm->tm_year + 1900),
				(ub1)(tm->tm_mon + 1), (ub1)tm->tm_mday);
			OCIDateSetTime(_o, (ub1)tm->tm_hour, (ub1)tm->tm_min,
				(ub1)tm->tm_sec);
			_m->addr = _o;
			_m->size = sizeof(*_o);
			_m->type = SQLT_ODT;
		}
		break;

	case DB1_BLOB:
		_m->addr = VAL_BLOB(_v).s;
		_m->size = VAL_BLOB(_v).len;
		_m->type = SQLT_CLOB;
		break;

	default:
	    LM_ERR("unknown data type\n");
	    return -1;
	}
	return 0;
}
