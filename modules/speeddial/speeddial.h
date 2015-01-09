/*
 * Copyright (C) 2004-2006 Voice Sistem SRL
 *
 * This file is part of Kamailio.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * 
 */


#ifndef _SPEEDDIAL_H_
#define _SPEEDDIAL_H_

#include "../../lib/srdb1/db.h"
#include "../../parser/msg_parser.h"


/* Module parameters variables */

extern str user_column;     /* 'username' column name */
extern str domain_column;   /* 'domain' column name */
extern str sd_user_column;     /* 'sd_username' column name */
extern str sd_domain_column;   /* 'sd_domain' column name */
extern str new_uri_column;   /* 'new_uri' column name */
extern int   use_domain;      /* use or not the domain for sd lookup */
extern str   dstrip_s;

extern db_func_t db_funcs;    /* Database functions */
extern db1_con_t* db_handle;   /* Database connection handle */

#endif /* _SPEEDDIAL_H_ */
