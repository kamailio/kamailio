/*
 * $Id$
 *
 * Usrlocation module interface
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
 *
 * History:
 * ---------
 * 2003-03-12 added replication and state column (nils)
 */


#ifndef UL_MOD_H
#define UL_MOD_H


#include "../../db/db.h"
#include "../../str.h"


/*
 * Module parameters
 */


#define NO_DB         0
#define WRITE_THROUGH 1
#define WRITE_BACK    2

#define TABLE_VERSION 4 

extern str user_col;
extern str domain_col;
extern str contact_col;
extern str expires_col;
extern str q_col;
extern str callid_col;
extern str cseq_col;
extern str method_col;
extern str replicate_col;
extern str flags_col;
extern str state_col;
extern str db_url;
extern int timer_interval;
extern int db_mode;
extern int use_domain;
extern int desc_time_order;

extern db_con_t* ul_dbh;   /* Dabase connection handle */
extern db_func_t ul_dbf;


#endif /* UL_MOD_H */
