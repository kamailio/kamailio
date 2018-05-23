/**
 * Copyright (C) 2017 kamailio.org
 * Copyright (C) 2017 flowroute.com
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file
 * \brief Kamailio topos ::
 * \ingroup topos
 * Module: \ref topos
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../core/dprint.h"
#include "../../core/ut.h"

#include "../ndb_redis/api.h"
#include "../topos/api.h"

#include "topos_redis_storage.h"


extern str _topos_redis_serverid;
extern ndb_redis_api_t _tps_redis_api;
extern topos_api_t _tps_api;

static str _tps_redis_bprefix = str_init("b:x:");
static str _tps_redis_dprefix = str_init("d:z:");

// void *redisCommandArgv(redisContext *c, int argc, const char **argv, const size_t *argvlen);

#define TPS_REDIS_NR_KEYS	48
#define TPS_REDIS_DATA_SIZE	8192

static char _tps_redis_cbuf[TPS_REDIS_DATA_SIZE];

/**
 * storage keys
 */
str td_key_rectime = str_init("rectime");
str td_key_a_callid = str_init("a_callid");
str td_key_a_uuid = str_init("a_uuid");
str td_key_b_uuid = str_init("b_uuid");
str td_key_a_contact = str_init("a_contact");
str td_key_b_contact = str_init("b_contact");
str td_key_as_contact = str_init("as_contact");
str td_key_bs_contact = str_init("bs_contact");
str td_key_a_tag = str_init("a_tag");
str td_key_b_tag = str_init("b_tag");
str td_key_a_rr = str_init("a_rr");
str td_key_b_rr = str_init("b_rr");
str td_key_s_rr = str_init("s_rr");
str td_key_iflags = str_init("iflags");
str td_key_a_uri = str_init("a_uri");
str td_key_b_uri = str_init("b_uri");
str td_key_r_uri = str_init("r_uri");
str td_key_a_srcaddr = str_init("a_srcaddr");
str td_key_b_srcaddr = str_init("b_srcaddr");
str td_key_s_method = str_init("s_method");
str td_key_s_cseq = str_init("s_cseq");

str tt_key_rectime = str_init("rectime");
str tt_key_a_callid = str_init("a_callid");
str tt_key_a_uuid = str_init("a_uuid");
str tt_key_b_uuid = str_init("b_uuid");
str tt_key_direction = str_init("direction");
str tt_key_x_via = str_init("x_via");
str tt_key_x_vbranch = str_init("x_vbranch");
str tt_key_x_rr = str_init("x_rr");
str tt_key_y_rr = str_init("y_rr");
str tt_key_s_rr = str_init("s_rr");
str tt_key_x_uri = str_init("x_uri");
str tt_key_a_contact = str_init("a_contact");
str tt_key_b_contact = str_init("b_contact");
str tt_key_as_contact = str_init("as_contact");
str tt_key_bs_contact = str_init("bs_contact");
str tt_key_x_tag = str_init("x_tag");
str tt_key_a_tag = str_init("a_tag");
str tt_key_b_tag = str_init("b_tag");
str tt_key_s_method = str_init("s_method");
str tt_key_s_cseq = str_init("s_cseq");

#define TPS_REDIS_SET_ARGSV(sval, argc, argv, argvlen) \
	do { \
		if((sval)->s!=NULL && (sval)->len>0) { \
			argv[argc] = (sval)->s; \
			argvlen[argc] = (sval)->len; \
			argc++; \
		} \
	} while(0)

#define TPS_REDIS_SET_ARGS(sval, argc, akey, argv, argvlen) \
	do { \
		if((sval)->s!=NULL && (sval)->len>0) { \
			argv[argc] = (akey)->s; \
			argvlen[argc] = (akey)->len; \
			argc++; \
			argv[argc] = (sval)->s; \
			argvlen[argc] = (sval)->len; \
			argc++; \
		} \
	} while(0)

#define TPS_REDIS_SET_ARGSX(sval, argc, akey, argv, argvlen) \
	do { \
		if((sval)!=NULL) { \
			TPS_REDIS_SET_ARGS(sval, argc, akey, argv, argvlen); \
		} \
	} while(0)

#define TPS_REDIS_SET_ARGN(nval, rp, sval, argc, akey, argv, argvlen) \
	do { \
		(sval)->s = int2bstr((unsigned long)nval, rp, &(sval)->len); \
		rp = (sval)->s + (sval)->len + 1; \
		TPS_REDIS_SET_ARGS((sval), argc, akey, argv, argvlen); \
	} while(0)

#define TPS_REDIS_SET_ARGNV(nval, rp, sval, argc, argv, argvlen) \
	do { \
		(sval)->s = int2bstr((unsigned long)nval, rp, &(sval)->len); \
		rp = (sval)->s + (sval)->len + 1; \
		TPS_REDIS_SET_ARGSV((sval), argc, argv, argvlen); \
	} while(0)


/**
 *
 */
int tps_redis_insert_dialog(tps_data_t *td)
{
	char* argv[TPS_REDIS_NR_KEYS];
	size_t argvlen[TPS_REDIS_NR_KEYS];
	int argc = 0;
	str rcmd = str_init("HMSET");
	str rkey = STR_NULL;
	char *rp;
	str rval = STR_NULL;
	redisc_server_t *rsrv = NULL;
	redisReply *rrpl = NULL;
	unsigned long lval = 0;

	if(td->a_uuid.len<=0 && td->b_uuid.len<=0) {
		LM_INFO("no uuid for this message\n");
		return -1;
	}

	rsrv = _tps_redis_api.get_server(&_topos_redis_serverid);
	if(rsrv==NULL) {
		LM_ERR("cannot find redis server [%.*s]\n",
				_topos_redis_serverid.len, _topos_redis_serverid.s);
		return -1;
	}

	memset(argv, 0, TPS_REDIS_NR_KEYS * sizeof(char*));
	memset(argvlen, 0, TPS_REDIS_NR_KEYS * sizeof(size_t));
	argc = 0;

	rp = _tps_redis_cbuf;
	memcpy(rp, _tps_redis_dprefix.s, _tps_redis_dprefix.len);

	if(td->a_uuid.len>0) {
		memcpy(rp + _tps_redis_dprefix.len,
				td->a_uuid.s, td->a_uuid.len);
		if(td->a_uuid.s[0]=='b') {
			rp[_tps_redis_dprefix.len] = 'a';
		}
		rp[_tps_redis_dprefix.len+td->a_uuid.len] = '\0';
		rkey.s = rp;
		rkey.len = _tps_redis_dprefix.len+td->a_uuid.len;
		rp += _tps_redis_dprefix.len+td->a_uuid.len+1;
	} else {
		memcpy(rp + _tps_redis_dprefix.len,
				td->b_uuid.s, td->b_uuid.len);
		if(td->b_uuid.s[0]=='b') {
			rp[_tps_redis_dprefix.len] = 'a';
		}
		rp[_tps_redis_dprefix.len+td->b_uuid.len] = '\0';
		rkey.s = rp;
		rkey.len = _tps_redis_dprefix.len+td->b_uuid.len;
		rp += _tps_redis_dprefix.len+td->b_uuid.len+1;
	}

	argv[argc]    = rcmd.s;
	argvlen[argc] = rcmd.len;
	argc++;

	argv[argc]    = rkey.s;
	argvlen[argc] = rkey.len;
	argc++;

	lval = (unsigned long)time(NULL);
	TPS_REDIS_SET_ARGN(lval, rp, &rval, argc, &td_key_rectime,
			argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->a_callid, argc, &td_key_a_callid, argv, argvlen);

	TPS_REDIS_SET_ARGS(&td->a_uuid, argc, &td_key_a_uuid, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->b_uuid, argc, &td_key_b_uuid, argv, argvlen);

	TPS_REDIS_SET_ARGS(&td->a_contact, argc, &td_key_a_contact, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->b_contact, argc, &td_key_b_contact, argv, argvlen);

	TPS_REDIS_SET_ARGS(&td->as_contact, argc, &td_key_as_contact, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->bs_contact, argc, &td_key_bs_contact, argv, argvlen);

	TPS_REDIS_SET_ARGS(&td->a_tag, argc, &td_key_a_tag, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->b_tag, argc, &td_key_b_tag, argv, argvlen);

	TPS_REDIS_SET_ARGS(&td->a_rr, argc, &td_key_a_rr, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->b_rr, argc, &td_key_b_rr, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->s_rr, argc, &td_key_s_rr, argv, argvlen);

	TPS_REDIS_SET_ARGN(td->iflags, rp, &rval, argc, &td_key_iflags,
			argv, argvlen);

	TPS_REDIS_SET_ARGS(&td->a_uri, argc, &td_key_a_uri, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->b_uri, argc, &td_key_b_uri, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->r_uri, argc, &td_key_r_uri, argv, argvlen);

	TPS_REDIS_SET_ARGS(&td->a_srcaddr, argc, &td_key_a_srcaddr, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->b_srcaddr, argc, &td_key_b_srcaddr, argv, argvlen);

	TPS_REDIS_SET_ARGS(&td->s_method, argc, &td_key_s_method, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->s_cseq, argc, &td_key_s_cseq, argv, argvlen);

	rrpl = _tps_redis_api.exec_argv(rsrv, argc, (const char **)argv, argvlen);
	if(rrpl==NULL) {
		LM_ERR("failed to execute redis command\n");
		if(rsrv->ctxRedis->err) {
			LM_ERR("redis error: %s\n", rsrv->ctxRedis->errstr);
		}
		return -1;
	}
	LM_DBG("inserted dialog record for [%.*s] with argc %d\n",
			rkey.len, rkey.s, argc);
	freeReplyObject(rrpl);

	/* set expire for the key */
	argc = 0;

	argv[argc]    = "EXPIRE";
	argvlen[argc] = 6;
	argc++;

	argv[argc]    = rkey.s;
	argvlen[argc] = rkey.len;
	argc++;

	lval = (unsigned long)_tps_api.get_dialog_expire();
	if(lval==0) {
		return 0;
	}
	TPS_REDIS_SET_ARGNV(lval, rp, &rval, argc, argv, argvlen);

	rrpl = _tps_redis_api.exec_argv(rsrv, argc, (const char **)argv, argvlen);
	if(rrpl==NULL) {
		LM_ERR("failed to execute expire redis command\n");
		if(rsrv->ctxRedis->err) {
			LM_ERR("redis error: %s\n", rsrv->ctxRedis->errstr);
		}
		return -1;
	}
	LM_DBG("expire set on dialog record for [%.*s] with argc %d\n",
			rkey.len, rkey.s, argc);
	freeReplyObject(rrpl);

	return 0;
}

/**
 *
 */
int tps_redis_clean_dialogs(void)
{
	return 0;
}

/**
 *
 */
int tps_redis_insert_invite_branch(tps_data_t *td)
{
	char* argv[TPS_REDIS_NR_KEYS];
	size_t argvlen[TPS_REDIS_NR_KEYS];
	int argc = 0;
	str rcmd = str_init("HMSET");
	str rkey = STR_NULL;
	char *rp;
	str rval = STR_NULL;
	redisc_server_t *rsrv = NULL;
	redisReply *rrpl = NULL;
	unsigned long lval = 0;

	if(td->x_vbranch1.len<=0) {
		LM_INFO("no via branch for this message\n");
		return -1;
	}

	rsrv = _tps_redis_api.get_server(&_topos_redis_serverid);
	if(rsrv==NULL) {
		LM_ERR("cannot find redis server [%.*s]\n",
				_topos_redis_serverid.len, _topos_redis_serverid.s);
		return -1;
	}

	memset(argv, 0, TPS_REDIS_NR_KEYS * sizeof(char*));
	memset(argvlen, 0, TPS_REDIS_NR_KEYS * sizeof(size_t));
	argc = 0;

	rp = _tps_redis_cbuf;
	rkey.len = snprintf(rp, TPS_REDIS_DATA_SIZE-128,
					"%.*sINVITE:%.*s:%.*s",
					_tps_redis_bprefix.len, _tps_redis_bprefix.s,
					td->a_callid.len, td->a_callid.s,
					td->b_tag.len, td->b_tag.s);
	if(rkey.len<0 || rkey.len>=TPS_REDIS_DATA_SIZE-128) {
		LM_ERR("error or insufficient buffer size: %d\n", rkey.len);
		return -1;
	}
	rkey.s = rp;
	rp += rkey.len+1;

	argv[argc]    = rcmd.s;
	argvlen[argc] = rcmd.len;
	argc++;

	argv[argc]    = rkey.s;
	argvlen[argc] = rkey.len;
	argc++;

	lval = (unsigned long)time(NULL);
	TPS_REDIS_SET_ARGN(lval, rp, &rval, argc, &tt_key_rectime,
			argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->x_vbranch1, argc, &tt_key_x_vbranch, argv, argvlen);

	rrpl = _tps_redis_api.exec_argv(rsrv, argc, (const char **)argv, argvlen);
	if(rrpl==NULL) {
		LM_ERR("failed to execute redis command\n");
		if(rsrv->ctxRedis->err) {
			LM_ERR("redis error: %s\n", rsrv->ctxRedis->errstr);
		}
		return -1;
	}
	LM_DBG("inserting branch record for [%.*s] with argc %d\n",
			rkey.len, rkey.s, argc);

	freeReplyObject(rrpl);

	/* set expire for the key */
	argc = 0;

	argv[argc]    = "EXPIRE";
	argvlen[argc] = 6;
	argc++;

	argv[argc]    = rkey.s;
	argvlen[argc] = rkey.len;
	argc++;

	lval = (unsigned long)_tps_api.get_branch_expire();
	if(lval==0) {
		return 0;
	}
	TPS_REDIS_SET_ARGNV(lval, rp, &rval, argc, argv, argvlen);

	rrpl = _tps_redis_api.exec_argv(rsrv, argc, (const char **)argv, argvlen);
	if(rrpl==NULL) {
		LM_ERR("failed to execute expire redis command\n");
		if(rsrv->ctxRedis->err) {
			LM_ERR("redis error: %s\n", rsrv->ctxRedis->errstr);
		}
		return -1;
	}
	LM_DBG("expire set on branch record for [%.*s] with argc %d\n",
			rkey.len, rkey.s, argc);
	freeReplyObject(rrpl);

	return 0;
}


/**
 *
 */
int tps_redis_insert_branch(tps_data_t *td)
{
	char* argv[TPS_REDIS_NR_KEYS];
	size_t argvlen[TPS_REDIS_NR_KEYS];
	int argc = 0;
	str rcmd = str_init("HMSET");
	str rkey = STR_NULL;
	char *rp;
	str rval = STR_NULL;
	redisc_server_t *rsrv = NULL;
	redisReply *rrpl = NULL;
	unsigned long lval = 0;

	if(td->x_vbranch1.len<=0) {
		LM_INFO("no via branch for this message\n");
		return -1;
	}

	rsrv = _tps_redis_api.get_server(&_topos_redis_serverid);
	if(rsrv==NULL) {
		LM_ERR("cannot find redis server [%.*s]\n",
				_topos_redis_serverid.len, _topos_redis_serverid.s);
		return -1;
	}

	memset(argv, 0, TPS_REDIS_NR_KEYS * sizeof(char*));
	memset(argvlen, 0, TPS_REDIS_NR_KEYS * sizeof(size_t));
	argc = 0;

	rp = _tps_redis_cbuf;
	memcpy(rp, _tps_redis_bprefix.s, _tps_redis_bprefix.len);
	memcpy(rp + _tps_redis_bprefix.len,
			td->x_vbranch1.s, td->x_vbranch1.len);
	rp[_tps_redis_bprefix.len+td->x_vbranch1.len] = '\0';
	rkey.s = rp;
	rkey.len = _tps_redis_bprefix.len+td->x_vbranch1.len;
	rp += _tps_redis_bprefix.len+td->x_vbranch1.len+1;

	argv[argc]    = rcmd.s;
	argvlen[argc] = rcmd.len;
	argc++;

	argv[argc]    = rkey.s;
	argvlen[argc] = rkey.len;
	argc++;

	lval = (unsigned long)time(NULL);
	TPS_REDIS_SET_ARGN(lval, rp, &rval, argc, &tt_key_rectime,
			argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->a_callid, argc, &tt_key_a_callid, argv, argvlen);

	TPS_REDIS_SET_ARGS(&td->a_uuid, argc, &tt_key_a_uuid, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->b_uuid, argc, &tt_key_b_uuid, argv, argvlen);

	TPS_REDIS_SET_ARGN(td->direction, rp, &rval, argc, &tt_key_direction,
			argv, argvlen);

	TPS_REDIS_SET_ARGS(&td->x_via, argc, &tt_key_x_via, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->x_vbranch1, argc, &tt_key_x_vbranch, argv, argvlen);

	TPS_REDIS_SET_ARGS(&td->x_rr, argc, &tt_key_x_rr, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->y_rr, argc, &tt_key_y_rr, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->s_rr, argc, &tt_key_s_rr, argv, argvlen);

	TPS_REDIS_SET_ARGS(&td->x_uri, argc, &tt_key_x_uri, argv, argvlen);

	TPS_REDIS_SET_ARGS(&td->x_tag, argc, &tt_key_x_tag, argv, argvlen);

	TPS_REDIS_SET_ARGS(&td->s_method, argc, &tt_key_s_method, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->s_cseq, argc, &tt_key_s_cseq, argv, argvlen);

	TPS_REDIS_SET_ARGS(&td->a_contact, argc, &tt_key_a_contact, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->b_contact, argc, &tt_key_b_contact, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->as_contact, argc, &tt_key_as_contact, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->bs_contact, argc, &tt_key_bs_contact, argv, argvlen);

	TPS_REDIS_SET_ARGS(&td->a_tag, argc, &tt_key_a_tag, argv, argvlen);
	TPS_REDIS_SET_ARGS(&td->b_tag, argc, &tt_key_b_tag, argv, argvlen);

	rrpl = _tps_redis_api.exec_argv(rsrv, argc, (const char **)argv, argvlen);
	if(rrpl==NULL) {
		LM_ERR("failed to execute redis command\n");
		if(rsrv->ctxRedis->err) {
			LM_ERR("redis error: %s\n", rsrv->ctxRedis->errstr);
		}
		return -1;
	}
	LM_DBG("inserting branch record for [%.*s] with argc %d\n",
			rkey.len, rkey.s, argc);

	freeReplyObject(rrpl);

	/* set expire for the key */
	argc = 0;

	argv[argc]    = "EXPIRE";
	argvlen[argc] = 6;
	argc++;

	argv[argc]    = rkey.s;
	argvlen[argc] = rkey.len;
	argc++;

	lval = (unsigned long)_tps_api.get_branch_expire();
	if(lval==0) {
		return 0;
	}
	TPS_REDIS_SET_ARGNV(lval, rp, &rval, argc, argv, argvlen);

	rrpl = _tps_redis_api.exec_argv(rsrv, argc, (const char **)argv, argvlen);
	if(rrpl==NULL) {
		LM_ERR("failed to execute expire redis command\n");
		if(rsrv->ctxRedis->err) {
			LM_ERR("redis error: %s\n", rsrv->ctxRedis->errstr);
		}
		return -1;
	}
	LM_DBG("expire set on branch record for [%.*s] with argc %d\n",
			rkey.len, rkey.s, argc);
	freeReplyObject(rrpl);

	return 0;
}

/**
 *
 */
int tps_redis_clean_branches(void)
{
	return 0;
}

#define TPS_REDIS_DATA_APPEND(_sd, _k, _v, _r) \
	do { \
		if((_sd)->cp + (_v)->len >= (_sd)->cbuf + TPS_DATA_SIZE) { \
			LM_ERR("not enough space for %.*s\n", (_k)->len, (_k)->s); \
			goto error; \
		} \
		if((_v)->len>0) { \
			(_r)->s = (_sd)->cp; \
			(_r)->len = (_v)->len; \
			memcpy((_sd)->cp, (_v)->s, (_v)->len); \
			(_sd)->cp += (_v)->len; \
			(_sd)->cp[0] = '\0'; \
			(_sd)->cp++; \
		} \
	} while(0)

/**
 *
 */
int tps_redis_load_invite_branch(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd)
{
	char* argv[TPS_REDIS_NR_KEYS];
	size_t argvlen[TPS_REDIS_NR_KEYS];
	int argc = 0;
	str rcmd = str_init("HGETALL");
	str rkey = STR_NULL;
	char *rp;
	int i;
	redisc_server_t *rsrv = NULL;
	redisReply *rrpl = NULL;
	str skey = STR_NULL;
	str sval = STR_NULL;

	if(msg==NULL || md==NULL || sd==NULL)
		return -1;

	if(md->a_callid.len<=0 || md->b_tag.len<=0) {
		LM_INFO("no call-id or to-rag for this message\n");
		return -1;
	}

	rsrv = _tps_redis_api.get_server(&_topos_redis_serverid);
	if(rsrv==NULL) {
		LM_ERR("cannot find redis server [%.*s]\n",
				_topos_redis_serverid.len, _topos_redis_serverid.s);
		return -1;
	}

	memset(argv, 0, TPS_REDIS_NR_KEYS * sizeof(char*));
	memset(argvlen, 0, TPS_REDIS_NR_KEYS * sizeof(size_t));
	argc = 0;

	rp = _tps_redis_cbuf;

	rkey.len = snprintf(rp, TPS_REDIS_DATA_SIZE,
					"%.*sINVITE:%.*s:%.*s",
					_tps_redis_bprefix.len, _tps_redis_bprefix.s,
					md->a_callid.len, md->a_callid.s,
					md->b_tag.len, md->b_tag.s);
	if(rkey.len<0 || rkey.len>=TPS_REDIS_DATA_SIZE) {
		LM_ERR("error or insufficient buffer size: %d\n", rkey.len);
		return -1;
	}
	rkey.s = rp;
	rp += rkey.len+1;

	argv[argc]    = rcmd.s;
	argvlen[argc] = rcmd.len;
	argc++;

	argv[argc]    = rkey.s;
	argvlen[argc] = rkey.len;
	argc++;

	LM_DBG("loading branch record for [%.*s]\n", rkey.len, rkey.s);

	rrpl = _tps_redis_api.exec_argv(rsrv, argc, (const char **)argv, argvlen);
	if(rrpl==NULL) {
		LM_ERR("failed to execute redis command\n");
		if(rsrv->ctxRedis->err) {
			LM_ERR("redis error: %s\n", rsrv->ctxRedis->errstr);
		}
		return -1;
	}

	if(rrpl->type != REDIS_REPLY_ARRAY) {
		LM_WARN("invalid redis result type: %d\n", rrpl->type);
		freeReplyObject(rrpl);
		return -1;
	}

	if(rrpl->elements<=0) {
		LM_DBG("hmap with key [%.*s] not found\n", rkey.len, rkey.s);
		freeReplyObject(rrpl);
		return 1;
	}
	if(rrpl->elements % 2) {
		LM_DBG("hmap with key [%.*s] has invalid result\n", rkey.len, rkey.s);
		freeReplyObject(rrpl);
		return -1;
	}

	memset(sd, 0, sizeof(tps_data_t));
	sd->cp = sd->cbuf;

	for(i=0; i<rrpl->elements; i++) {
		if(rrpl->element[i]->type != REDIS_REPLY_STRING) {
			LM_ERR("invalid type for hmap[%.*s] key pos[%d]\n",
					rkey.len, rkey.s, i);
			freeReplyObject(rrpl);
			return -1;
		}
		skey.s = rrpl->element[i]->str;
		skey.len = rrpl->element[i]->len;
		i++;
		if(rrpl->element[i]==NULL) {
			continue;
		}
		sval.s = NULL;
		switch(rrpl->element[i]->type) {
			case REDIS_REPLY_STRING:
				LM_DBG("r[%d]: s[%.*s]\n", i, rrpl->element[i]->len,
						rrpl->element[i]->str);
				sval.s = rrpl->element[i]->str;
				sval.len = rrpl->element[i]->len;
				break;
			case REDIS_REPLY_INTEGER:
				LM_DBG("r[%d]: n[%lld]\n", i, rrpl->element[i]->integer);
				break;
			default:
				LM_WARN("unexpected type [%d] at pos [%d]\n",
						rrpl->element[i]->type, i);
		}
		if(sval.s==NULL) {
			continue;
		}

		if(skey.len==tt_key_rectime.len
				&& strncmp(skey.s, tt_key_rectime.s, skey.len)==0) {
			/* skip - not needed */
		} else if(skey.len==tt_key_x_vbranch.len
				&& strncmp(skey.s, tt_key_x_vbranch.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->x_vbranch1);
		} else {
			LM_INFO("unuseful key[%.*s]\n", skey.len, skey.s);
		}
	}

	freeReplyObject(rrpl);
	return 0;

error:
	if(rrpl) freeReplyObject(rrpl);
	return -1;
}

/**
 *
 */
int tps_redis_load_branch(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd,
		uint32_t mode)
{
	char* argv[TPS_REDIS_NR_KEYS];
	size_t argvlen[TPS_REDIS_NR_KEYS];
	int argc = 0;
	str rcmd = str_init("HGETALL");
	str rkey = STR_NULL;
	char *rp;
	int i;
	redisc_server_t *rsrv = NULL;
	redisReply *rrpl = NULL;
	str skey = STR_NULL;
	str sval = STR_NULL;
	str *xvbranch1 = NULL;
	tps_data_t id;

	if(msg==NULL || md==NULL || sd==NULL)
		return -1;

	if(mode==0 && md->x_vbranch1.len<=0) {
		LM_INFO("no via branch for this message\n");
		return -1;
	}

	rsrv = _tps_redis_api.get_server(&_topos_redis_serverid);
	if(rsrv==NULL) {
		LM_ERR("cannot find redis server [%.*s]\n",
				_topos_redis_serverid.len, _topos_redis_serverid.s);
		return -1;
	}

	memset(argv, 0, TPS_REDIS_NR_KEYS * sizeof(char*));
	memset(argvlen, 0, TPS_REDIS_NR_KEYS * sizeof(size_t));
	argc = 0;

	if(mode==0) {
		/* load same transaction using Via branch */
		xvbranch1 = &md->x_vbranch1;
	} else {
		/* load corresponding INVITE transaction using call-id + to-tag */
		if(tps_redis_load_invite_branch(msg, md, &id)<0) {
			LM_ERR("failed to load the INVITE branch value\n");
			return -1;
		}
		xvbranch1 = &id.x_vbranch1;
	}
	rp = _tps_redis_cbuf;
	memcpy(rp, _tps_redis_bprefix.s, _tps_redis_bprefix.len);
	memcpy(rp + _tps_redis_bprefix.len,
			xvbranch1->s, xvbranch1->len);
	rp[_tps_redis_bprefix.len+xvbranch1->len] = '\0';
	rkey.s = rp;
	rkey.len = _tps_redis_bprefix.len+xvbranch1->len;
	rp += _tps_redis_bprefix.len+xvbranch1->len+1;

	argv[argc]    = rcmd.s;
	argvlen[argc] = rcmd.len;
	argc++;

	argv[argc]    = rkey.s;
	argvlen[argc] = rkey.len;
	argc++;

	LM_DBG("loading branch record for [%.*s]\n", rkey.len, rkey.s);

	rrpl = _tps_redis_api.exec_argv(rsrv, argc, (const char **)argv, argvlen);
	if(rrpl==NULL) {
		LM_ERR("failed to execute redis command\n");
		if(rsrv->ctxRedis->err) {
			LM_ERR("redis error: %s\n", rsrv->ctxRedis->errstr);
		}
		return -1;
	}

	if(rrpl->type != REDIS_REPLY_ARRAY) {
		LM_WARN("invalid redis result type: %d\n", rrpl->type);
		freeReplyObject(rrpl);
		return -1;
	}

	if(rrpl->elements<=0) {
		LM_DBG("hmap with key [%.*s] not found\n", rkey.len, rkey.s);
		freeReplyObject(rrpl);
		return 1;
	}
	if(rrpl->elements % 2) {
		LM_DBG("hmap with key [%.*s] has invalid result\n", rkey.len, rkey.s);
		freeReplyObject(rrpl);
		return -1;
	}

	sd->cp = sd->cbuf;

	for(i=0; i<rrpl->elements; i++) {
		if(rrpl->element[i]->type != REDIS_REPLY_STRING) {
			LM_ERR("invalid type for hmap[%.*s] key pos[%d]\n",
					rkey.len, rkey.s, i);
			freeReplyObject(rrpl);
			return -1;
		}
		skey.s = rrpl->element[i]->str;
		skey.len = rrpl->element[i]->len;
		i++;
		if(rrpl->element[i]==NULL) {
			continue;
		}
		sval.s = NULL;
		switch(rrpl->element[i]->type) {
			case REDIS_REPLY_STRING:
				LM_DBG("r[%d]: s[%.*s]\n", i, rrpl->element[i]->len,
						rrpl->element[i]->str);
				sval.s = rrpl->element[i]->str;
				sval.len = rrpl->element[i]->len;
				break;
			case REDIS_REPLY_INTEGER:
				LM_DBG("r[%d]: n[%lld]\n", i, rrpl->element[i]->integer);
				break;
			default:
				LM_WARN("unexpected type [%d] at pos [%d]\n",
						rrpl->element[i]->type, i);
		}
		if(sval.s==NULL) {
			continue;
		}

		if(skey.len==tt_key_rectime.len
				&& strncmp(skey.s, tt_key_rectime.s, skey.len)==0) {
			/* skip - not needed */
		} else if(skey.len==tt_key_a_callid.len
				&& strncmp(skey.s, tt_key_a_callid.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->a_callid);
		} else if(skey.len==tt_key_a_uuid.len
				&& strncmp(skey.s, tt_key_a_uuid.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->a_uuid);
		} else if(skey.len==tt_key_b_uuid.len
				&& strncmp(skey.s, tt_key_b_uuid.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->b_uuid);
		} else if(skey.len==tt_key_direction.len
				&& strncmp(skey.s, tt_key_direction.s, skey.len)==0) {
			/* skip - not needed */
		} else if(skey.len==tt_key_x_via.len
				&& strncmp(skey.s, tt_key_x_via.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->x_via);
		} else if(skey.len==tt_key_x_vbranch.len
				&& strncmp(skey.s, tt_key_x_vbranch.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->x_vbranch1);
		} else if(skey.len==tt_key_x_rr.len
				&& strncmp(skey.s, tt_key_x_rr.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->x_rr);
		} else if(skey.len==tt_key_y_rr.len
				&& strncmp(skey.s, tt_key_y_rr.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->y_rr);
		} else if(skey.len==tt_key_s_rr.len
				&& strncmp(skey.s, tt_key_s_rr.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->s_rr);
		} else if(skey.len==tt_key_x_uri.len
				&& strncmp(skey.s, tt_key_x_uri.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->x_uri);
		} else if(skey.len==tt_key_x_tag.len
				&& strncmp(skey.s, tt_key_x_tag.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->x_tag);
		} else if(skey.len==tt_key_s_method.len
				&& strncmp(skey.s, tt_key_s_method.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->s_method);
		} else if(skey.len==tt_key_s_cseq.len
				&& strncmp(skey.s, tt_key_s_cseq.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->s_cseq);
		} else if(skey.len==tt_key_a_contact.len
				&& strncmp(skey.s, tt_key_a_contact.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->a_contact);
		} else if(skey.len==tt_key_b_contact.len
				&& strncmp(skey.s, tt_key_b_contact.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->b_contact);
		} else if(skey.len==tt_key_as_contact.len
				&& strncmp(skey.s, tt_key_as_contact.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->as_contact);
		} else if(skey.len==tt_key_bs_contact.len
				&& strncmp(skey.s, tt_key_bs_contact.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->bs_contact);
		} else if(skey.len==tt_key_a_tag.len
				&& strncmp(skey.s, tt_key_a_tag.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->a_tag);
		} else if(skey.len==tt_key_b_tag.len
				&& strncmp(skey.s, tt_key_b_tag.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->b_tag);
		} else {
			LM_WARN("unknow key[%.*s]\n", skey.len, skey.s);
		}
	}

	freeReplyObject(rrpl);
	return 0;

error:
	if(rrpl) freeReplyObject(rrpl);
	return -1;
}

/**
 *
 */
int tps_redis_load_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd)
{
	char* argv[TPS_REDIS_NR_KEYS];
	size_t argvlen[TPS_REDIS_NR_KEYS];
	int argc = 0;
	str rcmd = str_init("HGETALL");
	str rkey = STR_NULL;
	char *rp;
	int i;
	redisc_server_t *rsrv = NULL;
	redisReply *rrpl = NULL;
	str skey = STR_NULL;
	str sval = STR_NULL;

	if(msg==NULL || md==NULL || sd==NULL)
		return -1;

	if(md->a_uuid.len<=0 && md->b_uuid.len<=0) {
		LM_DBG("no dlg uuid provided\n");
		return -1;
	}
	rsrv = _tps_redis_api.get_server(&_topos_redis_serverid);
	if(rsrv==NULL) {
		LM_ERR("cannot find redis server [%.*s]\n",
				_topos_redis_serverid.len, _topos_redis_serverid.s);
		return -1;
	}

	memset(argv, 0, TPS_REDIS_NR_KEYS * sizeof(char*));
	memset(argvlen, 0, TPS_REDIS_NR_KEYS * sizeof(size_t));
	argc = 0;

	rp = _tps_redis_cbuf;
	memcpy(rp, _tps_redis_dprefix.s, _tps_redis_dprefix.len);

	if(md->a_uuid.len>0) {
		memcpy(rp + _tps_redis_dprefix.len,
				md->a_uuid.s, md->a_uuid.len);
		if(md->a_uuid.s[0]=='b') {
			rp[_tps_redis_dprefix.len] = 'a';
		}
		rp[_tps_redis_dprefix.len+md->a_uuid.len] = '\0';
		rkey.s = rp;
		rkey.len = _tps_redis_dprefix.len+md->a_uuid.len;
		rp += _tps_redis_dprefix.len+md->a_uuid.len+1;
	} else {
		memcpy(rp + _tps_redis_dprefix.len,
				md->b_uuid.s, md->b_uuid.len);
		if(md->b_uuid.s[0]=='b') {
			rp[_tps_redis_dprefix.len] = 'a';
		}
		rp[_tps_redis_dprefix.len+md->b_uuid.len] = '\0';
		rkey.s = rp;
		rkey.len = _tps_redis_dprefix.len+md->b_uuid.len;
		rp += _tps_redis_dprefix.len+md->b_uuid.len+1;
	}

	argv[argc]    = rcmd.s;
	argvlen[argc] = rcmd.len;
	argc++;

	argv[argc]    = rkey.s;
	argvlen[argc] = rkey.len;
	argc++;

	LM_DBG("loading dialog record for [%.*s]\n", rkey.len, rkey.s);

	rrpl = _tps_redis_api.exec_argv(rsrv, argc, (const char **)argv, argvlen);
	if(rrpl==NULL) {
		LM_ERR("failed to execute redis command\n");
		if(rsrv->ctxRedis->err) {
			LM_ERR("redis error: %s\n", rsrv->ctxRedis->errstr);
		}
		return -1;
	}

	if(rrpl->type != REDIS_REPLY_ARRAY) {
		LM_WARN("invalid redis result type: %d\n", rrpl->type);
		freeReplyObject(rrpl);
		return -1;
	}

	if(rrpl->elements<=0) {
		LM_DBG("hmap with key [%.*s] not found\n", rkey.len, rkey.s);
		freeReplyObject(rrpl);
		return 1;
	}
	if(rrpl->elements % 2) {
		LM_DBG("hmap with key [%.*s] has invalid result\n", rkey.len, rkey.s);
		freeReplyObject(rrpl);
		return -1;
	}

	sd->cp = sd->cbuf;

	for(i=0; i<rrpl->elements; i++) {
		if(rrpl->element[i]->type != REDIS_REPLY_STRING) {
			LM_ERR("invalid type for hmap[%.*s] key pos[%d]\n",
					rkey.len, rkey.s, i);
			freeReplyObject(rrpl);
			return -1;
		}
		skey.s = rrpl->element[i]->str;
		skey.len = rrpl->element[i]->len;
		i++;
		if(rrpl->element[i]==NULL) {
			continue;
		}
		sval.s = NULL;
		switch(rrpl->element[i]->type) {
			case REDIS_REPLY_STRING:
				LM_DBG("r[%d]: s[%.*s]\n", i, rrpl->element[i]->len,
						rrpl->element[i]->str);
				sval.s = rrpl->element[i]->str;
				sval.len = rrpl->element[i]->len;
				break;
			case REDIS_REPLY_INTEGER:
				LM_DBG("r[%d]: n[%lld]\n", i, rrpl->element[i]->integer);
				break;
			default:
				LM_WARN("unexpected type [%d] at pos [%d]\n",
						rrpl->element[i]->type, i);
		}
		if(sval.s==NULL) {
			continue;
		}
		if(skey.len==td_key_rectime.len
				&& strncmp(skey.s, td_key_rectime.s, skey.len)==0) {
			/* skip - not needed */
		} else if(skey.len==td_key_a_callid.len
				&& strncmp(skey.s, td_key_a_callid.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->a_callid);
		} else if(skey.len==td_key_a_uuid.len
				&& strncmp(skey.s, td_key_a_uuid.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->a_uuid);
		} else if(skey.len==td_key_b_uuid.len
				&& strncmp(skey.s, td_key_b_uuid.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->b_uuid);
		} else if(skey.len==td_key_a_contact.len
				&& strncmp(skey.s, td_key_a_contact.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->a_contact);
		} else if(skey.len==td_key_b_contact.len
				&& strncmp(skey.s, td_key_b_contact.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->b_contact);
		} else if(skey.len==td_key_as_contact.len
				&& strncmp(skey.s, td_key_as_contact.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->as_contact);
		} else if(skey.len==td_key_bs_contact.len
				&& strncmp(skey.s, td_key_bs_contact.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->bs_contact);
		} else if(skey.len==td_key_a_tag.len
				&& strncmp(skey.s, td_key_a_tag.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->a_tag);
		} else if(skey.len==td_key_b_tag.len
				&& strncmp(skey.s, td_key_b_tag.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->b_tag);
		} else if(skey.len==td_key_a_rr.len
				&& strncmp(skey.s, td_key_a_rr.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->a_rr);
		} else if(skey.len==td_key_b_rr.len
				&& strncmp(skey.s, td_key_b_rr.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->b_rr);
		} else if(skey.len==td_key_s_rr.len
				&& strncmp(skey.s, td_key_s_rr.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->s_rr);
		} else if(skey.len==td_key_iflags.len
				&& strncmp(skey.s, td_key_iflags.s, skey.len)==0) {
			/* skip - not needed */
		} else if(skey.len==td_key_a_uri.len
				&& strncmp(skey.s, td_key_a_uri.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->a_uri);
		} else if(skey.len==td_key_b_uri.len
				&& strncmp(skey.s, td_key_b_uri.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->b_uri);
		} else if(skey.len==td_key_r_uri.len
				&& strncmp(skey.s, td_key_r_uri.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->r_uri);
		} else if(skey.len==td_key_a_srcaddr.len
				&& strncmp(skey.s, td_key_a_srcaddr.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->a_srcaddr);
		} else if(skey.len==td_key_b_srcaddr.len
				&& strncmp(skey.s, td_key_b_srcaddr.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->b_srcaddr);
		} else if(skey.len==td_key_s_method.len
				&& strncmp(skey.s, td_key_s_method.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->s_method);
		} else if(skey.len==td_key_s_cseq.len
				&& strncmp(skey.s, td_key_s_cseq.s, skey.len)==0) {
			TPS_REDIS_DATA_APPEND(sd, &skey, &sval, &sd->s_cseq);
		} else {
			LM_WARN("unknow key[%.*s]\n", skey.len, skey.s);
		}
	}

	freeReplyObject(rrpl);
	return 0;

error:
	if(rrpl) freeReplyObject(rrpl);
	return -1;
}

/**
 *
 */
int tps_redis_update_branch(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd,
		uint32_t mode)
{
	char* argv[TPS_REDIS_NR_KEYS];
	size_t argvlen[TPS_REDIS_NR_KEYS];
	int argc = 0;
	str rcmd = str_init("HMSET");
	str rkey = STR_NULL;
	char *rp;
	redisc_server_t *rsrv = NULL;
	redisReply *rrpl = NULL;

	if(sd->a_uuid.len<=0 && sd->b_uuid.len<=0) {
		LM_INFO("no uuid for this message\n");
		return -1;
	}

	if(md->s_method.len==6 && strncmp(md->s_method.s, "INVITE", 6)==0) {
		if(tps_redis_insert_invite_branch(md)<0) {
			LM_ERR("failed to insert INVITE extra branch data\n");
			return -1;
		}
	}
	rsrv = _tps_redis_api.get_server(&_topos_redis_serverid);
	if(rsrv==NULL) {
		LM_ERR("cannot find redis server [%.*s]\n",
				_topos_redis_serverid.len, _topos_redis_serverid.s);
		return -1;
	}

	memset(argv, 0, TPS_REDIS_NR_KEYS * sizeof(char*));
	memset(argvlen, 0, TPS_REDIS_NR_KEYS * sizeof(size_t));
	argc = 0;

	rp = _tps_redis_cbuf;
	memcpy(rp, _tps_redis_bprefix.s, _tps_redis_bprefix.len);

	memcpy(rp + _tps_redis_bprefix.len,
			sd->x_vbranch1.s, sd->x_vbranch1.len);
	rp[_tps_redis_bprefix.len+sd->x_vbranch1.len] = '\0';
	rkey.s = rp;
	rkey.len = _tps_redis_bprefix.len+sd->x_vbranch1.len;
	rp += _tps_redis_bprefix.len+sd->x_vbranch1.len+1;

	argv[argc]    = rcmd.s;
	argvlen[argc] = rcmd.len;
	argc++;

	argv[argc]    = rkey.s;
	argvlen[argc] = rkey.len;
	argc++;

	if(mode & TPS_DBU_CONTACT) {
		TPS_REDIS_SET_ARGS(&md->b_contact, argc, &tt_key_b_contact,
				argv, argvlen);
		TPS_REDIS_SET_ARGS(&md->b_contact, argc, &tt_key_b_contact,
				argv, argvlen);
	}

	if((mode & TPS_DBU_RPLATTRS) && msg->first_line.type==SIP_REPLY) {
		if(sd->b_tag.len<=0
				&& msg->first_line.u.reply.statuscode>=180
				&& msg->first_line.u.reply.statuscode<200) {

			TPS_REDIS_SET_ARGS(&md->b_rr, argc, &tt_key_y_rr, argv, argvlen);

			TPS_REDIS_SET_ARGS(&md->b_tag, argc, &tt_key_b_tag, argv, argvlen);
		}
	}

	if(argc<=2) {
		return 0;
	}

	rrpl = _tps_redis_api.exec_argv(rsrv, argc, (const char **)argv, argvlen);
	if(rrpl==NULL) {
		LM_ERR("failed to execute redis command for branch update\n");
		if(rsrv->ctxRedis->err) {
			LM_ERR("redis error: %s\n", rsrv->ctxRedis->errstr);
		}
		return -1;
	}
	LM_DBG("updated branch record for [%.*s] with argc %d\n",
			rkey.len, rkey.s, argc);
	freeReplyObject(rrpl);

	return 0;
}

/**
 *
 */
int tps_redis_update_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd,
		uint32_t mode)
{
	char* argv[TPS_REDIS_NR_KEYS];
	size_t argvlen[TPS_REDIS_NR_KEYS];
	int argc = 0;
	str rcmd = str_init("HMSET");
	str rkey = STR_NULL;
	char *rp;
	str rval = STR_NULL;
	redisc_server_t *rsrv = NULL;
	redisReply *rrpl = NULL;
	int32_t liflags;

	if(sd->a_uuid.len<=0 && sd->b_uuid.len<=0) {
		LM_INFO("no uuid for this message\n");
		return -1;
	}

	rsrv = _tps_redis_api.get_server(&_topos_redis_serverid);
	if(rsrv==NULL) {
		LM_ERR("cannot find redis server [%.*s]\n",
				_topos_redis_serverid.len, _topos_redis_serverid.s);
		return -1;
	}

	memset(argv, 0, TPS_REDIS_NR_KEYS * sizeof(char*));
	memset(argvlen, 0, TPS_REDIS_NR_KEYS * sizeof(size_t));
	argc = 0;

	rp = _tps_redis_cbuf;
	memcpy(rp, _tps_redis_dprefix.s, _tps_redis_dprefix.len);

	if(sd->a_uuid.len>0) {
		memcpy(rp + _tps_redis_dprefix.len,
				sd->a_uuid.s, sd->a_uuid.len);
		if(sd->a_uuid.s[0]=='b') {
			rp[_tps_redis_dprefix.len] = 'a';
		}
		rp[_tps_redis_dprefix.len+sd->a_uuid.len] = '\0';
		rkey.s = rp;
		rkey.len = _tps_redis_dprefix.len+sd->a_uuid.len;
		rp += _tps_redis_dprefix.len+sd->a_uuid.len+1;
	} else {
		memcpy(rp + _tps_redis_dprefix.len,
				sd->b_uuid.s, sd->b_uuid.len);
		if(sd->b_uuid.s[0]=='b') {
			rp[_tps_redis_dprefix.len] = 'a';
		}
		rp[_tps_redis_dprefix.len+sd->b_uuid.len] = '\0';
		rkey.s = rp;
		rkey.len = _tps_redis_dprefix.len+sd->b_uuid.len;
		rp += _tps_redis_dprefix.len+sd->b_uuid.len+1;
	}

	argv[argc]    = rcmd.s;
	argvlen[argc] = rcmd.len;
	argc++;

	argv[argc]    = rkey.s;
	argvlen[argc] = rkey.len;
	argc++;

	if(mode & TPS_DBU_CONTACT) {
		TPS_REDIS_SET_ARGS(&md->b_contact, argc, &td_key_b_contact,
				argv, argvlen);
		TPS_REDIS_SET_ARGS(&md->b_contact, argc, &td_key_b_contact,
				argv, argvlen);
	}

	if((mode & TPS_DBU_RPLATTRS) && msg->first_line.type==SIP_REPLY) {
		if(sd->b_tag.len<=0
				&& msg->first_line.u.reply.statuscode>=200
				&& msg->first_line.u.reply.statuscode<300) {

			if((sd->iflags&TPS_IFLAG_DLGON) == 0) {
				TPS_REDIS_SET_ARGS(&md->b_rr, argc, &td_key_b_rr, argv, argvlen);
			}

			TPS_REDIS_SET_ARGS(&md->b_tag, argc, &td_key_b_tag, argv, argvlen);

			liflags = sd->iflags|TPS_IFLAG_DLGON;
			TPS_REDIS_SET_ARGN(liflags, rp, &rval, argc, &td_key_iflags,
					argv, argvlen);
		}
	}

	if(argc<=2) {
		return 0;
	}

	rrpl = _tps_redis_api.exec_argv(rsrv, argc, (const char **)argv, argvlen);
	if(rrpl==NULL) {
		LM_ERR("failed to execute redis command for dialog update\n");
		if(rsrv->ctxRedis->err) {
			LM_ERR("redis error: %s\n", rsrv->ctxRedis->errstr);
		}
		return -1;
	}
	LM_DBG("updated dialog record for [%.*s] with argc %d\n",
			rkey.len, rkey.s, argc);
	freeReplyObject(rrpl);

	return 0;
}

/**
 *
 */
int tps_redis_end_dialog(sip_msg_t *msg, tps_data_t *md, tps_data_t *sd)
{
	char* argv[TPS_REDIS_NR_KEYS];
	size_t argvlen[TPS_REDIS_NR_KEYS];
	int argc = 0;
	str rcmd = str_init("HMSET");
	str rkey = STR_NULL;
	char *rp;
	str rval = STR_NULL;
	redisc_server_t *rsrv = NULL;
	redisReply *rrpl = NULL;
	int32_t liflags;
	unsigned long lval = 0;

	if(sd->a_uuid.len<=0 && sd->b_uuid.len<=0) {
		LM_INFO("no uuid for this message\n");
		return -1;
	}

	rsrv = _tps_redis_api.get_server(&_topos_redis_serverid);
	if(rsrv==NULL) {
		LM_ERR("cannot find redis server [%.*s]\n",
				_topos_redis_serverid.len, _topos_redis_serverid.s);
		return -1;
	}

	memset(argv, 0, TPS_REDIS_NR_KEYS * sizeof(char*));
	memset(argvlen, 0, TPS_REDIS_NR_KEYS * sizeof(size_t));
	argc = 0;

	rp = _tps_redis_cbuf;
	memcpy(rp, _tps_redis_dprefix.s, _tps_redis_dprefix.len);

	if(sd->a_uuid.len>0) {
		memcpy(rp + _tps_redis_dprefix.len,
				sd->a_uuid.s, sd->a_uuid.len);
		if(sd->a_uuid.s[0]=='b') {
			rp[_tps_redis_dprefix.len] = 'a';
		}
		rp[_tps_redis_dprefix.len+sd->a_uuid.len] = '\0';
		rkey.s = rp;
		rkey.len = _tps_redis_dprefix.len+sd->a_uuid.len;
		rp += _tps_redis_dprefix.len+sd->a_uuid.len+1;
	} else {
		memcpy(rp + _tps_redis_dprefix.len,
				sd->b_uuid.s, sd->b_uuid.len);
		if(sd->b_uuid.s[0]=='b') {
			rp[_tps_redis_dprefix.len] = 'a';
		}
		rp[_tps_redis_dprefix.len+sd->b_uuid.len] = '\0';
		rkey.s = rp;
		rkey.len = _tps_redis_dprefix.len+sd->b_uuid.len;
		rp += _tps_redis_dprefix.len+sd->b_uuid.len+1;
	}

	argv[argc]    = rcmd.s;
	argvlen[argc] = rcmd.len;
	argc++;

	argv[argc]    = rkey.s;
	argvlen[argc] = rkey.len;
	argc++;

	lval = (unsigned long)time(NULL);
	TPS_REDIS_SET_ARGN(lval, rp, &rval, argc, &td_key_rectime,
			argv, argvlen);
	liflags = 0;
	TPS_REDIS_SET_ARGN(liflags, rp, &rval, argc, &td_key_iflags,
					argv, argvlen);

	rrpl = _tps_redis_api.exec_argv(rsrv, argc, (const char **)argv, argvlen);
	if(rrpl==NULL) {
		LM_ERR("failed to execute redis command\n");
		if(rsrv->ctxRedis->err) {
			LM_ERR("redis error: %s\n", rsrv->ctxRedis->errstr);
		}
		return -1;
	}
	LM_DBG("updated on end the dialog record for [%.*s] with argc %d\n",
			rkey.len, rkey.s, argc);

	freeReplyObject(rrpl);

	/* set expire for the key */
	argc = 0;

	argv[argc]    = "EXPIRE";
	argvlen[argc] = 6;
	argc++;

	argv[argc]    = rkey.s;
	argvlen[argc] = rkey.len;
	argc++;

	/* dialog ended -- keep it for branch lifetime only */
	lval = (unsigned long)_tps_api.get_branch_expire();
	if(lval==0) {
		return 0;
	}
	TPS_REDIS_SET_ARGNV(lval, rp, &rval, argc, argv, argvlen);

	rrpl = _tps_redis_api.exec_argv(rsrv, argc, (const char **)argv, argvlen);
	if(rrpl==NULL) {
		LM_ERR("failed to execute expire redis command\n");
		if(rsrv->ctxRedis->err) {
			LM_ERR("redis error: %s\n", rsrv->ctxRedis->errstr);
		}
		return -1;
	}
	LM_DBG("expire set on branch record for [%.*s] with argc %d\n",
			rkey.len, rkey.s, argc);
	freeReplyObject(rrpl);

	return 0;
}
