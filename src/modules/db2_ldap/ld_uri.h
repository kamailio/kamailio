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
 * version.
 *
 * SER is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _LD_URI_H
#define _LD_URI_H

/** \addtogroup ldap
 * @{
 */

/** \file
 * The functions parsing and interpreting ldap: URIs.
 */

#include "../../lib/srdb2/db_uri.h"
#include "../../lib/srdb2/db_drv.h"

#include <ldap.h>

enum auth_type {
	LDAP_AUTHMECH_NONE = 0,
	LDAP_AUTHMECH_SIMPLE,
	LDAP_AUTHMECH_DIGESTMD5,
	LDAP_AUTHMECH_EXTERNAL
};

#define LDAP_MECHANISM_STR_DIGESTMD5 "digest-md5"
#define LDAP_MECHANISM_STR_EXTERNAL "external"



/** LDAP driver specific payload to attach to db_uri structures.
 * This is the LDAP specific structure that will be attached
 * to generic db_uri structures in the database API in SER. The
 * structure contains parsed elements of the ldap: URI.
 */
struct ld_uri {
	db_drv_t drv;
	char* username;
	char* password;
	char* uri;             /**< The whole URI, including scheme */
	int authmech;
	int tls;  /**<  TLS encryption enabled */
	char* ca_list;  /**< Path of the file that contains certificates of the CAs */
	char* req_cert;  /**< LDAP level of certificate request behaviour */
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
