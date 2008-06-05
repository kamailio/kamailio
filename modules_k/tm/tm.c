/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2003-02-18  added t_forward_nonack_{udp, tcp}, t_relay_to_{udp,tcp},
 *               t_replicate_{udp, tcp} (andrei)
 *  2003-02-19  added t_rely_{udp, tcp} (andrei)
 *  2003-03-06  voicemail changes accepted (jiri)
 *  2003-03-10  module export interface updated to the new format (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-19  replaced all mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-03-30  set_kr for requests only (jiri)
 *  2003-04-05  s/reply_route/failure_route, onreply_route introduced (jiri)
 *  2003-04-14  use protocol from uri (jiri)
 *  2003-07-07  added t_relay_to_tls, t_replicate_tls, t_forward_nonack_tls
 *              added #ifdef USE_TCP, USE_TLS
 *              removed t_relay_{udp,tcp,tls} (andrei)
 *  2003-09-26  added t_forward_nonack_uri() - same as t_forward_nonack() but
 *              takes no parameters -> forwards to uri (bogdan)
 *  2004-02-11  FIFO/CANCEL + alignments (hash=f(callid,cseq)) (uli+jiri)
 *  2004-02-18  t_reply exported via FIFO - imported from VM (bogdan)
 *  2004-10-01  added a new param.: restart_fr_on_each_reply (andrei)
 *  2005-05-30  light version of tm_load - find_export dropped -> module 
 *              interface dosen't need to export internal functions (bogdan)
 *  2006-01-15  merged functions which diff only via proto (like t_relay,
 *              t_replicate and t_forward_nonack) (bogdan)
 *  2007-01-25  t_forward_nonack removed as it merged into t_relay,
 *              t_replicate also accepts flags for controlling DNS failover
 *              (bogdan)
 *  2008-04-04  added support for local and remote dispaly name in TM dialogs
 *              (by Andrei Pisau <andrei.pisau at voice-system dot ro> )
 */


#include <stdio.h>
#include <string.h>
#include <netdb.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../script_cb.h"
#include "../../mi/mi.h"
#include "../../usr_avp.h"
#include "../../mem/mem.h"
#include "../../pvar.h"
#include "../../mod_fix.h"

#include "sip_msg.h"
#include "h_table.h"
#include "ut.h"
#include "t_reply.h"
#include "t_fwd.h"
#include "t_lookup.h"
#include "callid.h"
#include "t_cancel.h"
#include "t_fifo.h"
#include "mi.h"
#include "tm_load.h"

MODULE_VERSION

/* item functions */
static int pv_get_tm_branch_idx(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);
static int pv_get_tm_reply_code(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res);

/* fixup functions */
static int fixup_t_send_reply(void** param, int param_no);
static int fixup_local_replied(void** param, int param_no);
static int fixup_t_relay1(void** param, int param_no);
static int fixup_t_relay2(void** param, int param_no);
static int fixup_t_replicate(void** param, int param_no);


/* init functions */
static int mod_init(void);
static int child_init(int rank);


/* exported functions */
inline static int w_t_release(struct sip_msg* msg, char* , char* );
inline static int w_t_newtran(struct sip_msg* p_msg, char* , char* );
inline static int w_t_reply(struct sip_msg *msg, char* code, char* text);
inline static int w_pv_t_reply(struct sip_msg *msg, char* code, char* text);
inline static int w_t_relay(struct sip_msg *p_msg , char *proxy, char* flags);
inline static int w_t_replicate(struct sip_msg *p_msg, char *dst,char* );
inline static int w_t_on_negative(struct sip_msg* msg, char *go_to, char* );
inline static int w_t_on_reply(struct sip_msg* msg, char *go_to, char* );
inline static int w_t_on_branch(struct sip_msg* msg, char *go_to, char* );
inline static int t_check_status(struct sip_msg* msg, char *regexp, char* );
inline static int t_flush_flags(struct sip_msg* msg, char*, char* );
inline static int t_local_replied(struct sip_msg* msg, char *type, char* );
inline static int t_check_trans(struct sip_msg* msg, char* , char* );
inline static int t_was_cancelled(struct sip_msg* msg, char* , char* );


/* strings with avp definition */
static char *fr_timer_param = NULL;
static char *fr_inv_timer_param = NULL;

/* module parameteres */
int tm_enable_stats = 1;

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


static cmd_export_t cmds[]={
	{"t_newtran",       (cmd_function)w_t_newtran,      0, 0,
			0, REQUEST_ROUTE},
	{"t_reply",         (cmd_function)w_pv_t_reply,     2, fixup_t_send_reply,
			0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"t_release",       (cmd_function)w_t_release,      0, 0,
			0, REQUEST_ROUTE},
	{"t_replicate",     (cmd_function)w_t_replicate,    1, fixup_t_replicate,
			0, REQUEST_ROUTE},
	{"t_replicate",     (cmd_function)w_t_replicate,    2, fixup_t_replicate,
			0, REQUEST_ROUTE},
	{"t_relay",         (cmd_function)w_t_relay,        0, 0,
			0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"t_relay",         (cmd_function)w_t_relay,        1, fixup_t_relay1,
			0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"t_relay",         (cmd_function)w_t_relay,        2, fixup_t_relay2,
			0, REQUEST_ROUTE | FAILURE_ROUTE },
	{"t_on_failure",    (cmd_function)w_t_on_negative,  1, fixup_uint_null,
			0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE },
	{"t_on_reply",      (cmd_function)w_t_on_reply,     1, fixup_uint_null,
			0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE },
	{"t_on_branch",     (cmd_function)w_t_on_branch,    1, fixup_uint_null,
			0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE },
	{"t_check_status",  (cmd_function)t_check_status,   1, fixup_regexp_null,
			0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE },
	{"t_write_req",     (cmd_function)t_write_req,      2, fixup_t_write,
			0, REQUEST_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE },
	{"t_write_unix",    (cmd_function)t_write_unix,     2, fixup_t_write,
			0, REQUEST_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE },
	{"t_flush_flags",   (cmd_function)t_flush_flags,    0, 0,
			0, REQUEST_ROUTE | BRANCH_ROUTE  },
	{"t_local_replied", (cmd_function)t_local_replied,  1, fixup_local_replied,
			0, REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE | BRANCH_ROUTE },
	{"t_check_trans",   (cmd_function)t_check_trans,    0, 0,
			0, REQUEST_ROUTE | BRANCH_ROUTE },
	{"t_was_cancelled", (cmd_function)t_was_cancelled,  0, 0,
			0, FAILURE_ROUTE | ONREPLY_ROUTE },
	{"load_tm",         (cmd_function)load_tm,          0, 0,
			0, 0},
	{0,0,0,0,0,0}
};


static param_export_t params[]={
	{"ruri_matching",             INT_PARAM,
		&ruri_matching},
	{"via1_matching",             INT_PARAM,
		&via1_matching},
	{"fr_timer",                  INT_PARAM,
		&(timer_id2timeout[FR_TIMER_LIST])},
	{"fr_inv_timer",              INT_PARAM,
		&(timer_id2timeout[FR_INV_TIMER_LIST])},
	{"wt_timer",                  INT_PARAM,
		&(timer_id2timeout[WT_TIMER_LIST])},
	{"delete_timer",              INT_PARAM,
		&(timer_id2timeout[DELETE_LIST])},
	{"T1_timer",                  INT_PARAM,
		&(timer_id2timeout[RT_T1_TO_1])},
	{"T2_timer",                  INT_PARAM,
		&(timer_id2timeout[RT_T2])},
	{"unix_tx_timeout",           INT_PARAM,
		&tm_unix_tx_timeout},
	{"restart_fr_on_each_reply",  INT_PARAM,
		&restart_fr_on_each_reply},
	{"fr_timer_avp",              STR_PARAM,
		&fr_timer_param},
	{"fr_inv_timer_avp",          STR_PARAM,
		&fr_inv_timer_param},
	{"tw_append",                 STR_PARAM|USE_FUNC_PARAM,
		(void*)parse_tw_append },
	{ "enable_stats",             INT_PARAM,
		&tm_enable_stats },
	{ "pass_provisional_replies", INT_PARAM,
		&pass_provisional_replies },
	{ "syn_branch",               INT_PARAM,
		&syn_branch },
	{ "onreply_avp_mode",         INT_PARAM,
		&onreply_avp_mode },
	{ "disable_6xx_block",        INT_PARAM,
		&disable_6xx_block },
	{0,0,0}
};


static stat_export_t mod_stats[] = {
	{"received_replies" ,    0,              &tm_rcv_rpls    },
	{"relayed_replies" ,     0,              &tm_rld_rpls    },
	{"local_replies" ,       0,              &tm_loc_rpls    },
	{"UAS_transactions" ,    0,              &tm_uas_trans   },
	{"UAC_transactions" ,    0,              &tm_uac_trans   },
	{"2xx_transactions" ,    0,              &tm_trans_2xx   },
	{"3xx_transactions" ,    0,              &tm_trans_3xx   },
	{"4xx_transactions" ,    0,              &tm_trans_4xx   },
	{"5xx_transactions" ,    0,              &tm_trans_5xx   },
	{"6xx_transactions" ,    0,              &tm_trans_6xx   },
	{"inuse_transactions" ,  STAT_NO_RESET,  &tm_trans_inuse },
	{0,0,0}
};


/**
 * pseudo-variables exported by TM module
 */
static pv_export_t mod_items[] = {
	{ {"T_branch_idx", sizeof("T_branch_idx")-1}, 900, pv_get_tm_branch_idx, 0,
		 0, 0, 0, 0 },
	{ {"T_reply_code", sizeof("T_reply_code")-1}, 901, pv_get_tm_reply_code, 0,
		 0, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};


static mi_export_t mi_cmds [] = {
	{MI_TM_UAC,     mi_tm_uac_dlg,   MI_ASYNC_RPL_FLAG,  0,  0 },
	{MI_TM_CANCEL,  mi_tm_cancel,    0,                  0,  0 },
	{MI_TM_HASH,    mi_tm_hash,      MI_NO_INPUT_FLAG,   0,  0 },
	{MI_TM_REPLY,   mi_tm_reply,     0,                  0,  0 },
	{0,0,0,0,0}
};


#ifdef STATIC_TM
struct module_exports tm_exports = {
#else
struct module_exports exports= {
#endif
	"tm",      /* module name*/
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,      /* exported functions */
	params,    /* exported variables */
	mod_stats, /* exported statistics */
	mi_cmds,   /* exported MI functions */
	mod_items, /* exported pseudo-variables */
	0,         /* extra processes */
	mod_init,  /* module initialization function */
	(response_function) reply_received,
	(destroy_function) tm_shutdown,
	child_init /* per-child init function */
};



/**************************** fixup functions ******************************/
static int flag_fixup(void** param, int param_no)
{
	unsigned int flags;
	str s;

	if (param_no == 1) {
		s.s = (char*)*param;
		s.len = strlen(s.s);
		flags = 0;
		if ( strno2int(&s, &flags )<0 ) {
			return -1;
		}
		pkg_free(*param);
		*param = (void*)(unsigned long int)(flags<<1);
	}
	return 0;
}


static int fixup_t_replicate(void** param, int param_no)
{
	str *s;

	if (param_no == 1) {
		/* string */
		s = (str*)pkg_malloc( sizeof(str) );
		if (s==0) {
			LM_ERR("no more pkg mem\n");
			return E_OUT_OF_MEM;
		}
		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	} else {
		/* flags */
		if (flag_fixup( param, 1)!=0) {
			LM_ERR("bad flags <%s>\n", (char *)(*param));
			return E_CFG;
		}
	}
	return 0;
}


static int fixup_phostport2proxy(void** param, int param_no)
{
	struct proxy_l *proxy;
	char *s;
	int port;
	int proto;
	str host;

	if (param_no!=1) {
		LM_CRIT("called with more than  one parameter\n");
		return E_BUG;
	}

	s = (char *) (*param);
	if (s==0 || *s==0) {
		LM_CRIT("empty parameter\n");
		return E_UNSPEC;
	}

	if (parse_phostport( s, strlen(s), &host.s, &host.len, &port, &proto)!=0){
		LM_CRIT("invalid parameter <%s>\n",s);
		return E_UNSPEC;
	}

	proxy = mk_proxy( &host, port, proto, 0);
	if (proxy==0) {
		LM_ERR("failed to resolve <%.*s>\n", host.len, host.s );
		return ser_error;
	}
	*(param)=proxy;
	return 0;
}


static int fixup_t_relay1(void** param, int param_no)
{
	if (flag_fixup( param, 1)==0) {
		/* param is flag -> move it as second param */
		*((void**)(((char*)param)+sizeof(action_elem_t))) = *param;
		*param = 0;
		return 0;
	} else if (fixup_phostport2proxy( param, 1)==0 ) {
		/* param is OBP -> nothing else to do */
		return 0;
	} else {
		LM_ERR("param is neither flag, nor OBP <%s>\n",(char *)(*param));
		return E_CFG;
	}
}


static int fixup_t_relay2(void** param, int param_no)
{
	if (param_no==1) {
		return fixup_phostport2proxy( param, param_no);
	} else if (param_no==2) {
		if (flag_fixup( param, 1)!=0) {
			LM_ERR("bad flags <%s>\n", (char *)(*param));
			return E_CFG;
		}
	}
	return 0;
}


static int fixup_t_send_reply(void** param, int param_no)
{
	pv_elem_t *model=NULL;
	str s;

	/* convert to str */
	s.s = (char*)*param;
	s.len = strlen(s.s);
	if (s.len==0) {
		LM_ERR("param no. %d is empty!\n", param_no);
		return E_CFG;
	}

	model=NULL;
	if (param_no==1 || param_no==2) {
		if(pv_parse_format(&s ,&model) || model==NULL) {
			LM_ERR("wrong format [%s] for param no %d!\n", s.s, param_no);
			return E_CFG;
		}
		if(model->spec.getf==NULL && param_no==1) {
			if(str2int(&s,
			(unsigned int*)&model->spec.pvp.pvn.u.isname.name.n)!=0
			|| model->spec.pvp.pvn.u.isname.name.n<100
			|| model->spec.pvp.pvn.u.isname.name.n>699) {
				LM_ERR("wrong value [%s] for param no %d! - Allowed only"
					" 1xx - 6xx \n", s.s, param_no);
				return E_CFG;
			}
		}
		*param = (void*)model;
	}

	return 0;
}


static int fixup_local_replied(void** param, int param_no)
{
	char *val;
	int n = 0;

	if (param_no==1) {
		val = (char*)*param;
		if (strcasecmp(val,"all")==0) {
			n = 0;
		} else if (strcasecmp(val,"branch")==0) {
			n = 1;
		} else if (strcasecmp(val,"last")==0) {
			n = 2;
		} else {
			LM_ERR("invalid param \"%s\"\n", val);
			return E_CFG;
		}
		/* free string */
		pkg_free(*param);
		/* replace it with the compiled re */
		*param=(void*)(long)n;
	} else {
		LM_ERR("called with parameter != 1\n");
		return E_BUG;
	}
	return 0;
}



/***************************** init functions *****************************/
int load_tm( struct tm_binds *tmb)
{
	tmb->register_tmcb = register_tmcb;

	/* relay function */
	tmb->t_relay = (cmd_function)w_t_relay;
	/* reply functions */
	tmb->t_reply = (treply_f)w_t_reply;
	tmb->t_reply_with_body = t_reply_with_body;

	/* transaction location/status functions */
	tmb->t_newtran = t_newtran;
	tmb->t_is_local = t_is_local;
	tmb->t_get_trans_ident = t_get_trans_ident;
	tmb->t_lookup_ident = t_lookup_ident;
	tmb->t_gett = get_t;
	tmb->t_get_picked = t_get_picked_branch;

	tmb->t_lookup_original_t = t_lookupOriginalT;
	tmb->t_cancel_uac = t_uac_cancel;
	tmb->unref_cell = t_unref_cell;
	tmb->t_setkr = set_kr;

	/* tm uac functions */
	tmb->t_addblind = add_blind_uac;
	tmb->t_request_within = req_within;
	tmb->t_request_outside = req_outside;
	tmb->t_request = request;
	tmb->new_dlg_uac = new_dlg_uac;
	tmb->dlg_add_extra = dlg_add_extra;
	tmb->dlg_response_uac = dlg_response_uac;
	tmb->new_dlg_uas = new_dlg_uas;
	tmb->dlg_request_uas = dlg_request_uas;
	tmb->free_dlg = free_dlg;
	tmb->print_dlg = print_dlg;


	return 1;
}


static int do_t_unref( struct sip_msg *foo, void *bar)
{
	struct cell *t;

	t = get_cancelled_t();
	if (t!=NULL && t!=T_UNDEFINED)
		t_unref_cell(t);

	t = get_e2eack_t();
	if (t!=NULL && t!=T_UNDEFINED)
		t_unref_cell(t);

	return t_unref(foo);
}


static int script_init( struct sip_msg *foo, void *bar)
{
	/* we primarily reset all private memory here to make sure
	 * private values left over from previous message will
	 * not be used again */

	/* make sure the new message will not inherit previous
	 * message's t_on_negative value
	 */
	t_on_negative( 0 );
	t_on_reply(0);
	t_on_branch(0);
	set_t(T_UNDEFINED);
	reset_cancelled_t();
	reset_e2eack_t();
	/* reset the kr status */
	reset_kr();
	return 1;
}


static int mod_init(void)
{
	LM_INFO("TM - initializing...\n");

	/* checking if we have sufficient bitmap capacity for given
	   maximum number of  branches */
	if (MAX_BRANCHES+1>31) {
		LM_CRIT("Too many max UACs for UAC branch_bm_t bitmap: %d\n",
			MAX_BRANCHES );
		return -1;
	}

	/* if statistics are disabled, prevent their registration to core */
	if (tm_enable_stats==0)
#ifdef STATIC_TM
		tm_exports.stats = 0;
#else
		exports.stats = 0;
#endif

	if (init_callid() < 0) {
		LM_CRIT("Error while initializing Call-ID generator\n");
		return -1;
	}

	/* building the hash table*/
	if (!init_hash_table()) {
		LM_ERR("initializing hash_table failed\n");
		return -1;
	}

	/* init static hidden values */
	init_t();

	if (!tm_init_timers()) {
		LM_ERR("timer init failed\n");
		return -1;
	}

	/* register the timer functions */
	if (register_timer( timer_routine , 0, 1 )<0) {
		LM_ERR("failed to register timer\n");
		return -1;
	}
	if (register_utimer( utimer_routine , 0, 100*1000 )<0) {
		LM_ERR("failed to register utimer\n");
		return -1;
	}

	if (uac_init()==-1) {
		LM_ERR("uac_init failed\n");
		return -1;
	}

	if (init_tmcb_lists()!=1) {
		LM_CRIT("failed to init tmcb lists\n");
		return -1;
	}

	tm_init_tags();
	init_twrite_lines();
	if (init_twrite_sock() < 0) {
		LM_ERR("failed to create socket\n");
		return -1;
	}

	/* register post-script clean-up function */
	if (register_script_cb( do_t_unref, POST_SCRIPT_CB|REQ_TYPE_CB, 0)<0 ) {
		LM_ERR("failed to register POST request callback\n");
		return -1;
	}
	if (register_script_cb( script_init, PRE_SCRIPT_CB|REQ_TYPE_CB , 0)<0 ) {
		LM_ERR("failed to register PRE request callback\n");
		return -1;
	}

	if ( init_avp_params( fr_timer_param, fr_inv_timer_param)<0 ){
		LM_ERR("ERROR:tm:mod_init: failed to process timer AVPs\n");
		return -1;
	}

	return 0;
}


static int child_init(int rank)
{
	if (child_init_callid(rank) < 0) {
		LM_ERR("failed to initialize Call-ID generator\n");
		return -2;
	}

	return 0;
}




/**************************** wrapper functions ***************************/
static int t_check_status(struct sip_msg* msg, char *regexp, char *foo)
{
	regmatch_t pmatch;
	struct cell *t;
	char *status;
	char backup;
	int branch;
	int n;

	/* first get the transaction */
	t = get_t();
	if ( t==0 || t==T_UNDEFINED ) {
		LM_ERR("cannot check status for a reply which"
				" has no transaction-state established\n");
		return -1;
	}
	backup = 0;

	switch (route_type) {
		case REQUEST_ROUTE:
			/* use the status of the last sent reply */
			status = int2str( t->uas.status, 0);
			break;
		case ONREPLY_ROUTE:
			/* use the status of the current reply */
			status = msg->first_line.u.reply.status.s;
			backup = status[msg->first_line.u.reply.status.len];
			status[msg->first_line.u.reply.status.len] = 0;
			break;
		case FAILURE_ROUTE:
			/* use the status of the winning reply */
			if ( (branch=t_get_picked_branch())<0 ) {
				LM_CRIT("no picked branch (%d) for a final response"
						" in MODE_ONFAILURE\n", branch);
				return -1;
			}
			status = int2str( t->uac[branch].last_received , 0);
			break;
		default:
			LM_ERR("unsupported route_type %d\n", route_type);
			return -1;
	}

	LM_DBG("checked status is <%s>\n",status);
	/* do the checking */
	n = regexec((regex_t*)regexp, status, 1, &pmatch, 0);

	if (backup) status[msg->first_line.u.reply.status.len] = backup;
	if (n!=0) return -1;
	return 1;
}


inline static int t_check_trans(struct sip_msg* msg, char *foo, char *bar)
{
	struct cell *trans;

	if (msg->REQ_METHOD==METHOD_CANCEL) {
		/* parse needed hdrs*/
		if (check_transaction_quadruple(msg)==0) {
			LM_ERR("too few headers\n");
			return 0; /*drop request!*/
		}
		if (!msg->hash_index)
			msg->hash_index = tm_hash(msg->callid->body,get_cseq(msg)->number);
		/* performe lookup */
		trans = t_lookupOriginalT(  msg );
		return trans?1:-1;
	} else {
		trans = get_t();
		if (trans==NULL)
			return -1;
		if (trans!=T_UNDEFINED)
			return 1;
		switch ( t_lookup_request( msg , 0) ) {
			case 1:
				/* transaction found -> is it local ACK? */
				if (msg->REQ_METHOD==METHOD_ACK)
					return 1;
				/* .... else -> retransmission */
				trans = get_t();
				t_retransmit_reply(trans);
				UNREF(trans);
				set_t(0);
				return 0;
			case -2:
				/* e2e ACK found */
				return 1;
			default:
				/* notfound */
				return -1;
		}
	}
}


static int t_flush_flags(struct sip_msg* msg, char *foo, char *bar)
{
	struct cell *t;

	/* first get the transaction */
	t = get_t();
	if ( t==0 || t==T_UNDEFINED) {
		LM_ERR("failed to flush flags for a message which has"
				" no transaction-state established\n");
		return -1;
	}

	/* do the flush */
	t->uas.request->flags = msg->flags;
	return 1;
}


inline static int t_local_replied(struct sip_msg* msg, char *type, char *bar)
{
	struct cell *t;
	int branch;
	int i;

	t = get_t();
	if (t==0 || t==T_UNDEFINED) {
		LM_ERR("no trasaction created\n");
		return -1;
	}

	switch ( (int)(long)type ) {
		/* check all */
		case 0:
			for( i=t->first_branch ; i<t->nr_of_outgoings ; i++ ) {
				if (t->uac[i].flags&T_UAC_HAS_RECV_REPLY)
					return -1;
			}
			return 1;
		/* check branch */
		case 1:
			if (route_type==FAILURE_ROUTE) {
				/* use the winning reply */
				if ( (branch=t_get_picked_branch())<0 ) {
					LM_CRIT("no picked branch (%d) for"
						" a final response in MODE_ONFAILURE\n", branch);
					return -1;
				}
				if (t->uac[branch].flags&T_UAC_HAS_RECV_REPLY)
					return -1;
				return 1;
			}
			return -1;
		/* check last */
		case 2:
			if (route_type==FAILURE_ROUTE) {
				/* use the winning reply */
				if ( (branch=t_get_picked_branch())<0 ) {
					LM_CRIT("no picked branch (%d) for"
						" a final response in MODE_ONFAILURE\n", branch);
					return -1;
				}
				if (t->uac[branch].reply==FAKED_REPLY)
					return 1;
				return -1;
			}
			return (t->relaied_reply_branch==-2)?1:-1;
		default:
			return -1;
	}
}


static int t_was_cancelled(struct sip_msg* msg, char *foo, char *bar)
{
	struct cell *t;

	/* first get the transaction */
	t = get_t();
	if ( t==0 || t==T_UNDEFINED ) {
		LM_ERR("failed to check cancel flag for a reply"
				" without a transaction\n");
		return -1;
	}
	return was_cancelled(t)?1:-1;
}


inline static int w_t_reply(struct sip_msg* msg, char* str1, char* str2)
{
	struct cell *t;

	if (msg->REQ_METHOD==METHOD_ACK) {
		LM_WARN("ACKs are not replied\n");
		return -1;
	}
	t=get_t();
	if ( t==0 || t==T_UNDEFINED ) {
		LM_ERR("failed to send a t_reply to a message for which no "
			"transaction-state has been established\n");
		return -1;
	}
	/* if called from reply_route, make sure that unsafe version
	 * is called; we are already in a mutex and another mutex in
	 * the safe version would lead to a deadlock
	 */
	switch (route_type) {
		case FAILURE_ROUTE:
			LM_DBG("t_reply_unsafe called from w_t_reply\n");
			return t_reply_unsafe(t, msg, (unsigned int)(long)str1,(str*)str2);
		case REQUEST_ROUTE:
			return t_reply( t, msg, (unsigned int)(long) str1, (str*)str2);
		default:
			LM_CRIT("unsupported route_type (%d)\n", route_type);
			return -1;
	}
}


inline static int w_pv_t_reply(struct sip_msg *msg, char* code, char* text)
{
	str code_s;
	unsigned int code_i;

	if(((pv_elem_p)code)->spec.getf!=NULL) {
		if(pv_printf_s(msg, (pv_elem_p)code, &code_s)!=0)
			return -1;
		if(str2int(&code_s, &code_i)!=0 || code_i<100 || code_i>699)
			return -1;
	} else {
		code_i = ((pv_elem_p)code)->spec.pvp.pvn.u.isname.name.n;
	}

	if(((pv_elem_p)text)->spec.getf!=NULL) {
		if(pv_printf_s(msg, (pv_elem_p)text, &code_s)!=0 || code_s.len <=0)
			return -1;
	} else {
		code_s = ((pv_elem_p)text)->text;
	}

	return w_t_reply(msg, (char*)(unsigned long)code_i, (char*)&code_s);
}


inline static int w_t_release(struct sip_msg* msg, char* str, char* str2)
{
	struct cell *t;

	t=get_t();
	if ( t && t!=T_UNDEFINED ) 
		return t_release_transaction( t );
	return 1;
}


inline static int w_t_newtran( struct sip_msg* p_msg, char* foo, char* bar ) 
{
	/* t_newtran returns 0 on error (negative value means
	   'transaction exists' */
	return t_newtran( p_msg );
}


inline static int w_t_on_negative( struct sip_msg* msg, char *go_to, char *foo)
{
	t_on_negative( (unsigned int )(long) go_to );
	return 1;
}


inline static int w_t_on_reply( struct sip_msg* msg, char *go_to, char *foo )
{
	t_on_reply( (unsigned int )(long) go_to );
	return 1;
}


inline static int w_t_on_branch( struct sip_msg* msg, char *go_to, char *foo )
{
	t_on_branch( (unsigned int )(long) go_to );
	return 1;
}


inline static int w_t_replicate(struct sip_msg *p_msg, char *dst, char *flags)
{
	return t_replicate( p_msg, (str*)dst, (int)(long)flags);
}

static inline int t_relay_inerr2scripterr(void)
{
	switch (ser_error) {
		case E_BAD_URI:
		case E_BAD_REQ:
		case E_BAD_TO:
		case E_INVALID_PARAMS:
			/* bad message */
			return -2;
		case E_NO_DESTINATION:
			/* no available destination */
			return -3;
		case E_BAD_ADDRESS:
			/* bad destination */
			return -4;
		case E_IP_BLOCKED:
			/* destination filtered */
			return -5;
		case E_NO_SOCKET:
		case E_SEND:
			/* send failed */
			return -6;
		default:
			/* generic internal error */
			return -1;
	}
}


inline static int w_t_relay( struct sip_msg  *p_msg , char *proxy, char *flags)
{
	struct cell *t;
	int ret;

	t=get_t();

	if (!t || t==T_UNDEFINED) {
		/* no transaction yet */
		if (route_type==FAILURE_ROUTE) {
			LM_CRIT(" BUG - undefined transaction in failure route\n");
			return -1;
		}
		ret = t_relay_to( p_msg, (struct proxy_l *)proxy, (int)(long)flags );
		if (ret<0) {
			return t_relay_inerr2scripterr();
		}
		return ret;
	} else {
		/* transaction already created */

		if ( route_type!=REQUEST_ROUTE && route_type!=FAILURE_ROUTE )
			goto route_err;

		if (p_msg->REQ_METHOD==METHOD_ACK) {
			/* local ACK*/
			t_release_transaction(t);
			return 1;
		}

		if (((int)(long)flags)&TM_T_REPLY_nodnsfo_FLAG)
			t->flags|=T_NO_DNS_FAILOVER_FLAG;

		ret = t_forward_nonack( t, p_msg, (struct proxy_l *)proxy);
		if (ret<=0 ) {
			LM_ERR("t_forward_nonack failed\n");
			return t_relay_inerr2scripterr();
		}
		return ret;
	}

route_err:
	LM_CRIT("unsupported route type: %d\n", route_type);
	return 0;
}



/* item functions */
static int pv_get_tm_branch_idx(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	extern int _tm_branch_index;
	int l = 0;
	char *ch = NULL;

	if(msg==NULL || res==NULL)
		return -1;
	
	ch = int2str(_tm_branch_index, &l);

	res->rs.s = ch;
	res->rs.len = l;

	res->ri = _tm_branch_index;
	res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;

	return 0;
}

static int pv_get_tm_reply_code(struct sip_msg *msg, pv_param_t *param,
		pv_value_t *res)
{
	struct cell *t;
	int code;
	int branch;

	if(msg==NULL || res==NULL)
		return -1;

	/* first get the transaction */
	if (t_check( msg , 0 )==-1) return -1;
	if ( (t=get_t())==0) {
		/* no T */
		code = 0;
	} else {
		switch (route_type) {
			case REQUEST_ROUTE:
				/* use the status of the last sent reply */
				code = t->uas.status;
				break;
			case ONREPLY_ROUTE:
				/* use the status of the current reply */
				code = msg->first_line.u.reply.statuscode;
				break;
			case FAILURE_ROUTE:
				/* use the status of the winning reply */
				if ( (branch=t_get_picked_branch())<0 ) {
					LM_CRIT("no picked branch (%d) for a final response"
							" in MODE_ONFAILURE\n", branch);
					code = 0;
				} else {
					code = t->uac[branch].last_received;
				}
				break;
			default:
				LM_ERR("unsupported route_type %d\n", route_type);
				code = 0;
		}
	}

	LM_DBG("reply code is <%d>\n",code);

	res->rs.s = int2str( code, &res->rs.len);

	res->ri = code;
	res->flags = PV_VAL_STR|PV_VAL_INT|PV_TYPE_INT;
	return 0;
}

