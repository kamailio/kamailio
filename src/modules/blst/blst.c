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
 * blst module :: Blocklist related script functions
 *
 */


#include "../../core/modparam.h"
#include "../../core/dprint.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/hf.h"
#include "../../core/dst_blocklist.h"
#include "../../core/timer_ticks.h"
#include "../../core/ip_addr.h"
#include "../../core/compiler_opt.h"
#include "../../core/ut.h"
#include "../../core/globals.h"
#include "../../core/cfg_core.h"
#include "../../core/kemi.h"


MODULE_VERSION



static int blst_add0_f(struct sip_msg*, char*, char*);
static int blst_add1_f(struct sip_msg*, char*, char*);
static int blst_add_retry_after_f(struct sip_msg*, char*, char*);
static int blst_del_f(struct sip_msg*, char*, char*);
static int blst_is_blocklisted_f(struct sip_msg*, char*, char*);
static int blst_set_ignore_f(struct sip_msg*, char*, char*);
static int blst_clear_ignore_f(struct sip_msg*, char*, char*);
static int blst_rpl_set_ignore_f(struct sip_msg*, char*, char*);
static int blst_rpl_clear_ignore_f(struct sip_msg*, char*, char*);



static cmd_export_t cmds[]={
	{"blst_add",              blst_add0_f,             0,  0, 0,
			ANY_ROUTE},
	{"blst_add",              blst_add1_f,             1, fixup_var_int_1, 0,
			ANY_ROUTE},
	{"blst_add_retry_after",  blst_add_retry_after_f,  2, fixup_var_int_12, 0,
			ANY_ROUTE},
	{"blst_del",              blst_del_f,              0, 0, 0,
			ANY_ROUTE},
	{"blst_is_blocklisted",   blst_is_blocklisted_f,   0, 0, 0,
			ANY_ROUTE},
	{"blst_set_ignore",       blst_set_ignore_f,       0,  0, 0,
			ANY_ROUTE},
	{"blst_set_ignore",       blst_set_ignore_f,       1,  fixup_var_int_1, 0,
			ANY_ROUTE},
	{"blst_clear_ignore",     blst_clear_ignore_f,     0,  0, 0,
			ANY_ROUTE},
	{"blst_clear_ignore",     blst_clear_ignore_f,     1,  fixup_var_int_1, 0,
			ANY_ROUTE},
	{"blst_rpl_set_ignore",   blst_rpl_set_ignore_f,   0,  0, 0,
			ANY_ROUTE},
	{"blst_rpl_set_ignore",   blst_rpl_set_ignore_f,   1,  fixup_var_int_1, 0,
			ANY_ROUTE},
	{"blst_rpl_clear_ignore", blst_rpl_clear_ignore_f, 0,  0, 0,
			ANY_ROUTE},
	{"blst_rpl_clear_ignore", blst_rpl_clear_ignore_f, 1,  fixup_var_int_1, 0,
			ANY_ROUTE},
	{0,0,0,0,0,0}
};

static param_export_t params[]={
	{0,0,0}
	}; /* no params */

struct module_exports exports= {
	"blst",
        DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,        /* RPC methods */
	0, 	  /* pseudo-variables exports */
	0, 	  /* response function */
	0, 	  /* module initialization function */
	0, 	  /* per-child init function */
	0 	  /* destroy function */
};


/**
 *
 */
static int ki_blst_add(sip_msg_t* msg, int t)
{
#ifdef USE_DST_BLOCKLIST
	struct dest_info src;

	if (likely(cfg_get(core, core_cfg, use_dst_blocklist))){
		if (t==0)
			t=cfg_get(core, core_cfg, blst_timeout);
		init_dest_info(&src);
		src.send_sock=0;
		src.to=msg->rcv.src_su;
		src.id=msg->rcv.proto_reserved1;
		src.proto=msg->rcv.proto;
		dst_blocklist_force_add_to(BLST_ADM_PROHIBITED, &src, msg,
									S_TO_TICKS(t));
		return 1;
	}else{
		LM_WARN("blocklist support disabled\n");
	}
#else /* USE_DST_BLOCKLIST */
	LM_WARN("blocklist support not compiled-in - no effect\n");
#endif /* USE_DST_BLOCKLIST */
	return 1;
}

/**
 *
 */
static int ki_blst_add_default(sip_msg_t* msg)
{
	return ki_blst_add(msg, 0);
}

/**
 *
 */
static int blst_add0_f(struct sip_msg* msg, char* to, char* foo)
{
	return ki_blst_add(msg, 0);
}


/**
 *
 */
static int blst_add1_f(struct sip_msg* msg, char* to, char* foo)
{
	int t = 0;
	if (unlikely( to && (get_int_fparam(&t, msg, (fparam_t*)to)<0)))
		return -1;
	return ki_blst_add(msg, t);
}


/**
 * returns error if no retry_after hdr field is present
 */
static int ki_blst_add_retry_after(sip_msg_t* msg, int t_min, int t_max)
{
#ifdef USE_DST_BLOCKLIST
	int t;
	struct dest_info src;
	struct hdr_field* hf;

	if (likely(cfg_get(core, core_cfg, use_dst_blocklist))){
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
			dst_blocklist_force_add_to(BLST_ADM_PROHIBITED, &src, msg,
										S_TO_TICKS(t));
		return 1;
	}else{
		LM_WARN("blocklist support disabled\n");
	}
#else /* USE_DST_BLOCKLIST */
	LM_WARN("blocklist support not compiled-in - no effect -\n");
#endif /* USE_DST_BLOCKLIST */
	return 1;
}


/**
 * returns error if no retry_after hdr field is present
 */
static int blst_add_retry_after_f(struct sip_msg* msg, char* min, char* max)
{
	int t_min, t_max;

	if (unlikely(get_int_fparam(&t_min, msg, (fparam_t*)min)<0)) return -1;
	if (likely(max)){
		if (unlikely(get_int_fparam(&t_max, msg, (fparam_t*)max)<0))
			return -1;
	}else{
		t_max=0;
	}

	return ki_blst_add_retry_after(msg, t_min, t_max);
}


/**
 *
 */
static int ki_blst_del(sip_msg_t* msg)
{
#ifdef USE_DST_BLOCKLIST
	struct dest_info src;

	if (likely(cfg_get(core, core_cfg, use_dst_blocklist))){

		init_dest_info(&src);
		src.send_sock=0;
		src.to=msg->rcv.src_su;
		src.id=msg->rcv.proto_reserved1;
		src.proto=msg->rcv.proto;
		if (dst_blocklist_del(&src, msg))
			return 1;
	}else{
		LM_WARN("blocklist support disabled\n");
	}
#else /* USE_DST_BLOCKLIST */
	LM_WARN("blocklist support not compiled-in - no effect -\n");
#endif /* USE_DST_BLOCKLIST */
	return -1;
}


/**
 *
 */
static int blst_del_f(struct sip_msg* msg, char* foo, char* bar)
{
	return ki_blst_del(msg);
}


/**
 *
 */
static int ki_blst_is_blocklisted(sip_msg_t* msg)
{
#ifdef USE_DST_BLOCKLIST
	struct dest_info src;

	if (likely(cfg_get(core, core_cfg, use_dst_blocklist))){
		init_dest_info(&src);
		src.send_sock=0;
		src.to=msg->rcv.src_su;
		src.id=msg->rcv.proto_reserved1;
		src.proto=msg->rcv.proto;
		if (dst_is_blocklisted(&src, msg))
			return 1;
	}else{
		LM_WARN("blocklist support disabled\n");
	}
#else /* USE_DST_BLOCKLIST */
	LM_WARN("blocklist support not compiled-in - no effect -\n");
#endif /* USE_DST_BLOCKLIST */
	return -1;
}


/**
 *
 */
static int blst_is_blocklisted_f(struct sip_msg* msg, char* foo, char* bar)
{
	return ki_blst_is_blocklisted(msg);
}


/**
 *
 */
static int ki_blst_set_ignore(sip_msg_t* msg, int mask)
{
#ifdef USE_DST_BLOCKLIST
	unsigned char blst_imask;

	blst_imask=mask;
	msg->fwd_send_flags.blst_imask|=blst_imask;
	return 1;
#else /* USE_DST_BLOCKLIST */
	LM_WARN("blocklist support not compiled-in - no effect -\n");
#endif /* USE_DST_BLOCKLIST */
	return 1;
}

/**
 *
 */
static int ki_blst_set_ignore_all(sip_msg_t* msg)
{
	return ki_blst_set_ignore(msg, 0xff);
}


/**
 *
 */
static int blst_set_ignore_f(struct sip_msg* msg, char* flags, char* foo)
{
	int mask = 0xff;

	if (unlikely(flags && (get_int_fparam(&mask, msg, (fparam_t*)flags)<0)))
		return -1;

	return ki_blst_set_ignore(msg, mask);
}

/**
 *
 */
static int ki_blst_clear_ignore(sip_msg_t* msg, int mask)
{
#ifdef USE_DST_BLOCKLIST
	unsigned char blst_imask;

	blst_imask=mask;
	msg->fwd_send_flags.blst_imask&=~blst_imask;
	return 1;
#else /* USE_DST_BLOCKLIST */
	LM_WARN("blocklist support not compiled-in - no effect -\n");
#endif /* USE_DST_BLOCKLIST */
	return 1;
}


/**
 *
 */
static int ki_blst_clear_ignore_all(sip_msg_t* msg)
{
	return ki_blst_clear_ignore(msg, 0xff);
}

/**
 *
 */
static int blst_clear_ignore_f(struct sip_msg* msg, char* flags, char* foo)
{
	int mask = 0xff;

	if (unlikely(flags && (get_int_fparam(&mask, msg, (fparam_t*)flags)<0)))
		return -1;

	return ki_blst_clear_ignore(msg, mask);
}


/**
 *
 */
static int ki_blst_rpl_set_ignore(sip_msg_t* msg, int mask)
{
#ifdef USE_DST_BLOCKLIST
	unsigned char blst_imask;

	blst_imask=mask;
	msg->rpl_send_flags.blst_imask|=blst_imask;
	return 1;
#else /* USE_DST_BLOCKLIST */
	LM_WARN("blocklist support not compiled-in - no effect -\n");
#endif /* USE_DST_BLOCKLIST */
	return 1;
}


/**
 *
 */
static int ki_blst_rpl_set_ignore_all(sip_msg_t* msg)
{
	return ki_blst_rpl_set_ignore(msg, 0xff);
}


/**
 *
 */
static int blst_rpl_set_ignore_f(struct sip_msg* msg, char* flags, char* foo)
{
	int mask = 0xff;

	if (unlikely(flags && (get_int_fparam(&mask, msg, (fparam_t*)flags)<0)))
		return -1;

	return ki_blst_rpl_set_ignore(msg, mask);
}


/**
 *
 */
static int ki_blst_rpl_clear_ignore(sip_msg_t* msg, int mask)
{
#ifdef USE_DST_BLOCKLIST
	unsigned char blst_imask;

	blst_imask=mask;
	msg->rpl_send_flags.blst_imask&=~blst_imask;
	return 1;
#else /* USE_DST_BLOCKLIST */
	LM_WARN("blocklist support not compiled-in - no effect -\n");
#endif /* USE_DST_BLOCKLIST */
	return 1;
}


/**
 *
 */
static int ki_blst_rpl_clear_ignore_all(sip_msg_t* msg)
{
	return ki_blst_rpl_clear_ignore(msg, 0xff);
}


/**
 *
 */
static int blst_rpl_clear_ignore_f(struct sip_msg* msg, char* flags, char* foo)
{
	int mask = 0xff;

	if (unlikely(flags && (get_int_fparam(&mask, msg, (fparam_t*)flags)<0)))
		return -1;

	return ki_blst_rpl_clear_ignore(msg, mask);
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_blst_exports[] = {
	{ str_init("blst"), str_init("blst_add"),
		SR_KEMIP_INT, ki_blst_add,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("blst"), str_init("blst_add_default"),
		SR_KEMIP_INT, ki_blst_add_default,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("blst"), str_init("blst_add_retry_after"),
		SR_KEMIP_INT, ki_blst_add_retry_after,
		{ SR_KEMIP_INT, SR_KEMIP_INT, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("blst"), str_init("blst_del"),
		SR_KEMIP_INT, ki_blst_del,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("blst"), str_init("blst_is_blocklisted"),
		SR_KEMIP_INT, ki_blst_is_blocklisted,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("blst"), str_init("blst_set_ignore"),
		SR_KEMIP_INT, ki_blst_set_ignore,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("blst"), str_init("blst_set_ignore_all"),
		SR_KEMIP_INT, ki_blst_set_ignore_all,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("blst"), str_init("blst_clear_ignore"),
		SR_KEMIP_INT, ki_blst_clear_ignore,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("blst"), str_init("blst_clear_ignore_all"),
		SR_KEMIP_INT, ki_blst_clear_ignore_all,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("blst"), str_init("blst_rpl_set_ignore"),
		SR_KEMIP_INT, ki_blst_rpl_set_ignore,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("blst"), str_init("blst_rpl_set_ignore_all"),
		SR_KEMIP_INT, ki_blst_rpl_set_ignore_all,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("blst"), str_init("blst_rpl_clear_ignore"),
		SR_KEMIP_INT, ki_blst_rpl_clear_ignore,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("blst"), str_init("blst_rpl_clear_ignore_all"),
		SR_KEMIP_INT, ki_blst_rpl_clear_ignore_all,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},

	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_blst_exports);
	return 0;
}
