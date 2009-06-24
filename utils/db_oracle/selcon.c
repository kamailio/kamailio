#include "orasel.h"

//-----------------------------------------------------------------------------
void open_sess(con_t* con)
{
	sword status;

	if (   OCIEnvCreate(&con->envhp, OCI_DEFAULT | OCI_NEW_LENGTH_SEMANTICS,
			    NULL, NULL, NULL, NULL, 0, NULL) != OCI_SUCCESS
	    || OCIHandleAlloc(con->envhp, (dvoid**)(dvoid*)&con->errhp,
				OCI_HTYPE_ERROR, 0, NULL) != OCI_SUCCESS
	    || OCIHandleAlloc(con->envhp, (dvoid**)(dvoid*)&con->srvhp,
				OCI_HTYPE_SERVER, 0, NULL) != OCI_SUCCESS
	    || OCIHandleAlloc(con->envhp, (dvoid**)(dvoid*)&con->svchp,
				OCI_HTYPE_SVCCTX, 0, NULL) != OCI_SUCCESS
	    || OCIHandleAlloc(con->envhp, (dvoid**)(dvoid*)&con->authp,
				OCI_HTYPE_SESSION, 0, NULL) != OCI_SUCCESS
	    || OCIHandleAlloc(con->envhp, (dvoid**)(dvoid*)&con->stmthp,
				OCI_HTYPE_STMT, 0, NULL) != OCI_SUCCESS)
	{
		errxit("no oracle memory left");
	}

	status = OCIAttrSet(con->svchp, OCI_HTYPE_SVCCTX, con->srvhp, 0,
			OCI_ATTR_SERVER, con->errhp);
	if (status != OCI_SUCCESS) goto connect_err;
	status = OCIAttrSet(con->authp, OCI_HTYPE_SESSION,
			(text*)con->username->s, con->username->len,
			OCI_ATTR_USERNAME, con->errhp);
	if (status != OCI_SUCCESS) goto connect_err;
	status = OCIAttrSet(con->authp, OCI_HTYPE_SESSION,
			(text*)con->password->s, con->password->len,
			OCI_ATTR_PASSWORD, con->errhp);
	if (status != OCI_SUCCESS) goto connect_err;
	status = OCIAttrSet(con->svchp, OCI_HTYPE_SVCCTX, con->authp, 0,
			OCI_ATTR_SESSION, con->errhp);
	if (status != OCI_SUCCESS) goto connect_err;
	status = OCIServerAttach(con->srvhp, con->errhp, (OraText*)con->uri->s,
			con->uri->len, 0);
	if (status != OCI_SUCCESS) goto connect_err;
	status = OCISessionBegin(con->svchp, con->errhp, con->authp,
			OCI_CRED_RDBMS, OCI_DEFAULT);
	if (status != OCI_SUCCESS) {
connect_err:
		oraxit(status, con);
	}
}

//-----------------------------------------------------------------------------
void send_req(con_t* con, const Str* req)
{
	sword status;

	status = OCIStmtPrepare(con->stmthp, con->errhp, (text*)req->s, req->len,
			OCI_NTV_SYNTAX, OCI_DEFAULT);
	if (status != OCI_SUCCESS) goto request_err;
	status = OCIStmtExecute(con->svchp, con->stmthp, con->errhp, 0, 0, NULL,
			NULL, OCI_STMT_SCROLLABLE_READONLY);
	if (status != OCI_SUCCESS) {
request_err:
		fprintf(stderr, "%.*s\n", req->len, req->s);
		oraxit(status, con);
	}
}

//-----------------------------------------------------------------------------
