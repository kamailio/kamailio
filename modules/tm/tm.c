/*
 * $Id$
 *
 * TM module
 *
 *
 * ***************************************************
 * * Jiri's Source Memorial                          *
 * *                                                 *
 * * Welcome, pilgrim ! This is the greatest place   *
 * * where dramatic changes happend. There are not   *
 * * many places with a history like this, as there  *
 * * are not so many people like Jiri, one of the    *
 * * ser's fathers, who brought everywhere the wind  *
 * * of change, the flood of clean-up. We all felt   *
 * * his fatherly eye watching over us day and night.*
 * *                                                 *
 * * Please, preserve this codework heritage, as     *
 * * it's unlikly for fresh, juicy pices of code to  *
 * * arise to give him the again the chance to       *
 * * demonstrate his clean-up and improvement skills.*
 * *                                                 *
 * * Hereby, we solicit you to adopt this historical *
 * * piece of code. For $100, your name will be      *
 * * be printed in this banner and we will use       *
 * * collected funds to create and display an ASCII  *
 * * statue of Jiri  .                               *
 * ***************************************************
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * History:
 * --------
 *  2003-02-18  added t_forward_nonack_{udp, tcp}, t_relay_to_{udp,tcp},
 *               t_replicate_{udp, tcp} (andrei)
 *  2003-02-19  added t_rely_{udp, tcp} (andrei)
 *  2003-03-10  module export interface updated to the new format (andrei)
 */


#include "defs.h"


#include <stdio.h>
#include <string.h>
#include <netdb.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../script_cb.h"
#include "../../fifo_server.h"

#include "sip_msg.h"
#include "h_table.h"
#include "t_funcs.h"
#include "t_hooks.h"
#include "tm_load.h"
#include "ut.h"
#include "t_reply.h"
#include "uac.h"
#include "t_fwd.h"
#include "t_lookup.h"
#include "t_stats.h"



inline static int w_t_check(struct sip_msg* msg, char* str, char* str2);
inline static int w_t_reply(struct sip_msg* msg, char* str, char* str2);

inline static int w_t_release(struct sip_msg* msg, char* str, char* str2);
inline static int fixup_t_send_reply(void** param, int param_no);
inline static int fixup_str2int( void** param, int param_no);
inline static int w_t_retransmit_reply(struct sip_msg* p_msg, char* foo, char* bar );
inline static int w_t_newtran(struct sip_msg* p_msg, char* foo, char* bar );
inline static int w_t_newdlg( struct sip_msg* p_msg, char* foo, char* bar );
inline static int w_t_relay( struct sip_msg  *p_msg , char *_foo, char *_bar);
inline static int w_t_relay_udp( struct sip_msg  *p_msg,char *_foo,char *_bar);
inline static int w_t_relay_tcp( struct sip_msg  *p_msg,char *_foo,char *_bar);
inline static int w_t_relay_to( struct sip_msg  *p_msg , char *proxy, char *);
inline static int w_t_relay_to_udp( struct sip_msg  *p_msg , char *proxy, 
									char *);
inline static int w_t_relay_to_tcp( struct sip_msg  *p_msg , char *proxy,
									char *);
inline static int w_t_replicate( struct sip_msg  *p_msg , 
							char *proxy, /* struct proxy_l *proxy expected */
							char *_foo       /* nothing expected */ );
inline static int w_t_replicate_udp( struct sip_msg  *p_msg , 
							char *proxy, /* struct proxy_l *proxy expected */
							char *_foo       /* nothing expected */ );
inline static int w_t_replicate_tcp( struct sip_msg  *p_msg , 
							char *proxy, /* struct proxy_l *proxy expected */
							char *_foo       /* nothing expected */ );
inline static int w_t_forward_nonack(struct sip_msg* msg, char* str, char* );
inline static int w_t_forward_nonack_udp(struct sip_msg* msg, char* str,char*);
inline static int w_t_forward_nonack_tcp(struct sip_msg* msg, char* str,char*);
inline static int fixup_hostport2proxy(void** param, int param_no);
inline static int w_t_on_negative( struct sip_msg* msg, char *go_to, char *foo );


static int mod_init(void);

static int child_init(int rank);


static cmd_export_t cmds[]={
	{"t_newtran",          w_t_newtran,             0, 0                    },
	{"t_lookup_request",   w_t_check,               0, 0                    },
	{T_REPLY,              w_t_reply,               2, fixup_t_send_reply   },
	{"t_retransmit_reply", w_t_retransmit_reply,    0, 0                    },
	{"t_release",          w_t_release,             0, 0                    },
	{T_RELAY_TO,           w_t_relay_to,            2, fixup_hostport2proxy },
	{T_RELAY_TO_UDP,       w_t_relay_to_udp,        2, fixup_hostport2proxy },
	{T_RELAY_TO_TCP,       w_t_relay_to_tcp,        2, fixup_hostport2proxy },
	{"t_replicate",        w_t_replicate,           2, fixup_hostport2proxy },
	{"t_replicate_udp",    w_t_replicate_udp,       2, fixup_hostport2proxy },
	{"t_replicate_tcp",    w_t_replicate_tcp,       2, fixup_hostport2proxy },
	{T_RELAY,              w_t_relay,               0, 0                    },
	{T_RELAY_UDP,          w_t_relay_udp,           0, 0                    },
	{T_RELAY_TCP,          w_t_relay_tcp,           0, 0                    },
	{T_FORWARD_NONACK,     w_t_forward_nonack,      2, fixup_hostport2proxy },
	{T_FORWARD_NONACK_UDP, w_t_forward_nonack_udp,  2, fixup_hostport2proxy },
	{T_FORWARD_NONACK_TCP, w_t_forward_nonack_tcp,  2, fixup_hostport2proxy },
	{"t_on_negative",      w_t_on_negative,         1, fixup_str2int        },
	/* not applicable from the script -- ugly hack */
	{"register_tmcb",      (cmd_function)register_tmcb,     NO_SCRIPT,     0},
	{T_UAC_DLG,            (cmd_function)t_uac_dlg,         NO_SCRIPT,     0},
	{"load_tm",            (cmd_function)load_tm,           NO_SCRIPT,     0},
	{T_REPLY_WB,           (cmd_function)t_reply_with_body, NO_SCRIPT,     0},
	{T_IS_LOCAL,           (cmd_function)t_is_local,        NO_SCRIPT,     0},
	{T_GET_TI,             (cmd_function)t_get_trans_ident, NO_SCRIPT,     0},
	{T_LOOKUP_IDENT,       (cmd_function)t_lookup_ident,    NO_SCRIPT,     0},
	{T_ADDBLIND,           (cmd_function)add_blind_uac,     NO_SCRIPT,     0},
	{"t_newdlg",           (cmd_function)w_t_newdlg,        0,             0},
	{0,0,0,0}
};

static param_export_t params[]={
	{"ruri_matching", INT_PARAM, &ruri_matching                         },
	{"fr_timer",      INT_PARAM, &(timer_id2timeout[FR_TIMER_LIST])     },
	{"fr_inv_timer",  INT_PARAM, &(timer_id2timeout[FR_INV_TIMER_LIST]) },
	{"wt_timer",      INT_PARAM, &(timer_id2timeout[WT_TIMER_LIST])     },
	{"delete_timer",  INT_PARAM, &(timer_id2timeout[DELETE_LIST])       },
	{"retr_timer1p1", INT_PARAM, &(timer_id2timeout[RT_T1_TO_1])        },
	{"retr_timer1p2", INT_PARAM, &(timer_id2timeout[RT_T1_TO_2])        },
	{"retr_timer1p3", INT_PARAM, &(timer_id2timeout[RT_T1_TO_3])        },
	{"retr_timer2",   INT_PARAM, &(timer_id2timeout[RT_T2])             },
	{"noisy_ctimer",  INT_PARAM, &noisy_ctimer                          },
	{0,0,0}
};


#ifdef STATIC_TM
struct module_exports tm_exports = {
#else
struct module_exports exports= {
#endif
	"tm",
	/* -------- exported functions ----------- */
	cmds,
	/* ------------ exported variables ---------- */
	params,
	
	mod_init, /* module initialization function */
	(response_function) t_on_reply,
	(destroy_function) tm_shutdown,
	0, /* w_onbreak, */
	child_init /* per-child init function */
};

inline static int fixup_str2int( void** param, int param_no)
{
	unsigned long go_to;
	int err;

	if (param_no==1) {
		go_to=str2s(*param, strlen(*param), &err );
		if (err==0) {
			free(*param);
			*param=(void *)go_to;
			return 0;
		} else {
			LOG(L_ERR, "ERROR: fixup_str2int: bad number <%s>\n",
				(char *)(*param));
			return E_CFG;
		}
	}
	return 0;
}

static int w_t_unref( struct sip_msg *foo, void *bar)
{
	return t_unref(foo);
}

static int script_init( struct sip_msg *foo, void *bar)
{   
	/* we primarily reset all private memory here to make sure
	   private values left over from previous message will
	   not be used again
    */

	/* make sure the new message will not inherit previous
	   message's t_on_negative value
	*/
	t_on_negative( 0 );

	/* reset the kr status */
	set_kr(0);

	return 1;
}

static int mod_init(void)
{

	DBG( "TM - initializing...\n");
	/* checking if we have sufficient bitmap capacity for given
	   maximum number of  branches */
	if (MAX_BRANCHES+1>31) {
		LOG(L_CRIT, "Too many max UACs for UAC branch_bm_t bitmap: %d\n",
			MAX_BRANCHES );
		return -1;
	}


	if (register_fifo_cmd(fifo_uac_dlg, "t_uac_dlg", 0)<0) {
		LOG(L_CRIT, "cannot register fifo uac\n");
		return -1;
	}
	if (register_fifo_cmd(fifo_hash, "t_hash", 0)<0) {
		LOG(L_CRIT, "cannot register hash\n");
		return -1;
	}

	if (!init_hash_table()) {
		LOG(L_ERR, "ERROR: mod_init: initializing hash_table failed\n");
		return -1;
	}


	/* init static hidden values */
	init_t();

	if (!tm_init_timers()) {
		LOG(L_ERR, "ERROR: mod_init: timer init failed\n");
		return -1;
	}
	/* register the timer function */
	register_timer( timer_routine , 0 /* empty attr */, 1 );

	/* init_tm_stats calls process_count, which should
	 * NOT be called from mod_init, because one does not
	 * now, if a timer is used and thus how many processes
	 * will be started; however we started already our
	 * timers, so we know and process_count should not
	 * change any more
	 */	
	if (init_tm_stats()<0) {
		LOG(L_CRIT, "ERROR: mod_init: failed to init stats\n");
		return -1;
	}

	/* building the hash table*/

	if (uac_init()==-1) {
		LOG(L_ERR, "ERROR: mod_init: uac_init failed\n");
		return -1;
	}
	/* register post-script clean-up function */
	register_script_cb( w_t_unref, POST_SCRIPT_CB, 
			0 /* empty param */ );
	register_script_cb( script_init, PRE_SCRIPT_CB , 
			0 /* empty param */ );

	tm_init_tags();

	return 0;
}

static int child_init(int rank) {
	if (uac_child_init(rank)==-1) {
		LOG(L_ERR, "ERROR: child_init: uac_child_init error\n");
		return -1;
	}
	return 1;
}


/* (char *hostname, char *port_nr) ==> (struct proxy_l *, -)  */

inline static int fixup_hostport2proxy(void** param, int param_no)
{
	unsigned int port;
	char *host;
	int err;
	struct proxy_l *proxy;
	str s;
	
	DBG("TM module: fixup_t_forward(%s, %d)\n", (char*)*param, param_no);
	if (param_no==1){
		DBG("TM module: fixup_t_forward: param 1.. do nothing, wait for #2\n");
		return 0;
	} else if (param_no==2) {

		host=(char *) (*(param-1)); 
		port=str2s(*param, strlen(*param), &err);
		if (err!=0) {
			LOG(L_ERR, "TM module:fixup_t_forward: bad port number <%s>\n",
				(char*)(*param));
			 return E_UNSPEC;
		}
		s.s = host;
		s.len = strlen(host);
		proxy=mk_proxy(&s, port, 0); /* FIXME: udp or tcp? */
		if (proxy==0) {
			LOG(L_ERR, "ERROR: fixup_t_forwardv6: bad host name in URI <%s>\n",
				host );
			return E_BAD_ADDRESS;
		}
		/* success -- fix the first parameter to proxy now ! */

		/* FIXME: janakj, mk_proxy doesn't make copy of host !! */
		free( *(param-1));
		*(param-1)=proxy;
		return 0;
	} else {
		LOG(L_ERR, "ERROR: fixup_t_forwardv6 called with parameter #<>{1,2}\n");
		return E_BUG;
	}
}


/* (char *code, char *reason_phrase)==>(int code, r_p as is) */
inline static int fixup_t_send_reply(void** param, int param_no)
{
	unsigned long code;
	int err;

	if (param_no==1){
		code=str2s(*param, strlen(*param), &err);
		if (err==0){
			free(*param);
			*param=(void*)code;
			return 0;
		}else{
			LOG(L_ERR, "TM module:fixup_t_send_reply: bad  number <%s>\n",
					(char*)(*param));
			return E_UNSPEC;
		}
	}
	/* second param => no conversion*/
	return 0;
}




inline static int w_t_check(struct sip_msg* msg, char* str, char* str2)
{
	return t_check( msg , 0  ) ? 1 : -1;
}



inline static int _w_t_forward_nonack(struct sip_msg* msg, char* proxy,
									 char* _foo, int proto)
{
	struct cell *t;
	if (t_check( msg , 0 )==-1) {
		LOG(L_ERR, "ERROR: forward_nonack: "
				"can't forward when no transaction was set up\n");
		return -1;
	}
	t=get_t();
	if ( t && t!=T_UNDEFINED ) {
		if (msg->REQ_METHOD==METHOD_ACK) {
			LOG(L_WARN,"WARNING: you don't really want to fwd hbh ACK\n");
			return -1;
		}
		return t_forward_nonack(t, msg, ( struct proxy_l *) proxy, proto );
	} else {
		DBG("DEBUG: t_forward_nonack: no transaction found\n");
		return -1;
	}
}


inline static int w_t_forward_nonack( struct sip_msg* msg, char* proxy,
										char* foo)
{
	return _w_t_forward_nonack(msg, proxy, foo, msg->rcv.proto);
}

inline static int w_t_forward_nonack_udp( struct sip_msg* msg, char* proxy,
										char* foo)
{
	return _w_t_forward_nonack(msg, proxy, foo, PROTO_UDP);
}

inline static int w_t_forward_nonack_tcp( struct sip_msg* msg, char* proxy,
										char* foo)
{
	return _w_t_forward_nonack(msg, proxy, foo, PROTO_TCP);
}



inline static int w_t_reply(struct sip_msg* msg, char* str, char* str2)
{
	struct cell *t;

	if (msg->REQ_METHOD==METHOD_ACK) {
		LOG(L_WARN, "WARNING: t_reply: ACKs are not replied\n");
		return -1;
	}
	if (t_check( msg , 0 )==-1) return -1;
	t=get_t();
	if (!t) {
		LOG(L_ERR, "ERROR: t_reply: cannot send a t_reply to a message "
			"for which no T-state has been established\n");
		return -1;
	}
	/* if called from reply_route, make sure that unsafe version
	 * is called; we are already in a mutex and another mutex in
	 * the safe version would lead to a deadlock
	 */
	if (rmode==MODE_ONREPLY_REQUEST) { 
		DBG("DEBUG: t_reply_unsafe called from w_t_reply\n");
		return t_reply_unsafe(t, msg, (unsigned int)(long) str, str2);
	} else {
		return t_reply( t, msg, (unsigned int)(long) str, str2);
	}
}


inline static int w_t_release(struct sip_msg* msg, char* str, char* str2)
{
	struct cell *t;
	if (t_check( msg  , 0  )==-1) return -1;
	t=get_t();
	if ( t && t!=T_UNDEFINED ) 
		return t_release_transaction( t );
	return 1;
}




inline static int w_t_retransmit_reply( struct sip_msg* p_msg, char* foo, char* bar)
{
	struct cell *t;


	if (t_check( p_msg  , 0 )==-1) 
		return 1;
	t=get_t();
	if (t) {
		if (p_msg->REQ_METHOD==METHOD_ACK) {
			LOG(L_WARN, "WARNING: : ACKs ansmit_replies not replied\n");
			return -1;
		}
		return t_retransmit_reply( t );
	} else 
		return -1;
}





inline static int w_t_newdlg( struct sip_msg* p_msg, char* foo, char* bar ) 
{
	return t_newdlg( p_msg );
}

inline static int w_t_newtran( struct sip_msg* p_msg, char* foo, char* bar ) 
{
	/* t_newtran returns 0 on error (negative value means
	   'transaction exists'
	*/
	return t_newtran( p_msg );
}


inline static int w_t_on_negative( struct sip_msg* msg, char *go_to, char *foo )
{
	return t_on_negative( (unsigned int )(long) go_to );
}

inline static int w_t_relay_to( struct sip_msg  *p_msg , 
	char *proxy, /* struct proxy_l *proxy expected */
	char *_foo       /* nothing expected */ )
{
	return t_relay_to( p_msg, ( struct proxy_l *) proxy, p_msg->rcv.proto,
	0 /* no replication */ );
}

inline static int w_t_relay_to_udp( struct sip_msg  *p_msg , 
	char *proxy, /* struct proxy_l *proxy expected */
	char *_foo       /* nothing expected */ )
{
	return t_relay_to( p_msg, ( struct proxy_l *) proxy, PROTO_UDP,
	0 /* no replication */ );
}

inline static int w_t_relay_to_tcp( struct sip_msg  *p_msg , 
	char *proxy, /* struct proxy_l *proxy expected */
	char *_foo       /* nothing expected */ )
{
	return t_relay_to( p_msg, ( struct proxy_l *) proxy, PROTO_TCP,
	0 /* no replication */ );
}



inline static int w_t_replicate( struct sip_msg  *p_msg , 
	char *proxy, /* struct proxy_l *proxy expected */
	char *_foo       /* nothing expected */ )
{
	return t_replicate(p_msg, ( struct proxy_l *) proxy, p_msg->rcv.proto );
}

inline static int w_t_replicate_udp( struct sip_msg  *p_msg , 
	char *proxy, /* struct proxy_l *proxy expected */
	char *_foo       /* nothing expected */ )
{
	return t_replicate(p_msg, ( struct proxy_l *) proxy, PROTO_UDP );
}

inline static int w_t_replicate_tcp( struct sip_msg  *p_msg , 
	char *proxy, /* struct proxy_l *proxy expected */
	char *_foo       /* nothing expected */ )
{
	return t_replicate(p_msg, ( struct proxy_l *) proxy, PROTO_TCP );
}



inline static int w_t_relay( struct sip_msg  *p_msg , 
						char *_foo, char *_bar)
{
	return t_relay_to( p_msg, 
		(struct proxy_l *) 0 /* no proxy */, p_msg->rcv.proto,
		0 /* no replication */ );
}


inline static int w_t_relay_udp( struct sip_msg  *p_msg , 
						char *_foo, char *_bar)
{
	return t_relay_to( p_msg, 
		(struct proxy_l *) 0 /* no proxy */, PROTO_UDP,
		0 /* no replication */ );
}


inline static int w_t_relay_tcp( struct sip_msg  *p_msg , 
						char *_foo, char *_bar)
{
	return t_relay_to( p_msg, 
		(struct proxy_l *) 0 /* no proxy */, PROTO_TCP,
		0 /* no replication */ );
}
