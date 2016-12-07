/* 
 * $Id$ 
 *
 * UNIXODBC module result related functions
 *
 * Copyright (C) 2005-2006 Marco Lorrai
 * Copyright (C) 2007-2008 1&1 Internet AG
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

#ifndef RES_H
#define RES_H

#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db_con.h"


/**
 * Fill the result structure with data from the database.
 * \param _h database handle
 * \param _r result structure
 * \return zero on success, negative on errors
 */
int db_unixodbc_convert_result(const db1_con_t* _h, db1_res_t* _r);
int db_unixodbc_get_columns(const db1_con_t* _h, db1_res_t* _r);

#endif /* RES_H */
