/*
 * $Id$
 *
 * DBText library
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * DBText library
 *   
 * 2003-02-05 created by Daniel
 * 
 */


#ifndef _DBT_API_H_
#define _DBT_API_H_

#include "../../db/db_op.h"
#include "../../db/db_res.h"

int free_columns(db_res_t* _r);

/*
 * Release memory used by row
 */
int free_row(db_row_t* _r);

/*
 * Release memory used by rows
 */
int free_rows(db_res_t* _r);

/*
 * Release memory used by a result structure
 */
int free_result(db_res_t* _r);

/*
 * Retrieve result set
 */
int get_result(db_con_t* _h, db_res_t** _r);

/*
 * Get and convert columns from a result
 */
int get_columns(db_con_t* _h, db_res_t* _r);

/*
 * Convert rows from mysql to db API representation
 */
int convert_rows(db_con_t* _h, db_res_t* _r);

/*
 * Convert a row from result into db API representation
 */
int convert_row(db_con_t* _h, db_res_t* _res, db_row_t* _r);

#endif
