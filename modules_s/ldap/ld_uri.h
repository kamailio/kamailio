/* 
 * $Id$ 
 *
 * LDAP Database Driver for SER
 *
 * Copyright (C) 2008 iptelorg GmbH
 *
 * This file is part of SER, a free SIP server.
 *
 * SER is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version
 *
 * For a license to use the ser software under conditions other than those
 * described here, or to purchase support for this software, please contact
 * iptel.org by e-mail at the following addresses: info@iptel.org
 *
 * SER is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef _LD_URI_H
#define _LD_URI_H

/** \addtogroup ldap
 * @{ 
 */

/** \file 
 * The functions parsing and interpreting ldap: URIs.
 */

#include "../../db/db_uri.h"
#include "../../db/db_drv.h"

#include <ldap.h>


/** LDAP driver specific payload to attach to db_uri structures.
 * This is the LDAP specific structure that will be attached
 * to generic db_uri structures in the database API in SER. The 
 * structure contains parsed elements of the ldap: URI.
 */
struct ld_uri {
	db_drv_t drv;
	char* uri;             /**< The whole URI, including scheme */
	LDAPURLDesc* ldap_url; /**< URI parsed by the ldap client library */
};


/** Create a new ld_uri structure and parse the URI in parameter.
 * This function builds a new ld_uri structure from the body of
 * the generic URI given to it in parameter.
 * @param uri A generic db_uri structure.
 * @retval 0 on success
 * @retval A negative number on error.
 */
int ld_uri(db_uri_t* uri);


/** @} */

#endif /* _LD_URI_H */
