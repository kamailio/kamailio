/* 
 * $Id$ 
 *
 * MySQL module row related functions
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*! \file
 *  \brief DB_MYSQL :: Row related functions
 *  \ingroup db_mysql
 *  Module: \ref db_mysql
 */

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../db/db_row.h"
#include "../../db/db_ut.h"
#include "my_con.h"
#include "val.h"
#include "row.h"

/*!
 * \brief Convert a row from result into DB API representation
 * \param _h database connection
 * \param _res database result in the DB API representation
 * \param _r database result row
 * \return 0 on success, -1 on failure
 */
int db_mysql_convert_row(const db_con_t* _h, db_res_t* _res, db_row_t* _r)
{
	unsigned long* lengths;
	int i, len;

	if ((!_h) || (!_res) || (!_r)) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	len = sizeof(db_val_t) * RES_COL_N(_res);
	ROW_VALUES(_r) = (db_val_t*)pkg_malloc(len);
	if (!ROW_VALUES(_r)) {
		LM_ERR("no private memory left\n");
		return -1;
	}
	LM_DBG("allocate %d bytes for row values at %p\n", len,
			ROW_VALUES(_r));

	memset(ROW_VALUES(_r), 0, len);
	/* Save the number of columns in the ROW structure */
	ROW_N(_r) = RES_COL_N(_res);

	lengths = mysql_fetch_lengths(CON_RESULT(_h));

	for(i = 0; i < RES_COL_N(_res); i++) {
		if (db_mysql_str2val(RES_TYPES(_res)[i], &(ROW_VALUES(_r)[i]),
			    ((MYSQL_ROW)CON_ROW(_h))[i], lengths[i]) < 0) {
			LM_ERR("failed to convert value\n");
			LM_DBG("free row at %p\n", _r);
			db_free_row(_r);
			return -3;
		}
	}
	return 0;
}
