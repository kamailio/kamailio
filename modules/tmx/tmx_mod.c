/**
 * $Id$
 *
 * Copyright (C) 2009
 *
 * This file is part of SIP-Router.org, a free SIP server.
 *
 * SIP-Router is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mod_fix.h"
#include "../../route.h"
#include "../../modules/tm/tm_load.h"
#include "../../lib/kcore/kstats_wrapper.h"

#include "t_var.h"
#include "t_mi.h"
#include "api.h"

MODULE_VERSION


/** TM bind */
struct tm_binds _tmx_tmb;

/** parameters */

/** module functions */
static int mod_init(void);
static void destroy(void);

static int t_cancel_branches(struct sip_msg* msg, char *k, char *s2);
static int fixup_cancel_branches(void** param, int param_no);
static int t_cancel_callid(struct sip_msg* msg, char *cid, char *cseq,
				char *flag);
static int fixup_cancel_callid(void** param, int param_no);
static int t_reply_callid(struct sip_msg* msg, char *cid, char *cseq,
				char *rc, char *rs);
static int fixup_reply_callid(void** param, int param_no);

static int t_flush_flags(struct sip_msg* msg, char*, char* );
static int t_is_failure_route(struct sip_msg* msg, char*, char* );
static int t_is_branch_route(struct sip_msg* msg, char*, char* );
static int t_is_reply_route(struct sip_msg* msg, char*, char*);

static int w_t_suspend(struct sip_msg* msg, char*, char*);
static int w_t_continue(struct sip_msg* msg, char *idx, char *lbl, char *rtn);
static int fixup_t_continue(void** param, int param_no);

static int bind_tmx(tmx_api_t* api);

/* statistic variables */
stat_var *tm_rcv_rpls;
stat_var *tm_rld_rpls;
stat_var *tm_loc_rpls;
stat_var *tm_uas_trans;
stat_var *tm_uac_trans;
stat_var *tm_trans_2xx;
stat_var *tm_trans_3xx;
stat_var *tm_trans_4xx;
stat_var *tm_trans_5xx;
stat_var *tm_trans_6xx;
stat_var *tm_trans_inuse;

#ifdef STATISTICS

unsigned long tmx_stats_uas_trans(void);
unsigned long tmx_stats_uac_trans(void);
unsigned long tmx_stats_trans_2xx(void);
unsigned long tmx_stats_trans_3xx(void);
unsigned long tmx_stats_trans_4xx(void);
unsigned long tmx_stats_trans_5xx(void);
unsigned long tmx_stats_trans_6xx(void);
unsigned long tmx_stats_trans_inuse(void);
unsigned long tmx_stats_trans_active(void);
#if 0
unsigned long tmx_stats_rcv_rpls(void);
unsigned long tmx_stats_rld_rpls(void);
#endif
unsigned long tmx_stats_loc_rpls(void);

static stat_export_t mod_stats[] = {
	{"UAS_transactions" ,    STAT_IS_FUNC, (stat_var**)tmx_stats_uas_trans   },
	{"UAC_transactions" ,    STAT_IS_FUNC, (stat_var**)tmx_stats_uac_trans   },
	{"2xx_transactions" ,    STAT_IS_FUNC, (stat_var**)tmx_stats_trans_2xx   },
	{"3xx_transactions" ,    STAT_IS_FUNC, (stat_var**)tmx_stats_trans_3xx   },
	{"4xx_transactions" ,    STAT_IS_FUNC, (stat_var**)tmx_stats_trans_4xx   },
	{"5xx_transactions" ,    STAT_IS_FUNC, (stat_var**)tmx_stats_trans_5xx   },
	{"6xx_transactions" ,    STAT_IS_FUNC, (stat_var**)tmx_stats_trans_6xx   },
	{"inuse_transactions" ,  STAT_IS_FUNC, (stat_var**)tmx_stats_trans_inuse },
	{"active_transactions" , STAT_IS_FUNC, (stat_var**)tmx_stats_trans_active},
#if 0
	{"received_replies" ,    STAT_IS_FUNC, (stat_var**)tmx_stats_rcv_rpls    },
	{"relayed_replies" ,     STAT_IS_FUNC, (stat_var**)tmx_stats_rld_rpls    },
#endif
	{"local_replies" ,       STAT_IS_FUNC, (stat_var**)tmx_stats_loc_rpls    },
	{0,0,0}
};
#endif

/**
 * pseudo-variables exported by TM module
 */
static pv_export_t mod_pvs[] = {
	{ {"T_branch_idx", sizeof("T_branch_idx")-1}, PVT_OTHER,
		pv_get_tm_branch_idx, 0,
		 0, 0, 0, 0 },
	{ {"T_reply_ruid", sizeof("T_reply_ruid")-1}, PVT_OTHER,
		pv_get_tm_reply_ruid, 0,
		 0, 0, 0, 0 },
	{ {"T_reply_code", sizeof("T_reply_code")-1}, PVT_OTHER,
		pv_get_tm_reply_code, 0,
		 0, 0, 0, 0 },
	{ {"T_reply_reason", sizeof("T_reply_reason")-1}, PVT_OTHER,
		pv_get_tm_reply_reason, 0,
		 0, 0, 0, 0 },
	{ {"T_reply_last", sizeof("T_reply_last")-1}, PVT_OTHER,
		pv_get_tm_reply_last_received, 0,
		 0, 0, 0, 0 },
	{ {"T_inv", sizeof("T_inv")-1}, PVT_OTHER, pv_get_t_var_inv, 0,
		pv_parse_t_var_name, 0, 0, 0 },
	{ {"T_req", sizeof("T_req")-1}, PVT_OTHER, pv_get_t_var_req, 0,
		pv_parse_t_var_name, 0, 0, 0 },
	{ {"T_rpl", sizeof("T_rpl")-1}, PVT_OTHER, pv_get_t_var_rpl, 0,
		pv_parse_t_var_name, 0, 0, 0 },
	{ {"T", sizeof("T")-1}, PVT_OTHER, pv_get_t, 0,
		pv_parse_t_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static mi_export_t mi_cmds [] = {
	{MI_TM_UAC,     mi_tm_uac_dlg,   MI_ASYNC_RPL_FLAG,  0,  0 },
	{MI_TM_CANCEL,  mi_tm_cancel,    0,                  0,  0 },
	{MI_TM_HASH,    mi_tm_hash,      MI_NO_INPUT_FLAG,   0,  0 },
	{MI_TM_REPLY,   mi_tm_reply,     0,                  0,  0 },
	{MI_TM_REPLY_CALLID, mi_tm_reply_callid, 0,          0,  0 },
	{0,0,0,0,0}
};


static cmd_export_t cmds[]={
	{"t_cancel_branches", (cmd_function)t_cancel_branches,  1,
		fixup_cancel_branches, 0, ONREPLY_ROUTE },
	{"t_cancel_callid", (cmd_function)t_cancel_callid,  3,
		fixup_cancel_callid, 0, ANY_ROUTE },
	{"t_reply_callid", (cmd_function)t_reply_callid,    4,
		fixup_reply_callid, 0, ANY_ROUTE },
	{"t_flush_flags",   (cmd_function)t_flush_flags,    0, 0,
			0, ANY_ROUTE  },
	{"t_is_failure_route",   (cmd_function)t_is_failure_route,   0, 0,
			0, ANY_ROUTE  },
	{"t_is_branch_route",    (cmd_function)t_is_branch_route,    0, 0,
			0, ANY_ROUTE  },
	{"t_is_reply_route",    (cmd_function)t_is_reply_route,    0, 0,
			0, ANY_ROUTE  },
	{"t_suspend",    (cmd_function)w_t_suspend,    0, 0,
			0, ANY_ROUTE  },
	{"t_continue", (cmd_function)w_t_continue,     3,
		fixup_t_continue, 0, ANY_ROUTE },
	{"bind_tmx", (cmd_function)bind_tmx, 1,
		0, 0, ANY_ROUTE },
	{0,0,0,0,0,0}
};

static param_export_t params[]={
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"tmx",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
#ifdef STATISTICS
	mod_stats,  /* exported statistics */
#else
	0,
#endif
	mi_cmds,    /* exported MI functions */
	mod_pvs,    /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	(destroy_function) destroy,
	0           /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	/* load the TM API */
	if (load_tm_api(&_tmx_tmb)!=0) {
		LM_ERR("can't load TM API\n");
		return -1;
	}

	if(register_mi_mod(exports.name, mi_cmds)!=0)
	{
		LM_ERR("failed to register MI commands\n");
		return -1;
	}
#ifdef STATISTICS
	/* register statistics */
	if (register_module_stats( exports.name, mod_stats)!=0 ) {
		LM_ERR("failed to register statistics\n");
		return -1;
	}
#endif
	pv_tmx_data_init();

	return 0;
}

/**
 * destroy function
 */
static void destroy(void)
{
	return;
}

/**
 *
 */
static int fixup_cancel_branches(void** param, int param_no)
{
	char *val;
	int n = 0;

	if (param_no==1) {
		val = (char*)*param;
		if (strcasecmp(val,"all")==0) {
			n = 0;
		} else if (strcasecmp(val,"others")==0) {
			n = 1;
		} else if (strcasecmp(val,"this")==0) {
			n = 2;
		} else {
			LM_ERR("invalid param \"%s\"\n", val);
			return E_CFG;
		}
		pkg_free(*param);
		*param=(void*)(long)n;
	} else {
		LM_ERR("called with parameter != 1\n");
		return E_BUG;
	}
	return 0;
}

/**
 *
 */
static int t_cancel_branches(struct sip_msg* msg, char *k, char *s2)
{
	struct cancel_info cancel_data;
	struct cell *t = 0;
	tm_ctx_t *tcx = 0;
	int n=0;
	int idx = 0;
	t=_tmx_tmb.t_gett();
	if (t==NULL || t==T_UNDEFINED || !is_invite(t))
		return -1;
	tcx = _tmx_tmb.tm_ctx_get();
	if(tcx != NULL)
		idx = tcx->branch_index;
	n = (int)(long)k;
	init_cancel_info(&cancel_data);
	switch(n) {
		case 1:
			/* prepare cancel for every branch except idx */
			_tmx_tmb.prepare_to_cancel(t,
					&cancel_data.cancel_bitmap, 1<<idx);
		case 2:
			if(msg->first_line.u.reply.statuscode>=200)
				break;
			cancel_data.cancel_bitmap = 1<<idx;
		break;
		default:
			if (msg->first_line.u.reply.statuscode>=200)
				/* prepare cancel for every branch except idx */
				_tmx_tmb.prepare_to_cancel(t,
						&cancel_data.cancel_bitmap, 1<<idx);
			else
				_tmx_tmb.prepare_to_cancel(t,
						&cancel_data.cancel_bitmap, 0);
	}
	LM_DBG("canceling %d/%d\n", n, (int)cancel_data.cancel_bitmap);
	if(cancel_data.cancel_bitmap==0)
		return -1;
	_tmx_tmb.cancel_uacs(t, &cancel_data, 0);
	return 1;
}

/**
 *
 */
static int fixup_cancel_callid(void** param, int param_no)
{
	if (param_no==1 || param_no==2) {
		return fixup_spve_null(param, 1);
	}
	if (param_no==3) {
		return fixup_igp_null(param, 1);
	}
	return 0;
}

/**
 *
 */
static int t_cancel_callid(struct sip_msg* msg, char *cid, char *cseq, char *flag)
{
	struct cell *trans;
	struct cell *bkt;
	int bkb;
	struct cancel_info cancel_data;
	str cseq_s;
	str callid_s;
	int fl;

	fl = -1;

	if(fixup_get_svalue(msg, (gparam_p)cid, &callid_s)<0)
	{
		LM_ERR("cannot get callid\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)cseq, &cseq_s)<0)
	{
		LM_ERR("cannot get cseq\n");
		return -1;
	}

	if(fixup_get_ivalue(msg, (gparam_p)flag, &fl)<0)
	{
		LM_ERR("cannot get flag\n");
		return -1;
	}

	bkt = _tmx_tmb.t_gett();
	bkb = _tmx_tmb.t_gett_branch();
	if( _tmx_tmb.t_lookup_callid(&trans, callid_s, cseq_s) < 0 ) {
		DBG("Lookup failed - no transaction\n");
		return -1;
	}

	DBG("Now calling cancel_uacs\n");
	if(trans->uas.request && fl>0 && fl<32)
		setflag(trans->uas.request, fl);
	init_cancel_info(&cancel_data);
	cancel_data.cancel_bitmap = 0;
	_tmx_tmb.prepare_to_cancel(trans, &cancel_data.cancel_bitmap, 0);
	_tmx_tmb.cancel_uacs(trans, &cancel_data, 0);

	//_tmx_tmb.unref_cell(trans);
	_tmx_tmb.t_sett(bkt, bkb);

	return 1;
}

/**
 *
 */
static int fixup_reply_callid(void** param, int param_no)
{
	if (param_no==1 || param_no==2 || param_no==4) {
		return fixup_spve_null(param, 1);
	}
	if (param_no==3) {
		return fixup_igp_null(param, 1);
	}
	return 0;
}

/**
 *
 */
static int t_reply_callid(struct sip_msg* msg, char *cid, char *cseq,
		char *rc, char *rs)
{
	struct cell *trans;
	str cseq_s;
	str callid_s;
	str status_s;
	unsigned int code;

	if(fixup_get_svalue(msg, (gparam_p)cid, &callid_s)<0)
	{
		LM_ERR("cannot get callid\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)cseq, &cseq_s)<0)
	{
		LM_ERR("cannot get cseq\n");
		return -1;
	}

	if(fixup_get_ivalue(msg, (gparam_p)rc, (int*)&code)<0)
	{
		LM_ERR("cannot get reply code\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)rs, &status_s)<0)
	{
		LM_ERR("cannot get reply status\n");
		return -1;
	}

	if(_tmx_tmb.t_lookup_callid(&trans, callid_s, cseq_s) < 0 )
	{
		DBG("Lookup failed - no transaction\n");
		return -1;
	}

	DBG("Now calling internal replay\n");
	if(_tmx_tmb.t_reply_trans(trans, trans->uas.request, code, status_s.s)>0)
		return 1;

	return -1;
}

/**
 *
 */
static int t_flush_flags(struct sip_msg* msg, char *foo, char *bar)
{
	struct cell *t;

	t=_tmx_tmb.t_gett();
	if ( t==0 || t==T_UNDEFINED) {
		LM_ERR("failed to flush flags - no transaction found\n");
		return -1;
	}

	t->uas.request->flags = msg->flags;
	return 1;
}

/**
 *
 */
static int t_is_failure_route(struct sip_msg* msg, char *foo, char *bar)
{
	if(route_type==FAILURE_ROUTE)
		return 1;
	return -1;
}

/**
 *
 */
static int t_is_branch_route(struct sip_msg* msg, char *foo, char *bar)
{
	if(route_type==BRANCH_ROUTE)
		return 1;
	return -1;
}

/**
 *
 */
static int t_is_reply_route(struct sip_msg* msg, char *foo, char *bar)
{
	if(route_type & ONREPLY_ROUTE)
		return 1;
	return -1;
}

/**
 *
 */
static int w_t_suspend(struct sip_msg* msg, char *p1, char *p2)
{
	unsigned int tindex;
	unsigned int tlabel;
	tm_cell_t *t = 0;

	t=_tmx_tmb.t_gett();
	if (t==NULL || t==T_UNDEFINED)
	{
		if(_tmx_tmb.t_newtran(msg)<0)
		{
			LM_ERR("cannot create the transaction\n");
			return -1;
		}
		t = _tmx_tmb.t_gett();
		if (t==NULL || t==T_UNDEFINED)
		{
			LM_ERR("cannot lookup the transaction\n");
			return -1;
		}
	}
	if(_tmx_tmb.t_suspend(msg, &tindex, &tlabel)<0)
	{
		LM_ERR("failed to suppend the processing\n");
		return -1;
	}

	LM_DBG("transaction suspended [%u:%u]\n", tindex, tlabel);
	return 1;
}

/**
 *
 */
static int w_t_continue(struct sip_msg* msg, char *idx, char *lbl, char *rtn)
{
	unsigned int tindex;
	unsigned int tlabel;
	str rtname;
	cfg_action_t *act;
	int ri;

	if(fixup_get_ivalue(msg, (gparam_p)idx, (int*)&tindex)<0)
	{
		LM_ERR("cannot get transaction index\n");
		return -1;
	}

	if(fixup_get_ivalue(msg, (gparam_p)lbl, (int*)&tlabel)<0)
	{
		LM_ERR("cannot get transaction label\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)rtn, &rtname)<0)
	{
		LM_ERR("cannot get route block name\n");
		return -1;
	}

	ri = route_get(&main_rt, rtname.s);
	if(ri<0)
	{
		LM_ERR("unable to find route block [%.*s]\n", rtname.len, rtname.s);
		return -1;
	}
	act = main_rt.rlist[ri];
	if(act==NULL)
	{
		LM_ERR("empty action lists in route block [%.*s]\n",
				rtname.len, rtname.s);
		return -1;
	}

	if(_tmx_tmb.t_continue(tindex, tlabel, act)<0)
	{
		LM_ERR("resuming the processing of transaction [%u:%u] failed\n",
				tindex, tlabel);
		return -1;
	}
	return 1;
}

/**
 *
 */
static int fixup_t_continue(void** param, int param_no)
{
	if (param_no==1 || param_no==2) {
		return fixup_igp_null(param, 1);
	}
	if (param_no==3) {
		return fixup_spve_null(param, 1);
	}

	return 0;
}

static int bind_tmx(tmx_api_t* api)
{
	if (!api)
		return -1;

	api->t_suspend = w_t_suspend;
	return 0;
}

#ifdef STATISTICS

/*** tm stats ***/

static struct t_proc_stats _tmx_stats_all;
static ticks_t _tmx_stats_tm = 0;
void tmx_stats_update(void)
{
	ticks_t t;
	t = get_ticks();
	if(t!=_tmx_stats_tm) {
		_tmx_tmb.get_stats(&_tmx_stats_all);
		_tmx_stats_tm = t;
	}
}

unsigned long tmx_stats_uas_trans(void)
{
	tmx_stats_update();
	return _tmx_stats_all.transactions;
}

unsigned long tmx_stats_uac_trans(void)
{
	tmx_stats_update();
	return _tmx_stats_all.client_transactions;
}

unsigned long tmx_stats_trans_2xx(void)
{
	tmx_stats_update();
	return _tmx_stats_all.completed_2xx;
}

unsigned long tmx_stats_trans_3xx(void)
{
	tmx_stats_update();
	return _tmx_stats_all.completed_3xx;
}

unsigned long tmx_stats_trans_4xx(void)
{
	tmx_stats_update();
	return _tmx_stats_all.completed_4xx;
}

unsigned long tmx_stats_trans_5xx(void)
{
	tmx_stats_update();
	return _tmx_stats_all.completed_5xx;
}

unsigned long tmx_stats_trans_6xx(void)
{
	tmx_stats_update();
	return _tmx_stats_all.completed_6xx;
}

unsigned long tmx_stats_trans_inuse(void)
{
	tmx_stats_update();
	return (_tmx_stats_all.transactions - _tmx_stats_all.deleted);
}

unsigned long tmx_stats_trans_active(void)
{
	tmx_stats_update();
	return (_tmx_stats_all.transactions - _tmx_stats_all.waiting);
}

#if 0
unsigned long tmx_stats_rcv_rpls(void)
{
	tmx_stats_update();
	return 0;
}

unsigned long tmx_stats_rld_rpls(void)
{
	tmx_stats_update();
	return 0;
}
#endif

unsigned long tmx_stats_loc_rpls(void)
{
	tmx_stats_update();
	return _tmx_stats_all.replied_locally;
}

#endif
