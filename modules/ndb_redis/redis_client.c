/**
 * $Id$
 *
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../hashes.h"
#include "../../ut.h"

#include "redis_client.h"

#define redisCommandNR(a...) (int)({ void *__tmp; __tmp = redisCommand(a); if (__tmp) freeReplyObject(__tmp); __tmp ? 0 : -1;})

static redisc_server_t *_redisc_srv_list=NULL;

static redisc_reply_t *_redisc_rpl_list=NULL;

/**
 *
 */
int redisc_init(void)
{
	char *addr;
	unsigned int port, db;
	redisc_server_t *rsrv=NULL;
	param_t *pit = NULL;
	struct timeval tv;

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	if(_redisc_srv_list==NULL)
	{
		LM_ERR("no redis servers defined\n");
		return -1;
	}

	for(rsrv=_redisc_srv_list; rsrv; rsrv=rsrv->next)
	{
		addr = "127.0.0.1";
		port = 6379;
		db = 0;
		for (pit = rsrv->attrs; pit; pit=pit->next)
		{
			if(pit->name.len==4 && strncmp(pit->name.s, "addr", 4)==0) {
				addr = pit->body.s;
				addr[pit->body.len] = '\0';
			} else if(pit->name.len==4 && strncmp(pit->name.s, "port", 4)==0) {
				if(str2int(&pit->body, &port) < 0)
					port = 6379;
			} else if(pit->name.len==2 && strncmp(pit->name.s, "db", 2)==0) {
				if(str2int(&pit->body, &db) < 0)
					db = 0;
			}
		}
		rsrv->ctxRedis = redisConnectWithTimeout(addr, port, tv);
		if(!rsrv->ctxRedis)
			goto err;
		if (rsrv->ctxRedis->err)
			goto err2;
		if (redisCommandNR(rsrv->ctxRedis, "PING"))
			goto err2;
		if (redisCommandNR(rsrv->ctxRedis, "SELECT %i", db))
			goto err2;

	}

	return 0;

err2:
	LM_ERR("error communicating with redis server [%.*s] (%s:%d/%d): %s\n",
		rsrv->sname->len, rsrv->sname->s, addr, port, db, rsrv->ctxRedis->errstr);
	return -1;
err:
	LM_ERR("failed to connect to redis server [%.*s] (%s:%d/%d)\n",
		rsrv->sname->len, rsrv->sname->s, addr, port, db);
	return -1;
}

/**
 *
 */
int redisc_destroy(void)
{
	redisc_server_t *rsrv=NULL;
	redisc_server_t *rsrv1=NULL;
	if(_redisc_srv_list==NULL)
		return -1;
	rsrv=_redisc_srv_list;
	while(rsrv!=NULL)
	{
		rsrv1 = rsrv;
		rsrv=rsrv->next;
		if(rsrv1->ctxRedis!=NULL)
			redisFree(rsrv1->ctxRedis);
		free_params(rsrv1->attrs);
		pkg_free(rsrv1);
	}
	return 0;
}

/**
 *
 */
int redisc_add_server(char *spec)
{
	param_t *pit=NULL;
	param_hooks_t phooks;
	redisc_server_t *rsrv=NULL;
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
	rsrv = (redisc_server_t*)pkg_malloc(sizeof(redisc_server_t));
	if(rsrv==NULL)
	{
		LM_ERR("no more pkg\n");
		goto error;
	}
	memset(rsrv, 0, sizeof(redisc_server_t));
	rsrv->attrs = pit;
	for (pit = rsrv->attrs; pit; pit=pit->next)
	{
		if(pit->name.len==4 && strncmp(pit->name.s, "name", 4)==0) {
			rsrv->sname = &pit->body;
			rsrv->hname = get_hash1_raw(rsrv->sname->s, rsrv->sname->len);
			break;
		}
	}
	if(rsrv->sname==NULL)
	{
		LM_ERR("no server name\n");
		goto error;
	}
	rsrv->next = _redisc_srv_list;
	_redisc_srv_list = rsrv;

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
redisc_server_t *redisc_get_server(str *name)
{
	redisc_server_t *rsrv=NULL;
	unsigned int hname;

	hname = get_hash1_raw(name->s, name->len);
	rsrv=_redisc_srv_list;
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
int redisc_exec(str *srv, str *cmd, str *argv1, str *argv2, str *argv3,
		str *res)
{
	redisc_server_t *rsrv=NULL;
	redisc_reply_t *rpl;
	char c;

	rsrv = redisc_get_server(srv);
	if(srv==NULL || cmd==NULL || res==NULL)
	{
		LM_ERR("invalid parameters");
		return -1;
	}
	if(rsrv==NULL)
	{
		LM_ERR("no redis server found: %.*s\n", srv->len, srv->s);
		return -1;
	}
	if(rsrv->ctxRedis==NULL)
	{
		LM_ERR("no redis context for server: %.*s\n", srv->len, srv->s);
		return -1;
	}
	rpl = redisc_get_reply(res);
	if(rpl==NULL)
	{
		LM_ERR("no redis reply id found: %.*s\n", res->len, res->s);
		return -1;
	}
	if(rpl->rplRedis!=NULL)
	{
		/* clean up previous redis reply */
		freeReplyObject(rpl->rplRedis);
		rpl->rplRedis = NULL;
	}
	c = cmd->s[cmd->len];
	cmd->s[cmd->len] = '\0';
	rpl->rplRedis = redisCommand(rsrv->ctxRedis, cmd->s);
	cmd->s[cmd->len] = c;
	return 0;
}


/**
 *
 */
redisc_reply_t *redisc_get_reply(str *name)
{
	redisc_reply_t *rpl;
	unsigned int hid;

	hid = get_hash1_raw(name->s, name->len);

	for(rpl=_redisc_rpl_list; rpl; rpl=rpl->next) {
		if(rpl->hname==hid && rpl->rname.len==name->len
				&& strncmp(rpl->rname.s, name->s, name->len)==0)
			return rpl;
	}
	/* not found - add a new one */

	rpl = (redisc_reply_t*)pkg_malloc(sizeof(redisc_reply_t));
	if(rpl==NULL)
	{
		LM_ERR("no more pkg\n");
		return NULL;
	}
	memset(rpl, 0, sizeof(redisc_reply_t));
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
	rpl->next = _redisc_rpl_list;
	_redisc_rpl_list = rpl;
	return rpl;
}
