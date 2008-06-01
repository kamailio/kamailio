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
 * Functions related to connections to LDAP servers.
 */

#define LDAP_DEPRECATED 1

#include "ld_con.h"
#include "ld_uri.h"

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"

#include <ldap.h>
#include <stdlib.h>
#include <string.h>


/** Free all memory allocated for a ld_con structure.
 * This function function frees all memory that is in use by
 * a ld_con structure.
 * @param con A generic db_con connection structure.
 * @param payload LDAP specific payload to be freed.
 */
static void ld_con_free(db_con_t* con, struct ld_con* payload)
{
	struct ld_uri* luri;
	int ret;
	if (!payload) return;

	luri = DB_GET_PAYLOAD(con->uri);

	/* Delete the structure only if there are no more references
	 * to it in the connection pool
	 */
	if (db_pool_remove((db_pool_entry_t*)payload) == 0) return;
	
	db_pool_entry_free(&payload->gen);
	if (payload->con) {
		ret = ldap_unbind_ext_s(payload->con, NULL, NULL);
		if (ret != LDAP_SUCCESS) {
			ERR("ldap: Error while unbinding from %s: %s\n", 
				luri->uri, ldap_err2string(ret));
		}
	}
	pkg_free(payload);
}


int ld_con(db_con_t* con)
{
	struct ld_con* lcon;
	struct ld_uri* luri;

	luri = DB_GET_PAYLOAD(con->uri);

	/* First try to lookup the connection in the connection pool and
	 * re-use it if a match is found
	 */
	lcon = (struct ld_con*)db_pool_get(con->uri);
	if (lcon) {
		DBG("ldap: Connection to %s found in connection pool\n",
			luri->uri);
		goto found;
	}

	lcon = (struct ld_con*)pkg_malloc(sizeof(struct ld_con));
	if (!lcon) {
		ERR("ldap: No memory left\n");
		goto error;
	}
	memset(lcon, '\0', sizeof(struct ld_con));
	if (db_pool_entry_init(&lcon->gen, ld_con_free, con->uri) < 0) goto error;

	DBG("ldap: Preparing new connection to %s\n", luri->uri);

	/* Put the newly created LDAP connection into the pool */
	db_pool_put((struct db_pool_entry*)lcon);
	DBG("ldap: Connection stored in connection pool\n");

 found:
	/* Attach driver payload to the db_con structure and set connect and
	 * disconnect functions
	 */
	DB_SET_PAYLOAD(con, lcon);
	con->connect = ld_con_connect;
	con->disconnect = ld_con_disconnect;
	return 0;

 error:
	if (lcon) {
		db_pool_entry_free(&lcon->gen);
		pkg_free(lcon);
	}
	return -1;
}


int ld_con_connect(db_con_t* con)
{
	struct ld_con* lcon;
	struct ld_uri* luri;
	int ret, version = 3;
	
	lcon = DB_GET_PAYLOAD(con);
	luri = DB_GET_PAYLOAD(con->uri);
	
	/* Do not reconnect already connected connections */
	if (lcon->flags & LD_CONNECTED) return 0;

	DBG("ldap: Connecting to %s\n", luri->uri);

	if (lcon->con) {
		ret = ldap_unbind_ext_s(lcon->con, NULL, NULL);
		if (ret != LDAP_SUCCESS) {
			ERR("ldap: Error while unbinding from %s: %s\n", 
				luri->uri, ldap_err2string(ret));
		}
	}

	ret = ldap_initialize(&lcon->con, luri->uri);
	if (lcon->con == NULL) {
		ERR("ldap: Error while initializing new LDAP connection to %s\n",
			luri->uri);
		goto error;
	}

	ret = ldap_set_option(lcon->con, LDAP_OPT_PROTOCOL_VERSION, &version);
	if (ret != LDAP_OPT_SUCCESS) {
		ERR("ldap: Error while setting protocol version 3: %s\n", 
			ldap_err2string(ret));
		goto error;
	}

	/* FIXME: Currently only anonymous binds are supported */
	ret = ldap_simple_bind_s(lcon->con, NULL, NULL);
	if (ret != LDAP_SUCCESS) {
		ERR("ldap: Bind to %s failed: %s\n", 
			luri->uri, ldap_err2string(ret));
		goto error;
	}

	DBG("ldap: Successfully bound to %s\n", luri->uri);
	lcon->flags |= LD_CONNECTED;
	return 0;

 error:
	if (lcon->con) {
		ret = ldap_unbind_ext_s(lcon->con, NULL, NULL);
		if (ret) {
			ERR("ldap: Error while unbinding from %s: %s\n", 
				luri->uri, ldap_err2string(ret));
		}
	}
	lcon->con = NULL;
	return -1;
}


void ld_con_disconnect(db_con_t* con)
{
	struct ld_con* lcon;
	struct ld_uri* luri;
	int ret;

	lcon = DB_GET_PAYLOAD(con);
	luri = DB_GET_PAYLOAD(con->uri);

	if ((lcon->flags & LD_CONNECTED) == 0) return;

	DBG("ldap: Unbinding from %s\n", luri->uri);

	if (lcon->con) {
		ret = ldap_unbind_ext_s(lcon->con, NULL, NULL);
		if (ret) {
			ERR("ldap: Error while unbinding from %s: %s\n", 
				luri->uri, ldap_err2string(ret));
		}
	}

	lcon->con = NULL;
	lcon->flags &= ~LD_CONNECTED;
}


/** @} */
