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
#include "../../core/mod_fix.h"
#include "../../core/ut.h"
#include "../../core/kemi.h"
#include "../../core/utils/sruid.h"
#include "../../core/timer_proc.h"

#include "dlgs_records.h"

MODULE_VERSION

int _dlgs_lifetime = 10800;
int _dlgs_initlifetime = 180;
static int _dlgs_timer_interval = 30;

static int _dlgs_htsize_param = 9;
int _dlgs_htsize = 512;

/* sruid to get internal uid */
sruid_t _dlgs_sruid;

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int w_dlgs_manage(sip_msg_t *msg, char *psrc, char *pdst, char *pdata);
static int w_dlgs_tags_add(sip_msg_t *msg, char *ptags, char *str2);
static int w_dlgs_tags_rm(sip_msg_t *msg, char *ptags, char *str2);
static int w_dlgs_tags_count(sip_msg_t *msg, char *ptags, char *str2);

/* clang-format off */
static cmd_export_t cmds[]={
	{"dlgs_manage", (cmd_function)w_dlgs_manage, 3, fixup_spve_all,
		fixup_free_spve_all, REQUEST_ROUTE|BRANCH_ROUTE|FAILURE_ROUTE|ONSEND_ROUTE},
	{"dlgs_tags_add", (cmd_function)w_dlgs_tags_add, 1, fixup_spve_null,
		fixup_spve_null, ANY_ROUTE},
	{"dlgs_tags_rm", (cmd_function)w_dlgs_tags_rm, 1, fixup_spve_null,
		fixup_spve_null, ANY_ROUTE},
	{"dlgs_tags_count", (cmd_function)w_dlgs_tags_rm, 1, fixup_spve_null,
		fixup_spve_null, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"lifetime",        PARAM_INT,     &_dlgs_lifetime},
	{"initlifetime",    PARAM_INT,     &_dlgs_initlifetime},
	{"timer_interval",  PARAM_INT,     &_dlgs_timer_interval},
	{"hash_size",       PARAM_INT,     &_dlgs_htsize_param},
	{0, 0, 0}
};

struct module_exports exports = {
	"dlgs",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
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
	if(dlgs_rpc_init() < 0) {
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(sruid_init(&_dlgs_sruid, '-', "dlgs", SRUID_INC) < 0) {
		return -1;
	}

	if(_dlgs_htsize_param>1) {
		if(_dlgs_htsize_param>16) {
			_dlgs_htsize_param = 16;
		}
		_dlgs_htsize = 1<<_dlgs_htsize_param;
	}
	if(_dlgs_timer_interval<=0) {
		_dlgs_timer_interval = 30;
	}

	if(sr_wtimer_add(dlgs_ht_timer, NULL, _dlgs_timer_interval) < 0) {
		return -1;
	}

	if(dlgs_init()<0) {
		return -1;
	}
	return 0;
}

/**
 * @brief Initialize module children
 */
static int child_init(int rank)
{
	if(sruid_init(&_dlgs_sruid, '-', "dlgs", SRUID_INC)<0) {
		return -1;
	}

	if(rank != PROC_MAIN) {
		return 0;
	}

	return 0;
}

/**
 * destroy module function
 */
static void mod_destroy(void)
{
	dlgs_destroy();
}

/**
 *
 */
static int ki_dlgs_manage(sip_msg_t *msg, str *src, str *dst, str *data)
{
	return 1;
}

/**
 *
 */
static int w_dlgs_manage(sip_msg_t *msg, char *psrc, char *pdst, char *pdata)
{
	return 1;
}

/**
 *
 */
static int w_dlgs_tags_add(sip_msg_t *msg, char *ptags, char *str2)
{
	return 1;
}

/**
 *
 */
static int w_dlgs_tags_rm(sip_msg_t *msg, char *ptags, char *str2)
{
	return 1;
}

/**
 *
 */
static int w_dlgs_tags_count(sip_msg_t *msg, char *ptags, char *str2)
{
	return 1;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_dlgs_exports[] = {
	{ str_init("dlgs"), str_init("dlgs_manage"),
		SR_KEMIP_INT, ki_dlgs_manage,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
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
	sr_kemi_modules_add(sr_kemi_dlgs_exports);
	return 0;
}
