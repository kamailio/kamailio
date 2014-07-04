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

/** \addtogroup ldap
 * @{
 */

/** \file
 * Functions related to connections to LDAP servers.
 */

#include "ld_con.h"
#include "ld_uri.h"

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"

#include <ldap.h>
#include <stdlib.h>
#include <string.h>
#include <sasl/sasl.h>

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


int lutil_sasl_interact(
	LDAP *ld,
	unsigned flags,
	void *defaults,
	void *in )
{
	sasl_interact_t *interact = in;
	const char *dflt = interact->defresult;


	if (ld == NULL)
		return LDAP_PARAM_ERROR;

	while (interact->id != SASL_CB_LIST_END) {
		switch( interact->id ) {
			// the username to authenticate
			case SASL_CB_AUTHNAME:
				if (defaults)
					dflt = ((struct ld_uri*)defaults)->username;
				break;
			// the password for the provided username
			case SASL_CB_PASS:
				if (defaults)
					dflt = ((struct ld_uri*)defaults)->password;
				break;
			// the realm for the authentication attempt
			case SASL_CB_GETREALM:
			// the username to use for proxy authorization
			case SASL_CB_USER:
			// generic prompt for input with input echoing disabled
			case SASL_CB_NOECHOPROMPT:
			// generic prompt for input with input echoing enabled
			case SASL_CB_ECHOPROMPT:
				break;
		}

		interact->result = (dflt && *dflt) ? dflt : "";
		interact->len = strlen(interact->result);

		interact++;
	}

	return LDAP_SUCCESS;
}


int ld_con_connect(db_con_t* con)
{
	struct ld_con* lcon;
	struct ld_uri* luri;
	int ret, version = 3;
	char* err_str = NULL;

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

	/* we pass the TLS_REQCERT and TLS_REQCERT attributes over environment
	   variables to ldap library */
	if (luri->tls) {
		if (setenv("LDAPTLS_CACERT", luri->ca_list, 1)) {
			ERR("ldap: Can't set environment variable 'LDAPTLS_CACERT'\n");
			goto error;
		}
		if (setenv("LDAPTLS_REQCERT", luri->req_cert, 1)) {
			ERR("ldap: Can't set environment variable 'LDAPTLS_REQCERT'\n");
			goto error;
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

	if (luri->tls) {
		ret = ldap_start_tls_s(lcon->con, NULL, NULL);
		if (ret != LDAP_SUCCESS) {
			/* get addition info of this error */
#ifdef OPENLDAP23
			ldap_get_option(lcon->con, LDAP_OPT_ERROR_STRING, &err_str);
#elif OPENLDAP24
			ldap_get_option(lcon->con, LDAP_OPT_DIAGNOSTIC_MESSAGE, &err_str);
#endif
			ERR("ldap: Error while starting TLS: %s\n", ldap_err2string(ret));
			if (err_str) {
				ERR("ldap: %s\n", err_str);
				ldap_memfree(err_str);
			}
			goto error;
		}
	}

	switch (luri->authmech) {
		case LDAP_AUTHMECH_NONE:
			ret = ldap_simple_bind_s(lcon->con, NULL, NULL);
			break;
		case LDAP_AUTHMECH_SIMPLE:
			ret = ldap_simple_bind_s(lcon->con, luri->username, luri->password);
			break;
		case LDAP_AUTHMECH_DIGESTMD5:
			ret = ldap_sasl_interactive_bind_s( lcon->con, NULL,
					LDAP_MECHANISM_STR_DIGESTMD5, NULL, NULL,
					0, lutil_sasl_interact, luri );
			break;
		case LDAP_AUTHMECH_EXTERNAL:
		default:
			ret = !LDAP_SUCCESS;
			break;
	}

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
