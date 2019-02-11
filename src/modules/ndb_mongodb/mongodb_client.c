/**
 * Copyright (C) 2014 Daniel-Constantin Mierla (asipto.com)
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdarg.h>

#include "../../core/mem/mem.h"
#include "../../core/dprint.h"
#include "../../core/hashes.h"
#include "../../core/ut.h"

#include "mongodb_client.h"
#include "api.h"

static mongodbc_server_t *_mongodbc_srv_list=NULL;

static mongodbc_reply_t *_mongodbc_rpl_list=NULL;

void mongodbc_destroy_reply(mongodbc_reply_t *rpl);

/**
 *
 */
int mongodbc_init(void)
{
	mongodbc_server_t *rsrv=NULL;

	if(_mongodbc_srv_list==NULL)
	{
		LM_ERR("no mongodb servers defined\n");
		return -1;
	}

	for(rsrv=_mongodbc_srv_list; rsrv; rsrv=rsrv->next)
	{
		if(rsrv->uri==NULL || rsrv->uri->len<=0) {
			LM_ERR("no uri for server: %.*s\n",
					rsrv->sname->len, rsrv->sname->s);
			return -1;
		}
		rsrv->client = mongoc_client_new (rsrv->uri->s);
		if(rsrv->client==NULL) {
			LM_ERR("failed to connect to: %.*s (%.*s)\n",
					rsrv->sname->len, rsrv->sname->s,
					rsrv->uri->len, rsrv->uri->s);
			return -1;
		}
	}
	return 0;
}

/**
 *
 */
int mongodbc_destroy(void)
{
	mongodbc_reply_t *rpl, *next_rpl;

	mongodbc_server_t *rsrv=NULL;
	mongodbc_server_t *rsrv1=NULL;

	rpl = _mongodbc_rpl_list;
	while(rpl != NULL)
	{
		next_rpl = rpl->next;
		mongodbc_destroy_reply(rpl);
		pkg_free(rpl);
		rpl = next_rpl;
	}
	_mongodbc_rpl_list = NULL;

	if(_mongodbc_srv_list==NULL)
		return -1;
	rsrv=_mongodbc_srv_list;
	while(rsrv!=NULL)
	{
		rsrv1 = rsrv;
		rsrv=rsrv->next;
		if(rsrv1->client!=NULL)
			mongoc_client_destroy(rsrv1->client);
		free_params(rsrv1->attrs);
		pkg_free(rsrv1);
	}
	_mongodbc_srv_list = NULL;

	return 0;
}

/**
 *
 */
int mongodbc_add_server(char *spec)
{
	param_t *pit=NULL;
	param_hooks_t phooks;
	mongodbc_server_t *rsrv=NULL;
	str s;

	s.s = spec;
	s.len = strlen(spec);
	if(s.s[s.len-1]==';')
		s.len--;
	if (parse_params(&s, CLASS_ANY, &phooks, &pit)<0)
	{
		LM_ERR("failed parsing params value\n");
		goto error;
	}
	rsrv = (mongodbc_server_t*)pkg_malloc(sizeof(mongodbc_server_t));
	if(rsrv==NULL)
	{
		LM_ERR("no more pkg\n");
		goto error;
	}
	memset(rsrv, 0, sizeof(mongodbc_server_t));
	rsrv->attrs = pit;
	for (pit = rsrv->attrs; pit; pit=pit->next)
	{
		if(pit->name.len==4 && strncmp(pit->name.s, "name", 4)==0) {
			rsrv->sname = &pit->body;
			rsrv->hname = get_hash1_raw(rsrv->sname->s, rsrv->sname->len);
		} else if(pit->name.len==3 && strncmp(pit->name.s, "uri", 3)==0) {
			rsrv->uri = &pit->body;
			if(rsrv->uri->s[rsrv->uri->len]!='\0') rsrv->uri->s[rsrv->uri->len]='\0';
		}
	}
	if(rsrv->sname==NULL || rsrv->uri==NULL)
	{
		LM_ERR("no server name or uri\n");
		goto error;
	}
	LM_DBG("added server[%.*s]=%.*s\n",
			rsrv->sname->len, rsrv->sname->s,
			rsrv->uri->len, rsrv->uri->s);
	rsrv->next = _mongodbc_srv_list;
	_mongodbc_srv_list = rsrv;

	return 0;
error:
	if(pit!=NULL)
		free_params(pit);
	if(rsrv!=NULL)
		pkg_free(rsrv);
	return -1;
}

/**
 *
 */
mongodbc_server_t *mongodbc_get_server(str *name)
{
	mongodbc_server_t *rsrv=NULL;
	unsigned int hname;

	hname = get_hash1_raw(name->s, name->len);
	rsrv=_mongodbc_srv_list;
	while(rsrv!=NULL)
	{
		if(rsrv->hname==hname && rsrv->sname->len==name->len
				&& strncmp(rsrv->sname->s, name->s, name->len)==0)
			return rsrv;
		rsrv=rsrv->next;
	}
	return NULL;
}

/**
 *
 */
int mongodbc_reconnect_server(mongodbc_server_t *rsrv)
{
	mongoc_init();

	if(rsrv->client!=NULL)
		mongoc_client_destroy(rsrv->client);

	rsrv->client = mongoc_client_new (rsrv->uri->s);
	if(rsrv->client!=NULL) {
		LM_ERR("failed to connect to: %.*s (%.*s)\n",
				rsrv->sname->len, rsrv->sname->s,
				rsrv->uri->len, rsrv->uri->s);
		return -1;
	}

	return 0;
}

/**
 *
 */
int mongodbc_exec_cmd(str *srv, str *dname, str *cname, str *cmd, str *res, int emode)
{
	mongodbc_server_t *rsrv=NULL;
	mongodbc_reply_t *rpl=NULL;
	bson_error_t error;
	bson_t command;
	bson_t reply;
#if MONGOC_CHECK_VERSION(1, 5, 0)
	bson_t *opts;
#else
	int nres;
#endif
	const bson_t *cdoc;
	char c;
	int ret;

	if(srv==NULL || cmd==NULL || res==NULL)
	{
		LM_ERR("invalid parameters");
		goto error_exec;
	}
	if(srv->len==0 || res->len==0 || cmd->len==0)
	{
		LM_ERR("invalid parameters");
		goto error_exec;
	}
	rsrv = mongodbc_get_server(srv);
	if(rsrv==NULL)
	{
		LM_ERR("no mongodb server found: %.*s\n", srv->len, srv->s);
		goto error_exec;
	}
	if(rsrv->client==NULL)
	{
		if(mongodbc_reconnect_server(rsrv)<0) {
			LM_ERR("no mongodb context for server: %.*s\n", srv->len, srv->s);
			goto error_exec;
		}
	}
	rpl = mongodbc_get_reply(res);
	if(rpl==NULL)
	{
		LM_ERR("no mongodb reply id found: %.*s\n", res->len, res->s);
		goto error_exec;
	}
	mongodbc_destroy_reply(rpl);

	rpl->collection = mongoc_client_get_collection (rsrv->client, dname->s, cname->s);

	LM_DBG("trying to execute: [[%.*s]]\n", cmd->len, cmd->s);
	c = cmd->s[cmd->len];
	cmd->s[cmd->len] = '\0';
	if(!bson_init_from_json(&command, cmd->s, cmd->len, &error)) {
		cmd->s[cmd->len] = c;
		LM_ERR("Failed to run command: %s\n", error.message);
		goto error_exec;
	}
	cmd->s[cmd->len] = c;
	if(emode==0) {
		ret = mongoc_collection_command_simple (rpl->collection, &command, NULL, &reply, &error);
		if (!ret) {
			LM_ERR("Failed to run command: %s\n", error.message);
			bson_destroy (&command);
			goto error_exec;
		}
		bson_destroy (&command);
		rpl->jsonrpl.s = bson_as_json (&reply, NULL);
		rpl->jsonrpl.len = (rpl->jsonrpl.s)?strlen(rpl->jsonrpl.s):0;
		bson_destroy (&reply);
	} else {
		if(emode==1) {
			rpl->cursor = mongoc_collection_command (rpl->collection,
					MONGOC_QUERY_NONE,
					0,
					0,
					0,
					&command,
					NULL,
					0);
		} else {
#if MONGOC_CHECK_VERSION(1, 5, 0)
			if(emode==3) opts = BCON_NEW("limit", BCON_INT32 (1));
			else opts = BCON_NEW("limit", BCON_INT32 (0));
			if(opts==NULL) {
				LM_ERR("cannot initialize opts bson document\n");
				bson_destroy (&command);
				goto error_exec;
			}
			rpl->cursor = mongoc_collection_find_with_opts (rpl->collection,
					&command,
					opts,
					NULL);
			bson_destroy (opts);
#else
			nres = 0;
			if(emode==3) nres = 1; /* return one result */
			rpl->cursor = mongoc_collection_find (rpl->collection,
					MONGOC_QUERY_NONE,
					0,
					nres,
					0,
					&command,
					NULL,
					NULL);
#endif
		}
		bson_destroy (&command);
		if(rpl->cursor==NULL) {
			LM_ERR("Failed to get cursor: %s\n", error.message);
			goto error_exec;
		}
		if (!mongoc_cursor_next (rpl->cursor, &cdoc)) {
			if (mongoc_cursor_error (rpl->cursor, &error)) {
				LM_ERR("Cursor failure: %s\n", error.message);
			}
			goto error_exec;
		}
		rpl->jsonrpl.s = bson_as_json (cdoc, NULL);
		rpl->jsonrpl.len = (rpl->jsonrpl.s)?strlen(rpl->jsonrpl.s):0;
	}

	LM_DBG("command result: [[%s]]\n", (rpl->jsonrpl.s)?rpl->jsonrpl.s:"<null>");

	return 0;

error_exec:
	return -1;

}

/**
 *
 */
int mongodbc_exec_simple(str *srv, str *dname, str *cname, str *cmd, str *res)
{
	return mongodbc_exec_cmd(srv, dname, cname, cmd, res, 0);
}

/**
 *
 */
int mongodbc_exec(str *srv, str *dname, str *cname, str *cmd, str *res)
{
	return mongodbc_exec_cmd(srv, dname, cname, cmd, res, 1);
}

/**
 *
 */
int mongodbc_find(str *srv, str *dname, str *cname, str *cmd, str *res)
{
	return mongodbc_exec_cmd(srv, dname, cname, cmd, res, 2);
}

/**
 *
 */
int mongodbc_find_one(str *srv, str *dname, str *cname, str *cmd, str *res)
{
	return mongodbc_exec_cmd(srv, dname, cname, cmd, res, 3);
}

/**
 *
 */
mongodbc_reply_t *mongodbc_get_reply(str *name)
{
	mongodbc_reply_t *rpl;
	unsigned int hid;

	hid = get_hash1_raw(name->s, name->len);

	for(rpl=_mongodbc_rpl_list; rpl; rpl=rpl->next) {
		if(rpl->hname==hid && rpl->rname.len==name->len
				&& strncmp(rpl->rname.s, name->s, name->len)==0)
			return rpl;
	}
	/* not found - add a new one */

	rpl = (mongodbc_reply_t*)pkg_malloc(sizeof(mongodbc_reply_t));
	if(rpl==NULL)
	{
		LM_ERR("no more pkg\n");
		return NULL;
	}
	memset(rpl, 0, sizeof(mongodbc_reply_t));
	rpl->hname = hid;
	rpl->rname.s = (char*)pkg_malloc(name->len+1);
	if(rpl->rname.s==NULL)
	{
		LM_ERR("no more pkg.\n");
		pkg_free(rpl);
		return NULL;
	}
	strncpy(rpl->rname.s, name->s, name->len);
	rpl->rname.len = name->len;
	rpl->rname.s[name->len] = '\0';
	rpl->next = _mongodbc_rpl_list;
	_mongodbc_rpl_list = rpl;
	return rpl;
}

/**
 *
 */
void mongodbc_destroy_reply(mongodbc_reply_t *rpl)
{
	if(rpl->jsonrpl.s!=NULL) {
		bson_free(rpl->jsonrpl.s);
		rpl->jsonrpl.s = NULL;
		rpl->jsonrpl.len = 0;
	}
	if(rpl->cursor) {
		mongoc_cursor_destroy (rpl->cursor);
		rpl->cursor = NULL;
	}
	if(rpl->collection) {
		mongoc_collection_destroy (rpl->collection);
		rpl->collection = NULL;
	}
}

/**
 *
 */
int mongodbc_free_reply(str *name)
{
	mongodbc_reply_t *rpl;
	unsigned int hid;

	if(name==NULL || name->len==0) {
		LM_ERR("invalid parameters");
		return -1;
	}

	hid = get_hash1_raw(name->s, name->len);

	rpl = _mongodbc_rpl_list;
	while(rpl) {
		if(rpl->hname==hid && rpl->rname.len==name->len
				&& strncmp(rpl->rname.s, name->s, name->len)==0) {
			mongodbc_destroy_reply(rpl);
			return 0;
		}
		rpl = rpl->next;
	}

	/* reply entry not found. */
	return -1;
}

/**
 *
 */
int mongodbc_next_reply(str *name)
{
	mongodbc_reply_t *rpl;
	unsigned int hid;
	bson_error_t error;
	const bson_t *cdoc;

	if(name==NULL || name->len==0) {
		LM_ERR("invalid parameters");
		return -1;
	}

	hid = get_hash1_raw(name->s, name->len);

	rpl = _mongodbc_rpl_list;
	while(rpl) {
		if(rpl->hname==hid && rpl->rname.len==name->len
				&& strncmp(rpl->rname.s, name->s, name->len)==0) {
			break;
		}
		rpl = rpl->next;
	}
	if(!rpl) {
		/* reply entry not found. */
		return -1;
	}
	if(rpl->cursor==NULL) {
		LM_DBG("No active cursor for: %.*s\n", rpl->rname.len, rpl->rname.s);
		return -2;
	}
	if (!mongoc_cursor_next (rpl->cursor, &cdoc)) {
		if (mongoc_cursor_error (rpl->cursor, &error)) {
			LM_ERR("Cursor failure: %s\n", error.message);
		}
		return -2;
	}
	if(rpl->jsonrpl.s) {
		bson_free(rpl->jsonrpl.s);
		rpl->jsonrpl.s = NULL;
		rpl->jsonrpl.len = 0;
	}
	rpl->jsonrpl.s = bson_as_json (cdoc, NULL);
	rpl->jsonrpl.len = (rpl->jsonrpl.s)?strlen(rpl->jsonrpl.s):0;
	LM_DBG("next cursor result: [[%s]]\n", (rpl->jsonrpl.s)?rpl->jsonrpl.s:"<null>");
	return 0;
}

/**
 *
 */
int bind_ndb_mongodb(ndb_mongodb_api_t* api)
{
	if (!api) {
		ERR("Invalid parameter value\n");
		return -1;
	}

	memset(api, 0, sizeof(ndb_mongodb_api_t));
	api->cmd		= mongodbc_exec;
	api->cmd_simple	= mongodbc_exec_simple;
	api->find		= mongodbc_find;
	api->find_one	= mongodbc_find_one;
	api->next_reply	= mongodbc_next_reply;
	api->free_reply	= mongodbc_free_reply;

	return 0;
}

