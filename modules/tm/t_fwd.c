/*
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

#include "defs.h"


#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "../../timer.h"
#include "../../hash_func.h"
#include "../../globals.h"
#include "../../cfg_core.h"
#include "../../mem/mem.h"
#include "../../dset.h"
#include "../../action.h"
#include "../../data_lump.h"
#include "../../onsend.h"
#include "../../compiler_opt.h"
#include "../../route.h"
#include "../../script_cb.h"
#include "t_funcs.h"
#include "t_hooks.h"
#include "t_msgbuilder.h"
#include "ut.h"
#include "t_cancel.h"
#include "t_lookup.h"
#include "t_fwd.h"
#include "t_reply.h"
#include "h_table.h"
#include "../../fix_lumps.h"
#include "config.h"
#ifdef USE_DNS_FAILOVER
#include "../../dns_cache.h"
#include "../../cfg_core.h" /* cfg_get(core, core_cfg, use_dns_failover) */
#include "../../msg_translator.h"
#include "lw_parser.h"
#endif
#ifdef USE_DST_BLACKLIST
#include "../../dst_blacklist.h"
#endif
#include "../../atomic_ops.h" /* membar_depends() */


extern int tm_failure_exec_mode;
extern int tm_dns_reuse_rcv_socket;

static int goto_on_branch = 0, branch_route = 0;

void t_on_branch( unsigned int go_to )
{
	struct cell *t = get_t();

       /* in REPLY_ROUTE and FAILURE_ROUTE T will be set to current transaction;
        * in REQUEST_ROUTE T will be set only if the transaction was already
        * created; if not -> use the static variable */
	if (!t || t==T_UNDEFINED ) {
		goto_on_branch=go_to;
	} else {
		t->on_branch = go_to;
	}
}

unsigned int get_on_branch(void)
{
	return goto_on_branch;
}

void set_branch_route( unsigned int on_branch)
{
	branch_route = on_branch;
}




/** prepares a new branch "buffer".
 * Creates the buffer used in the branch rb, fills everything needed (
   the sending information: t->uac[branch].request.dst, branch buffer, uri
   path vector a.s.o.) and runs the on_branch route.
 * t->uac[branch].request.dst will be filled if next_hop !=0 with the result
 * of the DNS resolution (next_hop, fproto and fsocket).
 * If next_hop is 0 all the dst members except the send_flags are read-only
 * (send_flags it's updated) and are supposed to be pre-filled.
 *
 * @param t  - transaction
 * @param i_req - corresponding sip_msg, must be non-null, flags might be
 *                be modified (on_branch route)
 * @param branch - branch no
 * @param uri
 * @param path  - path vector (list of route like destination in text form,
 *                 e.g.: "<sip:1.2.3.4;lr>, <sip:5.6.7.8;lr>")
 * @param next_hop - uri of the next hop. If non 0 it will be used
 *              for DNS resolution and the branch request.dst structure will
 *              be filled. If 0 the branch must already have
 *              a pre-filled valid request.dst.
 * @param fsocket - forced send socket for forwarding.
 * @param send_flags - special flags for sending (see SND_F_* / snd_flags_t).
 * @param fproto - forced proto for forwarding. Used only if next_hop!=0.
 * @param flags - 0, UAC_DNS_FAILOVER_F or UAC_SKIP_BR_DST_F for now.
 *
 * @return  0 on success, < 0 (ser_errror E***) on failure.
 */
static int prepare_new_uac( struct cell *t, struct sip_msg *i_req,
									int branch, str *uri, str* path,
									str* next_hop,
									struct socket_info* fsocket,
									snd_flags_t snd_flags,
									int fproto, int flags,
									str *instance, str *ruid,
									str *location_ua)
{
	char *shbuf;
	struct lump* add_rm_backup, *body_lumps_backup;
	struct sip_uri parsed_uri_bak;
	int ret;
	unsigned int len;
	int parsed_uri_ok_bak, free_new_uri;
	str msg_uri_bak;
	str dst_uri_bak;
	int dst_uri_backed_up;
	str path_bak;
	int free_path;
	str instance_bak;
	int free_instance;
	str ruid_bak;
	int free_ruid;
	str ua_bak;
	int free_ua;
	int backup_route_type;
	snd_flags_t fwd_snd_flags_bak;
	snd_flags_t rpl_snd_flags_bak;
	struct socket_info *force_send_socket_bak;
	struct dest_info *dst;
	struct run_act_ctx ctx;

	shbuf=0;
	ret=E_UNSPEC;
	msg_uri_bak.s=0; /* kill warnings */
	msg_uri_bak.len=0;
	parsed_uri_ok_bak=0;
	free_new_uri=0;
	dst_uri_bak.s=0;
	dst_uri_bak.len=0;
	dst_uri_backed_up=0;
	path_bak.s=0;
	path_bak.len=0;
	free_path=0;
	instance_bak.s=0;
	instance_bak.len=0;
	free_instance=0;
	ruid_bak.s=0;
	ruid_bak.len=0;
	free_ruid=0;
	ua_bak.s=0;
	ua_bak.len=0;
	free_ua=0;
	dst=&t->uac[branch].request.dst;

	/* ... we calculate branch ... */	
	if (!t_calc_branch(t, branch, i_req->add_to_branch_s,
			&i_req->add_to_branch_len ))
	{
		LOG(L_ERR, "ERROR: print_uac_request: branch computation failed\n");
		ret=E_UNSPEC;
		goto error00;
	}

	/* dup lumps
	 * TODO: clone lumps only if needed */
	/* lumps can be set outside of the lock, make sure that we read
	 * the up-to-date values */
	membar_depends();
	add_rm_backup = i_req->add_rm;
	body_lumps_backup = i_req->body_lumps;
	if (unlikely(i_req->add_rm)){
		i_req->add_rm = dup_lump_list(i_req->add_rm);
		if (unlikely(i_req->add_rm==0)){
			ret=E_OUT_OF_MEM;
			goto error04;
		}
	}
	if (unlikely(i_req->body_lumps)){
		i_req->body_lumps = dup_lump_list(i_req->body_lumps);
		if (unlikely(i_req->body_lumps==0)){
			ret=E_OUT_OF_MEM;
			goto error04;
		}
	}
	/* backup uri & path: we need to change them so that build_req...()
	   will use uri & path and not the ones in the original msg (i_req)
	   => we must back them up so that we can restore them to the original
	   value after building the send buffer */
	msg_uri_bak=i_req->new_uri;
	parsed_uri_bak=i_req->parsed_uri;
	parsed_uri_ok_bak=i_req->parsed_uri_ok;
	path_bak=i_req->path_vec;
	instance_bak=i_req->instance;
	ruid_bak=i_req->ruid;
	ua_bak=i_req->location_ua;
	
	if (unlikely(branch_route || has_tran_tmcbs(t, TMCB_REQUEST_FWDED))){
		/* dup uris, path a.s.o. if we have a branch route or callback */
		/* ... set ruri ... */
		/* if uri points to new_uri, it needs to be "fixed" so that we can
		   change msg->new_uri */
		if (uri==&i_req->new_uri)
			uri=&msg_uri_bak;
		i_req->parsed_uri_ok=0;
		i_req->new_uri.s=pkg_malloc(uri->len);
		if (unlikely(i_req->new_uri.s==0)){
			ret=E_OUT_OF_MEM;
			goto error03;
		}
		free_new_uri=1;
		memcpy(i_req->new_uri.s, uri->s, uri->len);
		i_req->new_uri.len=uri->len;
	
		/* update path_vec */
		/* if path points to msg path_vec, it needs to be "fixed" so that we 
		   can change/update msg->path_vec */
		if (path==&i_req->path_vec)
			path=&path_bak;
		/* zero it first so that set_path_vector will work */
		i_req->path_vec.s=0;
		i_req->path_vec.len=0;
		if (unlikely(path)){
			if (unlikely(set_path_vector(i_req, path)<0)){
				ret=E_OUT_OF_MEM;
				goto error03;
			}
			free_path=1;
		}
		/* update instance */
		/* if instance points to msg instance, it needs to be "fixed" so that we 
		   can change/update msg->instance */
		if (instance==&i_req->instance)
			instance=&instance_bak;
		/* zero it first so that set_instance will work */
		i_req->instance.s=0;
		i_req->instance.len=0;
		if (unlikely(instance)){
			if (unlikely(set_instance(i_req, instance)<0)){
				ret=E_OUT_OF_MEM;
				goto error03;
			}
			free_instance=1;
		}

		/* update ruid */
		/* if ruid points to msg ruid, it needs to be "fixed" so that we 
		   can change/update msg->ruid */
		if (ruid==&i_req->ruid)
			ruid=&ruid_bak;
		/* zero it first so that set_ruid will work */
		i_req->ruid.s=0;
		i_req->ruid.len=0;
		if (unlikely(ruid)){
			if (unlikely(set_ruid(i_req, ruid)<0)){
				ret=E_OUT_OF_MEM;
				goto error03;
			}
			free_ruid=1;
		}

		/* update location_ua */
		/* if location_ua points to msg location_ua, it needs to be "fixed" so that we 
		   can change/update msg->location_ua */
		if (location_ua==&i_req->location_ua)
			location_ua=&ua_bak;
		/* zero it first so that set_ua will work */
		i_req->location_ua.s=0;
		i_req->location_ua.len=0;
		if (unlikely(location_ua)){
			if (unlikely(set_ua(i_req, location_ua)<0)){
				ret=E_OUT_OF_MEM;
				goto error03;
			}
			free_ua=1;
		}
	
		/* backup dst uri  & zero it*/
		dst_uri_bak=i_req->dst_uri;
		dst_uri_backed_up=1;
		/* if next_hop points to dst_uri, it needs to be "fixed" so that we
		   can change msg->dst_uri */
		if (next_hop==&i_req->dst_uri)
			next_hop=&dst_uri_bak;
		/* zero it first so that set_dst_uri will work */
		i_req->dst_uri.s=0;
		i_req->dst_uri.len=0;
		if (likely(next_hop)){
			if(unlikely((flags & UAC_SKIP_BR_DST_F)==0)){
				/* set dst uri to next_hop for the on_branch route */
				if (unlikely(set_dst_uri(i_req, next_hop)<0)){
					ret=E_OUT_OF_MEM;
					goto error03;
				}
			}
		}

		if (likely(branch_route)) {
			/* run branch_route actions if provided */
			backup_route_type = get_route_type();
			set_route_type(BRANCH_ROUTE);
			tm_ctx_set_branch_index(branch);
			/* no need to backup/set avp lists: the on_branch route is run
			   only in the main route context (e.g. t_relay() in the main
			   route) or in the failure route context (e.g. append_branch &
			   t_relay()) and in both cases the avp lists are properly set
			   Note: the branch route is not run on delayed dns failover 
			   (for that to work one would have to set branch_route prior to
			   calling add_uac(...) and then reset it afterwards).
			   (
			 */
			if (exec_pre_script_cb(i_req, BRANCH_CB_TYPE)>0) {
				/* backup ireq msg send flags and force_send_socket*/
				fwd_snd_flags_bak=i_req->fwd_send_flags;;
				rpl_snd_flags_bak=i_req->rpl_send_flags;
				force_send_socket_bak=i_req->force_send_socket;
				/* set the new values */
				i_req->fwd_send_flags=snd_flags /* intial value  */;
				set_force_socket(i_req, fsocket);
				if (run_top_route(branch_rt.rlist[branch_route], i_req, &ctx)
						< 0)
				{
					LOG(L_DBG, "negative return code in run_top_route\n");
				}
				/* update dst send_flags  and send socket*/
				snd_flags=i_req->fwd_send_flags;
				fsocket=i_req->force_send_socket;
				/* restore ireq_msg force_send_socket & flags */
				set_force_socket(i_req, force_send_socket_bak);
				i_req->fwd_send_flags=fwd_snd_flags_bak;
				i_req->rpl_send_flags=rpl_snd_flags_bak;
				exec_post_script_cb(i_req, BRANCH_CB_TYPE);
				/* if DROP was called in cfg, don't forward, jump to end */
				if (unlikely(ctx.run_flags&DROP_R_F))
				{
					tm_ctx_set_branch_index(T_BR_UNDEFINED);
					set_route_type(backup_route_type);
					/* triggered by drop in CFG */
					ret=E_CFG;
					goto error03;
				}
			}
			tm_ctx_set_branch_index(T_BR_UNDEFINED);
			set_route_type(backup_route_type);
		}

		/* run the specific callbacks for this transaction */
		if (unlikely(has_tran_tmcbs(t, TMCB_REQUEST_FWDED)))
			run_trans_callbacks( TMCB_REQUEST_FWDED , t, i_req, 0,
									-i_req->REQ_METHOD);
		
		if (likely( !(flags & UAC_DNS_FAILOVER_F) && i_req->dst_uri.s &&
					i_req->dst_uri.len)){
			/* no dns failover and non-empty dst_uri => use it as dst
			  (on dns failover dns_h will be non-empty => next_hop will be
			   ignored) */
			next_hop=&i_req->dst_uri;
		}
		/* no path vector initially, but now is set after branch route and
		 * callbacks execution */
		if(i_req->path_vec.s!=0 && free_path==0)
			free_path=1;
	}else{
		/* no branch route and no TMCB_REQUEST_FWDED callback => set
		   msg uri and path to the new values (if needed) */
		if (unlikely((uri->s!=i_req->new_uri.s || uri->len!=i_req->new_uri.len)
					&& (i_req->new_uri.s!=0 ||
						uri->s!=i_req->first_line.u.request.uri.s ||
						uri->len!=i_req->first_line.u.request.uri.len) )){
			/* uri is different from i_req uri => replace i_req uri and force
			   uri re-parsing */
			i_req->new_uri=*uri;
			i_req->parsed_uri_ok=0;
		}
		if (unlikely(path && (i_req->path_vec.s!=path->s ||
							  i_req->path_vec.len!=path->len))){
			i_req->path_vec=*path;
		}else if (unlikely(path==0 && i_req->path_vec.len!=0)){
			i_req->path_vec.s=0;
			i_req->path_vec.len=0;
		}
		if (unlikely(instance && (i_req->instance.s!=instance->s ||
							  i_req->instance.len!=instance->len))){
			i_req->instance=*instance;
		}else if (unlikely(instance==0 && i_req->instance.len!=0)){
			i_req->instance.s=0;
			i_req->instance.len=0;
		}
		if (unlikely(ruid && (i_req->ruid.s!=ruid->s ||
							  i_req->ruid.len!=ruid->len))){
			i_req->ruid=*ruid;
		}else if (unlikely(ruid==0 && i_req->ruid.len!=0)){
			i_req->ruid.s=0;
			i_req->ruid.len=0;
		}
		if (unlikely(location_ua && (i_req->location_ua.s!=location_ua->s ||
							  i_req->location_ua.len!=location_ua->len))){
			i_req->location_ua=*location_ua;
		}else if (unlikely(location_ua==0 && i_req->location_ua.len!=0)){
			i_req->location_ua.s=0;
			i_req->location_ua.len=0;
		}
	}
	
	if (likely(next_hop!=0 || (flags & UAC_DNS_FAILOVER_F))){
		/* next_hop present => use it for dns resolution */
#ifdef USE_DNS_FAILOVER
		if (uri2dst2(&t->uac[branch].dns_h, dst, fsocket, snd_flags,
							next_hop?next_hop:uri, fproto) == 0)
#else
		/* dst filled from the uri & request (send_socket) */
		if (uri2dst2(dst, fsocket, snd_flags,
							next_hop?next_hop:uri, fproto)==0)
#endif
		{
			ret=E_BAD_ADDRESS;
			goto error01;
		}
	} /* else next_hop==0 =>
		no dst_uri / empty dst_uri and initial next_hop==0 =>
		dst is pre-filled with a valid dst => use the pre-filled dst */

	/* Set on_reply and on_negative handlers for this branch to the handlers in the transaction */
	t->uac[branch].on_reply = t->on_reply;
	t->uac[branch].on_failure = t->on_failure;
	t->uac[branch].on_branch_failure = t->on_branch_failure;

	/* check if send_sock is ok */
	if (t->uac[branch].request.dst.send_sock==0) {
		LOG(L_ERR, "ERROR: can't fwd to af %d, proto %d "
			" (no corresponding listening socket)\n",
			dst->to.s.sa_family, dst->proto );
		ret=E_NO_SOCKET;
		goto error01;
	}
	/* ... and build it now */
	shbuf=build_req_buf_from_sip_req( i_req, &len, dst, BUILD_IN_SHM);
	if (!shbuf) {
		LM_ERR("could not build request\n"); 
		ret=E_OUT_OF_MEM;
		goto error01;
	}
#ifdef DBG_MSG_QA
	if (shbuf[len-1]==0) {
		LOG(L_ERR, "ERROR: print_uac_request: sanity check failed\n");
		abort();
	}
#endif
	/* things went well, move ahead and install new buffer! */
	t->uac[branch].request.buffer=shbuf;
	t->uac[branch].request.buffer_len=len;
	t->uac[branch].uri.s=t->uac[branch].request.buffer+
							i_req->first_line.u.request.method.len+1;
	t->uac[branch].uri.len=GET_RURI(i_req)->len;
	if (unlikely(i_req->path_vec.s && i_req->path_vec.len)){
		t->uac[branch].path.s=shm_malloc(i_req->path_vec.len+1);
		if (unlikely(t->uac[branch].path.s==0)) {
			shm_free(shbuf);
			t->uac[branch].request.buffer=0;
			t->uac[branch].request.buffer_len=0;
			t->uac[branch].uri.s=0;
			t->uac[branch].uri.len=0;
			ret=E_OUT_OF_MEM;
			goto error01;
		}
		t->uac[branch].path.len=i_req->path_vec.len;
		t->uac[branch].path.s[i_req->path_vec.len]=0;
		memcpy( t->uac[branch].path.s, i_req->path_vec.s, i_req->path_vec.len);
	}
	if (unlikely(i_req->instance.s && i_req->instance.len)){
		t->uac[branch].instance.s=shm_malloc(i_req->instance.len+1);
		if (unlikely(t->uac[branch].instance.s==0)) {
			shm_free(shbuf);
			t->uac[branch].request.buffer=0;
			t->uac[branch].request.buffer_len=0;
			t->uac[branch].uri.s=0;
			t->uac[branch].uri.len=0;
			ret=E_OUT_OF_MEM;
			goto error01;
		}
		t->uac[branch].instance.len=i_req->instance.len;
		t->uac[branch].instance.s[i_req->instance.len]=0;
		memcpy( t->uac[branch].instance.s, i_req->instance.s, i_req->instance.len);
	}
	if (unlikely(i_req->ruid.s && i_req->ruid.len)){
		t->uac[branch].ruid.s=shm_malloc(i_req->ruid.len+1);
		if (unlikely(t->uac[branch].ruid.s==0)) {
			shm_free(shbuf);
			t->uac[branch].request.buffer=0;
			t->uac[branch].request.buffer_len=0;
			t->uac[branch].uri.s=0;
			t->uac[branch].uri.len=0;
			ret=E_OUT_OF_MEM;
			goto error01;
		}
		t->uac[branch].ruid.len=i_req->ruid.len;
		t->uac[branch].ruid.s[i_req->ruid.len]=0;
		memcpy( t->uac[branch].ruid.s, i_req->ruid.s, i_req->ruid.len);
	}
	if (unlikely(i_req->location_ua.s && i_req->location_ua.len)){
		t->uac[branch].location_ua.s=shm_malloc(i_req->location_ua.len+1);
		if (unlikely(t->uac[branch].location_ua.s==0)) {
			shm_free(shbuf);
			t->uac[branch].request.buffer=0;
			t->uac[branch].request.buffer_len=0;
			t->uac[branch].uri.s=0;
			t->uac[branch].uri.len=0;
			ret=E_OUT_OF_MEM;
			goto error01;
		}
		t->uac[branch].location_ua.len=i_req->location_ua.len;
		t->uac[branch].location_ua.s[i_req->location_ua.len]=0;
		memcpy( t->uac[branch].location_ua.s, i_req->location_ua.s, i_req->location_ua.len);
	}

#ifdef TM_UAC_FLAGS
	len = count_applied_lumps(i_req->add_rm, HDR_RECORDROUTE_T);
	if(len==1)
		t->uac[branch].flags = TM_UAC_FLAG_RR;
	else if(len==2)
		t->uac[branch].flags = TM_UAC_FLAG_RR|TM_UAC_FLAG_R2;
#endif

	ret=0;

error01:
error03:
	/* restore the new_uri & path from the backup */
	if (unlikely(free_new_uri && i_req->new_uri.s)){
			pkg_free(i_req->new_uri.s);
	}
	if (unlikely(free_path)){
		reset_path_vector(i_req);
	}
	if (unlikely(free_instance)){
		reset_instance(i_req);
	}
	if (unlikely(free_ruid)){
		reset_ruid(i_req);
	}
	if (unlikely(free_ua)){
		reset_ua(i_req);
	}
	if (dst_uri_backed_up){
		reset_dst_uri(i_req); /* free dst_uri */
		i_req->dst_uri=dst_uri_bak;
	}
	/* restore original new_uri and path values */
	i_req->new_uri=msg_uri_bak;
	i_req->parsed_uri=parsed_uri_bak;
	i_req->parsed_uri_ok=parsed_uri_ok_bak;
	i_req->path_vec=path_bak;
	i_req->instance=instance_bak;
	i_req->ruid=ruid_bak;
	i_req->location_ua=ua_bak;
	
	/* Delete the duplicated lump lists, this will also delete
	 * all lumps created here, such as lumps created in per-branch
	 * routing sections, Via, and Content-Length headers created in
	 * build_req_buf_from_sip_req
	 */
error04:
	free_duped_lump_list(i_req->add_rm);
	free_duped_lump_list(i_req->body_lumps);
	     /* Restore the lists from backups */
	i_req->add_rm = add_rm_backup;
	i_req->body_lumps = body_lumps_backup;

error00:
	return ret;
}

#ifdef USE_DNS_FAILOVER
/* Similar to print_uac_request(), but this function uses the outgoing message
   buffer of the failed branch to construct the new message in case of DNS 
   failover.

   WARNING: only the first VIA header is replaced in the buffer, the rest
   of the message is untouched, thus, the send socket is corrected only in the
   VIA HF.
*/
static char *print_uac_request_from_buf( struct cell *t, struct sip_msg *i_req,
	int branch, str *uri, unsigned int *len, struct dest_info* dst,
	char *buf, short buf_len)
{
	char *shbuf;
	str branch_str;
	char *via, *old_via_begin, *old_via_end;
	unsigned int via_len;

	shbuf=0;

	/* ... we calculate branch ... */	
	if (!t_calc_branch(t, branch, i_req->add_to_branch_s,
			&i_req->add_to_branch_len ))
	{
		LOG(L_ERR, "ERROR: print_uac_request_from_buf: branch computation failed\n");
		goto error00;
	}
	branch_str.s = i_req->add_to_branch_s;
	branch_str.len = i_req->add_to_branch_len;

	/* find the beginning of the first via header in the buffer */
	old_via_begin = lw_find_via(buf, buf+buf_len);
	if (!old_via_begin) {
		LOG(L_ERR, "ERROR: print_uac_request_from_buf: beginning of via header not found\n");
		goto error00;
	}
	/* find the end of the first via header in the buffer */
	old_via_end = lw_next_line(old_via_begin, buf+buf_len);
	if (!old_via_end) {
		LOG(L_ERR, "ERROR: print_uac_request_from_buf: end of via header not found\n");
		goto error00;
	}

	/* create the new VIA HF */
	via = create_via_hf(&via_len, i_req, dst, &branch_str);
	if (!via) {
		LOG(L_ERR, "ERROR: print_uac_request_from_buf: via building failed\n");
		goto error00;
	}

	/* allocate memory for the new buffer */
	*len = buf_len + via_len - (old_via_end - old_via_begin);
	shbuf=(char *)shm_malloc(*len);
	if (!shbuf) {
		ser_error=E_OUT_OF_MEM;
		LOG(L_ERR, "ERROR: print_uac_request_from_buf: no shmem\n");
		goto error01;
	}

	/* construct the new buffer */
	memcpy(shbuf, buf, old_via_begin-buf);
	memcpy(shbuf+(old_via_begin-buf), via, via_len);
	memcpy(shbuf+(old_via_begin-buf)+via_len, old_via_end, (buf+buf_len)-old_via_end);

#ifdef DBG_MSG_QA
	if (shbuf[*len-1]==0) {
		LOG(L_ERR, "ERROR: print_uac_request_from_buf: sanity check failed\n");
		abort();
	}
#endif

error01:
	pkg_free(via);
error00:
	return shbuf;
}
#endif

/* introduce a new uac, which is blind -- it only creates the
   data structures and starts FR timer, but that's it; it does
   not print messages and send anything anywhere; that is good
   for FIFO apps -- the transaction must look operationally
   and FR must be ticking, whereas the request is "forwarded"
   using a non-SIP way and will be replied the same way
*/
int add_blind_uac( /*struct cell *t*/ )
{
	unsigned short branch;
	struct cell *t;

	t=get_t();
	if (t==T_UNDEFINED || !t ) {
		LOG(L_ERR, "ERROR: add_blind_uac: no transaction context\n");
		return -1;
	}

	branch=t->nr_of_outgoings;	
	if (branch==sr_dst_max_branches) {
		LOG(L_ERR, "ERROR: add_blind_uac: "
			"maximum number of branches exceeded\n");
		return -1;
	}
	/* make sure it will be replied */
	t->flags |= T_NOISY_CTIMER_FLAG;
	membar_write(); /* to allow lockless prepare_to_cancel() we want to be sure
					   all the writes finished before updating branch number*/
	t->nr_of_outgoings=(branch+1);
	t->async_backup.blind_uac = branch; /* whenever we create a blind UAC, lets save the current branch
					 * this is used in async tm processing specifically to be able to route replies
					 * that were possibly in response to a request forwarded on this blind UAC......
					 * we still want replies to be processed as if it were a normal UAC */
	
	/* start FR timer -- protocol set by default to PROTO_NONE,
       which means retransmission timer will not be started
    */
	if (start_retr(&t->uac[branch].request)!=0)
		LOG(L_CRIT, "BUG: add_blind_uac: start retr failed for %p\n",
				&t->uac[branch].request);
	/* we are on a timer -- don't need to put on wait on script
	   clean-up	
	*/
	set_kr(REQ_FWDED); 

	return 1; /* success */
}

/** introduce a new uac to transaction.
 *  It doesn't send a message yet -- a reply to it might interfere with the
 *  processes of adding multiple branches; On error returns <0 & sets ser_error
 *  to the same value.
 *  @param t - transaction
 *  @param request - corresponding sip_mst, must be non-null, flags might be
 *                   modified (on_branch route).
 *  @param uri - uri used for the branch (must be non-null).
 *  @param next_hop - next_hop in sip uri format. If null and proxy is null
 *                    too, the uri will be used 
 *  @param path     - path vector (list of route like destinations in sip
 *                     uri format, e.g.: "<sip:1.2.3.4;lr>, <sip:5.6.7.8;lr>").
 *  @param proxy    - proxy structure. If non-null it takes precedence over
 *                    next_hop/uri and it will be used for forwarding.
 *  @param fsocket  - forced forward send socket (can be 0).
 *  @param snd_flags - special send flags (see SND_F_* / snd_flags_t)
 *  @param proto    - forced protocol for forwarding (overrides the protocol
 *                    in next_hop/uri or proxy if != PROTO_NONE).
 *  @param flags    - special flags passed to prepare_new_uac().
 *                    @see prepare_new_uac().
 *  @returns branch id (>=0) or error (<0)
*/
int add_uac( struct cell *t, struct sip_msg *request, str *uri,
					str* next_hop, str* path, struct proxy_l *proxy,
					struct socket_info* fsocket, snd_flags_t snd_flags,
					int proto, int flags, str *instance, str *ruid,
					str *location_ua)
{

	int ret;
	unsigned short branch;

	branch=t->nr_of_outgoings;
	if (branch==sr_dst_max_branches) {
		LOG(L_ERR, "ERROR: add_uac: maximum number of branches exceeded\n");
		ret=ser_error=E_TOO_MANY_BRANCHES;
		goto error;
	}

	/* check existing buffer -- rewriting should never occur */
	if (t->uac[branch].request.buffer) {
		LOG(L_CRIT, "ERROR: add_uac: buffer rewrite attempt\n");
		ret=ser_error=E_BUG;
		goto error;
	}

	/* check DNS resolution */
	if (proxy){
		/* dst filled from the proxy */
		init_dest_info(&t->uac[branch].request.dst);
		t->uac[branch].request.dst.proto=get_proto(proto, proxy->proto);
		proxy2su(&t->uac[branch].request.dst.to, proxy);
		/* fill dst send_sock */
		t->uac[branch].request.dst.send_sock =
		get_send_socket( request, &t->uac[branch].request.dst.to,
								t->uac[branch].request.dst.proto);
		if (request)
			t->uac[branch].request.dst.send_flags=request->fwd_send_flags;
		else
			SND_FLAGS_INIT(&t->uac[branch].request.dst.send_flags);
		next_hop=0;
	}else {
		next_hop= next_hop?next_hop:uri;
	}

	/* now message printing starts ... */
	if (unlikely( (ret=prepare_new_uac(t, request, branch, uri, path,
										next_hop, fsocket, snd_flags,
										proto, flags, instance, ruid,
										location_ua)) < 0)){
		ser_error=ret;
		goto error01;
	}
	getbflagsval(0, &t->uac[branch].branch_flags);
	membar_write(); /* to allow lockless ops (e.g. prepare_to_cancel()) we want
					   to be sure everything above is fully written before
					   updating branches no. */
	t->nr_of_outgoings=(branch+1);

	/* update stats */
	if (proxy){
		proxy_mark(proxy, 1);
	}
	/* done! */
	ret=branch;
		
error01:
error:
	return ret;
}



#ifdef USE_DNS_FAILOVER
/* Similar to add_uac(), but this function uses the outgoing message buffer of
   the failed branch to construct the new message in case of DNS failover.
*/
static int add_uac_from_buf( struct cell *t, struct sip_msg *request,
								str *uri, str* path,
								struct socket_info* fsocket,
								snd_flags_t send_flags,
								int proto,
								char *buf, short buf_len,
								str *instance, str *ruid,
								str *location_ua)
{

	int ret;
	unsigned short branch;
	char *shbuf;
	unsigned int len;

	branch=t->nr_of_outgoings;
	if (branch==sr_dst_max_branches) {
		LOG(L_ERR, "ERROR: add_uac_from_buf: maximum number of branches"
					" exceeded\n");
		ret=ser_error=E_TOO_MANY_BRANCHES;
		goto error;
	}

	/* check existing buffer -- rewriting should never occur */
	if (t->uac[branch].request.buffer) {
		LOG(L_CRIT, "ERROR: add_uac_from_buf: buffer rewrite attempt\n");
		ret=ser_error=E_BUG;
		goto error;
	}

	if (uri2dst2(&t->uac[branch].dns_h, &t->uac[branch].request.dst,
					fsocket, send_flags, uri, proto) == 0)
	{
		ret=ser_error=E_BAD_ADDRESS;
		goto error;
	}
	
	/* check if send_sock is ok */
	if (t->uac[branch].request.dst.send_sock==0) {
		LOG(L_ERR, "ERROR: add_uac_from_buf: can't fwd to af %d, proto %d "
			" (no corresponding listening socket)\n",
			t->uac[branch].request.dst.to.s.sa_family, 
			t->uac[branch].request.dst.proto );
		ret=ser_error=E_NO_SOCKET;
		goto error;
	}

	/* now message printing starts ... */
	shbuf=print_uac_request_from_buf( t, request, branch, uri,
							&len, &t->uac[branch].request.dst,
							buf, buf_len);
	if (!shbuf) {
		ret=ser_error=E_OUT_OF_MEM;
		goto error;
	}

	/* things went well, move ahead and install new buffer! */
	t->uac[branch].request.buffer=shbuf;
	t->uac[branch].request.buffer_len=len;
	t->uac[branch].uri.s=t->uac[branch].request.buffer+
		request->first_line.u.request.method.len+1;
	t->uac[branch].uri.len=uri->len;
	/* copy the path */
	if (unlikely(path && path->s)){
		t->uac[branch].path.s=shm_malloc(path->len+1);
		if (unlikely(t->uac[branch].path.s==0)) {
			shm_free(shbuf);
			t->uac[branch].request.buffer=0;
			t->uac[branch].request.buffer_len=0;
			t->uac[branch].uri.s=0;
			t->uac[branch].uri.len=0;
			ret=ser_error=E_OUT_OF_MEM;
			goto error;
		}
		t->uac[branch].path.len=path->len;
		t->uac[branch].path.s[path->len]=0;
		memcpy( t->uac[branch].path.s, path->s, path->len);
	}
	/* copy the instance */
	if (unlikely(instance && instance->s)){
		t->uac[branch].instance.s=shm_malloc(instance->len+1);
		if (unlikely(t->uac[branch].instance.s==0)) {
			shm_free(shbuf);
			t->uac[branch].request.buffer=0;
			t->uac[branch].request.buffer_len=0;
			t->uac[branch].uri.s=0;
			t->uac[branch].uri.len=0;
			ret=ser_error=E_OUT_OF_MEM;
			goto error;
		}
		t->uac[branch].instance.len=instance->len;
		t->uac[branch].instance.s[instance->len]=0;
		memcpy( t->uac[branch].instance.s, instance->s, instance->len);
	}
	/* copy the ruid */
	if (unlikely(ruid && ruid->s)){
		t->uac[branch].ruid.s=shm_malloc(ruid->len+1);
		if (unlikely(t->uac[branch].ruid.s==0)) {
			shm_free(shbuf);
			t->uac[branch].request.buffer=0;
			t->uac[branch].request.buffer_len=0;
			t->uac[branch].uri.s=0;
			t->uac[branch].uri.len=0;
			ret=ser_error=E_OUT_OF_MEM;
			goto error;
		}
		t->uac[branch].ruid.len=ruid->len;
		t->uac[branch].ruid.s[ruid->len]=0;
		memcpy( t->uac[branch].ruid.s, ruid->s, ruid->len);
	}
	/* copy the location_ua */
	if (unlikely(location_ua && location_ua->s)){
		t->uac[branch].location_ua.s=shm_malloc(location_ua->len+1);
		if (unlikely(t->uac[branch].location_ua.s==0)) {
			shm_free(shbuf);
			t->uac[branch].request.buffer=0;
			t->uac[branch].request.buffer_len=0;
			t->uac[branch].uri.s=0;
			t->uac[branch].uri.len=0;
			ret=ser_error=E_OUT_OF_MEM;
			goto error;
		}
		t->uac[branch].location_ua.len=location_ua->len;
		t->uac[branch].location_ua.s[location_ua->len]=0;
		memcpy( t->uac[branch].location_ua.s, location_ua->s, location_ua->len);
	}

	t->uac[branch].on_reply = t->on_reply;
	t->uac[branch].on_failure = t->on_failure;
	t->uac[branch].on_branch_failure = t->on_branch_failure;

	membar_write(); /* to allow lockless ops (e.g. prepare_to_cancel()) we want
					   to be sure everything above is fully written before
					   updating branches no. */
	t->nr_of_outgoings=(branch+1);

	/* done! */
	ret=branch;
		
error:
	return ret;
}

/* introduce a new uac to transaction, based on old_uac and a possible
 *  new ip address (if the dns name resolves to more ips). If no more
 *   ips are found => returns -1.
 *  returns its branch id (>=0)
   or error (<0) and sets ser_error if needed; it doesn't send a message 
   yet -- a reply to it
   might interfere with the processes of adding multiple branches
   if lock_replies is 1 replies will be locked for t until the new branch
   is added (to prevent add branches races). Use 0 if the reply lock is
   already held, e.g. in failure route/handlers (WARNING: using 1 in a 
   failure route will cause a deadlock).
*/
int add_uac_dns_fallback(struct cell *t, struct sip_msg* msg,
									struct ua_client* old_uac,
									int lock_replies)
{
	int ret;
	
	ret=-1;
	if (cfg_get(core, core_cfg, use_dns_failover) &&
			!((t->flags & (T_DONT_FORK|T_DISABLE_FAILOVER)) ||
				uac_dont_fork(old_uac)) &&
			dns_srv_handle_next(&old_uac->dns_h, 0)){
			if (lock_replies){
				/* use reply lock to guarantee nobody is adding a branch
				 * in the same time */
				LOCK_REPLIES(t);
				/* check again that we can fork */
				if ((t->flags & T_DONT_FORK) || uac_dont_fork(old_uac)){
					UNLOCK_REPLIES(t);
					DBG("add_uac_dns_fallback: no forking on => no new"
							" branches\n");
					return ret;
				}
			}
			if (t->nr_of_outgoings >= sr_dst_max_branches){
				LOG(L_ERR, "ERROR: add_uac_dns_fallback: maximum number of "
							"branches exceeded\n");
				if (lock_replies)
					UNLOCK_REPLIES(t);
					ret=ser_error=E_TOO_MANY_BRANCHES;
				return ret;
			}
			/* copy the dns handle into the new uac */
			dns_srv_handle_cpy(&t->uac[t->nr_of_outgoings].dns_h,
								&old_uac->dns_h);

			if (cfg_get(tm, tm_cfg, reparse_on_dns_failover)){
				/* Reuse the old buffer and only replace the via header.
				 * The drawback is that the send_socket is not corrected
				 * in the rest of the message, only in the VIA HF (Miklos) */
				ret=add_uac_from_buf(t,  msg, &old_uac->uri,
							&old_uac->path,
							 (old_uac->request.dst.send_flags.f &
								SND_F_FORCE_SOCKET)?
									old_uac->request.dst.send_sock:
									((tm_dns_reuse_rcv_socket)
											?msg->rcv.bind_address:0),
							old_uac->request.dst.send_flags,
							old_uac->request.dst.proto,
							old_uac->request.buffer,
							old_uac->request.buffer_len,
							&old_uac->instance, &old_uac->ruid,
							&old_uac->location_ua);
			} else {
				/* add_uac will use dns_h => next_hop will be ignored.
				 * Unfortunately we can't reuse the old buffer, the branch id
				 *  must be changed and the send_socket might be different =>
				 *  re-create the whole uac */
				ret=add_uac(t,  msg, &old_uac->uri, 0, &old_uac->path, 0,
							 (old_uac->request.dst.send_flags.f &
								SND_F_FORCE_SOCKET)?
									old_uac->request.dst.send_sock:
									((tm_dns_reuse_rcv_socket)
											?msg->rcv.bind_address:0),
							old_uac->request.dst.send_flags,
							old_uac->request.dst.proto, UAC_DNS_FAILOVER_F,
							&old_uac->instance, &old_uac->ruid,
							&old_uac->location_ua);
			}

			if (ret<0){
				/* failed, delete the copied dns_h */
				dns_srv_handle_put(&t->uac[t->nr_of_outgoings].dns_h);
			}
			if (lock_replies){
				UNLOCK_REPLIES(t);
			}
	}
	return ret;
}

#endif

int e2e_cancel_branch( struct sip_msg *cancel_msg, struct cell *t_cancel, 
	struct cell *t_invite, int branch )
{
	int ret;
	char *shbuf;
	unsigned int len;
	snd_flags_t snd_flags;

	ret=-1;
	if (t_cancel->uac[branch].request.buffer) {
		LOG(L_CRIT, "ERROR: e2e_cancel_branch: buffer rewrite attempt\n");
		ret=ser_error=E_BUG;
		goto error;
	}
	if (t_invite->uac[branch].request.buffer==0){
		/* inactive / deleted  branch */
		goto error;
	}
	t_invite->uac[branch].request.flags|=F_RB_CANCELED;

	/* note -- there is a gap in proxy stats -- we don't update 
	   proxy stats with CANCEL (proxy->ok, proxy->tx, etc.)
	*/
	
	/* set same dst as the invite */
	t_cancel->uac[branch].request.dst=t_invite->uac[branch].request.dst;
	/* print */
	if (cfg_get(tm, tm_cfg, reparse_invite)) {
		/* buffer is built localy from the INVITE which was sent out */
		/* lumps can be set outside of the lock, make sure that we read
		 * the up-to-date values */
		membar_depends();
		if (cancel_msg->add_rm || cancel_msg->body_lumps) {
			LOG(L_WARN, "WARNING: e2e_cancel_branch: CANCEL is built locally, "
			"thus lumps are not applied to the message!\n");
		}
		shbuf=build_local_reparse( t_invite, branch, &len, CANCEL,
									CANCEL_LEN, &t_invite->to
#ifdef CANCEL_REASON_SUPPORT
									, 0
#endif /* CANCEL_REASON_SUPPORT */
									);
		if (unlikely(!shbuf)) {
			LOG(L_ERR, "e2e_cancel_branch: printing e2e cancel failed\n");
			ret=ser_error=E_OUT_OF_MEM;
			goto error;
		}
		/* install buffer */
		t_cancel->uac[branch].request.buffer=shbuf;
		t_cancel->uac[branch].request.buffer_len=len;
		t_cancel->uac[branch].uri.s=t_cancel->uac[branch].request.buffer+
			cancel_msg->first_line.u.request.method.len+1;
		t_cancel->uac[branch].uri.len=t_invite->uac[branch].uri.len;
	} else {
		SND_FLAGS_INIT(&snd_flags);
		/* buffer is constructed from the received CANCEL with lumps applied */
		/*  t_cancel...request.dst is already filled (see above) */
		if (unlikely((ret=prepare_new_uac( t_cancel, cancel_msg, branch,
									&t_invite->uac[branch].uri,
									&t_invite->uac[branch].path,
									0, 0, snd_flags, PROTO_NONE, 0,
									NULL, NULL, NULL)) <0)){
			ser_error=ret;
			goto error;
		}
	}
	/* success */
	ret=1;

error:
	return ret;
}



#ifdef CANCEL_REASON_SUPPORT
/** create a cancel reason structure packed into a single shm. block.
  * From a cause and a pointer to a str or cancel_msg, build a
  * packed cancel reason structure (CANCEL_REAS_PACKED_HDRS), using a
  * single memory allocation (so that it can be freed by a simple shm_free().
  * @param cause - cancel cause, @see cancel_reason for more details.
  * @param data - depends on the cancel cause.
  * @return pointer to shm. packed cancel reason struct. on success,
  *        0 on error
  */
static struct cancel_reason* cancel_reason_pack(short cause, void* data,
													struct cell* t)
{
	char* d;
	struct cancel_reason* cr;
	int reason_len;
	int code_len;
	struct hdr_field *reas1, *reas_last, *hdr;
	str* txt;
	struct sip_msg* e2e_cancel;
	
	if (likely(cause != CANCEL_REAS_UNKNOWN)){
		reason_len = 0;
		txt = 0;
		e2e_cancel = 0;
		reas1 = 0;
		reas_last = 0;
		if (likely(cause == CANCEL_REAS_RCVD_CANCEL &&
					data && !(t->flags & T_NO_E2E_CANCEL_REASON))) {
			/* parse the entire cancel, to get all the Reason headers */
			e2e_cancel = data;
			parse_headers(e2e_cancel, HDR_EOH_F, 0);
			for(hdr=get_hdr(e2e_cancel, HDR_REASON_T), reas1=hdr;
					hdr; hdr=next_sibling_hdr(hdr)) {
				/* hdr->len includes CRLF */
				reason_len += hdr->len;
				reas_last=hdr;
			}
		} else if (likely(cause > 0 &&
					cfg_get(tm, tm_cfg, local_cancel_reason))){
			txt = (str*) data;
			/* Reason: SIP;cause=<reason->cause>[;text=<reason->u.text.s>] */
			reason_len = REASON_PREFIX_LEN + USHORT2SBUF_MAX_LEN +
				((txt && txt->s)?
					REASON_TEXT_LEN + 1 + txt->len + 1 : 0) +
				CRLF_LEN;
		} else if (cause == CANCEL_REAS_PACKED_HDRS &&
					!(t->flags & T_NO_E2E_CANCEL_REASON) && data) {
			txt = (str*) data;
			reason_len = txt?txt->len:0;
		} else if (unlikely(cause < CANCEL_REAS_MIN)) {
			BUG("unhandled reason cause %d\n", cause);
			goto error;
		}
		
		if (unlikely(reason_len == 0))
			return 0; /* nothing to do, no reason */
		cr = shm_malloc(sizeof(struct cancel_reason) + reason_len);
		if (unlikely(cr == 0))
				goto error;
		d = (char*)cr +sizeof(*cr);
		cr->cause = CANCEL_REAS_PACKED_HDRS;
		cr->u.packed_hdrs.s = d;
		cr->u.packed_hdrs.len = reason_len;
		
		if (cause == CANCEL_REAS_RCVD_CANCEL) {
			for(hdr=reas1; hdr; hdr=next_sibling_hdr(hdr)) {
				/* hdr->len includes CRLF */
				append_str(d, hdr->name.s, hdr->len);
				if (likely(hdr==reas_last))
					break;
			}
		} else if (likely(cause > 0)) {
			append_str(d, REASON_PREFIX, REASON_PREFIX_LEN);
			code_len=ushort2sbuf(cause, d, reason_len - 
									(int)(d - (char*)cr - sizeof(*cr)));
			if (unlikely(code_len==0)) {
				shm_free(cr);
				cr = 0;
				BUG("not enough space to write reason code");
				goto error;
			}
			d+=code_len;
			if (txt && txt->s){
				append_str(d, REASON_TEXT, REASON_TEXT_LEN);
				*d='"'; d++;
				append_str(d, txt->s, txt->len);
				*d='"'; d++;
			}
			append_str(d, CRLF, CRLF_LEN);
		} else if (cause == CANCEL_REAS_PACKED_HDRS) {
			append_str(d, txt->s, txt->len);
		}
		return cr;
	}
error:
	return 0;
}
#endif /* CANCEL_REASON_SUPPORT */



void e2e_cancel( struct sip_msg *cancel_msg,
	struct cell *t_cancel, struct cell *t_invite )
{
	branch_bm_t cancel_bm;
#ifndef E2E_CANCEL_HOP_BY_HOP
	branch_bm_t tmp_bm;
#elif defined (CANCEL_REASON_SUPPORT)
	struct cancel_reason* reason;
	int free_reason;
#endif /* E2E_CANCEL_HOP_BY_HOP */
	int i;
	int lowest_error;
	int ret;
	struct tmcb_params tmcb;

	cancel_bm=0;
	lowest_error=0;

	if (unlikely(has_tran_tmcbs(t_invite, TMCB_E2ECANCEL_IN))){
		INIT_TMCB_PARAMS(tmcb, cancel_msg, 0, cancel_msg->REQ_METHOD);
		run_trans_callbacks_internal(&t_invite->tmcb_hl, TMCB_E2ECANCEL_IN, 
										t_invite, &tmcb);
	}
	/* mark transaction as canceled, so that no new message are forwarded
	 * on it and t_is_canceled() returns true 
	 * WARNING: it's safe to do it without locks, at least for now (in a race
	 * event even if a flag is unwillingly reset nothing bad will happen),
	 * however this should be rechecked for any future new flags use.
	 */
	t_invite->flags|=T_CANCELED;
	/* first check if there are any branches */
	if (t_invite->nr_of_outgoings==0){
		/* no branches yet => force a reply to the invite */
		t_reply( t_invite, t_invite->uas.request, 487, CANCELED );
		DBG("DEBUG: e2e_cancel: e2e cancel -- no more pending branches\n");
		t_reply( t_cancel, cancel_msg, 200, CANCEL_DONE );
		return;
	}
	
	/* determine which branches to cancel ... */
	prepare_to_cancel(t_invite, &cancel_bm, 0);
#ifdef E2E_CANCEL_HOP_BY_HOP
	/* we don't need to set t_cancel label to be the same as t_invite if
	 * we do hop by hop cancel. The cancel transaction will have a different 
	 * label, but this is not a problem since this transaction is only used to
	 * send a reply back. The cancels sent upstream will be part of the invite
	 * transaction (local_cancel retr. bufs) and they will be generated with
	 * the same via as the invite.
	 * Note however that setting t_cancel label the same as t_invite will work
	 * too (the upstream cancel replies will properly match the t_invite
	 * transaction and will not match the t_cancel because t_cancel will always
	 * have 0 branches and we check for the branch number in 
	 * t_reply_matching() ).
	 */
#ifdef CANCEL_REASON_SUPPORT
	free_reason = 0;
	reason = 0;
	if (likely(t_invite->uas.cancel_reas == 0)){
		reason = cancel_reason_pack(CANCEL_REAS_RCVD_CANCEL, cancel_msg,
									t_invite);
		/* set if not already set */
		if (unlikely(reason &&
					atomic_cmpxchg_long((void*)&t_invite->uas.cancel_reas,
										0, (long)reason) != 0)) {
			/* already set, failed to re-set it */
			free_reason = 1;
		}
	}
#endif /* CANCEL_REASON_SUPPORT */
	for (i=0; i<t_invite->nr_of_outgoings; i++)
		if (cancel_bm & (1<<i)) {
			/* it's safe to get the reply lock since e2e_cancel is
			 * called with the cancel as the "current" transaction so
			 * at most t_cancel REPLY_LOCK is held in this process =>
			 * no deadlock possibility */
			ret=cancel_branch(
				t_invite,
				i,
#ifdef CANCEL_REASON_SUPPORT
				reason,
#endif /* CANCEL_REASON_SUPPORT */
				cfg_get(tm,tm_cfg, cancel_b_flags)
					| ((t_invite->uac[i].request.buffer==NULL)?
						F_CANCEL_B_FAKE_REPLY:0) /* blind UAC? */
			);
			if (ret<0) cancel_bm &= ~(1<<i);
			if (ret<lowest_error) lowest_error=ret;
		}
#ifdef CANCEL_REASON_SUPPORT
	if (unlikely(free_reason)) {
		/* reason was not set as the global reason => free it */
		shm_free(reason);
	}
#endif /* CANCEL_REASON_SUPPORT */
#else /* ! E2E_CANCEL_HOP_BY_HOP */
	/* fix label -- it must be same for reply matching (the label is part of
	 * the generated via branch for the cancels sent upstream and if it
	 * would be different form the one in the INVITE the transactions would not
	 * match */
	t_cancel->label=t_invite->label;
	t_cancel->nr_of_outgoings=t_invite->nr_of_outgoings;
	/* ... and install CANCEL UACs */
	for (i=0; i<t_invite->nr_of_outgoings; i++)
		if ((cancel_bm & (1<<i)) && (t_invite->uac[i].last_received>=100)) {
			ret=e2e_cancel_branch(cancel_msg, t_cancel, t_invite, i);
			if (ret<0) cancel_bm &= ~(1<<i);
			if (ret<lowest_error) lowest_error=ret;
		}

	/* send them out */
	for (i = 0; i < t_cancel->nr_of_outgoings; i++) {
		if (cancel_bm & (1 << i)) {
			if (t_invite->uac[i].last_received>=100){
				/* Provisional reply received on this branch, send CANCEL */
				/* we do need to stop the retr. timers if the request is not 
				 * an invite and since the stop_rb_retr() cost is lower then
				 * the invite check we do it always --andrei */
				stop_rb_retr(&t_invite->uac[i].request);
				if (SEND_BUFFER(&t_cancel->uac[i].request) == -1) {
					LOG(L_ERR, "ERROR: e2e_cancel: send failed\n");
				}
				else{
					if (unlikely(has_tran_tmcbs(t_cancel, TMCB_REQUEST_SENT)))
						run_trans_callbacks_with_buf(TMCB_REQUEST_SENT,
						                             &t_cancel->uac[i].request,
						                             cancel_msg, 0, TMCB_LOCAL_F);
				}
				if (start_retr( &t_cancel->uac[i].request )!=0)
					LOG(L_CRIT, "BUG: e2e_cancel: failed to start retr."
							" for %p\n", &t_cancel->uac[i].request);
			} else {
				/* No provisional response received, stop
				 * retransmission timers */
				if (!(cfg_get(tm, tm_cfg, cancel_b_flags) & 
							F_CANCEL_B_FORCE_RETR))
					stop_rb_retr(&t_invite->uac[i].request);
				/* no need to stop fr, it will be stopped by relay_reply
				 * put_on_wait -- andrei */
				/* Generate faked reply */
				if (cfg_get(tm, tm_cfg, cancel_b_flags) &
						F_CANCEL_B_FAKE_REPLY){
					LOCK_REPLIES(t_invite);
					if (relay_reply(t_invite, FAKED_REPLY, i,
									487, &tmp_bm, 1) == RPS_ERROR) {
						lowest_error = -1;
					}
				}
			}
		}
	}
#endif /*E2E_CANCEL_HOP_BY_HOP */

	/* if error occurred, let it know upstream (final reply
	   will also move the transaction on wait state
	*/
	if (lowest_error<0) {
		LOG(L_ERR, "ERROR: cancel error\n");
		/* if called from failure_route, make sure that the unsafe version
		 * is called (we are already holding the reply mutex for the cancel
		 * transaction).
		 */
		if ((is_route_type(FAILURE_ROUTE)) && (t_cancel==get_t()))
			t_reply_unsafe( t_cancel, cancel_msg, 500, "cancel error");
		else
			t_reply( t_cancel, cancel_msg, 500, "cancel error");
	} else if (cancel_bm) {
		/* if there are pending branches, let upstream know we
		   are working on it
		*/
		DBG("DEBUG: e2e_cancel: e2e cancel proceeding\n");
		/* if called from failure_route, make sure that the unsafe version
		 * is called (we are already hold the reply mutex for the cancel
		 * transaction).
		 */
		if ((is_route_type(FAILURE_ROUTE)) && (t_cancel==get_t()))
			t_reply_unsafe( t_cancel, cancel_msg, 200, CANCELING );
		else
			t_reply( t_cancel, cancel_msg, 200, CANCELING );
	} else {
		/* if the transaction exists, but there are no more pending
		   branches, tell upstream we're done
		*/
		DBG("DEBUG: e2e_cancel: e2e cancel -- no more pending branches\n");
		/* if called from failure_route, make sure that the unsafe version
		 * is called (we are already hold the reply mutex for the cancel
		 * transaction).
		 */
		if ((is_route_type(FAILURE_ROUTE)) && (t_cancel==get_t()))
			t_reply_unsafe( t_cancel, cancel_msg, 200, CANCEL_DONE );
		else
			t_reply( t_cancel, cancel_msg, 200, CANCEL_DONE );
	}
}



/* sends one uac/branch buffer and fallbacks to other ips if
 *  the destination resolves to several addresses
 *  Takes care of starting timers a.s.o. (on send success)
 *  returns: -2 on error, -1 on drop,  current branch id on success,
 *   new branch id on send error/blacklist, when failover is possible
 *    (ret>=0 && ret!=branch)
 *    if lock_replies is 1, the replies for t will be locked when adding
 *     new branches (to prevent races). Use 0 from failure routes or other
 *     places where the reply lock is already held, to avoid deadlocks. */
int t_send_branch( struct cell *t, int branch, struct sip_msg* p_msg ,
					struct proxy_l * proxy, int lock_replies)
{
	struct ip_addr ip; /* debugging */
	int ret;
	struct ua_client* uac;
	
	uac=&t->uac[branch];
	ret=branch;
	if (run_onsend(p_msg,	&uac->request.dst, uac->request.buffer,
					uac->request.buffer_len)==0){
		/* disable the current branch: set a "fake" timeout
		 *  reply code but don't set uac->reply, to avoid overriding 
		 *  a higly unlikely, perfectly timed fake reply (to a message
		 *   we never sent).
		 * (code=final reply && reply==0 => t_pick_branch won't ever pick it)*/
			uac->last_received=408;
			su2ip_addr(&ip, &uac->request.dst.to);
			DBG("t_send_branch: onsend_route dropped msg. to %s:%d (%d)\n",
							ip_addr2a(&ip), su_getport(&uac->request.dst.to),
							uac->request.dst.proto);
#ifdef USE_DNS_FAILOVER
			/* if the destination resolves to more ips, add another
			 *  branch/uac */
			if (cfg_get(core, core_cfg, use_dns_failover)){
				ret=add_uac_dns_fallback(t, p_msg, uac, lock_replies);
				if (ret>=0){
					su2ip_addr(&ip, &uac->request.dst.to);
					DBG("t_send_branch: send on branch %d failed "
							"(onsend_route), trying another ip %s:%d (%d)\n",
							branch, ip_addr2a(&ip),
							su_getport(&uac->request.dst.to),
							uac->request.dst.proto);
					/* success, return new branch */
					return ret;
				}
			}
#endif /* USE_DNS_FAILOVER*/
		return -1; /* drop, try next branch */
	}
#ifdef USE_DST_BLACKLIST
	if (cfg_get(core, core_cfg, use_dst_blacklist)
		&& p_msg
		&& (p_msg->REQ_METHOD & cfg_get(tm, tm_cfg, tm_blst_methods_lookup))
	){
		if (dst_is_blacklisted(&uac->request.dst, p_msg)){
			su2ip_addr(&ip, &uac->request.dst.to);
			DBG("t_send_branch: blacklisted destination: %s:%d (%d)\n",
							ip_addr2a(&ip), su_getport(&uac->request.dst.to),
							uac->request.dst.proto);
			/* disable the current branch: set a "fake" timeout
			 *  reply code but don't set uac->reply, to avoid overriding 
			 *  a higly unlikely, perfectly timed fake reply (to a message
			 *   we never sent).  (code=final reply && reply==0 => 
			 *   t_pick_branch won't ever pick it)*/
			uac->last_received=408;
#ifdef USE_DNS_FAILOVER
			/* if the destination resolves to more ips, add another
			 *  branch/uac */
			if (cfg_get(core, core_cfg, use_dns_failover)){
				ret=add_uac_dns_fallback(t, p_msg, uac, lock_replies);
				if (ret>=0){
					su2ip_addr(&ip, &uac->request.dst.to);
					DBG("t_send_branch: send on branch %d failed (blacklist),"
							" trying another ip %s:%d (%d)\n", branch,
							ip_addr2a(&ip), su_getport(&uac->request.dst.to),
							uac->request.dst.proto);
					/* success, return new branch */
					return ret;
				}
			}
#endif /* USE_DNS_FAILOVER*/
			return -1; /* don't send */
		}
	}
#endif /* USE_DST_BLACKLIST */
	if (SEND_BUFFER( &uac->request)==-1) {
		/* disable the current branch: set a "fake" timeout
		 *  reply code but don't set uac->reply, to avoid overriding 
		 *  a highly unlikely, perfectly timed fake reply (to a message
		 *  we never sent).
		 * (code=final reply && reply==0 => t_pick_branch won't ever pick it)*/
		uac->last_received=408;
		su2ip_addr(&ip, &uac->request.dst.to);
		DBG("t_send_branch: send to %s:%d (%d) failed\n",
							ip_addr2a(&ip), su_getport(&uac->request.dst.to),
							uac->request.dst.proto);
#ifdef USE_DST_BLACKLIST
		dst_blacklist_add(BLST_ERR_SEND, &uac->request.dst, p_msg);
#endif
#ifdef USE_DNS_FAILOVER
		/* if the destination resolves to more ips, add another
		 *  branch/uac */
		if (cfg_get(core, core_cfg, use_dns_failover)){
			ret=add_uac_dns_fallback(t, p_msg, uac, lock_replies);
			if (ret>=0){
				/* success, return new branch */
				DBG("t_send_branch: send on branch %d failed, adding another"
						" branch with another ip\n", branch);
				return ret;
			}
		}
#endif
		uac->icode = 908; /* internal code set to delivery failure */
		LOG(L_WARN, "ERROR: t_send_branch: sending request on branch %d "
				"failed\n", branch);
		if (proxy) { proxy->errors++; proxy->ok=0; }
		if(tm_failure_exec_mode==1) {
			LM_DBG("putting branch %d on hold \n", branch);
			/* put on retransmission timer,
			 * but set proto to NONE, so actually it is not trying to resend */
			uac->request.dst.proto = PROTO_NONE;
			/* reset last_received, 408 reply is faked by timer */
			uac->last_received=0;
			/* add to retransmission timer */
			if (start_retr( &uac->request )!=0){
				LM_CRIT("retransmission already started for %p\n",
					&uac->request);
				return -2;
			}
			return branch;
		}
		return -2;
	} else {
		if (unlikely(has_tran_tmcbs(t, TMCB_REQUEST_SENT)))
			run_trans_callbacks_with_buf(TMCB_REQUEST_SENT, &uac->request, p_msg, 0,0);
		/* start retr. only if the send succeeded */
		if (start_retr( &uac->request )!=0){
			LOG(L_CRIT, "BUG: t_send_branch: retr. already started for %p\n",
					&uac->request);
			return -2;
		}
	}
	return ret;
}



/* function returns:
 *       1 - forward successful
 *      -1 - error during forward
 */
int t_forward_nonack( struct cell *t, struct sip_msg* p_msg , 
	struct proxy_l * proxy, int proto)
{
	int branch_ret, lowest_ret;
	str current_uri;
	branch_bm_t	added_branches;
	int first_branch;
	int i, q;
	struct cell *t_invite;
	int success_branch;
	int try_new;
	int lock_replies;
	str dst_uri, path, instance, ruid, location_ua;
	struct socket_info* si;
	flag_t backup_bflags = 0;
	flag_t bflags = 0;
	

	/* make -Wall happy */
	current_uri.s=0;

	getbflagsval(0, &backup_bflags);

	if (t->flags & T_CANCELED) goto canceled;

	if (p_msg->REQ_METHOD==METHOD_CANCEL) { 
		t_invite=t_lookupOriginalT(  p_msg );
		if (t_invite!=T_NULL_CELL) {
			e2e_cancel( p_msg, t, t_invite );
			UNREF(t_invite);
			/* it should be set to REQ_RPLD by e2e_cancel, which should
			 * send a final reply */
			set_kr(REQ_FWDED);
			return 1;
		}
	}

	/* if no more specific error code is known, use this */
	lowest_ret=E_UNSPEC;
	/* branches added */
	added_branches=0;
	/* branch to begin with */
	first_branch=t->nr_of_outgoings;

	if (t->on_branch) {
		/* tell add_uac that it should run branch route actions */
		branch_route = t->on_branch;
		/* save the branch route so that it
		 * can be used for adding branches later
		 */
		t->on_branch_delayed = t->on_branch;
		/* reset the flag before running the actions (so that it
		 * could be set again in branch_route if needed
		 */
		t_on_branch(0);
	} else {
		branch_route = 0;
	}
	
	/* on first-time forwarding, update the lumps */
	if (first_branch==0) {
		/* update the shmem-ized msg with the lumps */
		if ((is_route_type(REQUEST_ROUTE)) &&
			save_msg_lumps(t->uas.request, p_msg)) {
				LOG(L_ERR, "ERROR: t_forward_nonack: "
					"failed to save the message lumps\n");
				return -1;
			}
	}

	/* if ruri is not already consumed (by another invocation), use current
	   uri too. Else add only additional branches (which may be continuously
	   refilled).
	*/
	if (ruri_get_forking_state()) {
		try_new=1;
		branch_ret=add_uac( t, p_msg, GET_RURI(p_msg), GET_NEXT_HOP(p_msg),
							&p_msg->path_vec, proxy, p_msg->force_send_socket,
							p_msg->fwd_send_flags, proto,
							(p_msg->dst_uri.len)?0:UAC_SKIP_BR_DST_F, &p_msg->instance,
							&p_msg->ruid, &p_msg->location_ua);
		/* test if cancel was received meanwhile */
		if (t->flags & T_CANCELED) goto canceled;
		if (branch_ret>=0) 
			added_branches |= 1<<branch_ret;
		else
			lowest_ret=MIN_int(lowest_ret, branch_ret);
	} else try_new=0;

	init_branch_iterator();
	while((current_uri.s=next_branch( &current_uri.len, &q, &dst_uri, &path,
										&bflags, &si, &ruid, &instance, &location_ua))) {
		try_new++;
		setbflagsval(0, bflags);

		branch_ret=add_uac( t, p_msg, &current_uri,
							(dst_uri.len) ? (&dst_uri) : &current_uri,
							&path, proxy, si, p_msg->fwd_send_flags,
							proto, (dst_uri.len)?0:UAC_SKIP_BR_DST_F, &instance,
							&ruid, &location_ua);
		/* test if cancel was received meanwhile */
		if (t->flags & T_CANCELED) goto canceled;
		/* pick some of the errors in case things go wrong;
		   note that picking lowest error is just as good as
		   any other algorithm which picks any other negative
		   branch result */
		if (branch_ret>=0) 
			added_branches |= 1<<branch_ret;
		else
			lowest_ret=MIN_int(lowest_ret, branch_ret);
	}
	/* consume processed branches */
	clear_branches();

	setbflagsval(0, backup_bflags);

	/* update message flags, if changed in branch route */
	t->uas.request->flags = p_msg->flags;

	/* don't forget to clear all branches processed so far */

	/* things went wrong ... no new branch has been fwd-ed at all */
	if (added_branches==0) {
		if (try_new==0) {
			LOG(L_ERR, "ERROR: t_forward_nonack: no branches for"
						" forwarding\n");
			/* either failed to add branches, or there were no more branches
			*/
			ser_error=MIN_int(lowest_ret, E_CFG);
			return -1;
		}
		if(lowest_ret!=E_CFG)
			LOG(L_ERR, "ERROR: t_forward_nonack: failure to add branches\n");
		ser_error=lowest_ret;
		return lowest_ret;
	}

#ifdef TM_UAC_FLAGS
	/* mark the fist branch in this fwd step */
	t->uac[first_branch].flags |= TM_UAC_FLAG_FB;
#endif

	ser_error=0; /* clear branch adding errors */
	/* send them out now */
	success_branch=0;
	lock_replies= ! ((is_route_type(FAILURE_ROUTE)) && (t==get_t()));
	for (i=first_branch; i<t->nr_of_outgoings; i++) {
		if (added_branches & (1<<i)) {
			
			branch_ret=t_send_branch(t, i, p_msg , proxy, lock_replies);
			if (branch_ret>=0){ /* some kind of success */
				if (branch_ret==i) { /* success */
					success_branch++;
					if (unlikely(has_tran_tmcbs(t, TMCB_REQUEST_OUT)))
						run_trans_callbacks_with_buf( TMCB_REQUEST_OUT, &t->uac[i].request,
						                              p_msg, 0, -p_msg->REQ_METHOD);
				}
				else /* new branch added */
					added_branches |= 1<<branch_ret;
			}
		}
	}
	if (success_branch<=0) {
		/* return always E_SEND for now
		 * (the real reason could be: denied by onsend routes, blacklisted,
		 *  send failed or any of the errors listed before + dns failed
		 *  when attempting dns failover) */
		ser_error=E_SEND;
		/* else return the last error (?) */
		/* the caller should take care and delete the transaction */
		return -1;
	}
	ser_error=0; /* clear branch send errors, we have overall success */
	set_kr(REQ_FWDED);
	return 1;

canceled:
	DBG("t_forward_non_ack: no forwarding on a canceled transaction\n");
	/* reset processed branches */
	clear_branches();
	/* restore backup flags from initial env */
	setbflagsval(0, backup_bflags);
	/* update message flags, if changed in branch route */
	t->uas.request->flags = p_msg->flags;
	ser_error=E_CANCELED;
	return -1;
}



/* cancel handling/forwarding function
 * CANCELs with no matching transaction are handled in function of
 * the unmatched_cancel config var: they are either forwarded statefully,
 * statelessly or dropped.
 * function returns:
 *       1 - forward successful
 *       0 - error, but do not reply 
 *      <0 - error during forward
 * it also sets *tran if a transaction was created
 */
int t_forward_cancel(struct sip_msg* p_msg , struct proxy_l * proxy, int proto,
						struct cell** tran)
{
	struct cell* t_invite;
	struct cell* t;
	int ret;
	int new_tran;
	struct dest_info dst;
	str host;
	unsigned short port;
	short comp;
	
	t=0;
	/* handle cancels for which no transaction was created yet */
	if (cfg_get(tm, tm_cfg, unmatched_cancel)==UM_CANCEL_STATEFULL){
		/* create cancel transaction */
		new_tran=t_newtran(p_msg);
		if (new_tran<=0 && new_tran!=E_SCRIPT){
			if (new_tran==0)
				 /* retransmission => do nothing */
				ret=1;
			else
				/* some error => return it or DROP */
				ret=(ser_error==E_BAD_VIA && reply_to_via) ? 0: new_tran;
			goto end;
		}
		t=get_t();
		ret=t_forward_nonack(t, p_msg, proxy, proto);
		goto end;
	}
	
	t_invite=t_lookupOriginalT(  p_msg );
	if (t_invite!=T_NULL_CELL) {
		/* create cancel transaction */
		new_tran=t_newtran(p_msg);
		if (new_tran<=0 && new_tran!=E_SCRIPT){
			if (new_tran==0)
				 /* retransmission => do nothing */
				ret=1;
			else
				/* some error => return it or DROP */
				ret=(ser_error==E_BAD_VIA && reply_to_via) ? 0: new_tran;
			UNREF(t_invite);
			goto end;
		}
		t=get_t();
		e2e_cancel( p_msg, t, t_invite );
		UNREF(t_invite);
		ret=1;
		goto end;
	}else /* no coresponding INVITE transaction */
	     if (cfg_get(tm, tm_cfg, unmatched_cancel)==UM_CANCEL_DROP){
				DBG("t_forward_nonack: non matching cancel dropped\n");
				ret=1; /* do nothing -> drop */
				goto end;
		 }else{
			/* UM_CANCEL_STATELESS -> stateless forward */
				DBG( "SER: forwarding CANCEL statelessly \n");
				if (proxy==0) {
					init_dest_info(&dst);
					dst.proto=proto;
					if (get_uri_send_info(GET_NEXT_HOP(p_msg), &host,
								&port, &dst.proto, &comp)!=0){
						ret=E_BAD_ADDRESS;
						goto end;
					}
#ifdef USE_COMP
					dst.comp=comp;
#endif
					/* dst->send_sock not set, but forward_request 
					 * will take care of it */
					ret=forward_request(p_msg, &host, port, &dst);
					goto end;
				} else {
					init_dest_info(&dst);
					dst.proto=get_proto(proto, proxy->proto);
					proxy2su(&dst.to, proxy);
					/* dst->send_sock not set, but forward_request 
					 * will take care of it */
					ret=forward_request( p_msg , 0, 0, &dst) ;
					goto end;
				}
		}
end:
	if (tran)
		*tran=t;
	return ret;
}

/* Relays a CANCEL request if a corresponding INVITE transaction
 * can be found. The function is supposed to be used at the very
 * beginning of the script with reparse_invite=1 module parameter.
 *
 * return value:
 *    0: the CANCEL was successfully relayed
 *       (or error occured but reply cannot be sent) => DROP
 *    1: no corresponding INVITE transaction exisis
 *   <0: corresponding INVITE transaction exisis but error occured
 */
int t_relay_cancel(struct sip_msg* p_msg)
{
	struct cell* t_invite;
	struct cell* t;
	int ret;
	int new_tran;

	t_invite=t_lookupOriginalT(  p_msg );
	if (t_invite!=T_NULL_CELL) {
		/* create cancel transaction */
		new_tran=t_newtran(p_msg);
		if (new_tran<=0 && new_tran!=E_SCRIPT){
			if (new_tran==0)
				/* retransmission => DROP,
				t_newtran() takes care about it */
				ret=0;
			else
				/* some error => return it or DROP */
				ret=(ser_error==E_BAD_VIA && reply_to_via) ? 0: new_tran;
			UNREF(t_invite);
			goto end;
		}
		t=get_t();
		e2e_cancel( p_msg, t, t_invite );
		UNREF(t_invite);
		/* return 0 to stop the script processing */
		ret=0;
		goto end;

	} else {
		/* no corresponding INVITE trasaction found */
		ret=1;
	}
end:
	return ret;
}

/* WARNING: doesn't work from failure route (deadlock, uses t_relay_to which
 *  is failure route unsafe) */
int t_replicate(struct sip_msg *p_msg,  struct proxy_l *proxy, int proto )
{
	/* this is a quite horrible hack -- we just take the message
	   as is, including Route-s, Record-route-s, and Vias ,
	   forward it downstream and prevent replies received
	   from relaying by setting the replication/local_trans bit;

		nevertheless, it should be good enough for the primary
		customer of this function, REGISTER replication

		if we want later to make it thoroughly, we need to
		introduce delete lumps for all the header fields above
	*/
	return t_relay_to(p_msg, proxy, proto, 1 /* replicate */);
}

/* fixup function for reparse_on_dns_failover modparam */
int reparse_on_dns_failover_fixup(void *handle, str *gname, str *name, void **val)
{
#ifdef USE_DNS_FAILOVER
	if ((int)(long)(*val) && mhomed) {
		LOG(L_WARN, "WARNING: reparse_on_dns_failover_fixup:"
		"reparse_on_dns_failover is enabled on a "
		"multihomed host -- check the readme of tm module!\n");
	}
#endif
	return 0;
}
