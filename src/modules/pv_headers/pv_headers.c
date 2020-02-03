/*
 * pv_headers
 *
 * Copyright (C) 2018 Kirill Solomko <ksolomko@sipwise.com>
 *
 * This file is part of SIP Router, a free SIP server.
 *
 * SIP Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP Router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "../../core/sr_module.h"
#include "../../core/mod_fix.h"
#include "../../core/script_cb.h"
#include "../../modules/tm/tm_load.h"
#include "../../core/kemi.h"

#include "pv_headers.h"
#include "pvh_func.h"
#include "pvh_xavp.h"
#include "pvh_hash.h"

MODULE_VERSION

#define MODULE_NAME "pv_headers"

#define XAVP_NAME "headers"

#define FL_NAME_PV_HDRS_COLLECTED "pv_headers_collected"
#define FL_NAME_PV_HDRS_APPLIED "pv_headers_applied"

int FL_PV_HDRS_COLLECTED = 27;
int FL_PV_HDRS_APPLIED = 28;

uac_api_t uac;
static tm_api_t tmb;

str xavp_name = str_init(XAVP_NAME);

str xavp_parsed_xname = str_init("parsed_pv_headers");
static str skip_headers_param =
		str_init("Record-Route,Via,Route,Content-Length,Max-Forwards,CSeq");
static str split_headers_param = STR_NULL;
static int auto_msg_param = 1;

static str single_headers_param = str_init("");

str _hdr_from = {"From", 4};
str _hdr_to = {"To", 2};

unsigned int header_name_size = 255;
unsigned int header_value_size = 1024;


static void mod_destroy(void);
static int mod_init(void);

static void handle_tm_t(tm_cell_t *t, int type, struct tmcb_params *params);
static int handle_msg_cb(struct sip_msg *msg, unsigned int flags, void *cb);

static int w_pvh_collect_headers(struct sip_msg *msg, char *p1, char *p2)
{
	return pvh_collect_headers(msg, 0);
}

static int ki_pvh_collect_headers(struct sip_msg *msg)
{
	return pvh_collect_headers(msg, 0);
}

static int w_pvh_apply_headers(struct sip_msg *msg, char *p1, char *p2)
{
	return pvh_apply_headers(msg, 0);
}

static int ki_pvh_apply_headers(struct sip_msg *msg)
{
	return pvh_apply_headers(msg, 0);
}

static int w_pvh_reset_headers(struct sip_msg *msg, char *p1, char *p2)
{
	return pvh_reset_headers(msg);
}

static int w_pvh_check_header(struct sip_msg *msg, char *p1, char *p2)
{
	str hname = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_p)p1, &hname) < 0)
		return -1;

	return pvh_check_header(msg, &hname);
}

static int w_pvh_append_header(struct sip_msg *msg, char *p1, char *p2)
{
	str hname = STR_NULL, hvalue = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_p)p1, &hname) < 0)
		return -1;

	if(p2 && fixup_get_svalue(msg, (gparam_p)p2, &hvalue) < 0)
		return -1;

	return pvh_append_header(msg, &hname, &hvalue);
}

static int w_pvh_modify_header(
		struct sip_msg *msg, char *p1, char *p2, char *p3)
{
	int indx = 0;
	str hname = STR_NULL, hvalue = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_p)p1, &hname) < 0)
		return -1;

	if(p2 && fixup_get_svalue(msg, (gparam_p)p2, &hvalue) < 0)
		return -1;

	if(p3 && fixup_get_ivalue(msg, (gparam_p)p3, &indx) < 0)
		return -1;

	return pvh_modify_header(msg, &hname, &hvalue, indx);
}

static int w_pvh_remove_header(
		struct sip_msg *msg, char *p1, char *p2, char *p3)
{
	int indx = -1;
	str hname = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_p)p1, &hname) < 0)
		return -1;

	if(p2 && fixup_get_ivalue(msg, (gparam_p)p2, &indx) < 0)
		return -1;

	return pvh_remove_header(msg, &hname, indx);
}

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
		{"pvh_collect_headers", (cmd_function)w_pvh_collect_headers, 0, 0, 0,
				ANY_ROUTE},
		{"pvh_apply_headers", (cmd_function)w_pvh_apply_headers, 0, 0, 0,
				ANY_ROUTE},
		{"pvh_reset_headers", (cmd_function)w_pvh_reset_headers, 0, 0, 0,
				ANY_ROUTE},
		{"pvh_check_header", (cmd_function)w_pvh_check_header, 1,
				fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
		{"pvh_append_header", (cmd_function)w_pvh_append_header, 2,
				fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
		{"pvh_modify_header", (cmd_function)w_pvh_modify_header, 2,
				fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
		{"pvh_modify_header", (cmd_function)w_pvh_modify_header, 3,
				fixup_spve_all, fixup_free_spve_all, ANY_ROUTE},
		{"pvh_remove_header", (cmd_function)w_pvh_remove_header, 1,
				fixup_spve_null, fixup_free_spve_null, ANY_ROUTE},
		{"pvh_remove_header", (cmd_function)w_pvh_remove_header, 2,
				fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
		{0, 0, 0, 0, 0, 0}};

static pv_export_t mod_pvs[] = {
		{{"x_hdr", (sizeof("x_hdr") - 1)}, PVT_OTHER, pvh_get_header,
				pvh_set_header, pvh_parse_header_name, pv_parse_index, 0, 0},
		{{"x_fu", (sizeof("x_fu") - 1)}, PVT_OTHER, pvh_get_uri, pvh_set_uri, 0,
				0, pv_init_iname, 1},
		{{"x_fU", (sizeof("x_fU") - 1)}, PVT_OTHER, pvh_get_uri, pvh_set_uri, 0,
				0, pv_init_iname, 2},
		{{"x_fd", (sizeof("x_fd") - 1)}, PVT_OTHER, pvh_get_uri, pvh_set_uri, 0,
				0, pv_init_iname, 3},
		{{"x_fn", (sizeof("x_fn") - 1)}, PVT_OTHER, pvh_get_uri, pvh_set_uri, 0,
				0, pv_init_iname, 4},
		{{"x_ft", (sizeof("x_ft") - 1)}, PVT_OTHER, pvh_get_uri, /* ro */ 0, 0,
				0, pv_init_iname, 5},
		{{"x_tu", (sizeof("x_tu") - 1)}, PVT_OTHER, pvh_get_uri, pvh_set_uri, 0,
				0, pv_init_iname, 6},
		{{"x_tU", (sizeof("x_tU") - 1)}, PVT_OTHER, pvh_get_uri, pvh_set_uri, 0,
				0, pv_init_iname, 7},
		{{"x_td", (sizeof("x_td") - 1)}, PVT_OTHER, pvh_get_uri, pvh_set_uri, 0,
				0, pv_init_iname, 8},
		{{"x_tn", (sizeof("x_tn") - 1)}, PVT_OTHER, pvh_get_uri, pvh_set_uri, 0,
				0, pv_init_iname, 9},
		{{"x_tt", (sizeof("x_tt") - 1)}, PVT_OTHER, pvh_get_uri, /* ro */ 0, 0,
				0, pv_init_iname, 10},
		{{"x_rs", (sizeof("x_rs") - 1)}, PVT_OTHER, pvh_get_reply_sr,
				pvh_set_reply_sr, 0, 0, pv_init_iname, 1},
		{{"x_rr", (sizeof("x_rr") - 1)}, PVT_OTHER, pvh_get_reply_sr,
				pvh_set_reply_sr, 0, 0, pv_init_iname, 2},
		{{0, 0}, 0, 0, 0, 0, 0, 0, 0}};

static param_export_t params[] = {{"xavp_name", PARAM_STR, &xavp_name},
		{"header_value_size", PARAM_INT, &header_value_size},
		{"header_collect_flag", PARAM_INT, &FL_PV_HDRS_COLLECTED},
		{"header_apply_flag", PARAM_INT, &FL_PV_HDRS_APPLIED},
		{"skip_headers", PARAM_STR, &skip_headers_param},
		{"split_headers", PARAM_STR, &split_headers_param},
		{"auto_msg", PARAM_INT, &auto_msg_param}, {0, 0, 0}};

struct module_exports exports = {
		MODULE_NAME,	 /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			 /* exported functions */
		params,			 /* exported parameters */
		0,				 /* RPC method exports */
		mod_pvs,		 /* exported pseudo-variables */
		0,				 /* response handling function */
		mod_init,		 /* module initialization function */
		0,				 /* per-child init function */
		mod_destroy		 /* module destroy function */
};

int mod_init(void)
{
	LM_INFO("%s module init...\n", MODULE_NAME);

	if(load_uac_api(&uac) < 0) {
		LM_NOTICE("could not bind to the 'uac' module, From/To headers will "
				  "not be modifed\n");
	}

	if(load_tm_api(&tmb) < 0) {
		LM_NOTICE("could not bind to the 'tm' module, automatic headers "
				  "collect/apply is disabled\n");
		auto_msg_param = 0;
	} else {
		if(auto_msg_param
				&& register_script_cb(
						   handle_msg_cb, PRE_SCRIPT_CB | REQUEST_CB, 0)
						   < 0) {
			LM_ERR("cannot register PRE_SCRIPT_CB callbacks\n");
			return -1;
		}
	}

	if(header_value_size == 0) {
		LM_ERR("header_value_size must be >=0\n");
		return -1;
	}

	pvh_str_hash_init(&skip_headers, &skip_headers_param, "skip_headers");
	pvh_str_hash_init(&split_headers, &split_headers_param, "split_headers");
	pvh_str_hash_init(&single_headers, &single_headers_param, "single_headers");

	return 0;
}

void mod_destroy(void)
{
	pvh_str_hash_free(&skip_headers);
	pvh_str_hash_free(&split_headers);
	pvh_str_hash_free(&single_headers);
	pvh_free_xavp(&xavp_name);
	pvh_free_xavp(&xavp_parsed_xname);
	LM_INFO("%s module unload...\n", MODULE_NAME);
}

void handle_tm_t(tm_cell_t *t, int type, struct tmcb_params *params)
{
	struct sip_msg *msg = NULL;

	if(type & TMCB_RESPONSE_IN) {
		msg = params->rpl;
		if(msg != NULL && msg != FAKED_REPLY) {
			pvh_reset_headers(msg);
			pvh_collect_headers(msg, 1);
		}
	} else if(type & TMCB_REQUEST_FWDED) {
		msg = params->req;
	} else if(type & (TMCB_ON_BRANCH_FAILURE | TMCB_RESPONSE_FWDED)) {
		msg = params->rpl;
	} else {
		LM_ERR("unknown callback: %d\n", type);
		return;
	}

	if(msg != NULL && msg != FAKED_REPLY)
		pvh_apply_headers(msg, 1);

	return;
}

int handle_msg_cb(struct sip_msg *msg, unsigned int flags, void *cb)
{
	int cbs = TMCB_REQUEST_FWDED | TMCB_RESPONSE_FWDED | TMCB_RESPONSE_IN
			  | TMCB_ON_BRANCH_FAILURE;

	if(flags & (PRE_SCRIPT_CB | REQUEST_CB)) {
		if(tmb.register_tmcb(msg, 0, cbs, handle_tm_t, 0, 0) <= 0) {
			LM_ERR("cannot register TM callbacks\n");
			return -1;
		}
		pvh_collect_headers(msg, 1);
	} else {
		LM_ERR("unknown callback: %d\n", flags);
	}

	return 1;
}

/* clang-format off */
static sr_kemi_t pvh_kemi_exports[] = {
	{ str_init("pv_headers"), str_init("pvh_collect_headers"),
		SR_KEMIP_INT, ki_pvh_collect_headers,
			{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
				SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE}
	},
	{ str_init("pv_headers"), str_init("pvh_apply_headers"),
		SR_KEMIP_INT, ki_pvh_apply_headers,
			{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
				SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE}
	},
	{ str_init("pv_headers"), str_init("pvh_reset_headers"),
		SR_KEMIP_INT, pvh_reset_headers,
			{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
				SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE}
	},
	{ str_init("pv_headers"), str_init("pvh_check_header"),
		SR_KEMIP_INT, pvh_check_header,
			{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
				SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE}
	},
	{ str_init("pv_headers"), str_init("pvh_append_header"),
		SR_KEMIP_INT, pvh_append_header,
			{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
				SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE}
	},
	{ str_init("pv_headers"), str_init("pvh_modify_header"),
		SR_KEMIP_INT, pvh_modify_header,
			{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_INT,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE}
	},
	{ str_init("pv_headers"), str_init("pvh_remove_header"),
		SR_KEMIP_INT, pvh_remove_header,
			{ SR_KEMIP_STR, SR_KEMIP_INT, SR_KEMIP_NONE,
				SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE}
	},
	{{0, 0}, {0, 0}, 0, NULL, {0, 0, 0, 0, 0, 0}}
};
/* clang-format on */

int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(pvh_kemi_exports);
	return 0;
}
