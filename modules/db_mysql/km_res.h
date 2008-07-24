/* 
 * $Id$ 
 *
 * MySQL module result related functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2007-2008 1&1 Internet AG
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

/*! \file
 *  \brief DB_MYSQL :: Result related functions
 *  \ref res.c
 *  \ingroup db_mysql
 *  Module: \ref db_mysql
 */


#ifndef RES_H
#define RES_H

#include "../../db/db_res.h"
#include "../../db/db_con.h"


/*
 * Fill the structure with data from database
 */
int db_mysql_convert_result(const db_con_t* _h, db_res_t* _r);

int db_mysql_get_columns(const db_con_t* _h, db_res_t* _r);

#endif /* RES_H */
