/* 
 * $Id$
 * 
 * Copyright (C) 2008 iptelorg GmbH
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
/* 
 * sctp options
 */
/*
 * History:
 * --------
 *  2008-08-07  initial version (andrei)
 *  2009-05-26  runtime cfg support (andrei)
 */

/*!
 * \file
 * \brief SIP-router core :: 
 * \ingroup core
 * Module: \ref core
 */

#include <string.h>
#include <sys/types.h>
#ifdef USE_SCTP
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/sctp.h>
#endif /* USE_SCTP */
#include <errno.h>

#include "sctp_options.h"

#include "../../dprint.h"
#include "../../cfg/cfg.h"
#include "../../socket_info.h"

#include "sctp_server.h"

struct cfg_group_sctp sctp_default_cfg;



#ifdef USE_SCTP

#include "sctp_sockopts.h"

static int fix_autoclose(void* cfg_h, str* gname, str* name, void** val);
static void set_autoclose(str* gname, str* name);
static int fix_assoc_tracking(void* cfg_h, str* gname, str* name, void** val);
static int fix_assoc_reuse(void* cfg_h, str* gname, str* name, void** val);
static int fix_srto_initial(void* cfg_h, str* gname, str* name, void** val);
static void set_srto_initial(str* gname, str* name);
static int fix_srto_max(void* cfg_h, str* gname, str* name, void** val);
static void set_srto_max(str* gname, str* name);
static int fix_srto_min(void* cfg_h, str* gname, str* name, void** val);
static void set_srto_min(str* gname, str* name);
static int fix_asocmaxrxt(void* cfg_h, str* gname, str* name, void** val);
static void set_asocmaxrxt(str* gname, str* name);
static int fix_sinit_max_init_timeo(void* cfg_h, str* gname, str* name,
										void** val);
static void set_sinit_max_init_timeo(str* gname, str* name);
static int fix_sinit_max_attempts(void* cfg_h, str* gname, str* name,
										void** val);
static void set_sinit_max_attempts(str* gname, str* name);
static int fix_hbinterval(void* cfg_h, str* gname, str* name, void** val);
static void set_hbinterval(str* gname, str* name);
static int fix_pathmaxrxt(void* cfg_h, str* gname, str* name, void** val);
static void set_pathmaxrxt(str* gname, str* name);
static int fix_sack_delay(void* cfg_h, str* gname, str* name, void** val);
static void set_sack_delay(str* gname, str* name);
static int fix_sack_freq(void* cfg_h, str* gname, str* name, void** val);
static void set_sack_freq(str* gname, str* name);
static int fix_max_burst(void* cfg_h, str* gname, str* name, void** val);
static void set_max_burst(str* gname, str* name);

/** cfg_group_sctp description (for the config framework). */
static cfg_def_t sctp_cfg_def[] = {
	/*   name        , type |input type| chg type, min, max, fixup, proc. cbk.
	      description */
	{ "socket_rcvbuf", CFG_VAR_INT| CFG_READONLY, 512, 102400, 0, 0,
		"socket receive buffer size (read-only)" },
	{ "socket_sndbuf", CFG_VAR_INT| CFG_READONLY, 512, 102400, 0, 0,
		"socket send buffer size (read-only)" },
	{ "autoclose", CFG_VAR_INT| CFG_CB_ONLY_ONCE, 1, 1<<30,
		fix_autoclose, set_autoclose,
		"seconds before closing and idle connection (must be non-zero)" },
	{ "send_ttl", CFG_VAR_INT| CFG_ATOMIC, 0, 1<<30, 0, 0,
		"milliseconds before aborting a send" },
	{ "send_retries", CFG_VAR_INT| CFG_ATOMIC, 0, MAX_SCTP_SEND_RETRIES, 0, 0,
		"re-send attempts on failure" },
	{ "assoc_tracking", CFG_VAR_INT| CFG_ATOMIC, 0, 1, fix_assoc_tracking, 0,
		"connection/association tracking (see also assoc_reuse)" },
	{ "assoc_reuse", CFG_VAR_INT| CFG_ATOMIC, 0, 1, fix_assoc_reuse, 0,
		"connection/association reuse (for now used only for replies)"
		", depends on assoc_tracking being set"},
	{ "max_assocs", CFG_VAR_INT| CFG_ATOMIC, 0, 0, 0, 0,
		"maximum allowed open associations (-1 = disable, "
			"as many as allowed by the OS)"},
	{ "srto_initial", CFG_VAR_INT| CFG_CB_ONLY_ONCE, 0, 1<<30,
		fix_srto_initial, set_srto_initial,
		"initial value of the retr. timeout, used in RTO calculations,"
			" in msecs" },
	{ "srto_max", CFG_VAR_INT| CFG_CB_ONLY_ONCE, 0, 1<<30,
		fix_srto_max, set_srto_max,
		"maximum value of the retransmission timeout (RTO), in msecs" },
	{ "srto_min", CFG_VAR_INT| CFG_CB_ONLY_ONCE, 0, 1<<30,
		fix_srto_min, set_srto_min,
		"minimum value of the retransmission timeout (RTO), in msecs" },
	{ "asocmaxrxt", CFG_VAR_INT| CFG_CB_ONLY_ONCE, 0, 1<<10,
		fix_asocmaxrxt, set_asocmaxrxt,
		"maximum retransmission attempts per association" },
	{ "init_max_attempts", CFG_VAR_INT| CFG_CB_ONLY_ONCE, 0, 1<<10,
		fix_sinit_max_attempts, set_sinit_max_attempts,
		"max INIT retransmission attempts" },
	{ "init_max_timeo", CFG_VAR_INT| CFG_CB_ONLY_ONCE, 0, 1<<30,
		fix_sinit_max_init_timeo, set_sinit_max_init_timeo,
		"max INIT retransmission timeout (RTO max for INIT), in msecs" },
	{ "hbinterval", CFG_VAR_INT| CFG_CB_ONLY_ONCE, 0, 1<<30,
		fix_hbinterval, set_hbinterval, "heartbeat interval in msecs" },
	{ "pathmaxrxt", CFG_VAR_INT| CFG_CB_ONLY_ONCE, 0, 1<<10,
		fix_pathmaxrxt, set_pathmaxrxt,
		"maximum retransmission attempts per path" },
	{ "sack_delay", CFG_VAR_INT| CFG_CB_ONLY_ONCE, 0, 1<<30,
		fix_sack_delay, set_sack_delay,
		"time since the last received packet before sending a SACK, in msecs"},
	{ "sack_freq", CFG_VAR_INT| CFG_CB_ONLY_ONCE, 0, 1<<10,
		fix_sack_freq, set_sack_freq,
		"number of received packets that trigger the sending of a SACK"},
	{ "max_burst", CFG_VAR_INT| CFG_CB_ONLY_ONCE, 0, 1<<10,
		fix_max_burst, set_max_burst,
		"maximum burst of packets that can be emitted by an association"},
	{0, 0, 0, 0, 0, 0, 0}
};



void* sctp_cfg; /* sctp config handle */

#endif /* USE_SCTP */

void init_sctp_options()
{
#ifdef USE_SCTP
	sctp_get_os_defaults(&sctp_default_cfg);
#if 0
	sctp_default_cfg.so_rcvbuf=0; /* do nothing, use the kernel default */
	sctp_default_cfg.so_sndbuf=0; /* do nothing, use the kernel default */
#endif
	sctp_default_cfg.autoclose=DEFAULT_SCTP_AUTOCLOSE; /* in seconds */
	sctp_default_cfg.send_ttl=DEFAULT_SCTP_SEND_TTL;   /* in milliseconds */
	sctp_default_cfg.send_retries=DEFAULT_SCTP_SEND_RETRIES;
	sctp_default_cfg.max_assocs=-1; /* as much as possible by default */
#ifdef SCTP_CONN_REUSE
	sctp_default_cfg.assoc_tracking=1; /* on by default */
	sctp_default_cfg.assoc_reuse=1; /* on by default */
#else
	sctp_default_cfg.assoc_tracking=0;
	sctp_default_cfg.assoc_reuse=0;
#endif /* SCTP_CONN_REUSE */
#endif
}



#define W_OPT_NSCTP(option) \
	if (sctp_default_cfg.option){\
		WARN("sctp_options: " #option \
			" cannot be enabled (sctp support not compiled-in)\n"); \
			sctp_default_cfg.option=0; \
	}



void sctp_options_check()
{
#ifndef USE_SCTP
	W_OPT_NSCTP(autoclose);
	W_OPT_NSCTP(send_ttl);
	W_OPT_NSCTP(send_retries);
	W_OPT_NSCTP(assoc_tracking);
	W_OPT_NSCTP(assoc_reuse);
	W_OPT_NSCTP(max_assocs);
#else /* USE_SCTP */
	if (sctp_default_cfg.send_retries>MAX_SCTP_SEND_RETRIES) {
		WARN("sctp: sctp_send_retries too high (%d), setting it to %d\n",
				sctp_default_cfg.send_retries, MAX_SCTP_SEND_RETRIES);
		sctp_default_cfg.send_retries=MAX_SCTP_SEND_RETRIES;
	}
#ifndef CONN_REUSE
	if (sctp_default_cfg.assoc_tracking || sctp_default_cfg.assoc_reuse){
		WARN("sctp_options: assoc_tracking and assoc_reuse support cannnot"
				" be enabled (CONN_REUSE support not compiled-in)\n");
		sctp_default_cfg.assoc_tracking=0;
		sctp_default_cfg.assoc_reuse=0;
	}
#else /* CONN_REUSE */
	if (sctp_default_cfg.assoc_reuse && sctp_default_cfg.assoc_tracking==0){
		sctp_default_cfg.assoc_tracking=1;
	}
#endif /* CONN_REUSE */
#endif /* USE_SCTP */
}



void sctp_options_get(struct cfg_group_sctp *s)
{
#ifdef USE_SCTP
	*s=*(struct cfg_group_sctp*)sctp_cfg;
#else
	memset(s, 0, sizeof(*s));
#endif /* USE_SCTP */
}



#ifdef USE_SCTP
/** register sctp config into the configuration framework.
 * @return 0 on success, -1 on error */
int sctp_register_cfg()
{
	if (cfg_declare("sctp", sctp_cfg_def, &sctp_default_cfg, cfg_sizeof(sctp),
				&sctp_cfg))
		return -1;
	if (sctp_cfg==0){
		BUG("null sctp cfg");
		return -1;
	}
	return 0;
}



#define SCTP_SET_SOCKOPT_DECLS \
	int err; \
	struct socket_info* si


#define SCTP_SET_SOCKOPT_BODY_NRET(lev, opt_name, val, err_prefix) \
	err=0; \
	for (si=sctp_listen; si; si=si->next){ \
		err+=(sctp_setsockopt(si->socket, (lev), (opt_name), (void*)(&(val)), \
							sizeof((val)), (err_prefix))<0); \
	}

#define SCTP_SET_SOCKOPT_BODY(lev, opt_name, val, err_prefix) \
	SCTP_SET_SOCKOPT_BODY_NRET(lev, opt_name, val, err_prefix) ; \
	return -(err!=0)


static int fix_autoclose(void*cfg_h, str* gname, str* name, void** val)
{
#ifdef SCTP_AUTOCLOSE
	return 0;
#else
	ERR("no SCTP_AUTOCLOSE support, please upgrade your sctp library\n");
	return -1;
#endif /* SCTP_AUTOCLOSE */
}


static void set_autoclose(str* gname, str* name)
{
#ifdef SCTP_AUTOCLOSE
	int optval;
	SCTP_SET_SOCKOPT_DECLS;
	
	optval=cfg_get(sctp, sctp_cfg, autoclose);
	SCTP_SET_SOCKOPT_BODY_NRET(IPPROTO_SCTP, SCTP_AUTOCLOSE, optval,
								"cfg: setting SCTP_AUTOCLOSE");
#else
	ERR("no SCTP_AUTOCLOSE support, please upgrade your sctp library\n");
#endif /* SCTP_AUTOCLOSE */
}



static int fix_assoc_tracking(void* cfg_h, str* gname, str* name, void** val)
{
	int optval;
	
	optval=(int)(long)(*val);
#ifndef SCTP_CONN_REUSE
	if (optval!=0){
		ERR("no SCTP_CONN_REUSE support, please recompile with it enabled\n");
		return -1;
	}
#else /* SCTP_CONN_REUSE */
	if (optval==0){
		/* turn tracking off */
		/* check if assoc_reuse is off */
		if (cfg_get(sctp, cfg_h, assoc_reuse)!=0){
			ERR("cannot turn sctp assoc_tracking off while assoc_reuse is"
					" still on, please turn assoc_reuse off first\n");
			return -1;
		}
		sctp_con_tracking_flush();
	}else if (optval==1 && cfg_get(sctp, cfg_h, assoc_reuse)==0){
		/* turning from off to on, make sure we flush the tracked list
		   again, just incase the off flush was racing with a new connection*/
		sctp_con_tracking_flush();
	}
#endif /* SCTP_CONN_REUSE */
	return 0;
}



static int fix_assoc_reuse(void* cfg_h, str* gname, str* name, void** val)
{
	int optval;
	
	optval=(int)(long)(*val);
#ifndef SCTP_CONN_REUSE
	if (optval!=0){
		ERR("no SCTP_CONN_REUSE support, please recompile with it enabled\n");
		return -1;
	}
#else /* SCTP_CONN_REUSE */
	if (optval==1 && cfg_get(sctp, cfg_h, assoc_tracking)==0){
		/* conn reuse on, but assoc_tracking off => not possible */
		ERR("cannot turn sctp assoc_reuse on while assoc_tracking is"
					" off, please turn assoc_tracking on first\n");
		return -1;
	}
#endif /* SCTP_CONN_REUSE */
	return 0;
}



static int fix_srto_initial(void* cfg_h, str* gname, str* name, void** val)
{
#ifdef SCTP_RTOINFO
	if ((int)(long)(*val)==0){ /* do nothing for 0, keep the old value */
		*val=(void*)(long)cfg_get(sctp, cfg_h, srto_initial);
	}
	return 0;
#else
	ERR("no SCTP_RTOINFO support, please upgrade your sctp library\n");
	return -1;
#endif /* SCTP_RTOINFO */
}


static void set_srto_initial(str* gname, str* name)
{
#ifdef SCTP_RTOINFO
	struct sctp_rtoinfo rto;
	int optval;
	SCTP_SET_SOCKOPT_DECLS;
	
	optval=cfg_get(sctp, sctp_cfg, srto_initial);
	memset(&rto, 0, sizeof(rto)); /* zero everything we don't care about */
	rto.srto_assoc_id=0; /* all */
	rto.srto_initial=optval;
	SCTP_SET_SOCKOPT_BODY_NRET(IPPROTO_SCTP, SCTP_RTOINFO, rto,
								"cfg: setting SCTP_RTOINFO");
#else
	ERR("no SCTP_RTOINFO support, please upgrade your sctp library\n");
#endif /* SCTP_RTOINFO */
}



static int fix_srto_max(void* cfg_h, str* gname, str* name, void** val)
{
#ifdef SCTP_RTOINFO
	if ((int)(long)(*val)==0){ /* do nothing for 0, keep the old value */
		*val=(void*)(long)cfg_get(sctp, cfg_h, srto_max);
	}
	return 0;
#else
	ERR("no SCTP_RTOINFO support, please upgrade your sctp library\n");
	return -1;
#endif /* SCTP_RTOINFO */
}


static void set_srto_max(str* gname, str* name)
{
#ifdef SCTP_RTOINFO
	struct sctp_rtoinfo rto;
	SCTP_SET_SOCKOPT_DECLS;
	
	memset(&rto, 0, sizeof(rto)); /* zero everything we don't care about */
	rto.srto_assoc_id=0; /* all */
	rto.srto_max=cfg_get(sctp, sctp_cfg, srto_max);
	SCTP_SET_SOCKOPT_BODY_NRET(IPPROTO_SCTP, SCTP_RTOINFO, rto,
								"cfg: setting SCTP_RTOINFO");
#else
	ERR("no SCTP_RTOINFO support, please upgrade your sctp library\n");
#endif /* SCTP_RTOINFO */
}



static int fix_srto_min(void* cfg_h, str* gname, str* name, void** val)
{
#ifdef SCTP_RTOINFO
	if ((int)(long)(*val)==0){ /* do nothing for 0, keep the old value */
		*val=(void*)(long)cfg_get(sctp, cfg_h, srto_min);
	}
	return 0;
#else
	ERR("no SCTP_RTOINFO support, please upgrade your sctp library\n");
	return -1;
#endif /* SCTP_RTOINFO */
}


static void set_srto_min(str* gname, str* name)
{
#ifdef SCTP_RTOINFO
	struct sctp_rtoinfo rto;
	SCTP_SET_SOCKOPT_DECLS;
	
	memset(&rto, 0, sizeof(rto)); /* zero everything we don't care about */
	rto.srto_assoc_id=0; /* all */
	rto.srto_min=cfg_get(sctp, sctp_cfg, srto_min);
	SCTP_SET_SOCKOPT_BODY_NRET(IPPROTO_SCTP, SCTP_RTOINFO, rto,
								"cfg: setting SCTP_RTOINFO");
#else
	ERR("no SCTP_RTOINFO support, please upgrade your sctp library\n");
#endif /* SCTP_RTOINFO */
}



static int fix_asocmaxrxt(void* cfg_h, str* gname, str* name, void** val)
{
#ifdef SCTP_ASSOCINFO
	if ((int)(long)(*val)==0){ /* do nothing for 0, keep the old value */
		*val=(void*)(long)cfg_get(sctp, cfg_h, asocmaxrxt);
	}
	return 0;
#else
	ERR("no SCTP_ASSOCINFO support, please upgrade your sctp library\n");
	return -1;
#endif /* SCTP_ASSOCINFO */
}


static void set_asocmaxrxt(str* gname, str* name)
{
#ifdef SCTP_ASSOCINFO
	struct sctp_assocparams ap;
	SCTP_SET_SOCKOPT_DECLS;
	
	memset(&ap, 0, sizeof(ap)); /* zero everything we don't care about */
	ap.sasoc_assoc_id=0; /* all */
	ap.sasoc_asocmaxrxt= cfg_get(sctp, sctp_cfg, asocmaxrxt);
	SCTP_SET_SOCKOPT_BODY_NRET(IPPROTO_SCTP, SCTP_ASSOCINFO, ap,
								"cfg: setting SCTP_ASSOCINFO");
#else
	ERR("no SCTP_ASSOCINFO support, please upgrade your sctp library\n");
#endif /* SCTP_ASSOCINFO */
}



static int fix_sinit_max_init_timeo(void* cfg_h, str* gname, str* name,
									void** val)
{
#ifdef SCTP_INITMSG
	if ((int)(long)(*val)==0){ /* do nothing for 0, keep the old value */
		*val=(void*)(long)cfg_get(sctp, cfg_h, init_max_timeo);
	}
	return 0;
#else
	ERR("no SCTP_INITMSG support, please upgrade your sctp library\n");
	return -1;
#endif /* SCTP_INITMSG */
}


static void set_sinit_max_init_timeo(str* gname, str* name)
{
#ifdef SCTP_INITMSG
	struct sctp_initmsg im;
	SCTP_SET_SOCKOPT_DECLS;
	
	memset(&im, 0, sizeof(im)); /* zero everything we don't care about */
	im.sinit_max_init_timeo=cfg_get(sctp, sctp_cfg, init_max_timeo);
	SCTP_SET_SOCKOPT_BODY_NRET(IPPROTO_SCTP, SCTP_INITMSG, im,
								"cfg: setting SCTP_INITMSG");
#else
	ERR("no SCTP_INITMSG support, please upgrade your sctp library\n");
#endif /* SCTP_INITMSG */
}



static int fix_sinit_max_attempts(void* cfg_h, str* gname, str* name,
									void** val)
{
#ifdef SCTP_INITMSG
	if ((int)(long)(*val)==0){ /* do nothing for 0, keep the old value */
		*val=(void*)(long)cfg_get(sctp, cfg_h, init_max_attempts);
	}
	return 0;
#else
	ERR("no SCTP_INITMSG support, please upgrade your sctp library\n");
	return -1;
#endif /* SCTP_INITMSG */
}


static void set_sinit_max_attempts(str* gname, str* name)
{
#ifdef SCTP_INITMSG
	struct sctp_initmsg im;
	SCTP_SET_SOCKOPT_DECLS;
	
	memset(&im, 0, sizeof(im)); /* zero everything we don't care about */
	im.sinit_max_attempts=cfg_get(sctp, sctp_cfg, init_max_attempts);
	SCTP_SET_SOCKOPT_BODY_NRET(IPPROTO_SCTP, SCTP_INITMSG, im,
								"cfg: setting SCTP_INITMSG");
#else
	ERR("no SCTP_INITMSG support, please upgrade your sctp library\n");
#endif /* SCTP_INITMSG */
}



static int fix_hbinterval(void* cfg_h, str* gname, str* name,
									void** val)
{
#ifdef SCTP_PEER_ADDR_PARAMS
	if ((int)(long)(*val)==0){ /* do nothing for 0, keep the old value */
		*val=(void*)(long)cfg_get(sctp, cfg_h, hbinterval);
	}
	return 0;
#else
	ERR("no SCTP_PEER_ADDR_PARAMS support, please upgrade your"
			" sctp library\n");
	return -1;
#endif /* SCTP_PEER_ADDR_PARAMS */
}


static void set_hbinterval(str* gname, str* name)
{
#ifdef SCTP_PEER_ADDR_PARAMS
	struct sctp_paddrparams pp;
	int optval;
	SCTP_SET_SOCKOPT_DECLS;
	
	optval=cfg_get(sctp, sctp_cfg, hbinterval);
	memset(&pp, 0, sizeof(pp)); /* zero everything we don't care about */
	if (optval!=-1){
		pp.spp_hbinterval=optval;
		pp.spp_flags=SPP_HB_ENABLE;
	}else{
		pp.spp_flags=SPP_HB_DISABLE;
	}
	err=0;
	for (si=sctp_listen; si; si=si->next){
		/* set the AF, needed on older linux kernels even for INADDR_ANY */
		pp.spp_address.ss_family=si->address.af;
		err+=(sctp_setsockopt(si->socket, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS,
								(void*)(&pp), sizeof(pp),
								"cfg: setting SCTP_PEER_ADDR_PARAMS")<0);
	}
#else
	ERR("no SCTP_PEER_ADDR_PARAMS support, please upgrade your"
			" sctp library\n");
#endif /* SCTP_PEER_ADDR_PARAMS */
}



static int fix_pathmaxrxt(void* cfg_h, str* gname, str* name,
									void** val)
{
#ifdef SCTP_PEER_ADDR_PARAMS
	if ((int)(long)(*val)==0){ /* do nothing for 0, keep the old value */
		*val=(void*)(long)cfg_get(sctp, cfg_h, pathmaxrxt);
	}
	return 0;
#else
	ERR("no SCTP_PEER_ADDR_PARAMS support, please upgrade your"
			" sctp library\n");
	return -1;
#endif /* SCTP_PEER_ADDR_PARAMS */
}


static void set_pathmaxrxt(str* gname, str* name)
{
#ifdef SCTP_PEER_ADDR_PARAMS
	struct sctp_paddrparams pp;
	SCTP_SET_SOCKOPT_DECLS;
	
	memset(&pp, 0, sizeof(pp)); /* zero everything we don't care about */
	pp.spp_pathmaxrxt=cfg_get(sctp, sctp_cfg, pathmaxrxt);
	err=0;
	for (si=sctp_listen; si; si=si->next){
		/* set the AF, needed on older linux kernels even for INADDR_ANY */
		pp.spp_address.ss_family=si->address.af;
		err+=(sctp_setsockopt(si->socket, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS,
								(void*)(&pp), sizeof(pp),
								"cfg: setting SCTP_PEER_ADDR_PARAMS")<0);
	}
#else
	ERR("no SCTP_PEER_ADDR_PARAMS support, please upgrade your"
			" sctp library\n");
#endif /* SCTP_PEER_ADDR_PARAMS */
}



static int fix_sack_delay(void* cfg_h, str* gname, str* name, void** val)
{
#if defined SCTP_DELAYED_SACK || defined SCTP_DELAYED_ACK_TIME
	if ((int)(long)(*val)==0){ /* do nothing for 0, keep the old value */
		*val=(void*)(long)cfg_get(sctp, cfg_h, sack_delay);
	}
	return 0;
#else
	ERR("no SCTP_DELAYED_SACK support, please upgrade your sctp library\n");
	return -1;
#endif /* SCTP_DELAYED_SACK | SCTP_DELAYED_ACK_TIME */
}


static void set_sack_delay(str* gname, str* name)
{
#if defined SCTP_DELAYED_SACK || defined SCTP_DELAYED_ACK_TIME
#ifdef SCTP_DELAYED_SACK
	struct sctp_sack_info sack_info;
#endif /* SCTP_DELAYED_SACK */
#ifdef	SCTP_DELAYED_ACK_TIME
	struct sctp_assoc_value sack_val; /* old version, sack delay only */
#endif /* SCTP_DELAYED_ACK_TIME */
	SCTP_SET_SOCKOPT_DECLS;
	
#ifdef SCTP_DELAYED_SACK
	memset(&sack_info, 0, sizeof(sack_info)); /* zero everything we don't
												 care about */
	sack_info.sack_delay=cfg_get(sctp, sctp_cfg, sack_delay);
	SCTP_SET_SOCKOPT_BODY_NRET(IPPROTO_SCTP, SCTP_DELAYED_SACK, sack_info, 0);
	if (err==0){
		return;
	}else
#endif /* SCTP_DELAYED_SACK */
	{
		/* setting SCTP_DELAYED_SACK failed or no lib support for 
		   SCTP_DELAYED_SACK => try the old obsolete SCTP_DELAYED_ACK_TIME */
#ifdef	SCTP_DELAYED_ACK_TIME
		memset(&sack_val, 0, sizeof(sack_val)); /* zero everything we don't
												   care about */
		sack_val.assoc_value=cfg_get(sctp, sctp_cfg, sack_delay);
		SCTP_SET_SOCKOPT_BODY_NRET(IPPROTO_SCTP, SCTP_DELAYED_ACK_TIME,
									sack_val,
									"cfg: setting SCTP_DELAYED_ACK_TIME");
		if (err==0)
			return;
#else	/* SCTP_DELAYED_ACK_TIME */
		/* no SCTP_DELAYED_ACK_TIME support and SCTP_DELAYED_SACK failed
		   => error */
		ERR("cfg: setting SCTP_DELAYED_SACK: %s [%d]\n",
					strerror(errno), errno);
#endif /* SCTP_DELAYED_ACK_TIME */
	}
#else
	ERR("no SCTP_DELAYED_SACK support, please upgrade your sctp library\n");
#endif /* SCTP_DELAYED_SACK | SCTP_DELAYED_ACK_TIME */
}



static int fix_sack_freq(void* cfg_h, str* gname, str* name, void** val)
{
#ifdef SCTP_DELAYED_SACK
	if ((int)(long)(*val)==0){ /* do nothing for 0, keep the old value */
		*val=(void*)(long)cfg_get(sctp, cfg_h, sack_freq);
	}
	return 0;
#else
	ERR("no SCTP_DELAYED_SACK support, please upgrade your sctp library\n");
	return -1;
#endif /* SCTP_DELAYED_SACK */
}


static void set_sack_freq(str* gname, str* name)
{
#ifdef SCTP_DELAYED_SACK
	struct sctp_sack_info sa;
	SCTP_SET_SOCKOPT_DECLS;
	
	memset(&sa, 0, sizeof(sa)); /* zero everything we don't care about */
	sa.sack_freq=cfg_get(sctp, sctp_cfg, sack_freq);
	SCTP_SET_SOCKOPT_BODY_NRET(IPPROTO_SCTP, SCTP_DELAYED_SACK, sa,
								"cfg: setting SCTP_DELAYED_SACK");
#else
	ERR("no SCTP_DELAYED_SACK support, please upgrade your sctp library\n");
#endif /* SCTP_DELAYED_SACK */
}



static int fix_max_burst(void* cfg_h, str* gname, str* name, void** val)
{
#ifdef SCTP_MAX_BURST
	if ((int)(long)(*val)==0){ /* do nothing for 0, keep the old value */
		*val=(void*)(long)cfg_get(sctp, cfg_h, max_burst);
	}
	return 0;
#else
	ERR("no SCTP_MAX_BURST support, please upgrade your sctp library\n");
	return -1;
#endif /* SCTP_MAX_BURST */
}


static void set_max_burst(str* gname, str* name)
{
#ifdef SCTP_MAX_BURST
	struct sctp_assoc_value av;
	SCTP_SET_SOCKOPT_DECLS;
	
	memset(&av, 0, sizeof(av)); /* zero everything we don't care about */
	av.assoc_value=cfg_get(sctp, sctp_cfg, max_burst);
	SCTP_SET_SOCKOPT_BODY_NRET(IPPROTO_SCTP, SCTP_MAX_BURST, av,
								"cfg: setting SCTP_MAX_BURST");
#else
	ERR("no SCTP_MAX_BURST support, please upgrade your sctp library\n");
#endif /* SCTP_MAX_BURST */
}

#endif /* USE_SCTP */
