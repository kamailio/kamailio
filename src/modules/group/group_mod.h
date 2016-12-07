/*
 * Copyright (C) 2001-2003 FhG Fokus
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

/**
 * \file
 * \brief Group membership module
 * \ingroup group
 * - Module: \ref group
 */


#ifndef GROUP_MOD_H
#define GROUP_MOD_H

#include "../../lib/srdb1/db.h"
#include "../../str.h"
#include "../../parser/digest/digest.h" /* auth_body_t */
#include "../../parser/msg_parser.h"    /* struct sip_msg */


/*
 * Module parameters variables
 */
extern str table;           /*!< 'group' table name */
extern str user_column;     /*!< 'user' column name in group table */
extern str domain_column;   /*!< 'domain' column name in group table */
extern str group_column;    /*!< "group' column name in group table */
extern int use_domain;      /*!< Use domain in is_user_in */

extern str re_table;
extern str re_exp_column;
extern str re_gid_column;
extern int multiple_gid;

/* DB functions and handlers */
extern db_func_t group_dbf;
extern db1_con_t* group_dbh;


#endif
