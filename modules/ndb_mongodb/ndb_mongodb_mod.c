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
#include <ctype.h>

#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../mod_fix.h"
#include "../../trim.h"

#include "mongodb_client.h"
#include "api.h"

MODULE_VERSION

/** parameters */

int mongodb_srv_param(modparam_t type, void *val);
static int w_mongodb_find(sip_msg_t* msg, char* ssrv, char *sdname, char *scname,
		char* scmd, char* sres);
static int w_mongodb_find_one(sip_msg_t* msg, char* ssrv, char *sdname, char *scname,
		char* scmd, char* sres);
static int w_mongodb_cmd_simple(sip_msg_t* msg, char* ssrv, char *sdname, char *scname,
		char* scmd, char* sres);
static int w_mongodb_cmd(sip_msg_t* msg, char* ssrv, char *sdname, char *scname,
		char* scmd, char* sres);
static int fixup_mongodb_cmd(void** param, int param_no);
static int w_mongodb_free_reply(struct sip_msg* msg, char* res);
static int w_mongodb_next_reply(struct sip_msg* msg, char* res);

static void mod_destroy(void);
static int  child_init(int rank);

static int pv_get_mongodb(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res);
static int pv_parse_mongodb_name(pv_spec_p sp, str *in);

static pv_export_t mod_pvs[] = {
	{ {"mongodb", sizeof("mongodb")-1}, PVT_OTHER, pv_get_mongodb, 0,
		pv_parse_mongodb_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


static cmd_export_t cmds[]={
	{"mongodb_find", (cmd_function)w_mongodb_find, 5, fixup_mongodb_cmd,
		0, ANY_ROUTE},
	{"mongodb_find_one", (cmd_function)w_mongodb_find_one, 5, fixup_mongodb_cmd,
		0, ANY_ROUTE},
	{"mongodb_cmd_simple", (cmd_function)w_mongodb_cmd_simple, 5, fixup_mongodb_cmd,
		0, ANY_ROUTE},
	{"mongodb_cmd", (cmd_function)w_mongodb_cmd, 5, fixup_mongodb_cmd,
		0, ANY_ROUTE},
	{"mongodb_free", (cmd_function)w_mongodb_free_reply, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"mongodb_next", (cmd_function)w_mongodb_next_reply, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"bind_ndb_mongodb",   (cmd_function)bind_ndb_mongodb,  0,
		0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"server",         PARAM_STRING|USE_FUNC_PARAM, (void*)mongodb_srv_param},
	{0, 0, 0}
};

struct module_exports exports = {
	"ndb_mongodb",
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

	if(mongodbc_init()<0)
	{
		LM_ERR("failed to initialize mongodb connections\n");
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
	mongodbc_destroy();
}

/**
 *
 */
static int w_mongodb_do_cmd(sip_msg_t* msg, char* ssrv, char *sdname, char *scname,
		char* scmd, char* sres, int ctype)
{
	int ret;
	str s[5];

	if(fixup_get_svalue(msg, (gparam_t*)ssrv, &s[0])!=0)
	{
		LM_ERR("no mongodb server name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sdname, &s[1])!=0)
	{
		LM_ERR("no mongodb database name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)scname, &s[2])!=0)
	{
		LM_ERR("no mongodb collection name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)scmd, &s[3])!=0)
	{
		LM_ERR("no mongodb command\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)sres, &s[4])!=0)
	{
		LM_ERR("no mongodb reply name\n");
		return -1;
	}
	ret = -1;
	if(ctype==0) {
		ret = mongodbc_exec_simple(&s[0], &s[1], &s[2], &s[3], &s[4]);
	} else if(ctype==1) {
		ret = mongodbc_exec(&s[0], &s[1], &s[2], &s[3], &s[4]);
	} else if(ctype==2) {
		ret = mongodbc_find(&s[0], &s[1], &s[2], &s[3], &s[4]);
	} else if(ctype==3) {
		ret = mongodbc_find_one(&s[0], &s[1], &s[2], &s[3], &s[4]);
	}
	if(ret<0)
		return -1;
	return 1;
}

/**
 *
 */
static int w_mongodb_cmd_simple(sip_msg_t* msg, char* ssrv, char *sdname, char *scname,
		char* scmd, char* sres)
{
	return w_mongodb_do_cmd(msg, ssrv, sdname, scname, scmd, sres, 0);
}

/**
 *
 */
static int w_mongodb_cmd(sip_msg_t* msg, char* ssrv, char *sdname, char *scname,
		char* scmd, char* sres)
{
	return w_mongodb_do_cmd(msg, ssrv, sdname, scname, scmd, sres, 1);
}

/**
 *
 */
static int w_mongodb_find(sip_msg_t* msg, char* ssrv, char *sdname, char *scname,
		char* scmd, char* sres)
{
	return w_mongodb_do_cmd(msg, ssrv, sdname, scname, scmd, sres, 2);
}

/**
 *
 */
static int w_mongodb_find_one(sip_msg_t* msg, char* ssrv, char *sdname, char *scname,
		char* scmd, char* sres)
{
	return w_mongodb_do_cmd(msg, ssrv, sdname, scname, scmd, sres, 3);
}

/**
 *
 */
static int fixup_mongodb_cmd(void** param, int param_no)
{
	return fixup_spve_null(param, 1);
}

/**
 *
 */
static int w_mongodb_free_reply(struct sip_msg* msg, char* res)
{
	str name;

	if(fixup_get_svalue(msg, (gparam_t*)res, &name)!=0)
	{
		LM_ERR("no mongodb reply name\n");
		return -1;
	}

	if(mongodbc_free_reply(&name)<0)
		return -1;
	return 1;
}

/**
 *
 */
static int w_mongodb_next_reply(struct sip_msg* msg, char* res)
{
	str name;

	if(fixup_get_svalue(msg, (gparam_t*)res, &name)!=0)
	{
		LM_ERR("no mongodb reply name\n");
		return -1;
	}

	if(mongodbc_next_reply(&name)<0)
		return -1;
	return 1;;
}

/**
 *
 */
int mongodb_srv_param(modparam_t type, void *val)
{
	return mongodbc_add_server((char*)val);
}

/**
 *
 */
static int pv_parse_mongodb_name(pv_spec_p sp, str *in)
{
	mongodbc_pv_t *rpv=NULL;
	str pvs;
	int i;

	if(in->s==NULL || in->len<=0)
		return -1;

	rpv = (mongodbc_pv_t*)pkg_malloc(sizeof(mongodbc_pv_t));
	if(rpv==NULL)
		return -1;

	memset(rpv, 0, sizeof(mongodbc_pv_t));

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

	if(rpv->rkey.len>=5 && strncmp(rpv->rkey.s, "value", 5)==0) {
		rpv->rkeyid = 1;
	} else if(rpv->rkey.len>=4 && strncmp(rpv->rkey.s, "type", 4)==0) {
		rpv->rkeyid = 0;
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
static int pv_get_mongodb(struct sip_msg *msg,  pv_param_t *param,
		pv_value_t *res)
{
	mongodbc_pv_t *rpv;

	rpv = (mongodbc_pv_t*)param->pvn.u.dname;
	if(rpv->reply==NULL)
	{
		rpv->reply = mongodbc_get_reply(&rpv->rname);
		if(rpv->reply==NULL)
			return pv_get_null(msg, param, res);
	}


	switch(rpv->rkeyid) {
		case 1:
			/* value */
			if(rpv->reply->jsonrpl.s==NULL)
				return pv_get_null(msg, param, res);
			return pv_get_strval(msg, param, res, &rpv->reply->jsonrpl);
		case 2:
			/* info */
			return pv_get_null(msg, param, res);
		case 3:
			/* size */
			return pv_get_null(msg, param, res);
		case 0:
			/* type */
			return pv_get_sintval(msg, param, res, 0);
		default:
			/* We do nothing. */
			return pv_get_null(msg, param, res);
	}
}
