/* 
 * $Id$ 
 *
 * PostgreSQL Database Driver for SER
 *
 * Portions Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2003 August.Net Services, LLC
 * Portions Copyright (C) 2005-2008 iptelorg GmbH
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

/** \addtogroup postgres
 * @{ 
 */

/** \file
 * Implementation of functions related to database commands.
 */

#include "pg_cmd.h"
#include "pg_sql.h"
#include "pg_fld.h"
#include "pg_con.h"
#include "pg_mod.h"
#include "pg_uri.h"
#include "pg_res.h"

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"

#include <string.h>

/** A global counter used to generate unique command names.
 * This variable implements a global counter which is used to generate unique
 * names for server-side commands.
 */
static int server_query_no = 0;

static int upload_cmd(db_cmd_t* cmd);
static void free_pg_params(struct pg_params* cmd);


/** Destroys a pg_cmd structure.
 * This function frees all memory used by pg_cmd structure.
 * @param cmd A pointer to generic db_cmd command being freed.
 * @param payload A pointer to pg_cmd structure to be freed.
 */
static void pg_cmd_free(db_cmd_t* cmd, struct pg_cmd* payload)
{
	db_drv_free(&payload->gen);
	if (payload->sql_cmd.s) pkg_free(payload->sql_cmd.s);
	free_pg_params(&payload->params);
	if (payload->name) pkg_free(payload->name);
	if (payload->types) PQclear(payload->types);
	pkg_free(payload);
}


/** Generate a unique name for a server-side PostgreSQL command.
 * This function generates a unique name for each command that will be used to
 * identify the prepared statement on the server. The name has only has to be
 * unique within a connection to the server so we just keep a global counter
 * and the name will be that number converted to text.
 *
 * @param cmd A command whose name is to be generated 
 * @return A string allocated using pkg_malloc containing the name or NULL on
 *         error.
 */
static int gen_cmd_name(db_cmd_t* cmd)
{
	struct pg_cmd* pcmd;
	char* c;
	int len;

	pcmd = DB_GET_PAYLOAD(cmd);
	c = int2str(server_query_no, &len);

	pcmd->name = pkg_malloc(len + 1);
	if (pcmd->name == NULL) {
		ERR("postgres: No memory left\n");
		return -1;
	}
	memcpy(pcmd->name, c, len);
	pcmd->name[len] = '\0';
	server_query_no++;
	return 0;
}


/** Creates parameter data structures for PQexecPrepared.
 * This function creates auxiliary data structures that will be used to pass
 * parameter value and types to PQexecPrepared.  The function only allocates
 * memory buffers and determines oids of parameters, actual values will be
 * assigned by another function at runtime.
 * @param cmd A command where the data strctures will be created. 
 * @retval 0 on success.
 * @retval A negative number on error.
 */
static int create_pg_params(db_cmd_t* cmd)
{
	int num;
	struct pg_cmd* pcmd;

	pcmd = DB_GET_PAYLOAD(cmd);

	num = cmd->match_count + cmd->vals_count;

	if (num == 0) return 0;
	pcmd->params.val = (const char**)pkg_malloc(sizeof(const char*) * num);
	pcmd->params.len = (int*)pkg_malloc(sizeof(int) * num);
	pcmd->params.fmt = (int*)pkg_malloc(sizeof(int) * num);
	
	if (!pcmd->params.val || 
		!pcmd->params.len || !pcmd->params.fmt) {
		ERR("postgres: No memory left\n");
		goto error;
	}
	
	memset(pcmd->params.val, '\0', sizeof(const char*) * num);
	memset(pcmd->params.len, '\0', sizeof(int) * num);
	memset(pcmd->params.fmt, '\0', sizeof(int) * num);
	pcmd->params.n = num;
	return 0;

 error:
	free_pg_params(&pcmd->params);
	return -1;
}


/**
 * Free all memory used for PQexecParam parameters. That is
 * the arrays of Oids, values, lengths, and formats supplied
 * to PostgreSQL client API functions like PQexecParams.
 */
static void free_pg_params(struct pg_params* params)
{
	if (params == NULL) return;

	if (params->val) pkg_free(params->val);
    params->val = NULL;

	if (params->len) pkg_free(params->len);
	params->len = NULL;

	if (params->fmt) pkg_free(params->fmt);
	params->fmt = NULL;
}


/** Verify field type compatibility.
 * This function verifies the types of all parameters of a database command
 * with the types of corresponding fields on the server to make sure that they
 * can be converted.
 * @param cmd A command structure whose parameters are to be checked.
 * @retval 0 on success.
 * @retval A negative number if at least one field type does not match.
 * @todo Store oid and length as part of pg_fld, instead of the arrays used
 *       as parameters to PQ functions
 */
static int check_types(db_cmd_t* cmd) 
{ 
	struct pg_con* pcon;
	
	/* FIXME: The function should take the connection as one of parameters */
	pcon = DB_GET_PAYLOAD(cmd->ctx->con[db_payload_idx]);

	if (pg_check_fld2pg(cmd->match, pcon->oid)) return -1;
	if (pg_check_fld2pg(cmd->vals, pcon->oid)) return -1;
	if (pg_check_pg2fld(cmd->result, pcon->oid)) return -1;
	return 0;
}


static int get_types(db_cmd_t* cmd)
{
	struct pg_cmd* pcmd;
	struct pg_con* pcon;
	int i, n;
	pg_type_t *types;

	pcmd = DB_GET_PAYLOAD(cmd);
	/* FIXME */
	pcon = DB_GET_PAYLOAD(cmd->ctx->con[db_payload_idx]);

	types = pcon->oid;
	pcmd->types = PQdescribePrepared(pcon->con, pcmd->name);
	
	if (PQresultStatus(pcmd->types) != PGRES_COMMAND_OK) {
		ERR("postgres: Error while obtaining description of prepared statement\n");
		return -1;
	}
	/* adapted from check_result() in db_mysql */
	n = PQnfields(pcmd->types);
	if (cmd->result == NULL) {
		/* The result set parameter of db_cmd function was empty, that
		 * means the command is select * and we have to create the array
		 * of result fields in the cmd structure manually.
		 */
		cmd->result = db_fld(n + 1);
		cmd->result_count = n;
		for(i = 0; i < cmd->result_count; i++) {
			struct pg_fld *f;
			if (pg_fld(cmd->result + i, cmd->table.s) < 0) goto error;
			f = DB_GET_PAYLOAD(cmd->result + i);
			f->name = pkg_malloc(strlen(PQfname(pcmd->types, i))+1);
			if (f->name == NULL) {
				ERR("postgres: Out of private memory\n");
				goto error;
			}
			strcpy(f->name, PQfname(pcmd->types, i));
			cmd->result[i].name = f->name;
		}
	} else {
		if (cmd->result_count != n) {
			BUG("postgres: Number of fields in PQresult does not match number of parameters in DB API\n");
			goto error;
		}
	}

	/* Now iterate through all the columns in the result set and replace
	 * any occurrence of DB_UNKNOWN type with the type of the column
	 * retrieved from the database and if no column name was provided then
	 * update it from the database as well.
	 */
	for(i = 0; i < cmd->result_count; i++) {
		Oid type = PQftype(pcmd->types, i);
		if (cmd->result[i].type != DB_NONE) continue;

		if ((type == types[PG_INT2].oid) || (type == types[PG_INT4].oid) || (type == types[PG_INT8].oid))
			cmd->result[i].type = DB_INT;

		else if (type == types[PG_FLOAT4].oid)
			cmd->result[i].type = DB_FLOAT;

		else if (type == types[PG_FLOAT8].oid)
			cmd->result[i].type = DB_DOUBLE;

		else if ((type == types[PG_TIMESTAMP].oid) || (type == types[PG_TIMESTAMPTZ].oid))
			cmd->result[i].type = DB_DATETIME;

		else if ((type == types[PG_VARCHAR].oid) || (type == types[PG_CHAR].oid) || (type == types[PG_TEXT].oid))
			cmd->result[i].type = DB_STR;

		else if ((type == types[PG_BIT].oid) || (type == types[PG_VARBIT].oid))
			cmd->result[i].type = DB_BITMAP;

		else if (type == types[PG_BYTE].oid)
			cmd->result[i].type = DB_BLOB;

		else
		{
			ERR("postgres: Unsupported PostgreSQL column type: %d, table: %s, column: %s\n",
				type, cmd->table.s, PQfname(pcmd->types, i));
			goto error;
		}
	}
	return 0;
error:
	return -1;
}


int pg_cmd(db_cmd_t* cmd)
{
	struct pg_cmd* pcmd;
 
	pcmd = (struct pg_cmd*)pkg_malloc(sizeof(struct pg_cmd));
	if (pcmd == NULL) {
		ERR("postgres: No memory left\n");
		goto error;
	}
	memset(pcmd, '\0', sizeof(struct pg_cmd));
	if (db_drv_init(&pcmd->gen, pg_cmd_free) < 0) goto error;

	switch(cmd->type) {
	case DB_PUT:
		if (build_insert_sql(&pcmd->sql_cmd, cmd) < 0) goto error;
		break;
		
	case DB_DEL:
		if (build_delete_sql(&pcmd->sql_cmd, cmd) < 0) goto error;
		break;

	case DB_GET:
		if (build_select_sql(&pcmd->sql_cmd, cmd) < 0) goto error;
		break;

	case DB_UPD:
		if (build_update_sql(&pcmd->sql_cmd, cmd) < 0) goto error;
		break;
		
	case DB_SQL:
		pcmd->sql_cmd.s = (char*)pkg_malloc(cmd->table.len + 1);
		if (pcmd->sql_cmd.s == NULL) {
			ERR("postgres: Out of private memory\n");
			goto error;
		}
		memcpy(pcmd->sql_cmd.s,cmd->table.s, cmd->table.len);
		pcmd->sql_cmd.s[cmd->table.len] = '\0';
		pcmd->sql_cmd.len = cmd->table.len;
        break;
	}

	DB_SET_PAYLOAD(cmd, pcmd);

	/* Create parameter arrays for PostgreSQL API functions */
	if (create_pg_params(cmd) < 0) goto error;	

	/* Generate a unique name for the command on the server */
	if (gen_cmd_name(cmd) != 0) goto error; 

	/* Upload the command to the server */
	if (upload_cmd(cmd) != 0) goto error;

	/* Obtain the description of the uploaded command, this includes
	 * information about result and parameter fields */
	if (get_types(cmd) != 0) goto error;

	/* Update fields based on the information retrieved from the */
	if (pg_resolve_param_oids(cmd->vals, cmd->match,
							  cmd->vals_count, cmd->match_count,
							  pcmd->types)) 
		goto error;
	if (pg_resolve_result_oids(cmd->result, cmd->result_count, pcmd->types)) 
		goto error;

	if (check_types(cmd)) goto error;

	return 0;

 error:
	if (pcmd) {
		DB_SET_PAYLOAD(cmd, NULL);
		free_pg_params(&pcmd->params);

		if (pcmd->types) PQclear(pcmd->types);
		if (pcmd->name) pkg_free(pcmd->name);
		if (pcmd->sql_cmd.s) pkg_free(pcmd->sql_cmd.s);

		db_drv_free(&pcmd->gen);
		pkg_free(pcmd);
	}
	return -1;
}


int pg_getopt(db_cmd_t* cmd, char* optname, va_list ap)
{
	long long* id;

	if (!strcasecmp("last_id", optname)) {
		id = va_arg(ap, long long*);
		if (id == NULL) {
			BUG("postgres: NULL pointer passed to 'last_id' option\n");
			goto error;
		}
		return -1;
	} else {
		return 1;
	}
	return 0;

 error:
	return -1;
}


int pg_setopt(db_cmd_t* cmd, char* optname, va_list ap)
{
	return 1;
}


/** Uploads a database command to PostgreSQL server.
 * This function uploads a pre-compiled database command to PostgreSQL
 * server using PQprepare.
 * @param cmd A database command
 * @retval 0 on success.
 * @retval A negative number on error.
 */
static int upload_cmd(db_cmd_t* cmd)
{
	struct pg_cmd* pcmd;
	struct pg_con* pcon;
	PGresult* res;
	int st;

	pcmd = DB_GET_PAYLOAD(cmd);
	/* FIXME: The function should take the connection as one of parameters */
	pcon = DB_GET_PAYLOAD(cmd->ctx->con[db_payload_idx]);

	DBG("postgres: Uploading command '%s': '%s'\n", pcmd->name,
		pcmd->sql_cmd.s);

	res = PQprepare(pcon->con, pcmd->name, pcmd->sql_cmd.s, (cmd->match_count + cmd->vals_count), NULL);
	
	st = PQresultStatus(res);

	if (st != PGRES_COMMAND_OK && st != PGRES_NONFATAL_ERROR &&
		st != PGRES_TUPLES_OK) {
		ERR("postgres: Error while uploading command to server: %d, %s", 
			st, PQresultErrorMessage(res));
		ERR("postgres: Command: '%s'\n", pcmd->sql_cmd.s);
		PQclear(res);
		return -1;
	}

	PQclear(res);
	return 0;
}


int pg_cmd_exec(db_res_t* res, db_cmd_t* cmd)
{
	PGresult* tmp;
	int i, err, stat;
	db_con_t* con;
	struct pg_cmd* pcmd;
	struct pg_con* pcon;
	struct pg_uri* puri;
	struct pg_res* pres;

	/* First things first: retrieve connection info from the currently active
	 * connection and also mysql payload from the database command
	 */
	con = cmd->ctx->con[db_payload_idx];
	pcmd = DB_GET_PAYLOAD(cmd);
	pcon = DB_GET_PAYLOAD(con);
	puri = DB_GET_PAYLOAD(con->uri);

	for(i = 0; i <= pg_retries; i++) {
		/* Convert parameters from DB-API format to the format accepted
		 * by PostgreSQL */
		if (pg_fld2pg(&pcmd->params, 0, pcon->oid, cmd->match, pcon->flags) != 0) 
			return 1;

		if (pg_fld2pg(&pcmd->params, cmd->match_count,
					  pcon->oid, cmd->vals, pcon->flags) != 0) return 1;
		
		/* Execute the statement */
		tmp = PQexecPrepared(pcon->con, pcmd->name,
							 pcmd->params.n,
							 pcmd->params.val, pcmd->params.len,
							 pcmd->params.fmt, 1);
		if (!tmp) {
			ERR("postgres: PQexecPrepared returned no result\n");
			continue;
		}

		switch(PQresultStatus(tmp)) {
		case PGRES_COMMAND_OK:
		case PGRES_NONFATAL_ERROR:
		case PGRES_TUPLES_OK:
			if (res) {
				pres = DB_GET_PAYLOAD(res);
				pres->res = tmp;
				pres->rows = PQntuples(tmp);
			} else {
				PQclear(tmp);
			}
			return 0;
			
		default:
			break;
		}
		ERR("postgres: Command on server %s failed: %s: %s\n", 
			puri->host, PQresStatus(PQresultStatus(tmp)), 
			PQresultErrorMessage(tmp));
		PQclear(tmp);

		/* Command failed, first of all determine the status of the connection
		 * to server */
		if (PQstatus(pcon->con) != CONNECTION_OK) {
			INFO("postgres: Connection to server %s disconnected, attempting reconnect\n", 
				 puri->host);
			pg_con_disconnect(con);
			if (pg_con_connect(con)) {
				INFO("postgres: Failed to reconnect server %s, giving up\n", 
					 puri->host);
				return -1;
			}
			INFO("postgres: Successfully reconnected server on %s\n", 
				 puri->host);
		}

		/* Connection is either connected or has been successfully reconnected, 
		 * now figure out if the prepared command on the server still exist
		 */
		tmp = PQdescribePrepared(pcon->con, pcmd->name);
		if (tmp == NULL) {
			ERR("postgres: PQdescribePrepared returned no result\n");
		    continue;
		}
		stat = PQresultStatus(tmp);
		PQclear(tmp);
		switch (stat) {
		case PGRES_COMMAND_OK:
		case PGRES_NONFATAL_ERROR:
		case PGRES_TUPLES_OK:
			INFO("postgres: Command %s on server %s still exists, reusing\n",
				 pcmd->name, puri->host);
			/* Command is there, retry */
			continue;
		default:
			break;
		}

		/* Upload again */
		INFO("postgres: Command %s on server %s missing, uploading\n",
			 pcmd->name, puri->host);
		err = upload_cmd(cmd);
		if (err < 0) {
			continue;
		} else if (err > 0) {
			/* DB API error, this is a serious problem such
			 * as memory allocation failure, bail out
			 */
			return 1;
		}
	}

	INFO("postgres: Failed to execute command %s on server %s, giving up\n",
		 pcmd->name, puri->host);
	return -1;
}


int pg_cmd_first(db_res_t* res)
{
	struct pg_res* pres;

	pres = DB_GET_PAYLOAD(res);

	if (pres->rows <= 0) return 1; /* Empty table */
	pres->row = 0;
	return pg_cmd_next(res);
}


int pg_cmd_next(db_res_t* res)
{
	struct pg_res* pres;
	struct pg_con* pcon;

	pres = DB_GET_PAYLOAD(res);
	pcon = DB_GET_PAYLOAD(res->cmd->ctx->con[db_payload_idx]);

	if (pres->row >= pres->rows) return 1;

	if (pg_pg2fld(res->cmd->result, pres->res, pres->row, pcon->oid, pcon->flags)) return -1;
	res->cur_rec->fld = res->cmd->result;
	pres->row++;
	return 0;
}

/** @} */
