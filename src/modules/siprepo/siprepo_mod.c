/**
 * Copyright (C) 2022 Daniel-Constantin Mierla (asipto.com)
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
#include "../../core/ut.h"
#include "../../core/fmsg.h"
#include "../../core/receive.h"
#include "../../core/mod_fix.h"
#include "../../core/async_task.h"
#include "../../core/kemi.h"

#include "siprepo_data.h"

MODULE_VERSION

int _siprepo_table_size = 256;
int _siprepo_expire = 180;
int _siprepo_timer_interval = 10;
int _siprepo_timer_procs = 1;

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int w_sr_msg_push(sip_msg_t *msg, char *pmsgid, char *p2);
static int w_sr_msg_pull(sip_msg_t *msg, char *pcallid, char *pmsgid, char *prname);
static int w_sr_msg_async_pull(sip_msg_t *msg, char *pcallid, char *pmsgid,
		char *pgname, char *prname);
static int w_sr_msg_rm(sip_msg_t *msg, char *pcallid, char *pmsgid);
static int w_sr_msg_check(sip_msg_t *msg, char *p1, char *p2);

/* clang-format off */
typedef struct sworker_task_param {
	char *buf;
	int len;
	receive_info_t rcv;
	str xdata;
} sworker_task_param_t;

static cmd_export_t cmds[]={
	{"sr_msg_push", (cmd_function)w_sr_msg_push, 1, fixup_spve_null,
		fixup_free_spve_null, REQUEST_ROUTE|CORE_ONREPLY_ROUTE},
	{"sr_msg_pull", (cmd_function)w_sr_msg_pull, 2, fixup_spve_spve,
		fixup_free_spve_spve, REQUEST_ROUTE|CORE_ONREPLY_ROUTE},
	{"sr_msg_async_pull", (cmd_function)w_sr_msg_async_pull, 4, fixup_spve_all,
		fixup_free_spve_all, ANY_ROUTE},
	{"sr_msg_rm", (cmd_function)w_sr_msg_rm, 2, fixup_spve_spve,
		fixup_free_spve_spve, REQUEST_ROUTE|CORE_ONREPLY_ROUTE},
	{"sr_msg_check", (cmd_function)w_sr_msg_check, 0, 0,
		0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"hash_size",      PARAM_INT,   &_siprepo_table_size},
	{"expire",         PARAM_INT,   &_siprepo_expire},
	{"timer_interval", PARAM_INT,   &_siprepo_timer_interval},
	{"timer_procs",    PARAM_INT,   &_siprepo_timer_procs},
	{0, 0, 0}
};

struct module_exports exports = {
	"siprepo",       /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* exported functions */
	params,          /* exported parameters */
	0,               /* exported RPC methods */
	0,               /* exported pseudo-variables */
	0,               /* response function */
	mod_init,        /* module initialization function */
	child_init,      /* per child init function */
	mod_destroy      /* destroy function */
};
/* clang-format on */


/**
 * init module function
 */
static int mod_init(void)
{
	if(siprepo_table_init()<0) {
		LM_ERR("failed to initialize hash table\n");
		return -1;
	}
	return 0;
}

/**
 * @brief Initialize async module children
 */
static int child_init(int rank)
{
	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
}

/**
 *
 */
static int ki_sr_msg_push(sip_msg_t *msg, str *msgid)
{
	int ret;

	ret = siprepo_msg_set(msg, msgid);

	if(ret<0) {
		return ret;
	}
	return 1;
}

/**
 *
 */
static int w_sr_msg_push(sip_msg_t *msg, char *pmsgid, char *p2)
{
	str msgid = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t *)pmsgid, &msgid) != 0) {
		LM_ERR("cannot get msgid value\n");
		return -1;
	}

	return ki_sr_msg_push(msg, &msgid);
}

/**
 *
 */
static int ki_sr_msg_pull(sip_msg_t *msg, str *callid, str *msgid, str *rname)
{
	int ret;

	ret = siprepo_msg_pull(msg, callid, msgid, rname);

	if(ret<0) {
		return ret;
	}
	return 1;
}

/**
 *
 */
static int w_sr_msg_pull(sip_msg_t *msg, char *pcallid, char *pmsgid, char *prname)
{
	str callid = STR_NULL;
	str msgid = STR_NULL;
	str rname = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t *)pcallid, &callid) != 0) {
		LM_ERR("cannot get callid value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pmsgid, &msgid) != 0) {
		LM_ERR("cannot get msgid value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)prname, &rname) != 0) {
		LM_ERR("cannot get route name\n");
		return -1;
	}

	return ki_sr_msg_pull(msg, &callid, &msgid, &rname);
}

/**
 *
 */
static int ki_sr_msg_async_pull(sip_msg_t *msg, str *callid, str *msgid,
		str *gname, str *rname)
{
	return 1;
}

/**
 *
 */
static int w_sr_msg_async_pull(sip_msg_t *msg, char *pcallid, char *pmsgid,
		char *pgname, char *prname)
{
	str callid = STR_NULL;
	str msgid = STR_NULL;
	str gname = STR_NULL;
	str rname = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t *)pcallid, &callid) != 0) {
		LM_ERR("cannot get callid value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pmsgid, &msgid) != 0) {
		LM_ERR("cannot get msgid value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pgname, &gname) != 0) {
		LM_ERR("cannot get aync group name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)prname, &rname) != 0) {
		LM_ERR("cannot get route name\n");
		return -1;
	}

	return ki_sr_msg_async_pull(msg, &callid, &msgid, &gname, &rname);
}


/**
 *
 */
static int ki_sr_msg_rm(sip_msg_t *msg, str *callid, str *msgid)
{
	int ret;

	ret = siprepo_msg_rm(msg, callid, msgid);

	if(ret<0) {
		return ret;
	}
	return 1;
}

/**
 *
 */
static int w_sr_msg_rm(sip_msg_t *msg, char *pcallid, char *pmsgid)
{
	str callid = STR_NULL;
	str msgid = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t *)pcallid, &callid) != 0) {
		LM_ERR("cannot get callid value\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pmsgid, &msgid) != 0) {
		LM_ERR("cannot get msgid value\n");
		return -1;
	}

	return ki_sr_msg_rm(msg, &callid, &msgid);
}

/**
 *
 */
static int ki_sr_msg_check(sip_msg_t *msg)
{
	int ret;

	ret = siprepo_msg_check(msg);

	if(ret<=0) {
		return (ret-1);
	}
	return ret;
}

/**
 *
 */
static int w_sr_msg_check(sip_msg_t *msg, char *p1, char *p2)
{
	return ki_sr_msg_check(msg);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_sworker_exports[] = {
	{ str_init("siprepo"), str_init("sr_msg_push"),
		SR_KEMIP_INT, ki_sr_msg_push,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siprepo"), str_init("sr_msg_pull"),
		SR_KEMIP_INT, ki_sr_msg_pull,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siprepo"), str_init("sr_msg_async_pull"),
		SR_KEMIP_INT, ki_sr_msg_async_pull,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siprepo"), str_init("sr_msg_rm"),
		SR_KEMIP_INT, ki_sr_msg_rm,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("siprepo"), str_init("sr_msg_check"),
		SR_KEMIP_INT, ki_sr_msg_check,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_sworker_exports);
	return 0;
}
