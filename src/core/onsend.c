/*
 * Copyright (C) 2005 iptelorg GmbH
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
/*!
 * \file
 * \brief Kamailio core :: IP address handling
 * \author andrei
 * \ingroup core
 * Module: \ref core
 */


#include "onsend.h"

onsend_info_t *p_onsend=0; /* onsend route send info */


/*
 * returns: 0 drop the message, >= ok, <0 error (but forward the message)
 * it also migh change dst->send_flags!
 * WARNING: buf must be 0 terminated (to allow regex matches on it) */
int run_onsend(sip_msg_t* orig_msg, dest_info_t* dst, char* buf, int len)
{
	onsend_info_t onsnd_info = {0};
	int ret;
	run_act_ctx_t ra_ctx;
	run_act_ctx_t *bctx;
	int backup_route_type;
	snd_flags_t fwd_snd_flags_bak;
	snd_flags_t rpl_snd_flags_bak;
	sr_kemi_eng_t *keng = NULL;

	if(orig_msg==NULL || dst==NULL || buf==NULL) {
		LM_DBG("required parameters are not available - ignoring\n");
		return 1;
	}
	ret=1;
	// do if onsend_route{} or cfgengine exists
	if(kemi_onsend_route_callback.len>0) {
		keng = sr_kemi_eng_get();
	}
	if (onsend_rt.rlist[DEFAULT_RT] || keng){
		onsnd_info.to=&dst->to;
		onsnd_info.send_sock=dst->send_sock;
		onsnd_info.buf=buf;
		onsnd_info.len=len;
		onsnd_info.msg=orig_msg;
		p_onsend=&onsnd_info;
		backup_route_type=get_route_type();
		set_route_type(ONSEND_ROUTE);
		if (exec_pre_script_cb(orig_msg, ONSEND_CB_TYPE)>0) {
			/* backup orig_msg send flags */
			fwd_snd_flags_bak=orig_msg->fwd_send_flags;
			rpl_snd_flags_bak=orig_msg->rpl_send_flags;
			orig_msg->fwd_send_flags=dst->send_flags; /* initial value */
			init_run_actions_ctx(&ra_ctx);

			if(keng) {
				bctx = sr_kemi_act_ctx_get();
				sr_kemi_act_ctx_set(&ra_ctx);
				ret=sr_kemi_route(keng, orig_msg, ONSEND_ROUTE, NULL, NULL);
				sr_kemi_act_ctx_set(bctx);
			} else {
				ret=run_actions(&ra_ctx, onsend_rt.rlist[DEFAULT_RT], orig_msg);
			}

			/* update dst send_flags */
			dst->send_flags=orig_msg->fwd_send_flags;
			/* restore orig_msg flags */
			orig_msg->fwd_send_flags=fwd_snd_flags_bak;
			orig_msg->rpl_send_flags=rpl_snd_flags_bak;
			exec_post_script_cb(orig_msg, ONSEND_CB_TYPE);
			if((ret==0) && !(ra_ctx.run_flags&DROP_R_F)){
				ret = 1;
			}
		} else {
			ret=0; /* drop the message */
		}
		set_route_type(backup_route_type);
		p_onsend=0; /* reset it */
	}
	return ret;
}

/**
 * return: 0 drop the message
 *    >= ok
 *    <0 error (but forward the message)
 * it also migh change sndinfo dst->send_flags!
 * WARNING: sndinfo buf must be 0 terminated (to allow regex matches on it)
 */
int run_onsend_evroute(onsend_info_t *sndinfo, int evrt, str *evcb, str *evname)
{
	int ret;
	run_act_ctx_t ra_ctx;
	run_act_ctx_t *bctx;
	int backup_route_type;
	snd_flags_t fwd_snd_flags_bak;
	snd_flags_t rpl_snd_flags_bak;
	sr_kemi_eng_t *keng = NULL;

	if(sndinfo==NULL || sndinfo->dst==NULL
			|| sndinfo->msg==NULL || sndinfo->buf==NULL) {
		LM_DBG("required parameters are not available - ignoring\n");
		return 1;
	}
	ret=1;
	// do if onsend_route{} or cfgengine exists
	if(evcb!=NULL && evcb->len>0) {
		keng = sr_kemi_eng_get();
	}
	if (evrt<0 && keng==NULL) {
		return 1;
	}

	p_onsend = sndinfo;
	backup_route_type=get_route_type();
	set_route_type(EVENT_ROUTE);
	/* backup orig_msg send flags */
	fwd_snd_flags_bak=sndinfo->msg->fwd_send_flags;
	rpl_snd_flags_bak=sndinfo->msg->rpl_send_flags;
	sndinfo->msg->fwd_send_flags=sndinfo->dst->send_flags; /* initial value */
	init_run_actions_ctx(&ra_ctx);

	if(keng) {
		bctx = sr_kemi_act_ctx_get();
		sr_kemi_act_ctx_set(&ra_ctx);
		ret=sr_kemi_route(keng, sndinfo->msg, EVENT_ROUTE, evcb, evname);
		sr_kemi_act_ctx_set(bctx);
	} else {
		ret=run_actions(&ra_ctx, event_rt.rlist[evrt], sndinfo->msg);
	}

	/* update dst send_flags */
	sndinfo->dst->send_flags=sndinfo->msg->fwd_send_flags;
	/* restore orig_msg flags */
	sndinfo->msg->fwd_send_flags=fwd_snd_flags_bak;
	sndinfo->msg->rpl_send_flags=rpl_snd_flags_bak;
	if((ret==0) && !(ra_ctx.run_flags&DROP_R_F)){
		ret = 1;
	}
	set_route_type(backup_route_type);
	p_onsend=0; /* reset it */

	return ret;
}
