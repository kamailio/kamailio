/*
 * $Id$
 *
 * Copyright (C) 2007,2008 TRUNK MOBILE
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
 */

#include <string.h>
#include <stdio.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "asynch.h"
#include "ora_con.h"

/*************************************************************************/
/*
 * Create a new connection structure,
 * open the Oracle connection and set reference count to 1
 */
ora_con_t* db_oracle_new_connection(const struct db_id* id)
{
	ora_con_t* con;
	char buf[512];
	size_t uri_len;
	sword status;

	if (!id || !id->username || !*id->username || !id->password ||
		!*id->password || !id->database || !*id->database)
	{
bad_param:
		LM_ERR("invalid parameter value\n");
		return NULL;
	}


	if (!id->host || !*id->host) {
		if (id->port) goto bad_param;
		uri_len = snprintf(buf, sizeof(buf), "%s", id->database);
	} else if (id->port) {
		uri_len = snprintf(buf, sizeof(buf), "%s:%u/%s",
				id->host, id->port, id->database);
	} else {
		uri_len = snprintf(buf, sizeof(buf), "%s/%s",
				id->host, id->database);
	}
	if (uri_len >= sizeof(buf)) goto bad_param;
	LM_DBG("opening connection: oracle://xxxx:xxxx@%s\n", buf);

	con = (ora_con_t*)pkg_malloc(sizeof(*con) + uri_len+1);
	if (!con) {
		LM_ERR("no private memory left\n");
		return NULL;
	}

	memset(con, 0, sizeof(*con));
	con->hdr.ref = 1;
	con->hdr.id = (struct db_id*)id;	/* set here - freed on error */
	con->uri_len = uri_len;
	memcpy(con->uri, buf, uri_len+1);

	if (   OCIEnvCreate(&con->envhp,
			OCI_DEFAULT | OCI_NEW_LENGTH_SEMANTICS,
            		NULL, NULL, NULL, NULL, 0, NULL) != OCI_SUCCESS
	    || OCIHandleAlloc(con->envhp, (dvoid**)(dvoid*)&con->errhp,
		    OCI_HTYPE_ERROR, 0, NULL) != OCI_SUCCESS
	    || OCIHandleAlloc(con->envhp, (dvoid**)(dvoid*)&con->srvhp,
		    OCI_HTYPE_SERVER, 0, NULL) != OCI_SUCCESS
	    || OCIHandleAlloc(con->envhp, (dvoid**)(dvoid*)&con->svchp,
		    OCI_HTYPE_SVCCTX, 0, NULL) != OCI_SUCCESS
	    || OCIHandleAlloc(con->envhp, (dvoid**)(dvoid*)&con->authp,
        	    OCI_HTYPE_SESSION, 0, NULL) != OCI_SUCCESS)
	{

		LM_ERR("no oracle memory left\n");
		db_oracle_free_connection(con);
		return NULL;
	}

	status = OCIAttrSet(con->svchp, OCI_HTYPE_SVCCTX, con->srvhp, 0,
			OCI_ATTR_SERVER, con->errhp);
	if (status != OCI_SUCCESS) goto connect_err;
	status = OCIAttrSet(con->authp, OCI_HTYPE_SESSION,
                 id->username, (ub4)strlen(id->username),
                 OCI_ATTR_USERNAME, con->errhp);
	if (status != OCI_SUCCESS) goto connect_err;
	status = OCIAttrSet(con->authp, OCI_HTYPE_SESSION,
                 id->password, (ub4)strlen(id->password),
                 OCI_ATTR_PASSWORD, con->errhp);
	if (status != OCI_SUCCESS) goto connect_err;
	status = OCIAttrSet(con->svchp, OCI_HTYPE_SVCCTX, con->authp, 0,
                   OCI_ATTR_SESSION, con->errhp);
	if (status != OCI_SUCCESS) goto connect_err;
	status = db_oracle_reconnect(con);
	if (status != OCI_SUCCESS) {
connect_err:
		if (   (status != OCI_ERROR && status != OCI_SUCCESS_WITH_INFO)
		    || OCIErrorGet(con->errhp, 1, NULL, &status, (OraText*)buf,
			    sizeof(buf), OCI_HTYPE_ERROR) != OCI_SUCCESS)
		{
			LM_ERR("internal driver error\n");
		} else {
			LM_ERR("driver: %s\n", buf);
		}
drop_connection:
		db_oracle_free_connection(con);
		return NULL;
	}

	// timelimited operation
	status = begin_timelimit(con, 0);
	if (status != OCI_SUCCESS) goto connect_err;
	do status = OCIServerVersion(con->svchp, con->errhp, (OraText*)buf,
		(ub4)sizeof(buf), OCI_HTYPE_SVCCTX);
	while (wait_timelimit(con, status));
	if (done_timelimit(con, status)) goto drop_connection;
	if (status != OCI_SUCCESS) goto connect_err;
	LM_INFO("server version is %s\n", buf);
	return con;
}


/*
 * Close the connection and release memory
 */
void db_oracle_free_connection(ora_con_t* con)
{
	if (!con) return;

	if (con->connected)
		db_oracle_disconnect(con);
	if (con->svchp)
		OCIHandleFree(con->svchp, OCI_HTYPE_SVCCTX);
	if (con->authp)
		OCIHandleFree(con->authp, OCI_HTYPE_SESSION);
	if (con->srvhp)
		OCIHandleFree(con->srvhp, OCI_HTYPE_SERVER);
	if (con->errhp)
		OCIHandleFree(con->errhp, OCI_HTYPE_ERROR);
	if (con->envhp)
		OCIHandleFree(con->envhp, OCI_HTYPE_ENV);
	free_db_id(con->hdr.id);
	pkg_free(con);
}


/*
 * Disconnect after network error
 */
void db_oracle_disconnect(ora_con_t* con)
{
	sword status;

	switch (con->connected) {
	default:
		status = OCISessionEnd(con->svchp, con->errhp, con->authp,
			OCI_DEFAULT);
		if (status != OCI_SUCCESS)
			LM_ERR("driver: %s\n", db_oracle_error(con, status));
	case 1:
		status = OCIServerDetach(con->srvhp, con->errhp, OCI_DEFAULT);
		if (status != OCI_SUCCESS)
			LM_ERR("driver: %s\n", db_oracle_error(con, status));
		con->connected = 0;
	case 0:
		break;
	}
}


/*
 * Reconnect to server (after error)
 */
sword db_oracle_reconnect(ora_con_t* con)
{
	sword status;

	if (con->connected)
		db_oracle_disconnect(con);

	/* timelimited operation, but OCI tcp-network does not support it :( */
	status = OCIServerAttach(con->srvhp, con->errhp, (OraText*)con->uri,
		con->uri_len, 0);
	if (status == OCI_SUCCESS) {
		++con->connected;
		/*
		 * timelimited operation, but OCI has BUG in asynch
		 * implementation of OCISessionBegin :(.
		 *
		 * Next code is 'empiric hack' that work (tested) in v10/v11.
		 */
		status = begin_timelimit(con, 1);
		if (status != OCI_SUCCESS) goto done;
		status = OCISessionBegin(con->svchp, con->errhp, con->authp,
			OCI_CRED_RDBMS, OCI_DEFAULT);
		while (wait_timelimit(con, status)) {
			sword code;

			status = OCIServerVersion(con->svchp, con->errhp, NULL,
				0, OCI_HTYPE_SVCCTX);

			if (   status != OCI_ERROR
			    || OCIErrorGet(con->errhp, 1, NULL, &code, NULL, 0,
				     OCI_HTYPE_ERROR) != OCI_SUCCESS) break;
			switch (code) {
			case 24909:	/* other call in progress */
				status = OCI_STILL_EXECUTING;
				continue;

			case 3127:	/* no new operation until active ends */
				status = OCISessionBegin(con->svchp, con->errhp,
					con->authp, OCI_CRED_RDBMS, OCI_DEFAULT);
			default:
				break;
			}
			break;
		}
		if (done_timelimit(con, status)) goto done;

		if (status == OCI_SUCCESS)
			++con->connected;
	}
done:
	return status;
}
