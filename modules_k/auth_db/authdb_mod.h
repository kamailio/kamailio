/*
 * $Id$
 *
 * Digest Authentication - Database support
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef AUTHDB_MOD_H
#define AUTHDB_MOD_H

#include "../../str.h"
#include "../../db/db.h"
#include "../auth/api.h"
#include "../sl/sl_api.h"
#include "../../parser/msg_parser.h"
#include "../../db/module/db_auth_db.h"

/*
 * Module parameters variables
 */

extern int calc_ha1;          /* if set to 1, ha1 is calculated by the server */
extern int use_domain;        /* If set to 1 then the domain will be used when selecting a row */

extern auth_api_t auth_api;

extern struct aaa_avp* credentials;
extern int credentials_n;

/*
 * Pointer to reply function in stateless module
 */
extern struct sl_binds slb;

#endif /* AUTHDB_MOD_H */
