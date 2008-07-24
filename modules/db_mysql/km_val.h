/* 
 * $Id$ 
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2008 1&1 Internet AG
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
 *  \brief DB_MYSQL :: Conversions
 *  \ref val.c
 *  \ingroup db_mysql
 *  Module: \ref db_mysql
 */


#ifndef VAL_H
#define VAL_H

#include <mysql/mysql.h>
#include "../../db/db_val.h"
#include "../../db/db.h"


/**
 * Does not copy strings
 */
int db_mysql_str2val(const db_type_t _t, db_val_t* _v, const char* _s, const int _l);


/**
 * Used when converting result from a query
 */
int db_mysql_val2str(const db_con_t* _con, const db_val_t* _v, char* _s, int* _len);

#endif /* VAL_H */
