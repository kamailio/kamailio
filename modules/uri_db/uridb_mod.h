/*
 * $Id$
 *
 * Various URI related functions
 *
 * Copyright (C) 2001-2004 FhG FOKUS
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
 * History:
 * --------
 * 2003-02-26: created by janakj
 */


#ifndef URIDB_MOD_H
#define URIDB_MOD_H

#include "../../lib/srdb1/db.h"
#include "../../str.h"

/*
 * Module parameters variables
 */
extern str db_table;                  /**< Name of URI table */
extern str uridb_user_col;            /**< Name of username column in URI table */
extern str uridb_domain_col;          /**< Name of domain column in URI table */
extern str uridb_uriuser_col;         /**< Name of uri_user column in URI table */
extern int use_uri_table;             /**< Whether or not should be uri table used */
extern int use_domain;                /**< Should does_uri_exist honor the domain part ? */

#endif /* URI_MOD_H */
