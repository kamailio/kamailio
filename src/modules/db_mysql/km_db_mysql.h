/* 
 * MySQL module interface
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
 *
 */

/*! \file
 *  \brief DB_MYSQL :: Core
 *  \ingroup db_mysql
 *  Module: \ref db_mysql
 */


#ifndef KM_DB_MOD_H
#define KM_DB_MOD_H

#include "../../lib/srdb1/db.h"

extern unsigned int db_mysql_timeout_interval;
extern unsigned int db_mysql_auto_reconnect;
extern unsigned int db_mysql_insert_all_delayed;
extern unsigned int db_mysql_update_affected_found;

int db_mysql_bind_api(db_func_t *dbb);

int kam_mysql_mod_init(void);

/**
 * Allocate a buffer for database module
 * No function should be called before this
 * \return zero on success, negative value on failure
 */
int db_mysql_alloc_buffer(void);

#endif /* KM_DB_MOD_H */
