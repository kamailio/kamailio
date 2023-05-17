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
#include "../../core/events.h"
#include "../../core/utils/sruid.h"
#include "../../core/timer_proc.h"

#include "dlgs_records.h"

MODULE_VERSION

int _dlgs_active_lifetime = 10800;
int _dlgs_init_lifetime = 180;
int _dlgs_finish_lifetime = 10;
static int _dlgs_timer_interval = 30;

static int _dlgs_htsize_param = 9;
int _dlgs_htsize = 512;

/* sruid to get internal uid */
sruid_t _dlgs_sruid;

static int mod_init(void);
static int child_init(int);
static void mod_destroy(void);

static int w_dlgs_init(sip_msg_t *msg, char *psrc, char *pdst, char *pdata);
static int w_dlgs_update(sip_msg_t *msg, char *p1, char *p2);
static int w_dlgs_count(sip_msg_t *msg, char *pfield, char *pop, char *pdata);
static int w_dlgs_tags_add(sip_msg_t *msg, char *ptags, char *p2);
static int w_dlgs_tags_rm(sip_msg_t *msg, char *ptags, char *p2);
static int w_dlgs_tags_count(sip_msg_t *msg, char *ptags, char *p2);

static int dlgs_sip_reply_out(sr_event_param_t *evp);

/* clang-format off */
static cmd_export_t cmds[]={
	{"dlgs_init", (cmd_function)w_dlgs_init, 3, fixup_spve_all,
		fixup_free_spve_all, REQUEST_ROUTE|BRANCH_ROUTE|ONREPLY_ROUTE|ONSEND_ROUTE},
	{"dlgs_update", (cmd_function)w_dlgs_update, 0, 0,
		0, REQUEST_ROUTE|BRANCH_ROUTE|ONSEND_ROUTE},
	{"dlgs_count", (cmd_function)w_dlgs_count, 3, fixup_spve_all,
		fixup_free_spve_all, ANY_ROUTE},
	{"dlgs_tags_add", (cmd_function)w_dlgs_tags_add, 1, fixup_spve_null,
		fixup_spve_null, ANY_ROUTE},
	{"dlgs_tags_rm", (cmd_function)w_dlgs_tags_rm, 1, fixup_spve_null,
		fixup_spve_null, ANY_ROUTE},
	{"dlgs_tags_count", (cmd_function)w_dlgs_tags_count, 1, fixup_spve_null,
		fixup_spve_null, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"active_lifetime",  PARAM_INT,     &_dlgs_active_lifetime},
	{"init_lifetime",    PARAM_INT,     &_dlgs_init_lifetime},
	{"finish_lifetime",  PARAM_INT,     &_dlgs_finish_lifetime},
	{"timer_interval",   PARAM_INT,     &_dlgs_timer_interval},
	{"hash_size",        PARAM_INT,     &_dlgs_htsize_param},
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

	if(_dlgs_htsize_param > 1) {
		if(_dlgs_htsize_param > 16) {
			_dlgs_htsize_param = 16;
		}
		_dlgs_htsize = 1 << _dlgs_htsize_param;
	}
	if(_dlgs_timer_interval <= 0) {
		_dlgs_timer_interval = 30;
	}

	if(sr_wtimer_add(dlgs_ht_timer, NULL, _dlgs_timer_interval) < 0) {
		return -1;
	}

	sr_event_register_cb(SREV_SIP_REPLY_OUT, dlgs_sip_reply_out);

	if(dlgs_init() < 0) {
		return -1;
	}
	return 0;
}

/**
 * @brief Initialize module children
 */
static int child_init(int rank)
{
	if(sruid_init(&_dlgs_sruid, '-', "dlgs", SRUID_INC) < 0) {
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
static int ki_dlgs_init(sip_msg_t *msg, str *src, str *dst, str *data)
{
	int rtype = 0;
	int rmethod = 0;
	int ret = 0;

	if(msg->first_line.type == SIP_REQUEST) {
		rtype = SIP_REQUEST;
		rmethod = msg->first_line.u.request.method_value;
	} else {
		rtype = SIP_REPLY;
		if(msg->cseq == NULL
				&& ((parse_headers(msg, HDR_CSEQ_F, 0) == -1)
						|| (msg->cseq == NULL))) {
			LM_ERR("no CSEQ header\n");
			return -1;
		}
		rmethod = get_cseq(msg)->method_id;
	}

	if(rmethod == METHOD_INVITE) {
		ret = dlgs_add_item(msg, src, dst, data);
		LM_DBG("added item return code: %d\n", ret);
		if(rtype == SIP_REPLY) {
			dlgs_update_item(msg);
		}
	} else {
		dlgs_update_item(msg);
	}

	return 1;
}

/**
 *
 */
static int w_dlgs_init(sip_msg_t *msg, char *psrc, char *pdst, char *pdata)
{
	str vsrc = STR_NULL;
	str vdst = STR_NULL;
	str vdata = str_init("");

	if(fixup_get_svalue(msg, (gparam_t *)psrc, &vsrc) < 0) {
		LM_ERR("failed to get p1\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pdst, &vdst) < 0) {
		LM_ERR("failed to get p2\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pdata, &vdata) < 0) {
		LM_ERR("failed to get p3\n");
		return -1;
	}

	return ki_dlgs_init(msg, &vsrc, &vdst, &vdata);
}

/**
 *
 */
static int ki_dlgs_update(sip_msg_t *msg)
{
	dlgs_update_item(msg);

	return 1;
}

/**
 *
 */
static int w_dlgs_update(sip_msg_t *msg, char *p1, char *p2)
{
	return ki_dlgs_update(msg);
}

/**
 *
 */
static int ki_dlgs_count(sip_msg_t *msg, str *vfield, str *vop, str *vdata)
{
	int ret;

	LM_DBG("counting by: [%.*s] [%.*s] [%.*s]\n", vfield->len, vfield->s,
			vop->len, vop->s, vdata->len, vdata->s);
	ret = dlgs_count(msg, vfield, vop, vdata);
	if(ret <= 0) {
		return (ret - 1);
	}

	return ret;
}

/**
 *
 */
static int w_dlgs_count(sip_msg_t *msg, char *pfield, char *pop, char *pdata)
{
	str vfield = STR_NULL;
	str vop = STR_NULL;
	str vdata = str_init("");

	if(fixup_get_svalue(msg, (gparam_t *)pfield, &vfield) < 0) {
		LM_ERR("failed to get p1\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pop, &vop) < 0) {
		LM_ERR("failed to get p2\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t *)pdata, &vdata) < 0) {
		LM_ERR("failed to get p3\n");
		return -1;
	}
	return ki_dlgs_count(msg, &vfield, &vop, &vdata);
}

/**
 *
 */
static int ki_dlgs_tags_add(sip_msg_t *msg, str *vtags)
{
	if(dlgs_tags_add(msg, vtags) < 0) {
		return -1;
	}
	return 1;
}

/**
 *
 */
static int w_dlgs_tags_add(sip_msg_t *msg, char *ptags, char *p2)
{
	str vtags = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t *)ptags, &vtags) < 0) {
		LM_ERR("failed to get p1\n");
		return -1;
	}
	return ki_dlgs_tags_add(msg, &vtags);
}

/**
 *
 */
static int ki_dlgs_tags_rm(sip_msg_t *msg, str *vtags)
{
	if(dlgs_tags_rm(msg, vtags) < 0) {
		return -1;
	}
	return 1;
}

/**
 *
 */
static int w_dlgs_tags_rm(sip_msg_t *msg, char *ptags, char *p2)
{
	str vtags = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t *)ptags, &vtags) < 0) {
		LM_ERR("failed to get p1\n");
		return -1;
	}
	return ki_dlgs_tags_rm(msg, &vtags);
}

/**
 *
 */
static int ki_dlgs_tags_count(sip_msg_t *msg, str *vtags)
{
	int ret;

	ret = dlgs_tags_count(msg, vtags);
	return (ret <= 0) ? (ret - 1) : ret;
}

/**
 *
 */
static int w_dlgs_tags_count(sip_msg_t *msg, char *ptags, char *p2)
{
	str vtags = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_t *)ptags, &vtags) < 0) {
		LM_ERR("failed to get p1\n");
		return -1;
	}
	return ki_dlgs_tags_count(msg, &vtags);
}

/**
 *
 */
static int dlgs_sip_reply_out(sr_event_param_t *evp)
{
	if(evp->rpl != NULL) {
		dlgs_update_item(evp->rpl);
	}
	return 0;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_dlgs_exports[] = {
	{ str_init("dlgs"), str_init("dlgs_init"),
		SR_KEMIP_INT, ki_dlgs_init,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dlgs"), str_init("dlgs_update"),
		SR_KEMIP_INT, ki_dlgs_update,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dlgs"), str_init("dlgs_count"),
		SR_KEMIP_INT, ki_dlgs_count,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_STR,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dlgs"), str_init("dlgs_tags_add"),
		SR_KEMIP_INT, ki_dlgs_tags_add,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dlgs"), str_init("dlgs_tags_rm"),
		SR_KEMIP_INT, ki_dlgs_tags_rm,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("dlgs"), str_init("dlgs_tags_count"),
		SR_KEMIP_INT, ki_dlgs_tags_count,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
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
