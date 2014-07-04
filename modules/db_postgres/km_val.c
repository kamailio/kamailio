/*
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History
 * -------
 * 2003-04-06 initial code written (Greg Fausak/Andy Fullford)
 * 2003-04-14 gmtime changed to localtime because mktime later
 *            expects localtime, changed daylight saving bug
 *            previously found in mysql module (janakj)
 */

/*! \file
 *  \brief DB_POSTGRES :: Core
 *  \ingroup db_postgres
 *  Module: \ref db_postgres
 */

#include "../../lib/srdb1/db_val.h"
#include "../../lib/srdb1/db_ut.h"
#include "../../dprint.h"
#include "km_pg_con.h"

#include "../../mem/mem.h"
#include "km_val.h"


/*!
 * \brief Convert a str to a db value, copy strings
 *
 * Convert a str to a db value, copy strings.
 * The postgresql module uses a custom escape function for BLOBs.
 * If the _s is linked in the db_val result, it will be returned zero
 * \param _t destination value type
 * \param _v destination value
 * \param _s source string
 * \param _l string length
 * \return 0 on success, negative on error
 */
int db_postgres_str2val(const db_type_t _t, db_val_t* _v, const char* _s,
		const int _l)
{
	/* use common function for non BLOB, NULL setting and input
	 * parameter checking */
	if ( _t != DB1_BLOB || _s == NULL || _v == NULL) {
		return db_str2val(_t, _v, _s, _l, 1);
	} else {
		char * tmp_s = NULL;
		LM_DBG("converting BLOB [%.*s]\n", _l, _s);
		/*
		 * The string is stored in new allocated memory, which we could
		 * not free later thus we need to copy it to some new memory here.
		 */
		tmp_s = (char*)PQunescapeBytea((unsigned char*)_s,
					(size_t*)(void*)&(VAL_BLOB(_v).len));
		if(tmp_s==NULL) {
			LM_ERR("PQunescapeBytea failed\n");
			return -7;
		}
		VAL_BLOB(_v).s = pkg_malloc(VAL_BLOB(_v).len + 1);
		if (VAL_BLOB(_v).s == NULL) {
			LM_ERR("no private memory left\n");
			PQfreemem(tmp_s);
			return -8;
		}
		LM_DBG("allocate %d+1 bytes memory for BLOB at %p",
				VAL_BLOB(_v).len, VAL_BLOB(_v).s);
		memcpy(VAL_BLOB(_v).s, tmp_s, VAL_BLOB(_v).len);
		PQfreemem(tmp_s);

		VAL_BLOB(_v).s[VAL_BLOB(_v).len] = '\0';
		VAL_TYPE(_v) = DB1_BLOB;
		VAL_FREE(_v) = 1;

		LM_DBG("got blob len %d\n", _l);
		return 0;

	}
}


/*!
 * \brief Converting a value to a string
 *
 * Converting a value to a string, used when converting result from a query
 * \param _con database connection
 * \param _v source value
 * \param _s target string
 * \param _len target string length
 * \return 0 on success, negative on error
 */
int db_postgres_val2str(const db1_con_t* _con, const db_val_t* _v, char* _s, int* _len)
{
	int l, ret, tmp;
	int pgret;
	char *tmp_s;
	size_t tmp_len;
	char* old_s;

	tmp = db_val2str(_con, _v, _s, _len);
	if (tmp < 1)
		return tmp;

	switch(VAL_TYPE(_v)) {
	case DB1_STRING:
		l = strlen(VAL_STRING(_v));
		if (*_len < (l * 2 + 3)) {
			LM_ERR("destination buffer too short for string\n");
			return -6;
		} else {
			old_s = _s;
			*_s++ = '\'';
			ret = PQescapeStringConn(CON_CONNECTION(_con), _s, VAL_STRING(_v),
					l, &pgret);
			if(pgret!=0)
			{
				LM_ERR("PQescapeStringConn failed\n");
				return -6;
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

	case DB1_STR:
		l = VAL_STR(_v).len;
		if (*_len < (l * 2 + 3)) {
			LM_ERR("destination buffer too short for str\n");
			return -7;
		} else {
			old_s = _s;
			*_s++ = '\'';
			ret = PQescapeStringConn(CON_CONNECTION(_con), _s, VAL_STRING(_v),
					l, &pgret);
			if(pgret!=0)
			{
				LM_ERR("PQescapeStringConn failed \n");
				return -7;
			}
	        LM_DBG("PQescapeStringConn: in: %d chars, out: %d chars\n", l, ret);
			_s += ret;
			*_s++ = '\'';
			*_s = '\0'; /* FIXME */
			*_len = _s - old_s;
			return 0;
		}
		break;

	case DB1_BLOB:
		l = VAL_BLOB(_v).len;
		/* this estimation is not always correct, thus we need to check later again */
		if (*_len < (l * 2 + 3)) {
			LM_ERR("destination buffer too short for blob\n");
			return -9;
		} else {
			*_s++ = '\'';
			tmp_s = (char*)PQescapeByteaConn(CON_CONNECTION(_con), (unsigned char*)VAL_STRING(_v),
					(size_t)l, (size_t*)&tmp_len);
			if(tmp_s==NULL)
			{
				LM_ERR("PQescapeByteaConn failed\n");
				return -9;
			}
			if (tmp_len > *_len) {
				LM_ERR("escaped result too long\n");
				return -9;
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
		return -10;
	}
}
