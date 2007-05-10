/* 
 * $Id$
 *
 * UNIXODBC module row related functions
 *
 * Copyright (C) 2005-2006 Marco Lorrai
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
 *
 * History:
 * --------
 *  2005-12-01  initial commit (chgen)
 *  2006-05-05  passing proper lengths of column data (sgupta)
 */

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../db/db_row.h"
#include "../../db/db_ut.h"
#include "my_con.h"
#include "val.h"
#include "row.h"

/*
 * Convert a row from result into db API representation
 */
int convert_row(db_con_t* _h, db_res_t* _res, db_row_t* _r, unsigned long* lengths)
{
	int i;

	if ((!_h) || (!_res) || (!_r))
	{
		LOG(L_ERR, "convert_row: Invalid parameter value\n");
		return -1;
	}

	ROW_VALUES(_r) = (db_val_t*)pkg_malloc(sizeof(db_val_t) * RES_COL_N(_res));
	ROW_N(_r) = RES_COL_N(_res);
	if (!ROW_VALUES(_r))
	{
		LOG(L_ERR, "convert_row: No memory left\n");
		return -1;
	}
	for(i = 0; i < RES_COL_N(_res); i++)
	{
		if (str2val(RES_TYPES(_res)[i], &(ROW_VALUES(_r)[i]),
			((CON_ROW(_h))[i]), lengths[i]) < 0)
		{
			LOG(L_ERR, "convert_row: Error while converting value\n");
			free_row(_r);
			return -3;
		}
	}
	return 0;
}
