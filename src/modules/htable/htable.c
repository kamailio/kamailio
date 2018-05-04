/**
 * Copyright (C) 2008-2016 Elena-Ramona Modroiu (asipto.com)
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
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../core/sr_module.h"
#include "../../core/timer.h"
#include "../../core/timer_proc.h"
#include "../../core/route.h"
#include "../../core/dprint.h"
#include "../../core/hashes.h"
#include "../../core/mod_fix.h"
#include "../../core/ut.h"
#include "../../core/rpc.h"
#include "../../core/rpc_lookup.h"
#include "../../core/kemi.h"
#include "../../core/fmsg.h"

#include "../../core/pvar.h"
#include "ht_api.h"
#include "ht_db.h"
#include "ht_var.h"
#include "api.h"
#include "ht_dmq.h"


MODULE_VERSION

int  ht_timer_interval = 20;
int  ht_db_expires_flag = 0;
int  ht_enable_dmq = 0;
int  ht_timer_procs = 0;
static int ht_event_callback_mode = 0;

str ht_event_callback = STR_NULL;

static int htable_init_rpc(void);

/** module functions */
static int ht_print(struct sip_msg*, char*, char*);
static int mod_init(void);
static int child_init(int rank);
static void destroy(void);

static int fixup_ht_key(void** param, int param_no);
static int ht_rm_name_re(struct sip_msg* msg, char* key, char* foo);
static int ht_rm_value_re(struct sip_msg* msg, char* key, char* foo);
static int w_ht_rm_name(struct sip_msg* msg, char* hname, char* op, char *val);
static int w_ht_rm_value(struct sip_msg* msg, char* hname, char* op, char *val);
static int w_ht_slot_lock(struct sip_msg* msg, char* key, char* foo);
static int w_ht_slot_unlock(struct sip_msg* msg, char* key, char* foo);
static int ht_reset(struct sip_msg* msg, char* htname, char* foo);
static int w_ht_iterator_start(struct sip_msg* msg, char* iname, char* hname);
static int w_ht_iterator_next(struct sip_msg* msg, char* iname, char* foo);
static int w_ht_iterator_end(struct sip_msg* msg, char* iname, char* foo);

int ht_param(modparam_t type, void* val);

static pv_export_t mod_pvs[] = {
	{ {"sht", sizeof("sht")-1}, PVT_OTHER, pv_get_ht_cell, pv_set_ht_cell,
		pv_parse_ht_name, 0, 0, 0 },
	{ {"shtex", sizeof("shtex")-1}, PVT_OTHER, pv_get_ht_cell_expire,
		pv_set_ht_cell_expire,
		pv_parse_ht_name, 0, 0, 0 },
	{ {"shtcn", sizeof("shtcn")-1}, PVT_OTHER, pv_get_ht_cn, 0,
		pv_parse_ht_name, 0, 0, 0 },
	{ {"shtcv", sizeof("shtcv")-1}, PVT_OTHER, pv_get_ht_cv, 0,
		pv_parse_ht_name, 0, 0, 0 },
	{ {"shtinc", sizeof("shtinc")-1}, PVT_OTHER, pv_get_ht_inc, 0,
		pv_parse_ht_name, 0, 0, 0 },
	{ {"shtdec", sizeof("shtdec")-1}, PVT_OTHER, pv_get_ht_dec, 0,
		pv_parse_ht_name, 0, 0, 0 },
	{ {"shtrecord", sizeof("shtrecord")-1}, PVT_OTHER, pv_get_ht_expired_cell, 0,
		pv_parse_ht_expired_cell, 0, 0, 0 },
	{ {"shtitkey", sizeof("shtitkey")-1}, PVT_OTHER, pv_get_iterator_key, 0,
		pv_parse_iterator_name, 0, 0, 0 },
	{ {"shtitval", sizeof("shtitval")-1}, PVT_OTHER, pv_get_iterator_val, 0,
		pv_parse_iterator_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


static cmd_export_t cmds[]={
	{"sht_print",       (cmd_function)ht_print,        0, 0, 0,
		ANY_ROUTE},
	{"sht_rm_name_re",  (cmd_function)ht_rm_name_re,   1, fixup_ht_key, 0,
		ANY_ROUTE},
	{"sht_rm_value_re", (cmd_function)ht_rm_value_re,  1, fixup_ht_key, 0,
		ANY_ROUTE},
	{"sht_rm_name",  (cmd_function)w_ht_rm_name,   3, fixup_spve_all, 0,
		ANY_ROUTE},
	{"sht_rm_value", (cmd_function)w_ht_rm_value,  3, fixup_spve_all, 0,
		ANY_ROUTE},
	{"sht_lock",        (cmd_function)w_ht_slot_lock,    1, fixup_ht_key, 0,
		ANY_ROUTE},
	{"sht_unlock",      (cmd_function)w_ht_slot_unlock,  1, fixup_ht_key, 0,
		ANY_ROUTE},
	{"sht_reset",		(cmd_function)ht_reset,		   1, fixup_spve_null, 0,
		ANY_ROUTE},
	{"sht_iterator_start",	(cmd_function)w_ht_iterator_start,	2, fixup_spve_spve, 0,
		ANY_ROUTE},
	{"sht_iterator_next",	(cmd_function)w_ht_iterator_next,	1, fixup_spve_null, 0,
		ANY_ROUTE},
	{"sht_iterator_end",	(cmd_function)w_ht_iterator_end,	1, fixup_spve_null, 0,
		ANY_ROUTE},
	{"bind_htable",     (cmd_function)bind_htable,     0, 0, 0,
		ANY_ROUTE},
	{0,0,0,0,0,0}
};

static param_export_t params[]={
	{"htable",              PARAM_STRING|USE_FUNC_PARAM, (void*)ht_param},
	{"db_url",              PARAM_STR, &ht_db_url},
	{"key_name_column",     PARAM_STR, &ht_db_name_column},
	{"key_type_column",     PARAM_STR, &ht_db_ktype_column},
	{"value_type_column",   PARAM_STR, &ht_db_vtype_column},
	{"key_value_column",    PARAM_STR, &ht_db_value_column},
	{"expires_column",      PARAM_STR, &ht_db_expires_column},
	{"array_size_suffix",   PARAM_STR, &ht_array_size_suffix},
	{"fetch_rows",          INT_PARAM, &ht_fetch_rows},
	{"timer_interval",      INT_PARAM, &ht_timer_interval},
	{"db_expires",          INT_PARAM, &ht_db_expires_flag},
	{"enable_dmq",          INT_PARAM, &ht_enable_dmq},
	{"timer_procs",         PARAM_INT, &ht_timer_procs},
	{"event_callback",      PARAM_STR, &ht_event_callback},
	{"event_callback_mode", PARAM_INT, &ht_event_callback_mode},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"htable",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,          /* exported statistics */
	0,          /* exported MI functions */
	mod_pvs,    /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	(destroy_function) destroy,
	child_init  /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	if(htable_init_rpc()!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}

	if(ht_init_tables()!=0)
		return -1;
	ht_db_init_params();

	if(ht_db_url.len>0)
	{
		if(ht_db_init_con()!=0)
			return -1;
		if(ht_db_open_con()!=0)
			return -1;
		if(ht_db_load_tables()!=0)
		{
			ht_db_close_con();
			return -1;
		}
		ht_db_close_con();
	}
	if(ht_has_autoexpire())
	{
		LM_DBG("starting auto-expire timer\n");
		if(ht_timer_interval<=0)
			ht_timer_interval = 20;
		if(ht_timer_procs<=0) {
			if(register_timer(ht_timer, 0, ht_timer_interval)<0)
			{
				LM_ERR("failed to register timer function\n");
				return -1;
			}
		} else {
			register_sync_timers(ht_timer_procs);
		}
	}

	if (ht_enable_dmq>0 && ht_dmq_initialize()!=0) {
		LM_ERR("failed to initialize dmq integration\n");
		return -1;
	}

	ht_iterator_init();

	return 0;
}

/**
 *
 */
static int child_init(int rank)
{
	struct sip_msg *fmsg;
	struct run_act_ctx ctx;
	int rtb, rt;
	int i;
	sr_kemi_eng_t *keng = NULL;
	str evname = str_init("htable:mod-init");

	LM_DBG("rank is (%d)\n", rank);

	if(rank==PROC_MAIN) {
		if(ht_has_autoexpire() && ht_timer_procs>0) {
			for(i=0; i<ht_timer_procs; i++) {
				if(fork_sync_timer(PROC_TIMER, "HTable Timer", 1 /*socks flag*/,
						ht_timer, (void*)(long)i, ht_timer_interval)<0) {
					LM_ERR("failed to start timer routine as process\n");
					return -1; /* error */
				}
			}
		}
	}

	if (ht_event_callback_mode==0 && rank!=PROC_INIT)
		return 0;

	if (ht_event_callback_mode==1 && rank!=PROC_SIPINIT)
		return 0;

	rt = -1;
	if(ht_event_callback.s==NULL || ht_event_callback.len<=0) {
		rt = route_lookup(&event_rt, evname.s);
		if(rt<0 || event_rt.rlist[rt]==NULL) {
			rt = -1;
		}
	} else {
		keng = sr_kemi_eng_get();
		if(keng==NULL) {
			LM_DBG("event callback (%s) set, but no cfg engine\n",
					ht_event_callback.s);
			goto done;
		}
	}
	if(rt>=0 || ht_event_callback.len>0) {
		LM_DBG("executing event_route[%s] (%d)\n", evname.s, rt);
		if(faked_msg_init()<0)
			return -1;
		fmsg = faked_msg_next();
		rtb = get_route_type();
		set_route_type(REQUEST_ROUTE);
		init_run_actions_ctx(&ctx);
		if(rt>=0) {
			run_top_route(event_rt.rlist[rt], fmsg, &ctx);
		} else {
			if(keng!=NULL) {
				if(keng->froute(fmsg, EVENT_ROUTE,
							&ht_event_callback, &evname)<0) {
					LM_ERR("error running event route kemi callback\n");
					return -1;
				}
			}
		}
		set_route_type(rtb);
		if(ctx.run_flags&DROP_R_F) {
			LM_ERR("exit due to 'drop' in event route\n");
			return -1;
		}
	}

done:
	return 0;
}

/**
 * destroy function
 */
static void destroy(void)
{
	/* sync back to db */
	if(ht_db_url.len>0)
	{
		if(ht_db_init_con()==0)
		{
			if(ht_db_open_con()==0)
			{
				ht_db_sync_tables();
				ht_db_close_con();
			}
		}
	}
	ht_destroy();
}

/**
 * print hash table content
 */
static int ht_print(struct sip_msg *msg, char *s1, char *s2)
{
	ht_dbg();
	return 1;
}

static int fixup_ht_key(void** param, int param_no)
{
	pv_spec_t *sp;
	str s;

	if(param_no != 1)
	{
		LM_ERR("invalid parameter number %d\n", param_no);
		return -1;
	}
	sp = (pv_spec_t*)pkg_malloc(sizeof(pv_spec_t));
	if (sp == 0)
	{
		LM_ERR("no pkg memory left\n");
		return -1;
	}
	memset(sp, 0, sizeof(pv_spec_t));
	s.s = (char*)*param; s.len = strlen(s.s);
	if(pv_parse_ht_name(sp, &s)<0)
	{
		pkg_free(sp);
		LM_ERR("invalid parameter %d\n", param_no);
		return -1;
	}
	*param = (void*)sp;
	return 0;
}

static int ht_rm_name_re(struct sip_msg* msg, char* key, char* foo)
{
	ht_pv_t *hpv;
	str sre;
	pv_spec_t *sp;
	sp = (pv_spec_t*)key;
	int_str isval;

	hpv = (ht_pv_t*)sp->pvp.pvn.u.dname;

	if(hpv->ht==NULL)
	{
		hpv->ht = ht_get_table(&hpv->htname);
		if(hpv->ht==NULL)
			return 1;
	}
	if(pv_printf_s(msg, hpv->pve, &sre)!=0)
	{
		LM_ERR("cannot get $sht expression\n");
		return -1;
	}
	if (hpv->ht->dmqreplicate>0) {
		isval.s = sre;
		if (ht_dmq_replicate_action(HT_DMQ_RM_CELL_RE, &hpv->htname, NULL, AVP_VAL_STR, &isval, 0)!=0) {
			LM_ERR("dmq relication failed\n");
		}
	}
	if(ht_rm_cell_re(&sre, hpv->ht, 0)<0)
		return -1;
	return 1;
}

static int ht_rm_value_re(struct sip_msg* msg, char* key, char* foo)
{
	ht_pv_t *hpv;
	str sre;
	pv_spec_t *sp;
	sp = (pv_spec_t*)key;
	int_str isval;

	hpv = (ht_pv_t*)sp->pvp.pvn.u.dname;

	if(hpv->ht==NULL)
	{
		hpv->ht = ht_get_table(&hpv->htname);
		if(hpv->ht==NULL)
			return 1;
	}
	if(pv_printf_s(msg, hpv->pve, &sre)!=0)
	{
		LM_ERR("cannot get $sht expression\n");
		return -1;
	}

	if (hpv->ht->dmqreplicate>0) {
		isval.s = sre;
		if (ht_dmq_replicate_action(HT_DMQ_RM_CELL_RE, &hpv->htname, NULL, AVP_VAL_STR, &isval, 1)!=0) {
			LM_ERR("dmq relication failed\n");
		}
	}
	if(ht_rm_cell_re(&sre, hpv->ht, 1)<0)
		return -1;
	return 1;
}

static int ht_rm_items(sip_msg_t* msg, str* hname, str* op, str *val,
		int mkey)
{
	ht_t *ht;
	int_str isval;

	ht = ht_get_table(hname);
	if(ht==NULL) {
		LM_ERR("cannot get hash table [%.*s]\n", hname->len, hname->s);
		return -1;
	}

	switch(op->len) {
		case 2:
			if(strncmp(op->s, "re", 2)==0) {
				isval.s = *val;
				if (ht_dmq_replicate_action(HT_DMQ_RM_CELL_RE, &ht->name, NULL,
							AVP_VAL_STR, &isval, mkey)!=0) {
					LM_ERR("dmq relication failed (op %d)\n", mkey);
				}
				if(ht_rm_cell_re(val, ht, mkey)<0) {
					return -1;
				}
				return 1;
			} else if(strncmp(op->s, "sw", 2)==0) {
				if(ht_rm_cell_op(val, ht, mkey, HT_RM_OP_SW)<0) {
					return -1;
				}
			}
			LM_WARN("unsupported match operator: %.*s\n", op->len, op->s);
			break;
		default:
			LM_WARN("unsupported match operator: %.*s\n", op->len, op->s);
	}
	return -1;
}

static int w_ht_rm_items(sip_msg_t* msg, char* hname, char* op, char *val,
		int mkey)
{
	str sname;
	str sop;
	str sval;

	if(fixup_get_svalue(msg, (gparam_t*)hname, &sname)<0 || sname.len<=0) {
		LM_ERR("cannot get the hash table name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)op, &sop)<0 || sop.len<=0) {
		LM_ERR("cannot get the match operation\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)val, &sval)<0 || sval.len<=0) {
		LM_ERR("cannot the get match value\n");
		return -1;
	}

	return ht_rm_items(msg, &sname, &sop, &sval, mkey);
}

static int w_ht_rm_name(sip_msg_t* msg, char* hname, char* op, char *val)
{
	return w_ht_rm_items(msg, hname, op, val, 0);
}

static int w_ht_rm_value(sip_msg_t* msg, char* hname, char* op, char *val)
{
	return w_ht_rm_items(msg, hname, op, val, 1);
}

static int ht_reset_by_name(str *hname)
{
	ht_t *ht;
	ht = ht_get_table(hname);
	if(ht==NULL) {
		LM_ERR("cannot get hash table [%.*s]\n", hname->len, hname->s);
		return -1;
	}
	if(ht_reset_content(ht)<0)
		return -1;
	return 0;

}

static int ht_reset(struct sip_msg* msg, char* htname, char* foo)
{
	str sname;

	if(fixup_get_svalue(msg, (gparam_t*)htname, &sname)<0 || sname.len<=0)
	{
		LM_ERR("cannot get hash table name\n");
		return -1;
	}
	if(ht_reset_by_name(&sname)<0) {
		return -1;
	}

	return 1;
}

static int w_ht_iterator_start(struct sip_msg* msg, char* iname, char* hname)
{
	str siname;
	str shname;

	if(fixup_get_svalue(msg, (gparam_t*)iname, &siname)<0 || siname.len<=0)
	{
		LM_ERR("cannot get iterator name\n");
		return -1;
	}
	if(fixup_get_svalue(msg, (gparam_t*)hname, &shname)<0 || shname.len<=0)
	{
		LM_ERR("cannot get hash table name\n");
		return -1;
	}

	if(ht_iterator_start(&siname, &shname)<0)
		return -1;
	return 1;
}

static int ki_ht_iterator_start(sip_msg_t *msg, str *iname, str *hname)
{
	if(iname==NULL || iname->s==NULL || iname->len<=0
			|| hname==NULL || hname->s==NULL || hname->len<=0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}
	if(ht_iterator_start(iname, hname)<0)
		return -1;
	return 1;
}

static int w_ht_iterator_next(struct sip_msg* msg, char* iname, char* foo)
{
	str siname;

	if(fixup_get_svalue(msg, (gparam_t*)iname, &siname)<0 || siname.len<=0)
	{
		LM_ERR("cannot get iterator name\n");
		return -1;
	}
	if(ht_iterator_next(&siname)<0)
		return -1;
	return 1;
}

static int ki_ht_iterator_next(sip_msg_t *msg, str *iname)
{
	if(iname==NULL || iname->s==NULL || iname->len<=0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}
	if(ht_iterator_next(iname)<0)
		return -1;
	return 1;
}

static int w_ht_iterator_end(struct sip_msg* msg, char* iname, char* foo)
{
	str siname;

	if(fixup_get_svalue(msg, (gparam_t*)iname, &siname)<0 || siname.len<=0)
	{
		LM_ERR("cannot get iterator name\n");
		return -1;
	}
	if(ht_iterator_end(&siname)<0)
		return -1;
	return 1;
}

static int ki_ht_iterator_end(sip_msg_t *msg, str *iname)
{
	if(iname==NULL || iname->s==NULL || iname->len<=0) {
		LM_ERR("invalid parameters\n");
		return -1;
	}
	if(ht_iterator_end(iname)<0)
		return -1;
	return 1;
}

static int ki_ht_slot_xlock(sip_msg_t *msg, str *htname, str *skey, int lmode)
{
	ht_t *ht;
	unsigned int hid;
	unsigned int idx;

	ht = ht_get_table(htname);
	if(ht==NULL) {
		LM_ERR("cannot get hash table by name [%.*s] (%d)\n",
				htname->len, htname->s, lmode);
		return -1;
	}

	hid = ht_compute_hash(skey);

	idx = ht_get_entry(hid, ht->htsize);

	if(lmode==0) {
		LM_DBG("locking slot %.*s[%u] for key %.*s\n",
				htname->len, htname->s,
				idx, skey->len, skey->s);
		ht_slot_lock(ht, idx);
	} else {
		LM_DBG("unlocking slot %.*s[%u] for key %.*s\n",
				htname->len, htname->s,
				idx, skey->len, skey->s);
		ht_slot_unlock(ht, idx);
	}
	return 1;
}

static int ki_ht_slot_lock(sip_msg_t *msg, str *htname, str *skey)
{
	return ki_ht_slot_xlock(msg, htname, skey, 0);
}

static int ki_ht_slot_unlock(sip_msg_t *msg, str *htname, str *skey)
{
	return ki_ht_slot_xlock(msg, htname, skey, 1);
}

/**
 * lock the slot for a given key in a hash table
 */
static int w_ht_slot_lock(struct sip_msg* msg, char* key, char* foo)
{
	ht_pv_t *hpv;
	str skey;
	pv_spec_t *sp;
	unsigned int hid;
	unsigned int idx;

	sp = (pv_spec_t*)key;

	hpv = (ht_pv_t*)sp->pvp.pvn.u.dname;

	if(hpv->ht==NULL)
	{
		hpv->ht = ht_get_table(&hpv->htname);
		if(hpv->ht==NULL) {
			LM_ERR("cannot get $sht root\n");
			return -11;
		}
	}
	if(pv_printf_s(msg, hpv->pve, &skey)!=0)
	{
		LM_ERR("cannot get $sht key\n");
		return -1;
	}

	hid = ht_compute_hash(&skey);

	idx = ht_get_entry(hid, hpv->ht->htsize);

	LM_DBG("locking slot %.*s[%u] for key %.*s\n",
			hpv->htname.len, hpv->htname.s,
			idx, skey.len, skey.s);

	ht_slot_lock(hpv->ht, idx);

	return 1;
}

/**
 * unlock the slot for a given key in a hash table
 */
static int w_ht_slot_unlock(struct sip_msg* msg, char* key, char* foo)
{
	ht_pv_t *hpv;
	str skey;
	pv_spec_t *sp;
	unsigned int hid;
	unsigned int idx;

	sp = (pv_spec_t*)key;

	hpv = (ht_pv_t*)sp->pvp.pvn.u.dname;

	if(hpv->ht==NULL)
	{
		hpv->ht = ht_get_table(&hpv->htname);
		if(hpv->ht==NULL) {
			LM_ERR("cannot get $sht root\n");
			return -11;
		}
	}
	if(pv_printf_s(msg, hpv->pve, &skey)!=0)
	{
		LM_ERR("cannot get $sht key\n");
		return -1;
	}

	hid = ht_compute_hash(&skey);

	idx = ht_get_entry(hid, hpv->ht->htsize);

	LM_DBG("unlocking slot %.*s[%u] for key %.*s\n",
			hpv->htname.len, hpv->htname.s,
			idx, skey.len, skey.s);

	ht_slot_unlock(hpv->ht, idx);

	return 1;
}

int ht_param(modparam_t type, void *val)
{
	if(val==NULL)
		goto error;

	return ht_table_spec((char*)val);
error:
	return -1;

}

#define RPC_DATE_BUF_LEN 21

static const char* htable_dump_doc[2] = {
	"Dump the contents of hash table.",
	0
};
static const char* htable_delete_doc[2] = {
	"Delete one key from a hash table.",
	0
};
static const char* htable_get_doc[2] = {
	"Get one key from a hash table.",
	0
};
static const char* htable_sets_doc[2] = {
	"Set one key in a hash table to a string value.",
	0
};
static const char* htable_seti_doc[2] = {
	"Set one key in a hash table to an integer value.",
	0
};
static const char* htable_list_doc[2] = {
	"List all htables.",
	0
};
static const char* htable_stats_doc[2] = {
	"Statistics about htables.",
	0
};
static const char* htable_reload_doc[2] = {
	"Reload hash table.",
	0
};

static void htable_rpc_delete(rpc_t* rpc, void* c) {
	str htname, keyname;
	ht_t *ht;

	if (rpc->scan(c, "SS", &htname, &keyname) < 2) {
		rpc->fault(c, 500, "Not enough parameters (htable name & key name");
		return;
	}
	ht = ht_get_table(&htname);
	if (!ht) {
		rpc->fault(c, 500, "No such htable");
		return;
	}

	if (ht->dmqreplicate>0 && ht_dmq_replicate_action(HT_DMQ_DEL_CELL, &ht->name, &keyname, 0, NULL, 0)!=0) {
		LM_ERR("dmq relication failed\n");
	}

	ht_del_cell(ht, &keyname);
}

/*! \brief RPC htable.get command to get one item */
static void htable_rpc_get(rpc_t* rpc, void* c) {
	str htname, keyname;
	ht_t *ht;
	ht_cell_t *htc;	/*!< One HT cell */
	void* th;
	void* vh;
	struct tm *_expire_t;
	char expire_buf[RPC_DATE_BUF_LEN]="NEVER";

	if (rpc->scan(c, "SS", &htname, &keyname) < 2) {
		rpc->fault(c, 500, "Not enough parameters (htable name and key name)");
		return;
	}

	/* Find the htable */
	ht = ht_get_table(&htname);
	if (!ht) {
		rpc->fault(c, 500, "No such htable");
		return;
	}

	/* Find the  cell */
	htc = ht_cell_pkg_copy(ht, &keyname, NULL);
	if(htc == NULL) {
		/* Print error message */
		rpc->fault(c, 500, "Key name doesn't exist in htable.");
		return;
	}

	/* add entry node */
	if (rpc->add(c, "{", &th) < 0) {
		rpc->fault(c, 500, "Internal error creating rpc");
		goto error;
	}

	if(rpc->struct_add(th, "{", "item", &vh)<0) {
		rpc->fault(c, 500, "Internal error creating rpc");
		goto error;
	}

	if (htc->expire) {
		_expire_t = localtime(&htc->expire);
		strftime(expire_buf, RPC_DATE_BUF_LEN - 1,
			"%Y-%m-%d %H:%M:%S", _expire_t);
	}

	if(htc->flags&AVP_VAL_STR) {
		if(rpc->struct_add(vh, "SSds", "name",  &htc->name.s,
							"value", &htc->value.s,
							"flags", htc->flags,
							"expire", expire_buf)<0)
		{
			rpc->fault(c, 500, "Internal error adding item");
			goto error;
		}
	} else {
		if(rpc->struct_add(vh, "Sdds", "name",  &htc->name.s,
							"value", (int)htc->value.n,
							"flags", htc->flags,
							"expire", expire_buf)<0)
		{
			rpc->fault(c, 500, "Internal error adding item");
			goto error;
		}
	}

error:
	/* Release the allocated memory */
	ht_cell_pkg_free(htc);

	return;
}

/*! \brief RPC htable.sets command to set one item to string value */
static void htable_rpc_sets(rpc_t* rpc, void* c) {
	str htname, keyname;
	int_str keyvalue;
	ht_t *ht;

	if (rpc->scan(c, "SS.S", &htname, &keyname, &keyvalue.s) < 3) {
		rpc->fault(c, 500,
				"Not enough parameters (htable name, key name and value)");
		return;
	}

	/* Find the htable */
	ht = ht_get_table(&htname);
	if (!ht) {
		rpc->fault(c, 500, "No such htable");
		return;
	}

	if (ht->dmqreplicate>0 && ht_dmq_replicate_action(HT_DMQ_SET_CELL,
				&ht->name, &keyname, AVP_VAL_STR, &keyvalue, 1)!=0) {
		LM_ERR("dmq relication failed\n");
	}

	if(ht_set_cell(ht, &keyname, AVP_VAL_STR, &keyvalue, 1)!=0)
	{
		LM_ERR("cannot set $sht(%.*s=>%.*s)\n", htname.len, htname.s,
				keyname.len, keyname.s);
		rpc->fault(c, 500, "Failed to set the item");
		return;
	}

	return;
}

/*! \brief RPC htable.seti command to set one item to integer value */
static void htable_rpc_seti(rpc_t* rpc, void* c) {
	str htname, keyname;
	int_str keyvalue;
	ht_t *ht;

	if (rpc->scan(c, "SS.d", &htname, &keyname, &keyvalue.n) < 3) {
		rpc->fault(c, 500,
				"Not enough parameters (htable name, key name and value)");
		return;
	}

	/* Find the htable */
	ht = ht_get_table(&htname);
	if (!ht) {
		rpc->fault(c, 500, "No such htable");
		return;
	}

	if (ht->dmqreplicate>0 && ht_dmq_replicate_action(HT_DMQ_SET_CELL,
				&ht->name, &keyname, 0, &keyvalue, 1)!=0) {
		LM_ERR("dmq relication failed\n");
	}

	if(ht_set_cell(ht, &keyname, 0, &keyvalue, 1)!=0)
	{
		LM_ERR("cannot set $sht(%.*s=>%.*s)\n", htname.len, htname.s,
				keyname.len, keyname.s);
		rpc->fault(c, 500, "Failed to set the item");
		return;
	}

	return;
}

/*! \brief RPC htable.dump command to print content of a hash table */
static void  htable_rpc_dump(rpc_t* rpc, void* c)
{
	str htname;
	ht_t *ht;
	ht_cell_t *it;
	int i;
	void* th;
	void* ih;
	void* vh;

	if (rpc->scan(c, "S", &htname) < 1)
	{
		rpc->fault(c, 500, "No htable name given");
		return;
	}
	ht = ht_get_table(&htname);
	if(ht==NULL)
	{
		rpc->fault(c, 500, "No such htable");
		return;
	}
	for(i=0; i<ht->htsize; i++)
	{
		ht_slot_lock(ht, i);
		it = ht->entries[i].first;
		if(it)
		{
			/* add entry node */
			if (rpc->add(c, "{", &th) < 0)
			{
				rpc->fault(c, 500, "Internal error creating rpc");
				goto error;
			}
			if(rpc->struct_add(th, "dd[",
							"entry", i,
							"size",  (int)ht->entries[i].esize,
							"slot",  &ih)<0)
			{
				rpc->fault(c, 500, "Internal error creating rpc");
				goto error;
			}
			while(it)
			{
				if(rpc->array_add(ih, "{", &vh)<0)
				{
					rpc->fault(c, 500, "Internal error creating rpc");
					goto error;
				}
				if(it->flags&AVP_VAL_STR) {
					if(rpc->struct_add(vh, "SSs",
							"name",  &it->name.s,
							"value", &it->value.s,
							"type", "str")<0)
					{
						rpc->fault(c, 500, "Internal error adding item");
						goto error;
					}
				} else {
					if(rpc->struct_add(vh, "Sds",
							"name",  &it->name.s,
							"value", (int)it->value.n,
							"type", "int")<0)
					{
						rpc->fault(c, 500, "Internal error adding item");
						goto error;
					}
				}
				it = it->next;
			}
		}
		ht_slot_unlock(ht, i);
	}

	return;

error:
	ht_slot_unlock(ht, i);
}

static void  htable_rpc_list(rpc_t* rpc, void* c)
{
	ht_t *ht;
	void* th;
	char dbname[128];

	ht = ht_get_root();
	if(ht==NULL)
	{
		rpc->fault(c, 500, "No htables");
		return;
	}
	while (ht != NULL)
	{
		int len = 0;
		/* add entry node */
		if (rpc->add(c, "{", &th) < 0)
		{
			rpc->fault(c, 500, "Internal error creating structure rpc");
			goto error;
		}
		if (ht->dbtable.len > 0) {
			len = ht->dbtable.len > 127 ? 127 : ht->dbtable.len;
			memcpy(dbname, ht->dbtable.s, len);
			dbname[len] = '\0';
		} else {
			dbname[0] = '\0';
		}

		if(rpc->struct_add(th, "Ssddddd",
						"name", &ht->name,	/* String */
						"dbtable", &dbname ,	/* Char * */
						"dbmode", (int)  ht->dbmode,		/* u int */
						"expire", (int) ht->htexpire,		/* u int */
						"updateexpire", ht->updateexpire,	/* int */
						"size", (int) ht->htsize,			/* u int */
						"dmqreplicate", ht->dmqreplicate	/* int */
						) < 0) {
			rpc->fault(c, 500, "Internal error creating data rpc");
			goto error;
		}
		ht = ht->next;
	}

error:
	return;
}

static void  htable_rpc_stats(rpc_t* rpc, void* c)
{
	ht_t *ht;
	void* th;
	unsigned int min;
	unsigned int max;
	unsigned int all;
	unsigned int i;

	ht = ht_get_root();
	if(ht==NULL)
	{
		rpc->fault(c, 500, "No htables");
		return;
	}
	while (ht != NULL)
	{
		/* add entry node */
		if (rpc->add(c, "{", &th) < 0)
		{
			rpc->fault(c, 500, "Internal error creating structure rpc");
			goto error;
		}
		all = 0;
		max = 0;
		min = 4294967295U;
		for(i=0; i<ht->htsize; i++) {
			ht_slot_lock(ht, i);
			if(ht->entries[i].esize<min)
				min = ht->entries[i].esize;
			if(ht->entries[i].esize>max)
				max = ht->entries[i].esize;
			all += ht->entries[i].esize;
			ht_slot_unlock(ht, i);
		}

		if(rpc->struct_add(th, "Sdddd",
						"name", &ht->name,	/* str */
						"slots", (int)ht->htsize,	/* uint */
						"all", (int)all,	/* uint */
						"min", (int)min,	/* uint */
						"max", (int)max		/* uint */
						) < 0) {
			rpc->fault(c, 500, "Internal error creating rpc structure");
			goto error;
		}
		ht = ht->next;
	}

error:
	return;
}


/*! \brief RPC htable.reload command to reload content of a hash table */
static void htable_rpc_reload(rpc_t* rpc, void* c)
{
	str htname;
	ht_t *ht;
	ht_t nht;
	ht_cell_t *first;
	ht_cell_t *it;
	int i;

	if(ht_db_url.len<=0) {
		rpc->fault(c, 500, "No htable db_url");
		return;
	}
	if(ht_db_init_con()!=0) {
		rpc->fault(c, 500, "Failed to init htable db connection");
		return;
	}
	if(ht_db_open_con()!=0) {
		rpc->fault(c, 500, "Failed to open htable db connection");
		return;
	}

	if (rpc->scan(c, "S", &htname) < 1)
	{
		ht_db_close_con();
		rpc->fault(c, 500, "No htable name given");
		return;
	}
	ht = ht_get_table(&htname);
	if(ht==NULL)
	{
		ht_db_close_con();
		rpc->fault(c, 500, "No such htable");
		return;
	}


	memcpy(&nht, ht, sizeof(ht_t));
	/* it's temporary operation - use system malloc */
	nht.entries = (ht_entry_t*)malloc(nht.htsize*sizeof(ht_entry_t));
	if(nht.entries == NULL)
	{
		ht_db_close_con();
		rpc->fault(c, 500, "Mtree reload failed");
		return;
	}
	memset(nht.entries, 0, nht.htsize*sizeof(ht_entry_t));

	if(ht_db_load_table(&nht, &ht->dbtable, 0)<0)
	{
		/* free any entry set if it was a partial load */
		for(i=0; i<nht.htsize; i++)
		{
			first = nht.entries[i].first;
			while(first)
			{
				it = first;
				first = first->next;
				ht_cell_free(it);
			}
		}
		free(nht.entries);
		ht_db_close_con();
		rpc->fault(c, 500, "Mtree reload failed");
		return;
	}

	/* replace old entries */
	for(i=0; i<nht.htsize; i++)
	{
		ht_slot_lock(ht, i);
		first = ht->entries[i].first;
		ht->entries[i].first = nht.entries[i].first;
		ht->entries[i].esize = nht.entries[i].esize;
		ht_slot_unlock(ht, i);
		nht.entries[i].first = first;
	}
	/* free old entries */
	for(i=0; i<nht.htsize; i++)
	{
		first = nht.entries[i].first;
		while(first)
		{
			it = first;
			first = first->next;
			ht_cell_free(it);
		}
	}
	free(nht.entries);
	ht_db_close_con();
	return;
}

rpc_export_t htable_rpc[] = {
	{"htable.dump", htable_rpc_dump, htable_dump_doc, RET_ARRAY},
	{"htable.delete", htable_rpc_delete, htable_delete_doc, 0},
	{"htable.get", htable_rpc_get, htable_get_doc, 0},
	{"htable.sets", htable_rpc_sets, htable_sets_doc, 0},
	{"htable.seti", htable_rpc_seti, htable_seti_doc, 0},
	{"htable.listTables", htable_rpc_list, htable_list_doc, RET_ARRAY},
	{"htable.reload", htable_rpc_reload, htable_reload_doc, 0},
	{"htable.stats", htable_rpc_stats, htable_stats_doc, RET_ARRAY},
	{0, 0, 0, 0}
};

static int htable_init_rpc(void)
{
	if (rpc_register_array(htable_rpc)!=0)
	{
		LM_ERR("failed to register RPC commands\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
static sr_kemi_t sr_kemi_htable_exports[] = {
	{ str_init("htable"), str_init("sht_lock"),
		SR_KEMIP_INT, ki_ht_slot_lock,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("htable"), str_init("sht_unlock"),
		SR_KEMIP_INT, ki_ht_slot_unlock,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("htable"), str_init("sht_reset"),
		SR_KEMIP_INT, ht_reset_by_name,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("htable"), str_init("sht_iterator_start"),
		SR_KEMIP_INT, ki_ht_iterator_start,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("htable"), str_init("sht_iterator_next"),
		SR_KEMIP_INT, ki_ht_iterator_next,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("htable"), str_init("sht_iterator_end"),
		SR_KEMIP_INT, ki_ht_iterator_end,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_htable_exports);
	return 0;
}
