/**
 * Copyright (C) 2017 Daniel-Constantin Mierla (asipto.com)
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
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../core/sr_module.h"
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/kemi.h"

#include "app_jsdt_api.h"
//#include "app_jsdt_kemi_export.h"

MODULE_VERSION

static int mod_init(void);
static int child_init(int rank);
static void mod_destroy(void);

static int w_app_jsdt_dostring(sip_msg_t *msg, char *script, char *extra);
static int w_app_jsdt_dofile(sip_msg_t *msg, char *script, char *extra);
static int w_app_jsdt_runstring(sip_msg_t *msg, char *script, char *extra);
static int w_app_jsdt_run(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3);
static int w_app_jsdt_run0(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3);
static int w_app_jsdt_run1(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3);
static int w_app_jsdt_run2(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3);
static int w_app_jsdt_run3(sip_msg_t *msg, char *func, char *p1, char *p2,
		char *p3);

static int fixup_jsdt_run(void** param, int param_no);

extern str _sr_jsdt_load_file;

/* clang-format off */
static cmd_export_t cmds[]={
	{"jsdt_dostring", (cmd_function)w_app_jsdt_dostring, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"jsdt_dofile", (cmd_function)w_app_jsdt_dofile, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"jsdt_runstring", (cmd_function)w_app_jsdt_runstring, 1, fixup_spve_null,
		0, ANY_ROUTE},
	{"jsdt_run", (cmd_function)w_app_jsdt_run0, 1, fixup_jsdt_run,
		0, ANY_ROUTE},
	{"jsdt_run", (cmd_function)w_app_jsdt_run1, 2, fixup_jsdt_run,
		0, ANY_ROUTE},
	{"jsdt_run", (cmd_function)w_app_jsdt_run2, 3, fixup_jsdt_run,
		0, ANY_ROUTE},
	{"jsdt_run", (cmd_function)w_app_jsdt_run3, 4, fixup_jsdt_run,
		0, ANY_ROUTE},

	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"load", PARAM_STR, &_sr_jsdt_load_file},
	{0, 0, 0}
};

struct module_exports exports = {
	"app_jsdt",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,
	0,              /* exported MI functions */
	0,              /* exported pseudo-variables */
	0,              /* extra processes */
	mod_init,       /* module initialization function */
	0,              /* response function */
	mod_destroy,    /* destroy function */
	child_init      /* per child init function */
};
/* clang-format on */

/**
 * init module function
 */
static int mod_init(void)
{
	if(jsdt_sr_init_mod()<0)
		return -1;

	if(app_jsdt_init_rpc()<0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	return 0;
}

/**
 * init module children
 */
static int child_init(int rank)
{
	return jsdt_sr_init_child();
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	jsdt_sr_destroy();
}

/**
 *
 */
int sr_kemi_config_engine_jsdt(sip_msg_t *msg, int rtype, str *rname,
		str *rparam)
{
	int ret;

	ret = -1;
	if(rtype==REQUEST_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = app_jsdt_run_ex(msg, rname->s,
					(rparam && rparam->s)?rparam->s:NULL, NULL, NULL, 0);
		} else {
			ret = app_jsdt_run_ex(msg, "ksr_request_route", NULL, NULL, NULL, 1);
		}
	} else if(rtype==CORE_ONREPLY_ROUTE) {
		ret = app_jsdt_run_ex(msg, "ksr_reply_route", NULL, NULL, NULL, 0);
	} else if(rtype==BRANCH_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = app_jsdt_run_ex(msg, rname->s, NULL, NULL, NULL, 0);
		}
	} else if(rtype==FAILURE_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = app_jsdt_run_ex(msg, rname->s, NULL, NULL, NULL, 0);
		}
	} else if(rtype==BRANCH_FAILURE_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = app_jsdt_run_ex(msg, rname->s, NULL, NULL, NULL, 0);
		}
	} else if(rtype==TM_ONREPLY_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = app_jsdt_run_ex(msg, rname->s, NULL, NULL, NULL, 0);
		}
	} else if(rtype==ONSEND_ROUTE) {
		ret = app_jsdt_run_ex(msg, "ksr_onsend_route", NULL, NULL, NULL, 0);
	} else if(rtype==EVENT_ROUTE) {
		if(rname!=NULL && rname->s!=NULL) {
			ret = app_jsdt_run_ex(msg, rname->s,
					(rparam && rparam->s)?rparam->s:NULL, NULL, NULL, 0);
		}
	} else {
		if(rname!=NULL) {
			LM_ERR("route type %d with name [%.*s] not implemented\n",
				rtype, rname->len, rname->s);
		} else {
			LM_ERR("route type %d with no name not implemented\n",
				rtype);
		}
	}

	if(rname!=NULL) {
		LM_DBG("execution of route type %d with name [%.*s] returned %d\n",
				rtype, rname->len, rname->s, ret);
	} else {
		LM_DBG("execution of route type %d with no name returned %d\n",
			rtype, ret);
	}

	return 1;
}

#define JSDT_BUF_STACK_SIZE	1024
static char _jsdt_buf_stack[4][JSDT_BUF_STACK_SIZE];

/**
 *
 */
static int ki_app_jsdt_dostring(sip_msg_t *msg, str *script)
{
	if(script==NULL || script->s==NULL || script->len>=JSDT_BUF_STACK_SIZE-1) {
		LM_ERR("script too short or too long %d\n", (script)?script->len:0);
		return -1;
	}
	if(!jsdt_sr_initialized())
	{
		LM_ERR("jsdt env not intitialized");
		return -1;
	}
	memcpy(_jsdt_buf_stack[0], script->s, script->len);
	_jsdt_buf_stack[0][script->len] = '\0';
	return app_jsdt_dostring(msg, _jsdt_buf_stack[0]);
}

/**
 *
 */
static int w_app_jsdt_dostring(struct sip_msg *msg, char *script, char *extra)
{
	str s;
	if(fixup_get_svalue(msg, (gparam_p)script, &s)<0)
	{
		LM_ERR("cannot get the script\n");
		return -1;
	}
	return ki_app_jsdt_dostring(msg, &s);
}

/**
 *
 */
static int ki_app_jsdt_dofile(sip_msg_t *msg, str *script)
{
	if(script==NULL || script->s==NULL || script->len>=JSDT_BUF_STACK_SIZE-1) {
		LM_ERR("script too short or too long %d\n", (script)?script->len:0);
		return -1;
	}
	if(!jsdt_sr_initialized())
	{
		LM_ERR("jsdt env not intitialized");
		return -1;
	}
	memcpy(_jsdt_buf_stack[0], script->s, script->len);
	_jsdt_buf_stack[0][script->len] = '\0';
	return app_jsdt_dofile(msg, _jsdt_buf_stack[0]);
}

/**
 *
 */
static int w_app_jsdt_dofile(struct sip_msg *msg, char *script, char *extra)
{
	str s;
	if(fixup_get_svalue(msg, (gparam_p)script, &s)<0)
	{
		LM_ERR("cannot get the script\n");
		return -1;
	}
	return ki_app_jsdt_dofile(msg, &s);
}

/**
 *
 */
static int ki_app_jsdt_runstring(sip_msg_t *msg, str *script)
{
	if(script==NULL || script->s==NULL || script->len>=JSDT_BUF_STACK_SIZE-1) {
		LM_ERR("script too short or too long %d\n", (script)?script->len:0);
		return -1;
	}
	if(!jsdt_sr_initialized())
	{
		LM_ERR("jsdt env not intitialized");
		return -1;
	}
	memcpy(_jsdt_buf_stack[0], script->s, script->len);
	_jsdt_buf_stack[0][script->len] = '\0';
	return app_jsdt_runstring(msg, _jsdt_buf_stack[0]);
}

/**
 *
 */
static int w_app_jsdt_runstring(struct sip_msg *msg, char *script, char *extra)
{
	str s;
	if(fixup_get_svalue(msg, (gparam_p)script, &s)<0)
	{
		LM_ERR("cannot get the script\n");
		return -1;
	}
	return ki_app_jsdt_runstring(msg, &s);
}

/**
 *
 */
static int w_app_jsdt_run(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	str s;
	if(!jsdt_sr_initialized())
	{
		LM_ERR("jsdt env not intitialized");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_p)func, &s)<0)
	{
		LM_ERR("cannot get the function\n");
		return -1;
	}
	if(s.len>=JSDT_BUF_STACK_SIZE-1)
	{
		LM_ERR("function too long %d\n", s.len);
		return -1;
	}
	memcpy(_jsdt_buf_stack[0], s.s, s.len);
	_jsdt_buf_stack[0][s.len] = '\0';

	if(p1!=NULL)
	{
		if(fixup_get_svalue(msg, (gparam_p)p1, &s)<0)
		{
			LM_ERR("cannot get p1\n");
			return -1;
		}
		if(s.len>=JSDT_BUF_STACK_SIZE-1)
		{
			LM_ERR("p1 too long %d\n", s.len);
			return -1;
		}
		memcpy(_jsdt_buf_stack[1], s.s, s.len);
		_jsdt_buf_stack[1][s.len] = '\0';

		if(p2!=NULL)
		{
			if(fixup_get_svalue(msg, (gparam_p)p2, &s)<0)
			{
				LM_ERR("cannot get p2\n");
				return -1;
			}
			if(s.len>=JSDT_BUF_STACK_SIZE-1)
			{
				LM_ERR("p2 too long %d\n", s.len);
				return -1;
			}
			memcpy(_jsdt_buf_stack[2], s.s, s.len);
			_jsdt_buf_stack[2][s.len] = '\0';

			if(p3!=NULL)
			{
				if(fixup_get_svalue(msg, (gparam_p)p3, &s)<0)
				{
					LM_ERR("cannot get p3\n");
					return -1;
				}
				if(s.len>=JSDT_BUF_STACK_SIZE-1)
				{
					LM_ERR("p3 too long %d\n", s.len);
					return -1;
				}
				memcpy(_jsdt_buf_stack[3], s.s, s.len);
				_jsdt_buf_stack[3][s.len] = '\0';
			}
		} else {
			p3 = NULL;
		}
	} else {
		p2 = NULL;
		p3 = NULL;
	}

	return app_jsdt_run(msg, _jsdt_buf_stack[0],
			(p1!=NULL)?_jsdt_buf_stack[1]:NULL,
			(p2!=NULL)?_jsdt_buf_stack[2]:NULL,
			(p3!=NULL)?_jsdt_buf_stack[3]:NULL);
}

static int w_app_jsdt_run0(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return w_app_jsdt_run(msg, func, NULL, NULL, NULL);
}

static int w_app_jsdt_run1(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return w_app_jsdt_run(msg, func, p1, NULL, NULL);
}

static int w_app_jsdt_run2(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return w_app_jsdt_run(msg, func, p1, p2, NULL);
}

static int w_app_jsdt_run3(struct sip_msg *msg, char *func, char *p1, char *p2,
		char *p3)
{
	return w_app_jsdt_run(msg, func, p1, p2, p3);
}

static int fixup_jsdt_run(void** param, int param_no)
{
	return fixup_spve_null(param, 1);
}

/**
 *
 */
static int ki_app_jsdt_run(sip_msg_t *msg, str *func)
{
	if(func==NULL || func->s==NULL || func->len<0) {
		LM_ERR("invalid function name\n");
		return -1;
	}
	if(func->s[func->len]!='\0') {
		LM_ERR("invalid terminated function name\n");
		return -1;
	}
	return app_jsdt_run(msg, func->s, NULL, NULL, NULL);

}

/**
 *
 */
static int ki_app_jsdt_run_p1(sip_msg_t *msg, str *func, str *p1)
{
	if(func==NULL || func->s==NULL || func->len<=0) {
		LM_ERR("invalid function name\n");
		return -1;
	}
	if(func->s[func->len]!='\0') {
		LM_ERR("invalid terminated function name\n");
		return -1;
	}
	if(p1==NULL || p1->s==NULL || p1->len<0) {
		LM_ERR("invalid p1 value\n");
		return -1;
	}
	if(p1->s[p1->len]!='\0') {
		LM_ERR("invalid terminated p1 value\n");
		return -1;
	}
	return app_jsdt_run(msg, func->s, p1->s, NULL, NULL);
}

/**
 *
 */
static int ki_app_jsdt_run_p2(sip_msg_t *msg, str *func, str *p1, str *p2)
{
	if(func==NULL || func->s==NULL || func->len<=0) {
		LM_ERR("invalid function name\n");
		return -1;
	}
	if(func->s[func->len]!='\0') {
		LM_ERR("invalid terminated function name\n");
		return -1;
	}
	if(p1==NULL || p1->s==NULL || p1->len<0) {
		LM_ERR("invalid p1 value\n");
		return -1;
	}
	if(p1->s[p1->len]!='\0') {
		LM_ERR("invalid terminated p1 value\n");
		return -1;
	}
	if(p2==NULL || p2->s==NULL || p2->len<0) {
		LM_ERR("invalid p2 value\n");
		return -1;
	}
	if(p2->s[p2->len]!='\0') {
		LM_ERR("invalid terminated p2 value\n");
		return -1;
	}
	return app_jsdt_run(msg, func->s, p1->s, p2->s, NULL);
}

/**
 *
 */
static int ki_app_jsdt_run_p3(sip_msg_t *msg, str *func, str *p1, str *p2, str *p3)
{
	if(func==NULL || func->s==NULL || func->len<=0) {
		LM_ERR("invalid function name\n");
		return -1;
	}
	if(func->s[func->len]!='\0') {
		LM_ERR("invalid terminated function name\n");
		return -1;
	}
	if(p1==NULL || p1->s==NULL || p1->len<0) {
		LM_ERR("invalid p1 value\n");
		return -1;
	}
	if(p1->s[p1->len]!='\0') {
		LM_ERR("invalid terminated p1 value\n");
		return -1;
	}
	if(p2==NULL || p2->s==NULL || p2->len<0) {
		LM_ERR("invalid p2 value\n");
		return -1;
	}
	if(p2->s[p2->len]!='\0') {
		LM_ERR("invalid terminated p2 value\n");
		return -1;
	}
	if(p3==NULL || p3->s==NULL || p3->len<0) {
		LM_ERR("invalid p3 value\n");
		return -1;
	}
	if(p3->s[p3->len]!='\0') {
		LM_ERR("invalid terminated p3 value\n");
		return -1;
	}
	return app_jsdt_run(msg, func->s, p1->s, p2->s, p3->s);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_app_jsdt_exports[] = {
	{ str_init("app_jsdt"), str_init("dostring"),
		SR_KEMIP_INT, ki_app_jsdt_dostring,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_jsdt"), str_init("dofile"),
		SR_KEMIP_INT, ki_app_jsdt_dofile,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_jsdt"), str_init("runstring"),
		SR_KEMIP_INT, ki_app_jsdt_runstring,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_jsdt"), str_init("run"),
		SR_KEMIP_INT, ki_app_jsdt_run,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_jsdt"), str_init("run_p1"),
		SR_KEMIP_INT, ki_app_jsdt_run_p1,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_jsdt"), str_init("run_p2"),
		SR_KEMIP_INT, ki_app_jsdt_run_p2,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("app_jsdt"), str_init("run_p3"),
		SR_KEMIP_INT, ki_app_jsdt_run_p3,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */


/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	str ename = str_init("jsdt");

	*dlflags = RTLD_NOW | RTLD_GLOBAL;

	sr_kemi_eng_register(&ename, sr_kemi_config_engine_jsdt);
	sr_kemi_modules_add(sr_kemi_app_jsdt_exports);

	return 0;
}
