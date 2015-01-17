/*
 * Copyright (C) 2007 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*! \file
 * \brief
 * blst module :: Blacklist related script functions
 *
 */


#include "../../modparam.h"
#include "../../dprint.h"
#include "../../parser/msg_parser.h"
#include "../../parser/hf.h"
#include "../../dst_blacklist.h"
#include "../../timer_ticks.h"
#include "../../ip_addr.h"
#include "../../compiler_opt.h"
#include "../../ut.h"
#include "../../globals.h"
#include "../../cfg_core.h"


MODULE_VERSION



static int blst_add_f(struct sip_msg*, char*, char*);
static int blst_add_retry_after_f(struct sip_msg*, char*, char*);
static int blst_del_f(struct sip_msg*, char*, char*);
static int blst_is_blacklisted_f(struct sip_msg*, char*, char*);
static int blst_set_ignore_f(struct sip_msg*, char*, char*);
static int blst_clear_ignore_f(struct sip_msg*, char*, char*);
static int blst_rpl_set_ignore_f(struct sip_msg*, char*, char*);
static int blst_rpl_clear_ignore_f(struct sip_msg*, char*, char*);



static cmd_export_t cmds[]={
	{"blst_add",           blst_add_f,               0,  0,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|ONSEND_ROUTE},
	{"blst_add",           blst_add_f,               1, fixup_var_int_1,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|ONSEND_ROUTE},
	{"blst_add_retry_after", blst_add_retry_after_f, 2, fixup_var_int_12,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|ONSEND_ROUTE},
	{"blst_del",           blst_del_f,               0, 0,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|ONSEND_ROUTE},
	{"blst_is_blacklisted",   blst_is_blacklisted_f, 0, 0,
			REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|ONSEND_ROUTE},
	{"blst_set_ignore",         blst_set_ignore_f,   0,  0,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONSEND_ROUTE},
	{"blst_set_ignore",         blst_set_ignore_f,   1,  fixup_var_int_1,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONSEND_ROUTE},
	{"blst_clear_ignore",         blst_clear_ignore_f,   0,  0,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONSEND_ROUTE},
	{"blst_clear_ignore",         blst_clear_ignore_f,   1,  fixup_var_int_1,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE|ONSEND_ROUTE},
	{"blst_rpl_set_ignore",       blst_rpl_set_ignore_f, 0,  0,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"blst_rpl_set_ignore",      blst_rpl_set_ignore_f,  1,  fixup_var_int_1,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"blst_rpl_clear_ignore",   blst_rpl_clear_ignore_f, 0,  0,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"blst_rpl_clear_ignore",   blst_rpl_clear_ignore_f, 1,  fixup_var_int_1,
		REQUEST_ROUTE|ONREPLY_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{0,0,0,0,0}
};

static param_export_t params[]={
	{0,0,0}
	}; /* no params */

struct module_exports exports= {
	"blst",
	cmds,
	0,        /* RPC methods */
	params,
	0, /* module initialization function */
	0, /* response function */
	0, /* destroy function */
	0, /* on_cancel function */
	0, /* per-child init function */
};



static int blst_add_f(struct sip_msg* msg, char* to, char* foo)
{
#ifdef USE_DST_BLACKLIST
	int t;
	struct dest_info src;
	
	if (likely(cfg_get(core, core_cfg, use_dst_blacklist))){
		t=0;
		if (unlikely( to && (get_int_fparam(&t, msg, (fparam_t*)to)<0)))
			return -1;
		if (t==0)
			t=cfg_get(core, core_cfg, blst_timeout);
		init_dest_info(&src);
		src.send_sock=0;
		src.to=msg->rcv.src_su;
		src.id=msg->rcv.proto_reserved1;
		src.proto=msg->rcv.proto;
		dst_blacklist_force_add_to(BLST_ADM_PROHIBITED, &src, msg,
									S_TO_TICKS(t));
		return 1;
	}else{
		LOG(L_WARN, "WARNING: blst: blst_add: blacklist support disabled\n");
	}
#else /* USE_DST_BLACKLIST */
	LOG(L_WARN, "WARNING: blst: blst_add: blacklist support not compiled-in"
			" - no effect -\n");
#endif /* USE_DST_BLACKLIST */
	return 1;
}



/* returns error if no retry_after hdr field is present */
static int blst_add_retry_after_f(struct sip_msg* msg, char* min, char* max)
{
#ifdef USE_DST_BLACKLIST
	int t_min, t_max, t;
	struct dest_info src;
	struct hdr_field* hf;
	
	if (likely(cfg_get(core, core_cfg, use_dst_blacklist))){
		if (unlikely(get_int_fparam(&t_min, msg, (fparam_t*)min)<0)) return -1;
		if (likely(max)){
			if (unlikely(get_int_fparam(&t_max, msg, (fparam_t*)max)<0))
				return -1;
		}else{
			t_max=0;
		}
	
		init_dest_info(&src);
		src.send_sock=0;
		src.to=msg->rcv.src_su;
		src.id=msg->rcv.proto_reserved1;
		src.proto=msg->rcv.proto;
		t=-1;
		if ((parse_headers(msg, HDR_RETRY_AFTER_F, 0)==0) && 
			(msg->parsed_flag & HDR_RETRY_AFTER_F)){
			for (hf=msg->headers; hf; hf=hf->next)
				if (hf->type==HDR_RETRY_AFTER_T){
					/* found */
					t=(unsigned)(unsigned long)hf->parsed;
					break;
			}
		}
		if (t<0)
			return -1;
		
		t=MAX_unsigned(t, t_min);
		t=MIN_unsigned(t, t_max);
		if (likely(t))
			dst_blacklist_force_add_to(BLST_ADM_PROHIBITED, &src, msg,
										S_TO_TICKS(t));
		return 1;
	}else{
		LOG(L_WARN, "WARNING: blst: blst_add_retry_after:"
					" blacklist support disabled\n");
	}
#else /* USE_DST_BLACKLIST */
	LOG(L_WARN, "WARNING: blst: blst_add_retry_after:"
			" blacklist support not compiled-in - no effect -\n");
#endif /* USE_DST_BLACKLIST */
	return 1;
}



static int blst_del_f(struct sip_msg* msg, char* foo, char* bar)
{
#ifdef USE_DST_BLACKLIST
	struct dest_info src;
	
	if (likely(cfg_get(core, core_cfg, use_dst_blacklist))){
	
		init_dest_info(&src);
		src.send_sock=0;
		src.to=msg->rcv.src_su;
		src.id=msg->rcv.proto_reserved1;
		src.proto=msg->rcv.proto;
		if (dst_blacklist_del(&src, msg))
			return 1;
	}else{
		LOG(L_WARN, "WARNING: blst: blst_del: blacklist support disabled\n");
	}
#else /* USE_DST_BLACKLIST */
	LOG(L_WARN, "WARNING: blst: blst_del: blacklist support not compiled-in"
			" - no effect -\n");
#endif /* USE_DST_BLACKLIST */
	return -1;
}



static int blst_is_blacklisted_f(struct sip_msg* msg, char* foo, char* bar)
{
#ifdef USE_DST_BLACKLIST
	struct dest_info src;
	
	if (likely(cfg_get(core, core_cfg, use_dst_blacklist))){
		init_dest_info(&src);
		src.send_sock=0;
		src.to=msg->rcv.src_su;
		src.id=msg->rcv.proto_reserved1;
		src.proto=msg->rcv.proto;
		if (dst_is_blacklisted(&src, msg))
			return 1;
	}else{
		LOG(L_WARN, "WARNING: blst: blst_is_blacklisted:"
					" blacklist support disabled\n");
	}
#else /* USE_DST_BLACKLIST */
	LOG(L_WARN, "WARNING: blst: blst_is_blacklisted:"
				" blacklist support not compiled-in - no effect -\n");
#endif /* USE_DST_BLACKLIST */
	return -1;
}



static int blst_set_ignore_f(struct sip_msg* msg, char* flags, char* foo)
{
#ifdef USE_DST_BLACKLIST
	unsigned char blst_imask;
	int mask;
	
	if (unlikely(flags && (get_int_fparam(&mask, msg, (fparam_t*)flags)<0)))
		return -1;
	blst_imask=flags?mask:0xff;
	msg->fwd_send_flags.blst_imask|=blst_imask;
	return 1;
#else /* USE_DST_BLACKLIST */
	LOG(L_WARN, "WARNING: blst: blst_ignore_req: blacklist support"
				" not compiled-in - no effect -\n");
#endif /* USE_DST_BLACKLIST */
	return 1;
}



static int blst_clear_ignore_f(struct sip_msg* msg, char* flags, char* foo)
{
#ifdef USE_DST_BLACKLIST
	unsigned char blst_imask;
	int mask;
	
	if (unlikely(flags && (get_int_fparam(&mask, msg, (fparam_t*)flags)<0)))
		return -1;
	blst_imask=flags?mask:0xff;
	msg->fwd_send_flags.blst_imask&=~blst_imask;
	return 1;
#else /* USE_DST_BLACKLIST */
	LOG(L_WARN, "WARNING: blst: blst_ignore_req: blacklist support"
				" not compiled-in - no effect -\n");
#endif /* USE_DST_BLACKLIST */
	return 1;
}



static int blst_rpl_set_ignore_f(struct sip_msg* msg, char* flags, char* foo)
{
#ifdef USE_DST_BLACKLIST
	unsigned char blst_imask;
	int mask;
	
	if (unlikely(flags && (get_int_fparam(&mask, msg, (fparam_t*)flags)<0)))
		return -1;
	blst_imask=flags?mask:0xff;
	msg->rpl_send_flags.blst_imask|=blst_imask;
	return 1;
#else /* USE_DST_BLACKLIST */
	LOG(L_WARN, "WARNING: blst: blst_ignore_req: blacklist support"
				" not compiled-in - no effect -\n");
#endif /* USE_DST_BLACKLIST */
	return 1;
}



static int blst_rpl_clear_ignore_f(struct sip_msg* msg, char* flags, char* foo)
{
#ifdef USE_DST_BLACKLIST
	unsigned char blst_imask;
	int mask;
	
	if (unlikely(flags && (get_int_fparam(&mask, msg, (fparam_t*)flags)<0)))
		return -1;
	blst_imask=flags?mask:0xff;
	msg->rpl_send_flags.blst_imask&=~blst_imask;
	return 1;
#else /* USE_DST_BLACKLIST */
	LOG(L_WARN, "WARNING: blst: blst_ignore_req: blacklist support"
				" not compiled-in - no effect -\n");
#endif /* USE_DST_BLACKLIST */
	return 1;
}
