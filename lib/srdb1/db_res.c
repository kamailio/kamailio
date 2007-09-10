/* 
 * $Id$ 
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2007 1und1 Internet AG
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
 */

#include "db_res.h"

#include "db_row.h"
#include "../dprint.h"
#include "../mem/mem.h"

/*
 * Release memory used by rows
 */
inline int db_free_rows(db_res_t* _r)
{
	int i;

	if (!_r) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}
	LM_DBG("freeing %d rows\n", RES_ROW_N(_r));

	for(i = 0; i < RES_ROW_N(_r); i++) {
		LM_DBG("row[%d]=%p\n", i, &(RES_ROWS(_r)[i]));
		db_free_row(&(RES_ROWS(_r)[i]));
	}
	if (RES_ROWS(_r)) {
		LM_DBG("%p=pkg_free() RES_ROWS\n", RES_ROWS(_r));
		pkg_free(RES_ROWS(_r));
		RES_ROWS(_r) = NULL;
	}
	return 0;
}
