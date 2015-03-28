/*
 * transaction maintenance functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "../../dprint.h"
#include "../../hash_func.h"
#include "../../dset.h"
#include "../../mem/mem.h"
#include "../../sr_compat.h"
#include "../../pvar.h"
#include "defs.h"
#include "t_funcs.h"
#include "t_fwd.h"
#include "t_lookup.h"
#include "t_reply.h"
#include "config.h"
#include "t_stats.h"

/* if defined t_relay* error reply generation will be delayed till script
 * end (this allows the script writter to send its own error reply) */
#define TM_DELAYED_REPLY

/* fr_timer AVP specs */
static int     fr_timer_avp_type = 0;
static int_str fr_timer_avp = {0};
static str     fr_timer_str;
static int     fr_timer_index = 0;
int     fr_inv_timer_avp_type = 0;
int_str fr_inv_timer_avp = {0};
static str     fr_inv_timer_str;
static int     fr_inv_timer_index = 0;

int tm_error = 0; /* delayed tm error */

struct msgid_var user_cell_set_flags;   /* extra cell->flags to be set */
struct msgid_var user_cell_reset_flags; /* extra cell->flags to be reset */

/* ----------------------------------------------------- */
int send_pr_buffer(	struct retr_buf *rb, void *buf, int len
#ifdef EXTRA_DEBUG
						, char* file, const char *function, int line
#endif
					)
{
	if (buf && len && rb )
		return msg_send( &rb->dst, buf, len);
	else {
#ifdef EXTRA_DEBUG
		LOG(L_CRIT, "ERROR: send_pr_buffer: sending an empty buffer"
				"from %s: %s (%d)\n", file, function, line );
#else
		LOG(L_CRIT, "ERROR: send_pr_buffer: attempt to send an "
				"empty buffer\n");
#endif
		return -1;
	}
}

void tm_shutdown()
{

	DBG("DEBUG: tm_shutdown : start\n");

	/* destroy the hash table */
	DBG("DEBUG: tm_shutdown : emptying hash table\n");
	free_hash_table( );
	DBG("DEBUG: tm_shutdown : removing semaphores\n");
	lock_cleanup();
	DBG("DEBUG: tm_shutdown : destroying tmcb lists\n");
	destroy_tmcb_lists();
	free_tm_stats();
	DBG("DEBUG: tm_shutdown : done\n");
}


/*   returns 1 if everything was OK or -1 for error
*/
int t_release_transaction( struct cell *trans )
{
	set_kr(REQ_RLSD);

	stop_rb_timers(&trans->uas.response);
	cleanup_uac_timers( trans );
	
	put_on_wait( trans );
	return 1;
}


/* -----------------------HELPER FUNCTIONS----------------------- */


/*
  */
void put_on_wait(  struct cell  *Trans  )
{

#ifdef EXTRA_DEBUG
	DBG("DEBUG: put on WAIT \n");
#endif


	/* we put the transaction on wait timer; we do it only once
	   in transaction's timelife because putting it multiple-times
	   might result in a second instance of a wait timer to be
	   set after the first one fired; on expiration of the second
	   instance, the transaction would be re-deleted

			PROCESS1		PROCESS2		TIMER PROCESS
		0. 200/INVITE rx;
		   put_on_wait
		1.					200/INVITE rx;
		2.									WAIT fires; transaction
											about to be deleted
		3.					avoid putting
							on WAIT again
		4.									WAIT timer executed,
											transaction deleted
	*/
	if (timer_add(&Trans->wait_timer, cfg_get(tm, tm_cfg, wait_timeout))==0){
		/* sucess */
		t_stats_wait();
	}else{
		DBG("tm: put_on_wait: transaction %p already on wait\n", Trans);
	}
}



/* WARNING: doesn't work from failure route (deadlock, uses t_reply =>
 *  tries to get the reply lock again) */
int kill_transaction( struct cell *trans, int error )
{
	char err_buffer[128];
	int sip_err;
	int reply_ret;
	int ret;

	/*  we reply statefully and enter WAIT state since error might
		have occurred in middle of forking and we do not
		want to put the forking burden on upstream client;
		however, it may fail too due to lack of memory */

	ret=err2reason_phrase(error, &sip_err,
		err_buffer, sizeof(err_buffer), "TM" );
	if (ret>0) {
		reply_ret=t_reply( trans, trans->uas.request, 
			sip_err, err_buffer);
		/* t_release_transaction( T ); */
		return reply_ret;
	} else {
		LOG(L_ERR, "ERROR: kill_transaction: err2reason failed\n");
		return -1;
	}
}

/* unsafe version of kill_transaction() that works
 * in failure route 
 * WARNING: assumes that the reply lock is held!
 */
int kill_transaction_unsafe( struct cell *trans, int error )
{
	char err_buffer[128];
	int sip_err;
	int reply_ret;
	int ret;

	/*  we reply statefully and enter WAIT state since error might
		have occurred in middle of forking and we do not
		want to put the forking burden on upstream client;
		however, it may fail too due to lack of memory */

	ret=err2reason_phrase(error, &sip_err,
		err_buffer, sizeof(err_buffer), "TM" );
	if (ret>0) {
		reply_ret=t_reply_unsafe( trans, trans->uas.request, 
			sip_err, err_buffer);
		/* t_release_transaction( T ); */
		return reply_ret;
	} else {
		LOG(L_ERR, "ERROR: kill_transaction_unsafe: err2reason failed\n");
		return -1;
	}
}


/* WARNING: doesn't work from failure route (deadlock, uses t_reply => tries
 *  to get the reply lock again */
int t_relay_to( struct sip_msg  *p_msg , struct proxy_l *proxy, int proto,
				int replicate)
{
	int ret;
	int new_tran;
	/* struct hdr_field *hdr; */
	struct cell *t;
	struct dest_info dst;
	unsigned short port;
	str host;
	short comp;
#ifndef TM_DELAYED_REPLY
	int reply_ret;
#endif

	ret=0;
	
	/* special case for CANCEL */
	if ( p_msg->REQ_METHOD==METHOD_CANCEL){
		ret=t_forward_cancel(p_msg, proxy, proto, &t);
		if (t) goto handle_ret;
		goto done;
	}
	new_tran = t_newtran( p_msg );
	
	/* parsing error, memory alloc, whatever ... if via is bad
	   and we are forced to reply there, return with 0 (->break),
	   pass error status otherwise
	   MMA: return value E_SCRIPT means that transaction was already started
	   from the script so continue with that transaction
	*/
	if (likely(new_tran!=E_SCRIPT)) {
		if (new_tran<0) {
			ret = (ser_error==E_BAD_VIA && reply_to_via) ? 0 : new_tran;
			goto done;
		}
		/* if that was a retransmission, return we are happily done */
		if (new_tran==0) {
			ret = 1;
			goto done;
		}
	}else if (unlikely(p_msg->REQ_METHOD==METHOD_ACK)) {
			/* transaction previously found (E_SCRIPT) and msg==ACK
			    => ack to neg. reply  or ack to local trans.
			    => process it and exit */
			/* FIXME: there's no way to distinguish here between acks to
			   local trans. and neg. acks */
			/* in normal operation we should never reach this point, if we
			   do WARN(), it might hide some real bug (apart from possibly
			   hiding a bug the most harm done is calling the TMCB_ACK_NEG
			   callbacks twice) */
			WARN("negative or local ACK caught, please report\n");
			t=get_t();
			if (unlikely(has_tran_tmcbs(t, TMCB_ACK_NEG_IN)))
				run_trans_callbacks(TMCB_ACK_NEG_IN, t, p_msg, 0, 
										p_msg->REQ_METHOD);
			t_release_transaction(t);
			ret=1;
			goto done;
	}

	/* new transaction */

	/* at this point if the msg is an ACK it is an e2e ACK and
	   e2e ACKs do not establish a transaction and are fwd-ed statelessly */
	if ( p_msg->REQ_METHOD==METHOD_ACK) {
		DBG( "SER: forwarding ACK  statelessly \n");
		if (proxy==0) {
			init_dest_info(&dst);
			dst.proto=proto;
			if (get_uri_send_info(GET_NEXT_HOP(p_msg), &host, &port,
									&dst.proto, &comp)!=0){
				ret=E_BAD_ADDRESS;
				goto done;
			}
#ifdef USE_COMP
			dst.comp=comp;
#endif
			/* dst->send_sock not set, but forward_request will take care
			 * of it */
			ret=forward_request(p_msg, &host, port, &dst);
		} else {
			init_dest_info(&dst);
			dst.proto=get_proto(proto, proxy->proto);
			proxy2su(&dst.to, proxy);
			/* dst->send_sock not set, but forward_request will take care
			 * of it */
			ret=forward_request( p_msg , 0, 0, &dst) ;
		}
		goto done;
	}

	/* if replication flag is set, mark the transaction as local
	   so that replies will not be relayed */
	t=get_t();
	if (replicate) t->flags|=T_IS_LOCAL_FLAG;

	/* INVITE processing might take long, particularly because of DNS
	   look-ups -- let upstream know we're working on it */
	if (p_msg->REQ_METHOD==METHOD_INVITE && (t->flags&T_AUTO_INV_100)
		&& (t->uas.status < 100)
	) {
		DBG( "SER: new INVITE\n");
		if (!t_reply( t, p_msg , 100 ,
			cfg_get(tm, tm_cfg, tm_auto_inv_100_r)))
				DBG("SER: ERROR: t_reply (100)\n");
	} 

	/* now go ahead and forward ... */
	ret=t_forward_nonack(t, p_msg, proxy, proto);
handle_ret:
	if (ret<=0) {
		DBG( "ERROR:tm:t_relay_to:  t_forward_nonack returned error \n");
		/* we don't want to pass upstream any reply regarding replicating
		 * a request; replicated branch must stop at us*/
		if (likely(!replicate)) {
			if(t->flags&T_DISABLE_INTERNAL_REPLY) {
				/* flag set to don't generate the internal negative reply
				 * - let the transaction live further, processing should
				 *   continue in config */
				DBG("not generating immediate reply for error %d\n", ser_error);
				tm_error=ser_error;
				ret = -4;
				goto done;
			}
#ifdef TM_DELAYED_REPLY
			/* current error in tm_error */
			tm_error=ser_error;
			set_kr(REQ_ERR_DELAYED);
			DBG("%d error reply generation delayed \n", ser_error);
#else

			reply_ret=kill_transaction( t, ser_error );
			if (reply_ret>0) {
				/* we have taken care of all -- do nothing in
			  	script */
				DBG("ERROR: generation of a stateful reply "
					"on error succeeded\n");
				/*ret=0; -- we don't want to stop the script */
			}  else {
				DBG("ERROR: generation of a stateful reply "
					"on error failed\n");
				t_release_transaction(t);
			}
#endif /* TM_DELAYED_REPLY */
		}else{
			t_release_transaction(t); /* kill it  silently */
		}
	} else {
		DBG( "SER: new transaction fwd'ed\n");
	}

done:
	return ret;
}



/*
 * Initialize parameters containing the ID of
 * AVPs with various timers
 */
int init_avp_params(char *fr_timer_param, char *fr_inv_timer_param)
{
	pv_spec_t avp_spec;
	unsigned short avp_type;

	if (fr_timer_param && *fr_timer_param) {
		fr_timer_str.s = fr_timer_param;
		fr_timer_str.len = strlen(fr_timer_str.s);
		LM_WARN("using AVP for TM fr_timer is deprecated,"
				" use t_set_fr(...) instead\n");

		if(fr_timer_str.s[0]==PV_MARKER) {
			if (pv_parse_spec(&fr_timer_str, &avp_spec)==0
			        || avp_spec.type!=PVT_AVP) {
			        LM_ERR("malformed or non AVP %s AVP definition\n",
							fr_timer_param);
				return -1;
			}

			if(pv_get_avp_name(0, &avp_spec.pvp, &fr_timer_avp, &avp_type)!=0)
			{
				LM_ERR("[%s]- invalid AVP definition\n", fr_timer_param);
				return -1;
			}
			fr_timer_avp_type = avp_type;
		} else {
			if (parse_avp_spec( &fr_timer_str, &fr_timer_avp_type,
			&fr_timer_avp, &fr_timer_index)<0) {
				LOG(L_CRIT,"ERROR:tm:init_avp_params: invalid fr_timer "
					"AVP specs \"%s\"\n", fr_timer_param);
				return -1;
			}
			/* ser flavour uses the To track of AVPs */
			fr_timer_avp_type |= AVP_TRACK_TO;
		}
	}

	if (fr_inv_timer_param && *fr_inv_timer_param) {
		fr_inv_timer_str.s = fr_inv_timer_param;
		fr_inv_timer_str.len = strlen(fr_inv_timer_str.s);
		LM_WARN("using AVP for TM fr_inv_timer is deprecated,"
				" use t_set_fr(...) instead\n");

		if(fr_inv_timer_str.s[0]==PV_MARKER) {
			if (pv_parse_spec(&fr_inv_timer_str, &avp_spec)==0
					|| avp_spec.type!=PVT_AVP) {
				LM_ERR("malformed or non AVP %s AVP definition\n",
					fr_inv_timer_param);
				return -1;
			}

			if(pv_get_avp_name(0, &avp_spec.pvp, &fr_inv_timer_avp,
								&avp_type)!=0)
			{
				LM_ERR("[%s]- invalid AVP definition\n", fr_inv_timer_param);
				return -1;
			}
			fr_inv_timer_avp_type = avp_type;
		} else {
			if (parse_avp_spec( &fr_inv_timer_str, &fr_inv_timer_avp_type, 
			&fr_inv_timer_avp, &fr_inv_timer_index)<0) {
				LOG(L_CRIT,"ERROR:tm:init_avp_params: invalid fr_inv_timer "
					"AVP specs \"%s\"\n", fr_inv_timer_param);
				return -1;
			}
			/* ser flavour uses the To track of AVPs */
			fr_inv_timer_avp_type |= AVP_TRACK_TO;
		}
	}

	return 0;
}


/** Get the FR_{INV}_TIMER from corresponding AVP.
 * @return 0 on success (use *timer) or 1 on failure (avp non-existent,
 *  avp present  but empty/0, avp value not numeric).
 */
static inline int avp2timer(unsigned int* timer, int type, int_str name)
{
	struct usr_avp *avp;
	int_str val_istr;
	int err;

	avp = search_first_avp(type, name, &val_istr, 0);
	if (!avp) {
		/*
		 DBG("avp2timer: AVP '%.*s' not found\n", param.s->len, ZSW(param.s->s));
		 */
		return 1;
	}
	
	if (avp->flags & AVP_VAL_STR) {
		*timer = str2s(val_istr.s.s, val_istr.s.len, &err);
		if (err) {
			LOG(L_ERR, "avp2timer: Error while converting string to integer\n");
			return -1;
		}
	} else {
		*timer = val_istr.n;
	}
	return *timer==0; /* 1 if 0 (use default), 0 if !=0 (use *timer) */
}


int fr_avp2timer(unsigned int* timer)
{
	if (fr_timer_avp.n!=0)
		return avp2timer( timer, fr_timer_avp_type, fr_timer_avp);
	else
		return 1;
}


int fr_inv_avp2timer(unsigned int* timer)
{
	if (fr_inv_timer_avp.n!=0)
		return avp2timer( timer, fr_inv_timer_avp_type, fr_inv_timer_avp);
	else
		return 1;
}

void unref_cell(struct cell *t)
{
	UNREF(t);
}
