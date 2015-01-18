/*
 * db_berkeley module, portions of this code were templated using
 * the dbtext and postgres modules.

 * Copyright (C) 2007 Cisco Systems
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
 */

/*! \file
 * Berkeley DB : 
 *
 * \ingroup database
 */


#ifndef _KM_BDB_VAL_H_
#define _KM_BDB_VAL_H_

#include "../../lib/srdb1/db_op.h"
#include "../../lib/srdb1/db_res.h"
#include "../../lib/srdb1/db_con.h"

int km_bdb_val2str(db_val_t* _v, char* _s, int* _len);
int bdb_str2val(db_type_t _t, db_val_t* _v, char* _s, int _l);

#endif

