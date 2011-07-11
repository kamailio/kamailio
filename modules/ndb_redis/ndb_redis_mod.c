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

#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../mod_fix.h"
#include "../../trim.h"

#include "redis_client.h"

MODULE_VERSION

/** parameters */

int redis_srv_param(modparam_t type, void *val);
static int w_redis_cmd3(struct sip_msg* msg, char* ssrv, char* scmd,
		char* sres);
static int w_redis_cmd4(struct sip_msg* msg, char* ssrv, char* scmd,
		char *sargv1, char* sres);
static int w_redis_cmd5(struct sip_msg* msg, char* ssrv, char* scmd,
		char *sargv1, char *sargv2, char* sres);
static int w_redis_cmd6(struct sip_msg* msg, char* ssrv, char* scmd,
		char *sargv1, char *sargv2, char *sargv3, char* sres);
static int fixup_redis_cmd6(void** param, int param_no);

static int  mod_init(void);
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
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"server",         STR_PARAM|USE_FUNC_PARAM, (void*)redis_srv_param},
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
	mod_init,       /* module initialization function */
	0,              /* response function */
	mod_destroy,    /* destroy function */
	child_init      /* per child init function */
};



/**
 * init module function
 */
static int mod_init(void)
{
	/* success code */
	return 0;
}

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

	if(redisc_exec(&s[0], &s[1], NULL, NULL, NULL, &s[2])<0)
		return -1;
	return 1;
}

/**
 *
 */
static int w_redis_cmd4(struct sip_msg* msg, char* ssrv, char* scmd,
		char *sargv1, char* sres)
{
	str s[4];

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
	if(fixup_get_svalue(msg, (gparam_t*)sargv1, &s[2])!=0)
	{
		LM_ERR("no argument 1\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sres, &s[3])!=0)
	{
		LM_ERR("no redis reply name\n");
		return -1;
	}

	if(redisc_exec(&s[0], &s[1], &s[2], NULL, NULL, &s[3])<0)
		return -1;
	return 1;
}

/**
 *
 */
static int w_redis_cmd5(struct sip_msg* msg, char* ssrv, char* scmd,
		char *sargv1, char *sargv2, char* sres)
{
	str s[5];

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
	if(fixup_get_svalue(msg, (gparam_t*)sargv1, &s[2])!=0)
	{
		LM_ERR("no argument 1\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sargv2, &s[3])!=0)
	{
		LM_ERR("no argument 2\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sres, &s[4])!=0)
	{
		LM_ERR("no redis reply name\n");
		return -1;
	}

	if(redisc_exec(&s[0], &s[1], &s[2], &s[3], NULL, &s[4])<0)
		return -1;
	return 1;
}

/**
 *
 */
static int w_redis_cmd6(struct sip_msg* msg, char* ssrv, char* scmd,
		char *sargv1, char *sargv2, char *sargv3, char* sres)
{
	str s[6];

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
	if(fixup_get_svalue(msg, (gparam_t*)sargv1, &s[2])!=0)
	{
		LM_ERR("no argument 1\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sargv2, &s[3])!=0)
	{
		LM_ERR("no argument 2\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sargv3, &s[4])!=0)
	{
		LM_ERR("no argument 3\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sres, &s[5])!=0)
	{
		LM_ERR("no redis reply name\n");
		return -1;
	}

	if(redisc_exec(&s[0], &s[1], &s[2], &s[3], &s[4], &s[5])<0)
		return -1;
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
int redis_srv_param(modparam_t type, void *val)
{
	return redisc_add_server((char*)val);
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
		if(pvs.s[i]=='=')
		{
			if(pvs.s[i+1]!='>')
			{
				LM_ERR("invalid var spec [%.*s]\n",
						in->len, in->s);
				pkg_free(rpv);
				return -1;
			}
			rpv->rname.len = i;
			break;
		}
	}

	if(rpv->rname.len==0)
	{
		LM_ERR("invalid var spec [%.*s]\n", in->len, in->s);
		pkg_free(rpv);
		return -1;
	}
	i += 2;
	rpv->rkey.s   = pvs.s + i;
	rpv->rkey.len = pvs.len - i;

	if(rpv->rkey.len==5 && strncmp(rpv->rkey.s, "value", 5)==0) {
		rpv->rkeyid = 1;
	} else if(rpv->rkey.len==4 && strncmp(rpv->rkey.s, "type", 4)==0) {
		rpv->rkeyid = 0;
	} else if(rpv->rkey.len==4 && strncmp(rpv->rkey.s, "info", 4)==0) {
		rpv->rkeyid = 2;
	} else {
		LM_ERR("invalid key spec in [%.*s]\n", in->len, in->s);
		pkg_free(rpv);
		return -1;
	}

	sp->pvp.pvn.u.dname = (void*)rpv;
	sp->pvp.pvn.type = PV_NAME_OTHER;
	return 0;

}

/**
 *
 */
static int pv_get_redisc(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	redisc_pv_t *rpv;
	str s;

	rpv = (redisc_pv_t*)param->pvn.u.dname;
	if(rpv->reply==NULL)
	{
		rpv->reply = redisc_get_reply(&rpv->rname);
		if(rpv->reply==NULL)
			return pv_get_null(msg, param, res);
	}

	if(rpv->reply->rplRedis==NULL)
		return pv_get_null(msg, param, res);

	switch(rpv->rkeyid) {
		case 1:
			switch(rpv->reply->rplRedis->type) {
				case REDIS_REPLY_STRING:
					s.len = rpv->reply->rplRedis->len;
					s.s = rpv->reply->rplRedis->str;
					return pv_get_strval(msg, param, res, &s);
				case REDIS_REPLY_INTEGER:
					return pv_get_sintval(msg, param, res,
							(int)rpv->reply->rplRedis->integer);
				default:
					return pv_get_null(msg, param, res);
			}
		case 2:
			if(rpv->reply->rplRedis->str==NULL)
				return pv_get_null(msg, param, res);
			s.len = rpv->reply->rplRedis->len;
			s.s = rpv->reply->rplRedis->str;
			return pv_get_strval(msg, param, res, &s);
		default:
			return pv_get_sintval(msg, param, res,
					rpv->reply->rplRedis->type);
	}
}
