/* 
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*! \file
 *  \brief DB_MYSQL :: Row related functions
 *  \ref row.c
 *  \ingroup db_mysql
 *  Module: \ref db_mysql
 */


#ifndef KM_ROW_H
#define KM_ROW_H

#include "../../lib/srdb1/db_con.h"
#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db_row.h"


/*!
 * \brief Convert a row from result into DB API representation
 * \param _h database connection
 * \param _res database result in the DB API representation
 * \param _r database result row
 * \return 0 on success, -1 on failure
 */
int db_mysql_convert_row(const db1_con_t* _h, db1_res_t* _res, db_row_t* _r);

#endif
