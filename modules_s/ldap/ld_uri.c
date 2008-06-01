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
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

/** \addtogroup ldap
 * @{ 
 */

/** \file 
 * The implementation of parser parsing ldap:.. URIs.
 */

#include "ld_uri.h"

#include "../../mem/mem.h"
#include "../../ut.h"

#include <string.h>

/** compare s1 & s2  with a function f (which should return 0 if ==);
 * s1 & s2 can be null
 * return 0 if match, 1 if not 
 */
#define cmpstr(s1, s2, f) \
	((s1)!=(s2)) && ((s1)==0 || (s2)==0 || (f)((s1), (s2))!=0)


/** Compares two LDAP connection URIs.
 * This function is called whenever the database abstraction layer in
 * SER needs to compare to URIs with the ldap scheme. The function
 * compares hosts and port numbers of both URIs (host part comparison
 * is case insensitive). The URI comparison is mainly used to
 * by the connection pool to determine if a connection to a given
 * server already exists.
 **/
static unsigned char ld_uri_cmp(db_uri_t* uri1, db_uri_t* uri2)
{
	struct ld_uri* luri1, *luri2;

	if (!uri1 || !uri2) return 0;

	luri1 = DB_GET_PAYLOAD(uri1);
	luri2 = DB_GET_PAYLOAD(uri2);

	if (luri1->ldap_url->lud_port != luri2->ldap_url->lud_port) return 0;
	if (cmpstr(luri1->ldap_url->lud_host, 
			   luri2->ldap_url->lud_host, strcasecmp)) 
		return 0;
	return 1;
}


static void ld_uri_free(db_uri_t* uri, struct ld_uri* payload)
{
	if (payload == NULL) return;
	if (payload->ldap_url) ldap_free_urldesc(payload->ldap_url);
	if (payload->uri) pkg_free(payload->uri);
	db_drv_free(&payload->drv);
	pkg_free(payload);
}


int ld_uri(db_uri_t* uri)
{
	struct ld_uri* luri;

	luri = (struct ld_uri*)pkg_malloc(sizeof(struct ld_uri));
	if (luri == NULL) {
		ERR("ldap: No memory left\n");
		goto error;
	}
	memset(luri, '\0', sizeof(struct ld_uri));
	if (db_drv_init(&luri->drv, ld_uri_free) < 0) goto error;

	luri->uri = pkg_malloc(uri->scheme.len + 1 + uri->body.len + 1);
	if (luri->uri== NULL) {
		ERR("ldap: No memory left\n");
		goto error;
	}
	memcpy(luri->uri, uri->scheme.s, uri->scheme.len);
	luri->uri[uri->scheme.len] = ':';
	memcpy(luri->uri + uri->scheme.len + 1, uri->body.s, uri->body.len);
	luri->uri[uri->scheme.len + 1 + uri->body.len] = '\0';

	if (ldap_url_parse(luri->uri, &luri->ldap_url) != 0) {
		ERR("ldap: Error while parsing URL '%s'\n", luri->uri);
		goto error;
	}

	DB_SET_PAYLOAD(uri, luri);
	uri->cmp = ld_uri_cmp;
	return 0;

 error:
	if (luri) {
		if (luri->uri) pkg_free(luri->uri);
		if (luri->ldap_url) ldap_free_urldesc(luri->ldap_url);
		db_drv_free(&luri->drv);
		pkg_free(luri);
	}
	return -1;	
}


/** @} */
