/**
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
 *
 * Copyright (C) 2012 Vicente Hernando Ara (System One: www.systemonenoc.com)
 *     - for: redis array reply support
 *
 * Copyright (C) 2017 Carsten Bock (ng-voice GmbH)
 *     - for: Cluster support
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

#include "redis_client.h"

#define redisCommandNR(a...) (int)({ void *__tmp; __tmp = redisCommand(a); if (__tmp) freeReplyObject(__tmp); __tmp ? 0 : -1;})

static redisc_server_t ** _redisc_srv_list=NULL;

static redisc_reply_t *_redisc_rpl_list=NULL;

extern int init_without_redis;
extern int redis_connect_timeout_param;
extern int redis_cmd_timeout_param;
extern int redis_cluster_param;

/**
 *
 */
int redisc_init(void)
{
	char *addr, *pass, *unix_sock_path = NULL;
	unsigned int port, db;
	redisc_server_t *rsrv=NULL;
	param_t *pit = NULL;
	struct timeval tv_conn;
	struct timeval tv_cmd;

	tv_conn.tv_sec = (int) redis_connect_timeout_param / 1000;
	tv_conn.tv_usec = (int) (redis_connect_timeout_param % 1000) * 1000;

	tv_cmd.tv_sec = (int) redis_cmd_timeout_param / 1000;
	tv_cmd.tv_usec = (int) (redis_cmd_timeout_param % 1000) * 1000;

	if(*_redisc_srv_list==NULL)
	{
		LM_ERR("no redis servers defined\n");
		return -1;
	}

	for(rsrv=*_redisc_srv_list; rsrv; rsrv=rsrv->next)
	{
		addr = "127.0.0.1";
		port = 6379;
		db = 0;
		pass = NULL;

		for (pit = rsrv->attrs; pit; pit=pit->next)
		{
			if(pit->name.len==4 && strncmp(pit->name.s, "unix", 4)==0) {
				unix_sock_path = pit->body.s;
				unix_sock_path[pit->body.len] = '\0';
			} else if(pit->name.len==4 && strncmp(pit->name.s, "addr", 4)==0) {
				addr = pit->body.s;
				addr[pit->body.len] = '\0';
			} else if(pit->name.len==4 && strncmp(pit->name.s, "port", 4)==0) {
				if(str2int(&pit->body, &port) < 0)
					port = 6379;
			} else if(pit->name.len==2 && strncmp(pit->name.s, "db", 2)==0) {
				if(str2int(&pit->body, &db) < 0)
					db = 0;
			} else if(pit->name.len==4 && strncmp(pit->name.s, "pass", 4)==0) {
				pass = pit->body.s;
				pass[pit->body.len] = '\0';
			}
		}

		if(unix_sock_path != NULL) {
			LM_DBG("Connecting to unix socket: %s\n", unix_sock_path);
			rsrv->ctxRedis = redisConnectUnixWithTimeout(unix_sock_path,
					tv_conn);
		} else {
			rsrv->ctxRedis = redisConnectWithTimeout(addr, port, tv_conn);
		}

		if(!rsrv->ctxRedis)
			goto err;
		if (rsrv->ctxRedis->err)
			goto err2;
		if ((pass != NULL) && redisc_check_auth(rsrv, pass))
			goto err2;
		if (redisSetTimeout(rsrv->ctxRedis, tv_cmd))
			goto err2;
		if (redisCommandNR(rsrv->ctxRedis, "PING"))
			goto err2;
		if ((redis_cluster_param == 0) && redisCommandNR(rsrv->ctxRedis, "SELECT %i", db))
			goto err2;

	}

	return 0;

err2:
	if (unix_sock_path != NULL) {
		LM_ERR("error communicating with redis server [%.*s]"
				" (unix:%s db:%d): %s\n",
				rsrv->sname->len, rsrv->sname->s, unix_sock_path, db,
				rsrv->ctxRedis->errstr);
	} else {
		LM_ERR("error communicating with redis server [%.*s] (%s:%d/%d): %s\n",
				rsrv->sname->len, rsrv->sname->s, addr, port, db,
				rsrv->ctxRedis->errstr);
	}
	if (init_without_redis==1)
	{
		LM_WARN("failed to initialize redis connections, but initializing"
				" module anyway.\n");
		return 0;
	}

	return -1;
err:
	if (unix_sock_path != NULL) {
		LM_ERR("failed to connect to redis server [%.*s] (unix:%s db:%d)\n",
				rsrv->sname->len, rsrv->sname->s, unix_sock_path, db);
	} else {
		LM_ERR("failed to connect to redis server [%.*s] (%s:%d/%d)\n",
				rsrv->sname->len, rsrv->sname->s, addr, port, db);
	}
	if (init_without_redis==1)
	{
		LM_WARN("failed to initialize redis connections, but initializing"
				" module anyway.\n");
		return 0;
	}

	return -1;
}

/**
 *
 */
int redisc_destroy(void)
{
	redisc_reply_t *rpl, *next_rpl;

	redisc_server_t *rsrv=NULL;
	redisc_server_t *rsrv1=NULL;

	rpl = _redisc_rpl_list;
	while(rpl != NULL)
	{
		next_rpl = rpl->next;
		if(rpl->rplRedis)
			freeReplyObject(rpl->rplRedis);

		if(rpl->rname.s != NULL)
			shm_free(rpl->rname.s);

		pkg_free(rpl);
		rpl = next_rpl;
	}
	_redisc_rpl_list = NULL;

	if(_redisc_srv_list==NULL)
		return -1;
	if(*_redisc_srv_list==NULL)
		return -1;
	rsrv=*_redisc_srv_list;
	while(rsrv!=NULL)
	{
		rsrv1 = rsrv;
		rsrv=rsrv->next;
		if (rsrv1->settings != NULL)
			shm_free(rsrv1->settings);
		if (rsrv1->ctxRedis!=NULL)
			redisFree(rsrv1->ctxRedis);
		free_params(rsrv1->attrs);
		shm_free(rsrv1);
	}
	shm_free(*_redisc_srv_list);
	*_redisc_srv_list = NULL;
	_redisc_srv_list = NULL;

	return 0;
}

int init_list(void) {
	if (_redisc_srv_list == NULL) {
		_redisc_srv_list = (redisc_server_t **)shm_malloc(sizeof(redisc_server_t*));
		if(!_redisc_srv_list) {
			LM_ERR("Out of memory\n");
			return -1;
		}
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
	if (_redisc_srv_list == NULL) {
		_redisc_srv_list = (redisc_server_t **)shm_malloc(sizeof(redisc_server_t*));
		if(!_redisc_srv_list) {
			LM_ERR("Out of memory\n");
			return -1;
		}
	}
	rsrv = (redisc_server_t*)shm_malloc(sizeof(redisc_server_t));
	if(rsrv==NULL)
	{
		LM_ERR("no more shm\n");
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
	rsrv->next = *_redisc_srv_list;
	*_redisc_srv_list = rsrv;

	return 0;
error:
	if(pit!=NULL)
		free_params(pit);
	if(rsrv!=NULL)
		shm_free(rsrv);
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
	LM_DBG("Hash %u (%.*s)\n", hname, name->len, name->s);
	rsrv=*_redisc_srv_list;
	while(rsrv!=NULL)
	{
		LM_DBG("Entry %u (%.*s)\n", rsrv->hname, rsrv->sname->len, rsrv->sname->s);
		if(rsrv->hname==hname && rsrv->sname->len==name->len
				&& strncmp(rsrv->sname->s, name->s, name->len)==0)
			return rsrv;
		rsrv=rsrv->next;
	}
	LM_DBG("No entry found.\n");
	return NULL;
}

/**
 *
 */
int redisc_reconnect_server(redisc_server_t *rsrv)
{
	char *addr, *pass, *unix_sock_path = NULL;
	unsigned int port, db;
	param_t *pit = NULL;
	struct timeval tv_conn;
	struct timeval tv_cmd;

	tv_conn.tv_sec = (int) redis_connect_timeout_param / 1000;
	tv_conn.tv_usec = (int) (redis_connect_timeout_param % 1000) * 1000;

	tv_cmd.tv_sec = (int) redis_cmd_timeout_param / 1000;
	tv_cmd.tv_usec = (int) (redis_cmd_timeout_param % 1000) * 1000;

	addr = "127.0.0.1";
	port = 6379;
	db = 0;
	pass = NULL;
	for (pit = rsrv->attrs; pit; pit=pit->next)
	{
		if(pit->name.len==4 && strncmp(pit->name.s, "unix", 4)==0) {
			unix_sock_path = pit->body.s;
			unix_sock_path[pit->body.len] = '\0';
		} else if(pit->name.len==4 && strncmp(pit->name.s, "addr", 4)==0) {
			addr = pit->body.s;
			addr[pit->body.len] = '\0';
		} else if(pit->name.len==4 && strncmp(pit->name.s, "port", 4)==0) {
			if(str2int(&pit->body, &port) < 0)
				port = 6379;
		} else if(pit->name.len==2 && strncmp(pit->name.s, "db", 2)==0) {
			if(str2int(&pit->body, &db) < 0)
				db = 0;
		} else if(pit->name.len==4 && strncmp(pit->name.s, "pass", 4)==0) {
			pass = pit->body.s;
			pass[pit->body.len] = '\0';
		}
	}
	if(rsrv->ctxRedis!=NULL) {
		redisFree(rsrv->ctxRedis);
		rsrv->ctxRedis = NULL;
	}

	if(unix_sock_path != NULL) {
		rsrv->ctxRedis = redisConnectUnixWithTimeout(unix_sock_path, tv_conn);
	} else {
		rsrv->ctxRedis = redisConnectWithTimeout(addr, port, tv_conn);
	}
	if(!rsrv->ctxRedis)
		goto err;
	if (rsrv->ctxRedis->err)
		goto err2;
	if ((pass != NULL) && redisc_check_auth(rsrv, pass))
		goto err2;
	if (redisSetTimeout(rsrv->ctxRedis, tv_cmd))
		goto err2;
	if (redisCommandNR(rsrv->ctxRedis, "PING"))
		goto err2;
	if ((redis_cluster_param == 0) && redisCommandNR(rsrv->ctxRedis, "SELECT %i", db))
		goto err2;

	return 0;

err2:
	if (unix_sock_path != NULL) {
		LM_ERR("error communicating with redis server [%.*s]"
				" (unix:%s db:%d): %s\n",
				rsrv->sname->len, rsrv->sname->s, unix_sock_path, db,
				rsrv->ctxRedis->errstr);
	} else {
		LM_ERR("error communicating with redis server [%.*s] (%s:%d/%d): %s\n",
				rsrv->sname->len, rsrv->sname->s, addr, port, db,
				rsrv->ctxRedis->errstr);
	}
err:
	if (unix_sock_path != NULL) {
		LM_ERR("failed to connect to redis server [%.*s] (unix:%s db:%d)\n",
				rsrv->sname->len, rsrv->sname->s, unix_sock_path, db);
	} else {
		LM_ERR("failed to connect to redis server [%.*s] (%s:%d/%d)\n",
				rsrv->sname->len, rsrv->sname->s, addr, port, db);
	}
	return -1;
}

int check_cluster_reply(redisReply *reply, redisc_server_t **rsrv) {
	redisc_server_t *rsrv_new;
	char *pass;
	char buffer[100], buffername[100];
	unsigned int port, db;
	str addr = {0, 0}, tmpstr = {0, 0}, name = {0, 0};
	param_t *pit = NULL;
	if (redis_cluster_param) {
		LM_DBG("Redis replied: \"%.*s\"\n", reply->len, reply->str);
		if ((reply->len > 7) && (strncmp(reply->str, "MOVED", 5) == 0)) {
			port = 6379;
			db = 0;
			pass = NULL;
			// Copy DB and password from current server:
			for (pit = (*rsrv)->attrs; pit; pit=pit->next) {
				if (pit->name.len==2 && strncmp(pit->name.s, "db", 2) == 0) {
					if (str2int(&pit->body, &db) < 0)
						db = 0;
				} else if (pit->name.len==4 && strncmp(pit->name.s, "pass", 4) == 0) {
					pass = pit->body.s;
					pass[pit->body.len] = '\0';
				}
			}
			if (strchr(reply->str, ':') > 0) {
				tmpstr.s = strchr(reply->str, ':') + 1;
				tmpstr.len = reply->len - (tmpstr.s - reply->str);
				if(str2int(&tmpstr, &port) < 0)
					port = 6379;
				LM_DBG("Port \"%.*s\" [%i] => %i\n", tmpstr.len, tmpstr.s, tmpstr.len, port);
			} else {
				LM_ERR("No Port in REDIS MOVED Reply (%.*s)\n", reply->len, reply->str);
				return 0;
			}
			if (strchr(reply->str+6, ' ') > 0) {
				addr.len = tmpstr.s - strchr(reply->str+6, ' ') - 2;
				addr.s = strchr(reply->str+6, ' ') + 1;
				LM_DBG("Host \"%.*s\" [%i]\n", addr.len, addr.s, addr.len);
			} else {
				LM_ERR("No Host in REDIS MOVED Reply (%.*s)\n", reply->len, reply->str);
				return 0;
			}

			name.len = snprintf(buffername, sizeof(buffername), "%.*s-%i-%i", addr.len, addr.s, port, db);
			name.s = buffername;
			LM_DBG("Name of new connection: %.*s\n", name.len, name.s);
			rsrv_new = redisc_get_server(&name);
			if (rsrv_new) {
				LM_DBG("Reusing Connection\n");
				*rsrv = rsrv_new;
				return 1;
			} else {
				LM_DBG("New Connection\n");
				if (pass) {
					tmpstr.len = snprintf(buffer, sizeof(buffer)-1, "name=%.*s;addr=%.*s;port=%i;db=%i;pass=%s", name.len, name.s, addr.len, addr.s, port, db, pass);
					tmpstr.s = shm_malloc(tmpstr.len + 1);
					memcpy(tmpstr.s, buffer, tmpstr.len);
				} else {
					tmpstr.len = snprintf(buffer, sizeof(buffer)-1, "name=%.*s;addr=%.*s;port=%i;db=%i", name.len, name.s, addr.len, addr.s, port, db);
					tmpstr.s = shm_malloc(tmpstr.len + 1);
					memcpy(tmpstr.s, buffer, tmpstr.len);
				}
				tmpstr.s[tmpstr.len] = '\0';
				LM_DBG("Connection setup: %.*s\n", tmpstr.len, tmpstr.s);
				if (redisc_add_server(tmpstr.s) == 0) {
					rsrv_new = redisc_get_server(&name);
					if (rsrv_new) {
						rsrv_new->settings = tmpstr.s;
						redisc_reconnect_server(rsrv_new);
						LM_DBG("Connection successful\n");
						*rsrv = rsrv_new;
						return 1;
					}
				} else {
					LM_ERR("Failed to add Connection (%.*s)\n", tmpstr.len, tmpstr.s);
				}
			}
		}
	}
	return 0;
}

/**
 *
 */
int redisc_exec(str *srv, str *res, str *cmd, ...)
{
	redisc_server_t *rsrv=NULL;
	redisc_reply_t *rpl;
	char c;
	va_list ap;

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
	rsrv = redisc_get_server(srv);
	if(rsrv==NULL)
	{
		LM_ERR("no redis server found: %.*s\n", srv->len, srv->s);
		goto error_exec;
	}
	if(rsrv->ctxRedis==NULL)
	{
		LM_ERR("no redis context for server: %.*s\n", srv->len, srv->s);
		goto error_exec;
	}
	rpl = redisc_get_reply(res);
	if(rpl==NULL)
	{
		LM_ERR("no redis reply id found: %.*s\n", res->len, res->s);
		goto error_exec;
	}
query:
	va_start(ap, cmd);

	if(rpl->rplRedis!=NULL)
	{
		/* clean up previous redis reply */
		freeReplyObject(rpl->rplRedis);
		rpl->rplRedis = NULL;
	}
	c = cmd->s[cmd->len];
	cmd->s[cmd->len] = '\0';
	rpl->rplRedis = redisvCommand(rsrv->ctxRedis, cmd->s, ap );
	if(rpl->rplRedis == NULL)
	{
		/* null reply, reconnect and try again */
		if(rsrv->ctxRedis->err)
		{
			LM_ERR("Redis error: %s\n", rsrv->ctxRedis->errstr);
		}
		if(redisc_reconnect_server(rsrv)==0)
		{
			va_end(ap);
			rpl->rplRedis = redisvCommand(rsrv->ctxRedis, cmd->s, ap);
		} else {
			LM_ERR("unable to reconnect to redis server: %.*s\n",
					srv->len, srv->s);
			cmd->s[cmd->len] = c;
			goto error_exec;
		}
	}
	if (check_cluster_reply(rpl->rplRedis, &rsrv)) {
		va_end(ap);
		goto query;	
	}
	cmd->s[cmd->len] = c;
	va_end(ap);
	return 0;

error_exec:
	va_end(ap);
	return -1;

}

/**
 * Executes a redis command.
 * Command is coded using a vector of strings, and a vector of lenghts.
 *
 * @param rsrv Pointer to a redis_server_t structure.
 * @param argc number of elements in the command vector.
 * @param argv vector of zero terminated strings forming the command.
 * @param argvlen vector of command string lenghts or NULL.
 * @return redisReply structure or NULL if there was an error.
 */
redisReply* redisc_exec_argv(redisc_server_t *rsrv, int argc, const char **argv,
		const size_t *argvlen)
{
	redisReply *res=NULL;

	if(rsrv==NULL || rsrv->ctxRedis==NULL)
	{
		LM_ERR("no redis context found for server %.*s\n",
				(rsrv)?rsrv->sname->len:0,
				(rsrv)?rsrv->sname->s:"");
		return NULL;
	}
	if(argc<=0)
	{
		LM_ERR("invalid parameters\n");
		return NULL;
	}
	if(argv==NULL || *argv==NULL)
	{
		LM_ERR("invalid parameters\n");
		return NULL;
	}
again:
	res = redisCommandArgv(rsrv->ctxRedis, argc, argv, argvlen);
	if(res)
	{
		if (check_cluster_reply(res, &rsrv)) {
			goto again;
		}
		return res;
	}

	/* null reply, reconnect and try again */
	if(rsrv->ctxRedis->err)
	{
		LM_ERR("Redis error: %s\n", rsrv->ctxRedis->errstr);
	}
	if(redisc_reconnect_server(rsrv)==0)
	{
		res = redisCommandArgv(rsrv->ctxRedis, argc, argv, argvlen);
		if (res) {
			if (check_cluster_reply(res, &rsrv)) {
				goto again;
			}
		}
	}
	else
	{
		LM_ERR("Unable to reconnect to server: %.*s\n",
				rsrv->sname->len, rsrv->sname->s);
		return NULL;
	}

	return res;
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


/**
 *
 */
int redisc_free_reply(str *name)
{
	redisc_reply_t *rpl;
	unsigned int hid;

	if(name==NULL || name->len==0) {
		LM_ERR("invalid parameters");
		return -1;
	}

	hid = get_hash1_raw(name->s, name->len);

	rpl = _redisc_rpl_list;
	while(rpl) {

		if(rpl->hname==hid && rpl->rname.len==name->len
				&& strncmp(rpl->rname.s, name->s, name->len)==0) {
			if(rpl->rplRedis) {
				freeReplyObject(rpl->rplRedis);
				rpl->rplRedis = NULL;
			}

			return 0;
		}

		rpl = rpl->next;
	}

	/* reply entry not found. */
	return -1;
}

int redisc_check_auth(redisc_server_t *rsrv, char *pass)
{
	redisReply *reply;
	int retval = 0;

	reply = redisCommand(rsrv->ctxRedis, "AUTH %s", pass);
	if (reply->type == REDIS_REPLY_ERROR) {
		LM_ERR("Redis authentication error\n");
		retval = -1;
	}
	freeReplyObject(reply);
	return retval;
}
