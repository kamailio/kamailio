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


#ifndef KM_VAL_H
#define KM_VAL_H

#include <mysql/mysql.h>
#include "../../lib/srdb1/db_val.h"
#include "../../lib/srdb1/db.h"


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
int db_mysql_val2str(const db1_con_t* _con, const db_val_t* _v, char* _s, int* _len);

#endif
