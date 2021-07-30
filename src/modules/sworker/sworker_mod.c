/**
 * Copyright (C) 2020 Daniel-Constantin Mierla (asipto.com)
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

MODULE_VERSION

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int w_sworker_task(sip_msg_t *msg, char *pgname, char *p2);
static int w_sworker_active(sip_msg_t *msg, char *p1, char *p2);

static int _sworker_active = 0;

/* clang-format off */
typedef struct sworker_task_param {
	char *buf;
	int len;
	receive_info_t rcv;
} sworker_task_param_t;

static cmd_export_t cmds[]={
	{"sworker_task", (cmd_function)w_sworker_task, 1, fixup_spve_null,
		fixup_free_spve_null, REQUEST_ROUTE|CORE_ONREPLY_ROUTE},
	{"sworker_active", (cmd_function)w_sworker_active, 0, 0,
		0, REQUEST_ROUTE|CORE_ONREPLY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

struct module_exports exports = {
	"sworker",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	0,
	0,              /* exported RPC methods */
	0,              /* exported pseudo-variables */
	0,              /* response function */
	mod_init,       /* module initialization function */
	child_init,     /* per child init function */
	mod_destroy    	/* destroy function */
};
/* clang-format on */


/**
 * init module function
 */
static int mod_init(void)
{
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
/**
 *
 */
void sworker_exec_task(void *param)
{
	sworker_task_param_t *stp;
	static char buf[BUF_SIZE+1];
	receive_info_t rcvi;
	int len;

	stp = (sworker_task_param_t *)param;

	LM_DBG("received task [%p] - msg len [%d]\n", stp, stp->len);
	if(stp->len > BUF_SIZE) {
		LM_ERR("message is too large [%d]\n", stp->len);
		return;
	}

	memcpy(buf, stp->buf, stp->len);
	len = stp->len;
	memcpy(&rcvi, &stp->rcv, sizeof(receive_info_t));
	rcvi.rflags |= RECV_F_INTERNAL;

	_sworker_active = 1;
	receive_msg(buf, len, &rcvi);
	_sworker_active = 0;
}

/**
 *
 */
int sworker_send_task(sip_msg_t *msg, str *gname)
{
	async_task_t *at = NULL;
	sworker_task_param_t *stp = NULL;
	int dsize;

	dsize = sizeof(async_task_t) + sizeof(sworker_task_param_t)
		+ (msg->len+1)*sizeof(char);
	at = (async_task_t *)shm_malloc(dsize);
	if(at == NULL) {
		LM_ERR("no more shm memory\n");
		return -1;
	}
	memset(at, 0, dsize);
	at->exec = sworker_exec_task;
	at->param = (char *)at + sizeof(async_task_t);
	stp = (sworker_task_param_t *)at->param;
	stp->buf = (char*)stp+sizeof(sworker_task_param_t);
	memcpy(stp->buf, msg->buf, msg->len);
	stp->len = msg->len;
	memcpy(&stp->rcv, &msg->rcv, sizeof(receive_info_t));

	return async_task_group_push(gname, at);
}

/**
 *
 */
int ki_sworker_task(sip_msg_t *msg, str *gname)
{
	if(msg==NULL || faked_msg_match(msg)) {
		LM_ERR("invalid usage for null or faked message\n");
		return -1;
	}

	if(!(msg->rcv.rflags & RECV_F_PREROUTING)) {
		LM_WARN("not used in pre-routing phase\n");
		return -1;
	}
	if(sworker_send_task(msg, gname) < 0) {
		return -1;
	}

	return 1;
}

/**
 *
 */
static int w_sworker_task(sip_msg_t *msg, char *pgname, char *p2)
{
	str gname;

	if(msg == NULL) {
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_t *)pgname, &gname) != 0) {
		LM_ERR("no async route block name\n");
		return -1;
	}
	return ki_sworker_task(msg, &gname);
}

/**
 *
 */
static int ki_sworker_active(sip_msg_t *msg)
{
	if(_sworker_active==0) {
		return -1;
	}
	return 1;
}

/**
 *
 */
static int w_sworker_active(sip_msg_t *msg, char *p1, char *p2)
{
	if(_sworker_active==0) {
		return -1;
	}
	return 1;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_sworker_exports[] = {
	{ str_init("sworker"), str_init("task"),
		SR_KEMIP_INT, ki_sworker_task,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sworker"), str_init("active"),
		SR_KEMIP_INT, ki_sworker_active,
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
