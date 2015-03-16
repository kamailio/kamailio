/* 
 * Copyright (C) 2001-2003 FhG Fokus
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
 */

/*! \file
 *  \brief DB_MYSQL :: Data conversions
 *  \ingroup db_mysql
 *  Module: \ref db_mysql
 */

#include "../../dprint.h"
#include "../../lib/srdb1/db_ut.h"
#include "km_val.h"
#include "km_my_con.h"


/*!
 * \brief Converting a value to a string
 *
 * Converting a value to a string, used when converting result from a query
 * \param _c database connection
 * \param _v source value
 * \param _s target string
 * \param _len target string length
 * \return 0 on success, negative on error
 */
int db_mysql_val2str(const db1_con_t* _c, const db_val_t* _v, char* _s, int* _len)
{
	int l, tmp;
	char* old_s;

	tmp = db_val2str(_c, _v, _s, _len);
	if (tmp < 1)
		return tmp;

	switch(VAL_TYPE(_v)) {
	case DB1_STRING:
		l = strlen(VAL_STRING(_v));
		if (*_len < (l * 2 + 3)) {
			LM_ERR("destination buffer too short\n");
			return -6;
		} else {
			old_s = _s;
			*_s++ = '\'';
			_s += mysql_real_escape_string(CON_CONNECTION(_c), _s, VAL_STRING(_v), l);
			*_s++ = '\'';
			*_s = '\0'; /* FIXME */
			*_len = _s - old_s;
			return 0;
		}
		break;

	case DB1_STR:
		if (*_len < (VAL_STR(_v).len * 2 + 3)) {
			LM_ERR("destination buffer too short\n");
			return -7;
		} else {
			old_s = _s;
			*_s++ = '\'';
			_s += mysql_real_escape_string(CON_CONNECTION(_c), _s, VAL_STR(_v).s, VAL_STR(_v).len);
			*_s++ = '\'';
			*_s = '\0';
			*_len = _s - old_s;
			return 0;
		}
		break;

	case DB1_BLOB:
		l = VAL_BLOB(_v).len;
		if (*_len < (l * 2 + 3)) {
			LM_ERR("destination buffer too short\n");
			return -9;
		} else {
			old_s = _s;
			*_s++ = '\'';
			_s += mysql_real_escape_string(CON_CONNECTION(_c), _s, VAL_STR(_v).s, l);
			*_s++ = '\'';
			*_s = '\0';
			*_len = _s - old_s;
			return 0;
		}			
		break;

	default:
		LM_DBG("unknown data type\n");
		return -10;
	}
}
