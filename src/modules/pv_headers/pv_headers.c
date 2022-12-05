/*
 * pv_headers
 *
 * Copyright (C)
 * 2020 Victor Seva <vseva@sipwise.com>
 * 2018 Kirill Solomko <ksolomko@sipwise.com>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "../../core/sr_module.h"
#include "../../core/script_cb.h"
#include "../../core/mod_fix.h"
#include "../../modules/tm/tm_load.h"
#include "../../core/kemi.h"

#include "pv_headers.h"
#include "pvh_func.h"
#include "pvh_hash.h"
#include "pvh_xavp.h"

MODULE_VERSION

#define MODULE_NAME "pv_headers"
#define XAVP_NAME "headers"

uac_api_t uac;
static tm_api_t tmb;

str xavi_name = str_init(XAVP_NAME);
str xavi_helper_xname = str_init("modparam_pv_headers");
str xavi_parsed_xname = str_init("parsed_pv_headers");
unsigned int header_name_size = 255;
unsigned int header_value_size = 1024;
int FL_PV_HDRS_COLLECTED = 27;
int FL_PV_HDRS_APPLIED = 28;
static str skip_headers_param =
		str_init("Record-Route,Via,Route,Content-Length,Max-Forwards,CSeq");
static str split_headers_param = STR_NULL;
static str single_headers_param = str_init("");
static int auto_msg_param = 1;

str _hdr_from = {"From", 4};
str _hdr_to = {"To", 2};
str _hdr_reply_reason = {"@Reply-Reason", 13};
int _branch = T_BR_UNDEFINED;
int _reply_counter = -1;

static void mod_destroy(void);
static int mod_init(void);

static void handle_tm_t(tm_cell_t *t, int type, struct tmcb_params *params);
static int handle_msg_failed_cb(struct sip_msg *msg, unsigned int flags, void *cb);
static int handle_msg_cb(struct sip_msg *msg, unsigned int flags, void *cb);
static int handle_msg_branch_cb(
		struct sip_msg *msg, unsigned int flags, void *cb);
static int handle_msg_reply_cb(
		struct sip_msg *msg, unsigned int flags, void *cb);

/**
 *
 */
static int pvh_get_branch_index(struct sip_msg *msg, int *br_idx)
{
	int os = 0;
	int len = 0;
	char parsed_br_idx[header_value_size];

	if(msg->add_to_branch_len > header_value_size) {
		LM_ERR("branch name is too long\n");
		return -1;
	}

	os = msg->add_to_branch_len;
	while(os > 0 && memcmp(msg->add_to_branch_s + os - 1, ".", 1))
		os--;
	len = msg->add_to_branch_len - os;
	if(os > 0 && len > 0) {
		memcpy(parsed_br_idx, msg->add_to_branch_s + os, len);
		parsed_br_idx[len] = '\0';
		*br_idx = atoi(parsed_br_idx) + 1;
	} else {
		*br_idx = 0;
	}

	return 1;
}

static int w_pvh_collect_headers(struct sip_msg *msg, char *p1, char *p2)
{
	sr_xavp_t **backup_xavis = NULL;

	if(pvh_get_branch_index(msg, &_branch) < 0)
		return -1;
	if(msg->first_line.type == SIP_REPLY) {
		if((_reply_counter = pvh_reply_append(backup_xavis)) < 0) {
			return -1;
		}
	}
	return pvh_collect_headers(msg);
}

static int ki_pvh_collect_headers(struct sip_msg *msg)
{
	sr_xavp_t **backup_xavis = NULL;

	if(pvh_get_branch_index(msg, &_branch) < 0)
		return -1;
	if(msg->first_line.type == SIP_REPLY) {
		if((_reply_counter = pvh_reply_append(backup_xavis)) < 0) {
			return -1;
		}
	}
	return pvh_collect_headers(msg);
}

static int w_pvh_apply_headers(struct sip_msg *msg, char *p1, char *p2)
{
	if(pvh_get_branch_index(msg, &_branch) < 0)
		return -1;
	return pvh_apply_headers(msg);
}

static int ki_pvh_apply_headers(struct sip_msg *msg)
{
	if(pvh_get_branch_index(msg, &_branch) < 0)
		return -1;
	return pvh_apply_headers(msg);
}

static int w_pvh_reset_headers(struct sip_msg *msg, char *p1, char *p2)
{
	if(pvh_get_branch_index(msg, &_branch) < 0)
		return -1;
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

static int w_pvh_header_param_exists(
		struct sip_msg *msg, char *p1, char *p2)
{
	str hname = STR_NULL;
	str value = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_p)p1, &hname) < 0)
		return -1;

	if(p2 && fixup_get_svalue(msg, (gparam_p)p2, &value) < 0)
		return -1;

	return pvh_header_param_exists(msg, &hname, &value);
}

static int ki_pvh_remove_header_param(struct sip_msg *msg, str *hname, str *toRemove)
{
	int idx;
	int new_size;
	str dst = STR_NULL;
	sr_xavp_t *avi = pvh_xavi_get_child(msg, &xavi_name, hname);

	for(idx=0; avi != NULL; avi = xavi_get_next(avi)) {
		if (avi->val.type == SR_XTYPE_STR && avi->val.v.s.s != NULL) {
			if(str_casesearch(&avi->val.v.s, toRemove) != NULL) {
				new_size = pvh_remove_header_param_helper(&avi->val.v.s, toRemove, &dst);
				if(dst.len == 0) {
					LM_DBG("nothing left in the header:%.*s, remove it[%d]\n",
							STR_FMT(hname), idx);
					if(pvh_remove_header(msg, hname, idx) < 0)
						return -1;
				} else if(dst.len < 0 || new_size == avi->val.v.s.len) {
					LM_DBG("'%.*s' not found at '%.*s'\n", STR_FMT(toRemove),
						STR_FMT(&avi->val.v.s));
				} else {
					LM_DBG("old_value:'%.*s' new_value:'%.*s'\n",
						STR_FMT(&avi->val.v.s), STR_FMT(&dst));
					if(pvh_set_xavi(msg, &xavi_name, hname, &dst, SR_XTYPE_STR, idx, 0) < 0) {
						LM_ERR("can't set new value\n");
						return -1;
					}
				}
			} else {
				LM_DBG("'%.*s' not found at '%.*s'\n", STR_FMT(toRemove),
						STR_FMT(&avi->val.v.s));
			}
		}
		idx++;
	}
	return 1;
}

static int w_pvh_remove_header_param(struct sip_msg *msg, char *p1, char *p2)
{
	str hname = STR_NULL;
	str value = STR_NULL;

	if(fixup_get_svalue(msg, (gparam_p)p1, &hname) < 0)
		return -1;

	if(p2 && fixup_get_svalue(msg, (gparam_p)p2, &value) < 0)
		return -1;

	return ki_pvh_remove_header_param(msg, &hname, &value);
}

/* clang-format off */
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
	{"pvh_header_param_exists", (cmd_function)w_pvh_header_param_exists, 2,
			fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{"pvh_remove_header_param", (cmd_function)w_pvh_remove_header_param, 2,
			fixup_spve_spve, fixup_free_spve_spve, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

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
	{{0, 0}, 0, 0, 0, 0, 0, 0, 0}
};

static param_export_t params[] = {
	{"xavi_name", PARAM_STR, &xavi_name},
	{"header_value_size", PARAM_INT, &header_value_size},
	{"header_collect_flag", PARAM_INT, &FL_PV_HDRS_COLLECTED},
	{"header_apply_flag", PARAM_INT, &FL_PV_HDRS_APPLIED},
	{"skip_headers", PARAM_STR, &skip_headers_param},
	{"split_headers", PARAM_STR, &split_headers_param},
	{"auto_msg", PARAM_INT, &auto_msg_param},
	{0, 0, 0}
};

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
/* clang-format on */

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
	}
	if(auto_msg_param) {
		if(register_script_cb(handle_msg_cb, PRE_SCRIPT_CB | REQUEST_CB, 0)
				< 0) {
			LM_ERR("cannot register PRE_SCRIPT_CB REQUEST_CB callbacks\n");
			return -1;
		}
		if(register_script_cb(
				   handle_msg_branch_cb, PRE_SCRIPT_CB | BRANCH_CB, 0)
				< 0) {
			LM_ERR("cannot register PRE_SCRIPT_CB BRANCH_CB callbacks\n");
			return -1;
		}
		if(register_script_cb(
				   handle_msg_reply_cb, PRE_SCRIPT_CB | ONREPLY_CB, 0)
				< 0) {
			LM_ERR("cannot register PRE_SCRIPT_CB ONREPLY_CB callbacks\n");
			return -1;
		}
		if(register_script_cb(
				   handle_msg_failed_cb, PRE_SCRIPT_CB | FAILURE_CB, 0)
				< 0) {
			LM_ERR("cannot register PRE_SCRIPT_CB FAILURE_CB callbacks\n");
			return -1;
		}
	}

	pvh_str_hash_init(&skip_headers, &skip_headers_param, "skip_headers");
	pvh_str_hash_init(&split_headers, &split_headers_param, "split_headers");
	pvh_str_hash_init(&single_headers, &single_headers_param, "single_headers");

	return 0;
}

void mod_destroy(void)
{
	LM_INFO("%s module unload...\n", MODULE_NAME);
}

/* just for debug */
static inline char *tm_type_to_string(int type)
{
	switch(type) {
		case TMCB_REQUEST_IN:
			return "TMCB_REQUEST_IN";
		case TMCB_RESPONSE_IN:
			return "TMCB_RESPONSE_IN";
		case TMCB_E2EACK_IN:
			return "TMCB_E2EACK_IN";
		case TMCB_REQUEST_PENDING:
			return "TMCB_REQUEST_PENDING";
		case TMCB_REQUEST_FWDED:
			return "TMCB_REQUEST_FWDED";
		case TMCB_RESPONSE_FWDED:
			return "TMCB_RESPONSE_FWDED";
		case TMCB_ON_FAILURE_RO:
			return "TMCB_ON_FAILURE_RO";
		case TMCB_ON_FAILURE:
			return "TMCB_ON_FAILURE";
		case TMCB_REQUEST_OUT:
			return "TMCB_REQUEST_OUT";
		case TMCB_RESPONSE_OUT:
			return "TMCB_RESPONSE_OUT";
		case TMCB_LOCAL_COMPLETED:
			return "TMCB_LOCAL_COMPLETED";
		case TMCB_LOCAL_RESPONSE_OUT:
			return "TMCB_LOCAL_RESPONSE_OUT";
		case TMCB_ACK_NEG_IN:
			return "TMCB_ACK_NEG_IN";
		case TMCB_REQ_RETR_IN:
			return "TMCB_REQ_RETR_IN";
		case TMCB_LOCAL_RESPONSE_IN:
			return "TMCB_LOCAL_RESPONSE_IN";
		case TMCB_LOCAL_REQUEST_IN:
			return "TMCB_LOCAL_REQUEST_IN";
		case TMCB_DLG:
			return "TMCB_DLG";
		case TMCB_DESTROY:
			return "TMCB_DESTROY";
		case TMCB_E2ECANCEL_IN:
			return "TMCB_E2ECANCEL_IN";
		case TMCB_E2EACK_RETR_IN:
			return "TMCB_E2EACK_RETR_IN";
		case TMCB_RESPONSE_READY:
			return "TMCB_RESPONSE_READY";
		case TMCB_DONT_ACK:
			return "TMCB_DONT_ACK";
		case TMCB_REQUEST_SENT:
			return "TMCB_REQUEST_SENT";
		case TMCB_RESPONSE_SENT:
			return "TMCB_RESPONSE_SENT";
		case TMCB_ON_BRANCH_FAILURE:
			return "TMCB_ON_BRANCH_FAILURE";
		case TMCB_ON_BRANCH_FAILURE_RO:
			return "TMCB_ON_BRANCH_FAILURE_RO";
		case TMCB_MAX:
			return "TMCB_MAX";
	}

	return "UNKNOWN";
}

static inline void print_cb_flags(unsigned int flags)
{
	LM_DBG("flags:");
	if(flags & REQUEST_CB)
		LM_DBG("REQUEST_CB");
	if(flags & FAILURE_CB)
		LM_DBG("FAILURE_CB");
	if(flags & ONREPLY_CB)
		LM_DBG("ONREPLY_CB");
	if(flags & BRANCH_CB)
		LM_DBG("BRANCH_CB");
	if(flags & ONSEND_CB)
		LM_DBG("ONSEND_CB");
	if(flags & ERROR_CB)
		LM_DBG("ERROR_CB");
	if(flags & LOCAL_CB)
		LM_DBG("LOCAL_CB");
	if(flags & EVENT_CB)
		LM_DBG("EVENT_CB");
	if(flags & BRANCH_FAILURE_CB)
		LM_DBG("BRANCH_FAILURE_CB");
}

void handle_tm_t(tm_cell_t *t, int type, struct tmcb_params *params)
{
	struct sip_msg *msg = NULL;

	LM_DBG("T:%p params->branch:%d type:%s\n", t, params->branch,
			tm_type_to_string(type));


	if(type & TMCB_REQUEST_FWDED) {
		msg = params->req;
	} else if(type & (TMCB_ON_BRANCH_FAILURE | TMCB_RESPONSE_FWDED)) {
		msg = params->rpl;
	} else {
		LM_ERR("unknown callback: %d\n", type);
		return;
	}


	LM_DBG("T:%p picked_branch:%d label:%d branches:%d\n", t,
			tmb.t_get_picked_branch(), t->label, t->nr_of_outgoings);

	if(msg != NULL && msg != FAKED_REPLY) {
		pvh_get_branch_index(msg, &_branch);
		LM_DBG("T:%p set branch:%d\n", t, _branch);
		pvh_apply_headers(msg);
	}
	return;
}

int handle_msg_failed_cb(struct sip_msg *msg, unsigned int flags, void *cb)
{
	print_cb_flags(flags);

	if(pvh_parse_msg(msg) != 0)
		return 1;

	_branch = 0;
	LM_DBG("msg:%p set branch:%d\n", msg, _branch);
	return 1;
}

static int msg_cbs =
		TMCB_REQUEST_FWDED | TMCB_RESPONSE_FWDED | TMCB_ON_BRANCH_FAILURE;

int handle_msg_cb(struct sip_msg *msg, unsigned int flags, void *cb)
{
	print_cb_flags(flags);

	if(pvh_parse_msg(msg) != 0)
		return 1;

	if(tmb.register_tmcb(msg, 0, msg_cbs, handle_tm_t, 0, 0) <= 0) {
		LM_ERR("cannot register TM callbacks\n");
		return -1;
	}

	_branch = 0;
	LM_DBG("msg:%p set branch:%d\n", msg, _branch);
	pvh_collect_headers(msg);
	return 1;
}

int handle_msg_branch_cb(struct sip_msg *msg, unsigned int flags, void *cb)
{

	LM_DBG("msg:%p previous branch:%d\n", msg, _branch);
	print_cb_flags(flags);

	if(flags & PRE_SCRIPT_CB) {
		pvh_get_branch_index(msg, &_branch);
		LM_DBG("msg:%p set branch:%d\n", msg, _branch);
		pvh_clone_branch_xavi(msg, &xavi_name);
	}

	return 1;
}

int handle_msg_reply_cb(struct sip_msg *msg, unsigned int flags, void *cb)
{
	tm_cell_t *t = NULL;
	sr_xavp_t **backup_xavis = NULL;
	sr_xavp_t **list = NULL;

	if(pvh_parse_msg(msg) != 0)
		return 1;
	if(msg == FAKED_REPLY) {
		LM_DBG("FAKED_REPLY\n");
	}
	LM_DBG("msg:%p previous branch:%d\n", msg, _branch);
	print_cb_flags(flags);

	if(tmb.t_check(msg, &_branch) == -1) {
		LM_ERR("failed find UAC branch\n");
	} else {
		t = tmb.t_gett();
		if(t == NULL || t == T_UNDEFINED) {
			LM_DBG("cannot lookup the transaction\n");
		} else {
			LM_DBG("T:%p t_check-branch:%d xavi_list:%p branches:%d\n", t,
					_branch, &t->xavis_list, t->nr_of_outgoings);
			list = &t->xavis_list;
			backup_xavis = xavi_set_list(&t->xavis_list);
		}
	}

	pvh_get_branch_index(msg, &_branch);
	LM_DBG("T:%p set branch:%d picked_branch:%d\n", t, _branch,
			tmb.t_get_picked_branch());

	if((_reply_counter = pvh_reply_append(list)) < 0) {
		return -1;
	}
	pvh_collect_headers(msg);
	if(backup_xavis) {
		xavi_set_list(backup_xavis);
		LM_DBG("restored backup_xavis:%p\n", *backup_xavis);
	}
	if(t) {
		tmb.unref_cell(t);
		LM_DBG("T:%p unref\n", t);
	}
	tmb.t_sett(T_UNDEFINED, T_BR_UNDEFINED);
	LM_DBG("reset tm\n");

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
	{ str_init("pv_headers"), str_init("pvh_header_param_exists"),
		SR_KEMIP_INT, pvh_header_param_exists,
			{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
				SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE}
	},
	{ str_init("pv_headers"), str_init("pvh_remove_header_param"),
		SR_KEMIP_INT, ki_pvh_remove_header_param,
			{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
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
