/*
 * $Id$
 *
 * Various URI related functions
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
 * --------
 * 2003-02-26: created by janakj
 */


#ifndef URI_MOD_H
#define URI_MOD_H

#include "../../db/db.h"
#include "../../str.h"
#include "../../parser/digest/digest.h" /* auth_body_t */
#include "../../parser/msg_parser.h"    /* struct sip_msg */


/*
 * Module parameters variables
 */
extern char* db_url;                    /* Database URL */

extern char* uri_table;                 /* Name of URI table */
extern char* uri_domain_column;         /* Name of domain column in URI table */
extern char* uri_uriuser_column;        /* Name of uri_user column in URI table */

extern char* subscriber_table;          /* Name of subscriber table */
extern char* subscriber_user_column;    /* Name of user column in subscriber table */
extern char* subscriber_domain_column;  /* Name of domain column in subscriber table */

extern int use_uri_table ;              /* Whether or not should be uri table used */

extern db_con_t* db_handle;   /* Database connection handle */

#endif /* URI_MOD_H */
