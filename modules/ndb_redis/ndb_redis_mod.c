/**
 * $Id$
 *
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
 *
 * Copyright (C) 2012 Vicente Hernando Ara (System One: www.systemonenoc.com)
 *     - for: redis array reply support
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
#include <ctype.h>

#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../mod_fix.h"
#include "../../trim.h"

#include "redis_client.h"

MODULE_VERSION

/** parameters */

int redis_srv_param(modparam_t type, void *val);
int init_without_redis = 0;

static int w_redis_cmd3(struct sip_msg* msg, char* ssrv, char* scmd,
		char* sres);
static int w_redis_cmd4(struct sip_msg* msg, char* ssrv, char* scmd,
		char *sargv1, char* sres);
static int w_redis_cmd5(struct sip_msg* msg, char* ssrv, char* scmd,
		char *sargv1, char *sargv2, char* sres);
static int w_redis_cmd6(struct sip_msg* msg, char* ssrv, char* scmd,
		char *sargv1, char *sargv2, char *sargv3, char* sres);
static int fixup_redis_cmd6(void** param, int param_no);

static int w_redis_free_reply(struct sip_msg* msg, char* res);

static void mod_destroy(void);
static int  child_init(int rank);

static int pv_get_redisc(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res);
static int pv_parse_redisc_name(pv_spec_p sp, str *in);

static pv_export_t mod_pvs[] = {
	{ {"redis", sizeof("redis")-1}, PVT_OTHER, pv_get_redisc, 0,
		pv_parse_redisc_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


static cmd_export_t cmds[]={
	{"redis_cmd", (cmd_function)w_redis_cmd3, 3, fixup_redis_cmd6,
		0, ANY_ROUTE},
	{"redis_cmd", (cmd_function)w_redis_cmd4, 4, fixup_redis_cmd6,
		0, ANY_ROUTE},
	{"redis_cmd", (cmd_function)w_redis_cmd5, 5, fixup_redis_cmd6,
		0, ANY_ROUTE},
	{"redis_cmd", (cmd_function)w_redis_cmd6, 6, fixup_redis_cmd6,
		0, ANY_ROUTE},
	{"redis_free", (cmd_function)w_redis_free_reply, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"server",         PARAM_STRING|USE_FUNC_PARAM, (void*)redis_srv_param},
	{"init_without_redis", INT_PARAM, &init_without_redis},
	{0, 0, 0}
};

struct module_exports exports = {
	"ndb_redis",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,
	0,              /* exported MI functions */
	mod_pvs,        /* exported pseudo-variables */
	0,              /* extra processes */
	0,       /* module initialization function */
	0,              /* response function */
	mod_destroy,    /* destroy function */
	child_init      /* per child init function */
};



/* each child get a new connection to the database */
static int child_init(int rank)
{
	/* skip child init for non-worker process ranks */
	if (rank==PROC_INIT || rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0;

	if(redisc_init()<0)
	{
	  LM_ERR("failed to initialize redis connections\n");
	  return -1;
	}
	return 0;
}

/**
 *
 */
static void mod_destroy(void)
{
	LM_DBG("cleaning up\n");
	redisc_destroy();
}

/**
 *
 */
static int w_redis_cmd3(struct sip_msg* msg, char* ssrv, char* scmd, char* sres)
{
	str s[3];

	if(fixup_get_svalue(msg, (gparam_t*)ssrv, &s[0])!=0)
	{
		LM_ERR("no redis server name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)scmd, &s[1])!=0)
	{
		LM_ERR("no redis command\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sres, &s[2])!=0)
	{
		LM_ERR("no redis reply name\n");
		return -1;
	}

	if(redisc_exec(&s[0], &s[2], &s[1])<0)
		return -1;
	return 1;
}

/**
 *
 */
static int w_redis_cmd4(struct sip_msg* msg, char* ssrv, char* scmd,
		char *sargv1, char* sres)
{
	str s[3];
	str arg1;
	char c1;

	if(fixup_get_svalue(msg, (gparam_t*)ssrv, &s[0])!=0)
	{
		LM_ERR("no redis server name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)scmd, &s[1])!=0)
	{
		LM_ERR("no redis command\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sargv1, &arg1)!=0)
	{
		LM_ERR("no argument 1\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sres, &s[2])!=0)
	{
		LM_ERR("no redis reply name\n");
		return -1;
	}

	c1 = arg1.s[arg1.len];
	arg1.s[arg1.len] = '\0';
	if(redisc_exec(&s[0], &s[2], &s[1], arg1.s)<0) {
		arg1.s[arg1.len] = c1;
		return -1;
	}
	arg1.s[arg1.len] = c1;
	return 1;
}

/**
 *
 */
static int w_redis_cmd5(struct sip_msg* msg, char* ssrv, char* scmd,
		char *sargv1, char *sargv2, char* sres)
{
	str s[3];
	str arg1, arg2;
	char c1, c2;

	if(fixup_get_svalue(msg, (gparam_t*)ssrv, &s[0])!=0)
	{
		LM_ERR("no redis server name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)scmd, &s[1])!=0)
	{
		LM_ERR("no redis command\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sargv1, &arg1)!=0)
	{
		LM_ERR("no argument 1\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sargv2, &arg2)!=0)
	{
		LM_ERR("no argument 2\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sres, &s[2])!=0)
	{
		LM_ERR("no redis reply name\n");
		return -1;
	}

	c1 = arg1.s[arg1.len];
	c2 = arg2.s[arg2.len];
	arg1.s[arg1.len] = '\0';
	arg2.s[arg2.len] = '\0';
	if(redisc_exec(&s[0], &s[2], &s[1], arg1.s, arg2.s)<0) {
		arg1.s[arg1.len] = c1;
		arg2.s[arg2.len] = c2;
		return -1;
	}
	arg1.s[arg1.len] = c1;
	arg2.s[arg2.len] = c2;
	return 1;
}

/**
 *
 */
static int w_redis_cmd6(struct sip_msg* msg, char* ssrv, char* scmd,
		char *sargv1, char *sargv2, char *sargv3, char* sres)
{
	str s[3];
	str arg1, arg2, arg3;
	char c1, c2, c3;

	if(fixup_get_svalue(msg, (gparam_t*)ssrv, &s[0])!=0)
	{
		LM_ERR("no redis server name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)scmd, &s[1])!=0)
	{
		LM_ERR("no redis command\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sargv1, &arg1)!=0)
	{
		LM_ERR("no argument 1\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sargv2, &arg2)!=0)
	{
		LM_ERR("no argument 2\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sargv3, &arg3)!=0)
	{
		LM_ERR("no argument 3\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sres, &s[2])!=0)
	{
		LM_ERR("no redis reply name\n");
		return -1;
	}

	c1 = arg1.s[arg1.len];
	c2 = arg2.s[arg2.len];
	c3 = arg3.s[arg3.len];
	arg1.s[arg1.len] = '\0';
	arg2.s[arg2.len] = '\0';
	arg3.s[arg3.len] = '\0';
	if(redisc_exec(&s[0], &s[2], &s[1], arg1.s, arg2.s, arg3.s)<0) {
		arg1.s[arg1.len] = c1;
		arg2.s[arg2.len] = c2;
		arg3.s[arg3.len] = c3;
		return -1;
	}
	arg1.s[arg1.len] = c1;
	arg2.s[arg2.len] = c2;
	arg3.s[arg3.len] = c3;
	return 1;
}

/**
 *
 */
static int fixup_redis_cmd6(void** param, int param_no)
{
	return fixup_spve_null(param, 1);
}


/**
 *
 */
static int w_redis_free_reply(struct sip_msg* msg, char* res)
{
	str name;

	if(fixup_get_svalue(msg, (gparam_t*)res, &name)!=0)
	{
		LM_ERR("no redis reply name\n");
		return -1;
	}

	if(redisc_free_reply(&name)<0)
		return -1;

	return 1;
}

/**
 *
 */
int redis_srv_param(modparam_t type, void *val)
{
	return redisc_add_server((char*)val);
}

/**
 *
 */
int redis_parse_index(str *in, gparam_t *gp)
{
	if(in->s[0]==PV_MARKER)
	{
		gp->type = GPARAM_TYPE_PVS;
		gp->v.pvs = (pv_spec_t*)pkg_malloc(sizeof(pv_spec_t));
		if (gp->v.pvs == NULL)
		{
			LM_ERR("no pkg memory left for pv_spec_t\n");
			pkg_free(gp);
			return -1;
		}

		if(pv_parse_spec(in, gp->v.pvs)==NULL)
		{
			LM_ERR("invalid PV identifier\n");
			pkg_free(gp->v.pvs);
			pkg_free(gp);
			return -1;
		}
	} else {
		gp->type = GPARAM_TYPE_INT;
		if(str2sint(in, &gp->v.i) != 0)
		{
			LM_ERR("bad number <%.*s>\n", in->len, in->s);
			return -1;
		}
	}
	return 0;
}


/**
 *
 */
int redis_parse_token(str *in, gparam_t *gp, int i)
{
	str tok;

	while(i<in->len && isspace(in->s[i]))
		i++;

	if(i>=in->len-2)
		return -1;

	if(in->s[i++]!='[')
		return -1;

	while(i<in->len-1 && isspace(in->s[i]))
		i++;
	if(i==in->len-1 || in->s[i]==']')
		return -1;
	tok.s = &(in->s[i]);

	while(i<in->len && !isspace(in->s[i]) && in->s[i]!=']')
		i++;
	if(i==in->len)
		return -1;
	tok.len = &(in->s[i]) - tok.s;
	if(redis_parse_index(&tok, gp)!=0)
		return -1;

	while(i<in->len && isspace(in->s[i]))
		i++;
	if(i==in->len || in->s[i]!=']')
		return -1;

	return 0;
}

/**
 *
 */
static int pv_parse_redisc_name(pv_spec_p sp, str *in)
{
	redisc_pv_t *rpv=NULL;
	str pvs;
	int i;

	if(in->s==NULL || in->len<=0)
		return -1;

	rpv = (redisc_pv_t*)pkg_malloc(sizeof(redisc_pv_t));
	if(rpv==NULL)
		return -1;

	memset(rpv, 0, sizeof(redisc_pv_t));

	pvs = *in;
	trim(&pvs);

	rpv->rname.s = pvs.s;
	for(i=0; i<pvs.len-2; i++)
	{
		if(isspace(pvs.s[i]) || pvs.s[i]=='=') {
			rpv->rname.len = i;
			break;
		}
	}
	rpv->rname.len = i;

	if(rpv->rname.len==0)
		goto error_var;

	while(i<pvs.len-2 && isspace(pvs.s[i]))
		i++;

	if(pvs.s[i]!='=')
		goto error_var;

	if(pvs.s[i+1]!='>')
		goto error_var;

	i += 2;
	while(i<pvs.len && isspace(pvs.s[i]))
		i++;

	if(i>=pvs.len)
		goto error_key;

	rpv->rkey.s   = pvs.s + i;
	rpv->rkey.len = pvs.len - i;

	/* Default pos param initialization. */
	rpv->pos.type = GPARAM_TYPE_INT;
	rpv->pos.v.i = -1;

	if(rpv->rkey.len>=5 && strncmp(rpv->rkey.s, "value", 5)==0) {
		rpv->rkeyid = 1;
		if(rpv->rkey.len>5)
		{
			i+=5;
			if(redis_parse_token(&pvs, &(rpv->pos), i)!=0)
				goto error_key;
		}
	} else if(rpv->rkey.len>=4 && strncmp(rpv->rkey.s, "type", 4)==0) {
		rpv->rkeyid = 0;
		if(rpv->rkey.len>4)
		{
			i+=4;
			if(redis_parse_token(&pvs, &(rpv->pos), i)!=0)
				goto error_key;
		}
	} else if(rpv->rkey.len==4 && strncmp(rpv->rkey.s, "info", 4)==0) {
		rpv->rkeyid = 2;
	} else if(rpv->rkey.len==4 && strncmp(rpv->rkey.s, "size", 4)==0) {
		rpv->rkeyid = 3;
	} else {
		goto error_key;
	}

	sp->pvp.pvn.u.dname = (void*)rpv;
	sp->pvp.pvn.type = PV_NAME_OTHER;
	return 0;

error_var:
	LM_ERR("invalid var spec [%.*s]\n", in->len, in->s);
	pkg_free(rpv);
	return -1;

error_key:
	LM_ERR("invalid key spec in [%.*s]\n", in->len, in->s);
	pkg_free(rpv);
	return -1;
}

/**
 *
 */
static int pv_get_redisc(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	redisc_pv_t *rpv;
	str s;
	int pos;

	rpv = (redisc_pv_t*)param->pvn.u.dname;
	if(rpv->reply==NULL)
	{
		rpv->reply = redisc_get_reply(&rpv->rname);
		if(rpv->reply==NULL)
			return pv_get_null(msg, param, res);
	}

	if(rpv->reply->rplRedis==NULL)
		return pv_get_null(msg, param, res);


	if(fixup_get_ivalue(msg, &rpv->pos, &pos)!=0)
		return pv_get_null(msg, param, res);

	switch(rpv->rkeyid) {
		case 1:
			/* value */
			switch(rpv->reply->rplRedis->type) {
				case REDIS_REPLY_STRING:
					if(pos!=-1)
						return pv_get_null(msg, param, res);
					s.len = rpv->reply->rplRedis->len;
					s.s = rpv->reply->rplRedis->str;
					return pv_get_strval(msg, param, res, &s);
				case REDIS_REPLY_INTEGER:
					if(pos!=-1)
						return pv_get_null(msg, param, res);
					return pv_get_sintval(msg, param, res,
										  (int)rpv->reply->rplRedis->integer);
				case REDIS_REPLY_ARRAY:
					if(pos<0 || pos>=(int)rpv->reply->rplRedis->elements)
						return pv_get_null(msg, param, res);
					if(rpv->reply->rplRedis->element[pos]==NULL)
						return pv_get_null(msg, param, res);
					switch(rpv->reply->rplRedis->element[pos]->type) {
						case REDIS_REPLY_STRING:
						s.len = rpv->reply->rplRedis->element[pos]->len;
							s.s = rpv->reply->rplRedis->element[pos]->str;
							return pv_get_strval(msg, param, res, &s);
						case REDIS_REPLY_INTEGER:
							return pv_get_sintval(msg, param, res,
												  (int)rpv->reply->rplRedis->element[pos]->integer);
						default:
							return pv_get_null(msg, param, res);
					}
				default:
					return pv_get_null(msg, param, res);
			}
		case 2:
			/* info */
			if(rpv->reply->rplRedis->str==NULL)
				return pv_get_null(msg, param, res);
			s.len = rpv->reply->rplRedis->len;
			s.s = rpv->reply->rplRedis->str;
			return pv_get_strval(msg, param, res, &s);
		case 3:
			/* size */
			if(rpv->reply->rplRedis->type == REDIS_REPLY_ARRAY) {
				return pv_get_uintval(msg, param, res, (unsigned int)rpv->reply->rplRedis->elements);
			} else {
				return pv_get_null(msg, param, res);
			}
		case 0:
			/* type */
			if(pos==-1)
				return pv_get_sintval(msg, param, res,
									  rpv->reply->rplRedis->type);
			if(rpv->reply->rplRedis->type != REDIS_REPLY_ARRAY)
				return pv_get_null(msg, param, res);
			if(pos<0 || pos>=(int)rpv->reply->rplRedis->elements)
				return pv_get_null(msg, param, res);
			if(rpv->reply->rplRedis->element[pos]==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_sintval(msg, param, res, rpv->reply->rplRedis->element[pos]->type);
		default:
			/* We do nothing. */
			return pv_get_null(msg, param, res);
	}
}
