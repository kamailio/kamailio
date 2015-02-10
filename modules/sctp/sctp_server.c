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
 * sctp one to many 
 */
/*
 * History:
 * --------
 *  2008-08-07  initial version (andrei)
 *  2009-02-27  blacklist support (andrei)
 *  2009-04-28  sctp stats & events macros (andrei)
 */

/*!
 * \file
 * \brief SIP-router core :: 
 * \ingroup core
 * Module: \ref core
 */

#ifdef USE_SCTP

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/sctp.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>


#include "sctp_sockopts.h"
#include "sctp_server.h"
#include "sctp_options.h"

#include "../../globals.h"
#include "../../config.h"
#include "../../dprint.h"
#include "../../receive.h"
#include "../../mem/mem.h"
#include "../../ip_addr.h"
#include "../../cfg/cfg_struct.h"
#ifdef USE_DST_BLACKLIST
#include "../../dst_blacklist.h"
#endif /* USE_DST_BLACKLIST */
#include "../../timer_ticks.h"
#include "../../clist.h"
#include "../../error.h"
#include "../../timer.h"

#include "sctp_stats.h"
#include "sctp_ev.h"



static atomic_t* sctp_conn_no;


#define ABORT_REASON_MAX_ASSOCS \
	"Maximum configured number of open associations exceeded"

/* check if the underlying OS supports sctp
   returns 0 if yes, -1 on error */
int sctp_check_support()
{
	int s;
	char buf[256];
	
	s = socket(PF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
	if (s!=-1){
		close(s);
		if (sctp_check_compiled_sockopts(buf, sizeof(buf))!=0){
			LOG(L_WARN, "WARNING: sctp: your ser version was compiled"
						" without support for the following sctp options: %s"
						", which might cause unforseen problems \n", buf);
			LOG(L_WARN, "WARNING: sctp: please consider recompiling ser with"
						" an upgraded sctp library version\n");
		}
		return 0;
	}
	return -1;
}



/* append a token to a buffer (uses space between tokens) */
inline static void append_tok2buf(char* buf, int blen, char* tok)
{
	char* p;
	char* end;
	int len;
	
	if (buf && blen){
		end=buf+blen;
		p=memchr(buf, 0, blen);
		if (p==0) goto error;
		if (p!=buf && p<(end-1)){
			*p=' ';
			p++;
		}
		len=MIN_int(strlen(tok), end-1-p);
		memcpy(p, tok, len);
		p[len]=0;
	}
error:
	return;
}



/* check if support fot all the needed sockopts  was compiled;
   an ascii list of the unsuported options is returned in buf
   returns 0 on success and  -number of unsuported options on failure
   (<0 on failure)
*/
int sctp_check_compiled_sockopts(char* buf, int size)
{
	int err;

	err=0;
	if (buf && (size>0)) *buf=0; /* "" */
#ifndef SCTP_FRAGMENT_INTERLEAVE
	err++;
	append_tok2buf(buf, size, "SCTP_FRAGMENT_INTERLEAVE");
#endif
#ifndef SCTP_PARTIAL_DELIVERY_POINT
	err++;
	append_tok2buf(buf, size, "SCTP_PARTIAL_DELIVERY_POINT");
#endif
#ifndef SCTP_NODELAY
	err++;
	append_tok2buf(buf, size, "SCTP_NODELAY");
#endif
#ifndef SCTP_DISABLE_FRAGMENTS
	err++;
	append_tok2buf(buf, size, "SCTP_DISABLE_FRAGMENTS");
#endif
#ifndef SCTP_AUTOCLOSE
	err++;
	append_tok2buf(buf, size, "SCTP_AUTOCLOSE");
#endif
#ifndef SCTP_EVENTS
	err++;
	append_tok2buf(buf, size, "SCTP_EVENTS");
#endif
	
	return -err;
}



/* init all the sockaddr_union members of the socket_info struct
   returns 0 on success and -1 on error */
inline static int sctp_init_su(struct socket_info* sock_info)
{
	union sockaddr_union* addr;
	struct addr_info* ai;
	
	addr=&sock_info->su;
	if (init_su(addr, &sock_info->address, sock_info->port_no)<0){
		LOG(L_ERR, "ERROR: sctp_init_su: could not init sockaddr_union for"
					"primary sctp address %.*s:%d\n",
					sock_info->address_str.len, sock_info->address_str.s,
					sock_info->port_no );
		goto error;
	}
	for (ai=sock_info->addr_info_lst; ai; ai=ai->next)
		if (init_su(&ai->su, &ai->address, sock_info->port_no)<0){
			LOG(L_ERR, "ERROR: sctp_init_su: could not init"
					"backup sctp sockaddr_union for %.*s:%d\n",
					ai->address_str.len, ai->address_str.s,
					sock_info->port_no );
			goto error;
		}
	return 0;
error:
	return -1;
}



/** set a socket option (wrapper over setsockopt).
  * @param err_prefix - if 0 no error message is printed on failure, if !=0
  *                     it will be prepended to the error message.
  * @return 0 on success, -1 on error */
int sctp_setsockopt(int s, int level, int optname, 
					void* optval, socklen_t optlen, char* err_prefix)
{
	if (setsockopt(s, level, optname, optval, optlen) ==-1){
		if (err_prefix)
			ERR("%s: %s [%d]\n", err_prefix, strerror(errno), errno);
		return -1;
	}
	return 0;
}



/** get a socket option (wrapper over getsockopt).
  * @param err_prefix - if 0 no error message is printed on failure, if !=0
  *                     it will be prepended to the error message.
  * @return 0 on success, -1 on error */
int sctp_getsockopt(int s, int level, int optname, 
					void* optval, socklen_t* optlen, char* err_prefix)
{
	if (getsockopt(s, level, optname, optval, optlen) ==-1){
		if (err_prefix)
			ERR("%s: %s [%d]\n", err_prefix, strerror(errno), errno);
		return -1;
	}
	return 0;
}



/** get the os defaults for cfg options with os correspondents.
 *  @param cfg - filled with the os defaults
 *  @return -1 on error, 0 on success
 */
int sctp_get_os_defaults(struct cfg_group_sctp* cfg)
{
	int s;
	int ret;
	
	s = socket(PF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
	if (s==-1)
		return -1;
	ret=sctp_get_cfg_from_sock(s, cfg);
	close(s);
	return ret;
}



/** get the os cfg options from a specific socket.
 *  @param s - intialized sctp socket
 *  @param cfg - filled with the os defaults
 *  @return -1 on error, 0 on success
 */
int sctp_get_cfg_from_sock(int s, struct cfg_group_sctp* cfg)
{
	int optval;
	socklen_t optlen;
#ifdef SCTP_RTOINFO
	struct sctp_rtoinfo rto;
#endif /* SCTP_RTOINFO */
#ifdef SCTP_ASSOCINFO
	struct sctp_assocparams ap;
#endif /* SCTP_ASSOCINFO */
#ifdef SCTP_INITMSG
	struct sctp_initmsg im;
#endif /* SCTP_INITMSG */
#ifdef SCTP_PEER_ADDR_PARAMS
	struct sctp_paddrparams pp;
#endif /* SCTP_PEER_ADDR_PARAMS */
#ifdef	SCTP_DELAYED_SACK
	struct sctp_sack_info sack_info;
#endif	/* SCTP_DELAYED_SACK */
#ifdef	SCTP_DELAYED_ACK_TIME
	struct sctp_assoc_value sack_val; /* old version */
#endif /* SCTP_DELAYED_ACK_TIME */
#ifdef SCTP_MAX_BURST
	struct sctp_assoc_value av;
#endif /* SCTP_MAX_BURST */
	
	/* SO_RCVBUF */
	optlen=sizeof(int);
	if (sctp_getsockopt(s, SOL_SOCKET, SO_RCVBUF, (void*)&optval,
							&optlen, "SO_RCVBUF")==0){
		/* success => hack to set the "default" values*/
		#ifdef __OS_linux
			optval/=2; /* in linux getsockopt() returns 2*set_value */
		#endif
		cfg->so_rcvbuf=optval;
	}
	/* SO_SNDBUF */
	optlen=sizeof(int);
	if (sctp_getsockopt(s, SOL_SOCKET, SO_SNDBUF, (void*)&optval,
							&optlen, "SO_SNDBUF")==0){
		/* success => hack to set the "default" values*/
		#ifdef __OS_linux
			optval/=2; /* in linux getsockopt() returns 2*set_value */
		#endif
		cfg->so_sndbuf=optval;
	}
	/* SCTP_AUTOCLOSE */
#ifdef SCTP_AUTOCLOSE
	optlen=sizeof(int);
	if (sctp_getsockopt(s, IPPROTO_SCTP, SCTP_AUTOCLOSE, (void*)&optval,
							&optlen, "SCTP_AUTOCLOSE")==0){
		cfg->autoclose=optval;
	}
#endif /* SCTP_AUTOCLOSE */
	/* SCTP_RTOINFO -> srto_initial, srto_min, srto_max */
#ifdef SCTP_RTOINFO
	optlen=sizeof(rto);
	rto.srto_assoc_id=0;
	if (sctp_getsockopt(s, IPPROTO_SCTP, SCTP_RTOINFO, (void*)&rto,
							&optlen, "SCTP_RTOINFO")==0){
		/* success => hack to set the "default" values*/
		cfg->srto_initial=rto.srto_initial;
		cfg->srto_min=rto.srto_min;
		cfg->srto_max=rto.srto_max;
	}
#endif /* SCTP_RTOINFO */
#ifdef SCTP_ASSOCINFO
	optlen=sizeof(ap);
	ap.sasoc_assoc_id=0;
	if (sctp_getsockopt(s, IPPROTO_SCTP, SCTP_ASSOCINFO, (void*)&ap,
							&optlen, "SCTP_ASSOCINFO")==0){
		/* success => hack to set the "default" values*/
		cfg->asocmaxrxt=ap.sasoc_asocmaxrxt;
	}
#endif /* SCTP_ASSOCINFO */
#ifdef SCTP_INITMSG
	optlen=sizeof(im);
	if (sctp_getsockopt(s, IPPROTO_SCTP, SCTP_INITMSG, (void*)&im,
							&optlen, "SCTP_INITMSG")==0){
		/* success => hack to set the "default" values*/
		cfg->init_max_attempts=im.sinit_max_attempts;
		cfg->init_max_timeo=im.sinit_max_init_timeo;
	}
#endif /* SCTP_INITMSG */
#ifdef SCTP_PEER_ADDR_PARAMS
	optlen=sizeof(pp);
	memset(&pp, 0, sizeof(pp)); /* get defaults */
	/* set the AF, needed on older linux kernels even for INADDR_ANY */
	pp.spp_address.ss_family=AF_INET;
	if (sctp_getsockopt(s, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, (void*)&pp,
							&optlen, "SCTP_PEER_ADDR_PARAMS")==0){
		/* success => hack to set the "default" values*/
		cfg->hbinterval=pp.spp_hbinterval;
		cfg->pathmaxrxt=pp.spp_pathmaxrxt;
	}
#endif /* SCTP_PEER_ADDR_PARAMS */
#if defined SCTP_DELAYED_SACK || defined SCTP_DELAYED_ACK_TIME
#ifdef SCTP_DELAYED_SACK
	optlen=sizeof(sack_info);
	memset(&sack_info, 0, sizeof(sack_info));
	if (sctp_getsockopt(s, IPPROTO_SCTP, SCTP_DELAYED_SACK, (void*)&sack_info,
							&optlen, 0)==0){
		/* success => hack to set the "default" values*/
		cfg->sack_delay=sack_info.sack_delay;
		cfg->sack_freq=sack_info.sack_freq;
	}else
#endif /* SCTP_DELAYED_SACK */
	{
#ifdef	SCTP_DELAYED_ACK_TIME
		optlen=sizeof(sack_val);
		memset(&sack_val, 0, sizeof(sack_val));
		/* if no SCTP_DELAYED_SACK supported by the sctp lib, or setting it
		   failed (not supported by the kernel) try using the obsolete
		   SCTP_DELAYED_ACK_TIME method */
		if (sctp_getsockopt(s, IPPROTO_SCTP, SCTP_DELAYED_ACK_TIME,
								(void*)&sack_val, &optlen, 
								"SCTP_DELAYED_ACK_TIME")==0){
			/* success => hack to set the "default" values*/
			cfg->sack_delay=sack_val.assoc_value;
			cfg->sack_freq=0; /* unknown */
		}
#else	/* SCTP_DELAYED_ACK_TIME */
		/* no SCTP_DELAYED_ACK_TIME support and SCTP_DELAYED_SACK failed
		   => error */
		ERR("cfg: SCTP_DELAYED_SACK: %s [%d]\n", strerror(errno), errno);
#endif /* SCTP_DELAYED_ACK_TIME */
	}
#endif /* SCTP_DELAYED_SACK  | SCTP_DELAYED_ACK_TIME*/
#ifdef SCTP_MAX_BURST
	optlen=sizeof(av);
	av.assoc_id=0;
	if (sctp_getsockopt(s, IPPROTO_SCTP, SCTP_MAX_BURST, (void*)&av,
							&optlen, "SCTP_MAX_BURST")==0){
		/* success => hack to set the "default" values*/
		cfg->max_burst=av.assoc_value;
	}
#endif /* SCTP_MAX_BURST */
	
	return 0;
}



/* set common (for one to many and one to one) sctp socket options
   tries to ignore non-critical errors (it will only log them), for
   improved portability (for example older linux kernel version support
   only a limited number of sctp socket options)
   returns 0 on success, -1 on error
   WARNING: please keep it sync'ed w/ sctp_check_compiled_sockopts() */
static int sctp_init_sock_opt_common(int s, int af)
{
	int optval;
	int pd_point;
	int saved_errno;
	socklen_t optlen;
	int sctp_err;
#ifdef SCTP_RTOINFO
	struct sctp_rtoinfo rto;
#endif /* SCTP_RTOINFO */
#ifdef SCTP_ASSOCINFO
	struct sctp_assocparams ap;
#endif /* SCTP_ASSOCINFO */
#ifdef SCTP_INITMSG
	struct sctp_initmsg im;
#endif /* SCTP_INITMSG */
#ifdef SCTP_PEER_ADDR_PARAMS
	struct sctp_paddrparams pp;
#endif /* SCTP_PEER_ADDR_PARAMS */
#ifdef SCTP_DELAYED_SACK
	struct sctp_sack_info sack_info;
#endif	/* SCTP_DELAYED_SACK */
#ifdef	SCTP_DELAYED_ACK_TIME
	struct sctp_assoc_value sack_val;
#endif /* defined SCTP_DELAYED_ACK_TIME */
#ifdef SCTP_MAX_BURST
	struct sctp_assoc_value av;
#endif /* SCTP_MAX_BURST */
	
#ifdef __OS_linux
	union {
		struct sctp_event_subscribe s;
		char padding[sizeof(struct sctp_event_subscribe)+sizeof(__u8)];
	} es;
#else
	struct sctp_event_subscribe es;
#endif
	struct sctp_event_subscribe* ev_s;
	
	sctp_err=0;
#ifdef __OS_linux
	ev_s=&es.s;
#else
	ev_s=&es;
#endif
	/* set tos */
	optval = tos;
	if(af==AF_INET){
		if (setsockopt(s, IPPROTO_IP, IP_TOS, (void*)&optval,
					sizeof(optval)) ==-1){
			LM_WARN("sctp_init_sock_opt_common: setsockopt tos: %s\n",
					strerror(errno));
			/* continue since this is not critical */
		}
	} else if(af==AF_INET6){
		if (setsockopt(s, IPPROTO_IPV6, IPV6_TCLASS,
					(void*)&optval, sizeof(optval)) ==-1) {
			LM_WARN("sctp_init_sock_opt_common: setsockopt v6 tos: %s\n",
					strerror(errno));
			/* continue since this is not critical */
		}
	}
	
	/* set receive buffer: SO_RCVBUF*/
	if (cfg_get(sctp, sctp_cfg, so_rcvbuf)){
		optval=cfg_get(sctp, sctp_cfg, so_rcvbuf);
		if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
					(void*)&optval, sizeof(optval)) ==-1){
			LOG(L_ERR, "ERROR: sctp_init_sock_opt_common: setsockopt:"
						" SO_RCVBUF (%d): %s\n", optval, strerror(errno));
			/* continue, non-critical */
		}
	}
	
	/* set send buffer: SO_SNDBUF */
	if (cfg_get(sctp, sctp_cfg, so_sndbuf)){
		optval=cfg_get(sctp, sctp_cfg, so_sndbuf);
		if (setsockopt(s, SOL_SOCKET, SO_SNDBUF,
					(void*)&optval, sizeof(optval)) ==-1){
			LOG(L_ERR, "ERROR: sctp_init_sock_opt_common: setsockopt:"
						" SO_SNDBUF (%d): %s\n", optval, strerror(errno));
			/* continue, non-critical */
		}
	}
	
	/* set reuseaddr */
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
						(void*)&optval, sizeof(optval))==-1){
			LOG(L_ERR, "ERROR: sctp_init_sock_opt_common: setsockopt:"
						" SO_REUSEADDR (%d): %s\n", optval, strerror(errno));
			/* continue, non-critical */
	}

	
	/* disable fragments interleave (SCTP_FRAGMENT_INTERLEAVE) --
	 * we don't want partial delivery, so fragment interleave must be off too
	 */
#ifdef SCTP_FRAGMENT_INTERLEAVE
	optval=0;
	if (setsockopt(s, IPPROTO_SCTP, SCTP_FRAGMENT_INTERLEAVE ,
					(void*)&optval, sizeof(optval)) ==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock_opt_common: setsockopt: "
					"SCTP_FRAGMENT_INTERLEAVE: %s\n", strerror(errno));
		sctp_err++;
		/* try to continue */
	}
#else
#warning no sctp lib support for SCTP_FRAGMENT_INTERLEAVE, consider upgrading
#endif /* SCTP_FRAGMENT_INTERLEAVE */
	
	/* turn off partial delivery: on linux setting SCTP_PARTIAL_DELIVERY_POINT
	 * to 0 or a very large number seems to be enough, however the portable
	 * way to do it is to set it to the socket receive buffer size
	 * (this is the maximum value allowed in the sctp api draft) */
#ifdef SCTP_PARTIAL_DELIVERY_POINT
	optlen=sizeof(optval);
	if (getsockopt(s, SOL_SOCKET, SO_RCVBUF,
					(void*)&optval, &optlen) ==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock_opt_common: getsockopt: "
						"SO_RCVBUF: %s\n", strerror(errno));
		/* try to continue */
		optval=0;
	}
#ifdef __OS_linux
	optval/=2; /* in linux getsockopt() returns twice the set value */
#endif
	pd_point=optval;
	saved_errno=0;
	while(pd_point &&
			setsockopt(s, IPPROTO_SCTP, SCTP_PARTIAL_DELIVERY_POINT,
					(void*)&pd_point, sizeof(pd_point)) ==-1){
		if (!saved_errno)
			saved_errno=errno;
		pd_point--;
	}
	
	if (pd_point!=optval){
		if (pd_point==0){
			/* all attempts failed */
			LOG(L_ERR, "ERROR: sctp_init_sock_opt_common: setsockopt: "
						"SCTP_PARTIAL_DELIVERY_POINT (%d): %s\n",
						optval, strerror(errno));
			sctp_err++;
			/* try to continue */
		}else{
			/* success but to a lower value (might not be disabled) */
			LOG(L_WARN, "setsockopt SCTP_PARTIAL_DELIVERY_POINT set to %d, but"
				" the socket rcvbuf is %d (higher values fail with"
				" \"%s\" [%d])\n",
				pd_point, optval, strerror(saved_errno), saved_errno);
		}
	}
#else
#warning no sctp lib support for SCTP_PARTIAL_DELIVERY_POINT, consider upgrading
#endif /* SCTP_PARTIAL_DELIVERY_POINT */
	
	/* nagle / no delay */
#ifdef SCTP_NODELAY
	optval=1;
	if (setsockopt(s, IPPROTO_SCTP, SCTP_NODELAY,
					(void*)&optval, sizeof(optval)) ==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock_opt_common: setsockopt: "
						"SCTP_NODELAY: %s\n", strerror(errno));
		sctp_err++;
		/* non critical, try to continue */
	}
#else
#warning no sctp lib support for SCTP_NODELAY, consider upgrading
#endif /* SCTP_NODELAY */
	
	/* enable message fragmentation (SCTP_DISABLE_FRAGMENTS)  (on send) */
#ifdef SCTP_DISABLE_FRAGMENTS
	optval=0;
	if (setsockopt(s, IPPROTO_SCTP, SCTP_DISABLE_FRAGMENTS,
					(void*)&optval, sizeof(optval)) ==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock_opt_common: setsockopt: "
						"SCTP_DISABLE_FRAGMENTS: %s\n", strerror(errno));
		sctp_err++;
		/* non critical, try to continue */
	}
#else
#warning no sctp lib support for SCTP_DISABLE_FRAGMENTS, consider upgrading
#endif /* SCTP_DISABLE_FRAGMENTS */
	
	/* set autoclose */
#ifdef SCTP_AUTOCLOSE
	optval=cfg_get(sctp, sctp_cfg, autoclose);
	if (setsockopt(s, IPPROTO_SCTP, SCTP_AUTOCLOSE,
					(void*)&optval, sizeof(optval)) ==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock_opt_common: setsockopt: "
						"SCTP_AUTOCLOSE: %s (critical)\n", strerror(errno));
		/* critical: w/o autoclose we could have sctp connection living
		   forever (if the remote side doesn't close them) */
		sctp_err++;
		goto error;
	}
#else
#error SCTP_AUTOCLOSE not supported, please upgrade your sctp library
#endif /* SCTP_AUTOCLOSE */
	/* set rtoinfo options: srto_initial, srto_min, srto_max */
#ifdef SCTP_RTOINFO
	memset(&rto, 0, sizeof(rto));
	rto.srto_initial=cfg_get(sctp, sctp_cfg, srto_initial);
	rto.srto_min=cfg_get(sctp, sctp_cfg, srto_min);
	rto.srto_max=cfg_get(sctp, sctp_cfg, srto_max);
	if (rto.srto_initial || rto.srto_min || rto.srto_max){
		/* if at least one is non-null => we have to set it */
		if (sctp_setsockopt(s, IPPROTO_SCTP, SCTP_RTOINFO, (void*)&rto,
							sizeof(rto), "setsockopt: SCTP_RTOINFO")!=0){
			sctp_err++;
			/* non critical, try to continue */
		}
	}
#else
#warning no sctp lib support for SCTP_RTOINFO, consider upgrading
#endif /* SCTP_RTOINFO */
	/* set associnfo options: assocmaxrxt */
#ifdef SCTP_ASSOCINFO
	memset(&ap, 0, sizeof(ap));
	ap.sasoc_asocmaxrxt=cfg_get(sctp, sctp_cfg, asocmaxrxt);
	if (ap.sasoc_asocmaxrxt){
		/* if at least one is non-null => we have to set it */
		if (sctp_setsockopt(s, IPPROTO_SCTP, SCTP_ASSOCINFO, (void*)&ap,
							sizeof(ap), "setsockopt: SCTP_ASSOCINFO")!=0){
			sctp_err++;
			/* non critical, try to continue */
		}
	}
#else
#warning no sctp lib support for SCTP_ASSOCINFO, consider upgrading
#endif /* SCTP_ASOCINFO */
	/* set initmsg options: init_max_attempts & init_max_init_timeo */
#ifdef SCTP_INITMSG
	memset(&im, 0, sizeof(im));
	im.sinit_max_attempts=cfg_get(sctp, sctp_cfg, init_max_attempts);
	im.sinit_max_init_timeo=cfg_get(sctp, sctp_cfg, init_max_timeo);
	if (im.sinit_max_attempts || im.sinit_max_init_timeo){
		/* if at least one is non-null => we have to set it */
		if (sctp_setsockopt(s, IPPROTO_SCTP, SCTP_INITMSG, (void*)&im,
							sizeof(im), "setsockopt: SCTP_INITMSG")!=0){
			sctp_err++;
			/* non critical, try to continue */
		}
	}
#else
#warning no sctp lib support for SCTP_INITMSG, consider upgrading
#endif /* SCTP_INITMSG */
	/* set sctp peer addr options: hbinterval & pathmaxrxt */
#ifdef SCTP_PEER_ADDR_PARAMS
	memset(&pp, 0, sizeof(pp));
	pp.spp_address.ss_family=af;
	pp.spp_hbinterval=cfg_get(sctp, sctp_cfg, hbinterval);
	pp.spp_pathmaxrxt=cfg_get(sctp, sctp_cfg, pathmaxrxt);
	if (pp.spp_hbinterval || pp.spp_pathmaxrxt){
		if (pp.spp_hbinterval > 0)
			pp.spp_flags=SPP_HB_ENABLE;
		else if (pp.spp_hbinterval==-1){
			pp.spp_flags=SPP_HB_DISABLE;
			pp.spp_hbinterval=0;
		}
#ifdef __OS_linux
		if (pp.spp_pathmaxrxt){
			/* hack to work on linux, pathmaxrxt is set only if
			   SPP_PMTUD_ENABLE */
			pp.spp_flags|=SPP_PMTUD_ENABLE;
		}
#endif /*__OS_linux */
		/* if at least one is non-null => we have to set it */
		if (sctp_setsockopt(s, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, (void*)&pp,
						sizeof(pp), "setsockopt: SCTP_PEER_ADDR_PARAMS")!=0){
			sctp_err++;
			/* non critical, try to continue */
		}
	}
#else
#warning no sctp lib support for SCTP_PEER_ADDR_PARAMS, consider upgrading
#endif /* SCTP_PEER_ADDR_PARAMS */
	/* set delayed ack options: sack_delay & sack_freq */
#if defined SCTP_DELAYED_SACK || defined SCTP_DELAYED_ACK_TIME
#ifdef SCTP_DELAYED_SACK
	memset(&sack_info, 0, sizeof(sack_info));
	sack_info.sack_delay=cfg_get(sctp, sctp_cfg, sack_delay);
	sack_info.sack_freq=cfg_get(sctp, sctp_cfg, sack_freq);
	if ((sack_info.sack_delay || sack_info.sack_freq) &&
		(sctp_setsockopt(s, IPPROTO_SCTP, SCTP_DELAYED_SACK,
							(void*)&sack_info, sizeof(sack_info), 0)!=0)) {
		/* if setting SCTP_DELAYED_SACK failed, try the old obsolete
		   SCTP_DELAYED_ACK_TIME */
#endif /* SCTP_DELAYED_SACK */
#ifdef SCTP_DELAYED_ACK_TIME
		memset(&sack_val, 0, sizeof(sack_val));
		sack_val.assoc_value=cfg_get(sctp, sctp_cfg, sack_delay);
		if (sack_val.assoc_value){
			if (sctp_setsockopt(s, IPPROTO_SCTP, SCTP_DELAYED_ACK_TIME,
									(void*)&sack_val, sizeof(sack_val),
									"setsockopt: SCTP_DELAYED_ACK_TIME")!=0){
				sctp_err++;
				/* non critical, try to continue */
			}
		}
#else /* SCTP_DELAYED_ACK_TIME */
		/* no SCTP_DELAYED_ACK_TIME support and SCTP_DELAYED_SACK failed
		   => error */
		if (sack_info.sack_delay){
			sctp_err++;
			ERR("cfg: setting SCTP_DELAYED_SACK: %s [%d]\n",
						strerror(errno), errno);
		}
#endif /* SCTP_DELAYED_ACK_TIME */
		if (cfg_get(sctp, sctp_cfg, sack_freq)){
#ifdef SCTP_DELAYED_SACK
			sctp_err++;
			WARN("could not set sctp sack_freq, please upgrade your kernel\n");
#else /* SCTP_DELAYED_SACK */
			WARN("could not set sctp sack_freq, please upgrade your sctp"
					" library\n");
#endif /* SCTP_DELAYED_SACK */
			((struct cfg_group_sctp*)sctp_cfg)->sack_freq=0;
		}
#ifdef SCTP_DELAYED_SACK
	}
#endif /* SCTP_DELAYED_SACK */
	
#else /* SCTP_DELAYED_SACK  | SCTP_DELAYED_ACK_TIME*/
#warning no sctp lib support for SCTP_DELAYED_SACK, consider upgrading
#endif /* SCTP_DELAYED_SACK  | SCTP_DELAYED_ACK_TIME*/
	/* set max burst option */
#ifdef SCTP_MAX_BURST
	memset(&av, 0, sizeof(av));
	av.assoc_value=cfg_get(sctp, sctp_cfg, max_burst);
	if (av.assoc_value){
		if (sctp_setsockopt(s, IPPROTO_SCTP, SCTP_MAX_BURST, (void*)&av,
							sizeof(av), "setsockopt: SCTP_MAX_BURST")!=0){
			sctp_err++;
			/* non critical, try to continue */
		}
	}
#else
#warning no sctp lib support for SCTP_MAX_BURST, consider upgrading
#endif /* SCTP_MAX_BURST */
	
	memset(&es, 0, sizeof(es));
	/* SCTP_EVENTS for SCTP_SNDRCV (sctp_data_io_event) -> per message
	 *  information in sctp_sndrcvinfo */
	ev_s->sctp_data_io_event=1;
	/* enable association event notifications */
	ev_s->sctp_association_event=1; /* SCTP_ASSOC_CHANGE */
	ev_s->sctp_address_event=1;  /* enable address events notifications */
	ev_s->sctp_send_failure_event=1; /* SCTP_SEND_FAILED */
	ev_s->sctp_peer_error_event=1;   /* SCTP_REMOTE_ERROR */
	ev_s->sctp_shutdown_event=1;     /* SCTP_SHUTDOWN_EVENT */
	ev_s->sctp_partial_delivery_event=1; /* SCTP_PARTIAL_DELIVERY_EVENT */
	/* ev_s->sctp_adaptation_layer_event=1; - not supported by lksctp<=1.0.6*/
	/* ev_s->sctp_authentication_event=1; -- not supported on linux 2.6.25 */
	
	/* enable the SCTP_EVENTS */
#ifdef SCTP_EVENTS
	if (setsockopt(s, IPPROTO_SCTP, SCTP_EVENTS, ev_s, sizeof(*ev_s))==-1){
		/* on linux the checks for the struct sctp_event_subscribe size
		   are too strict, making certain lksctp/kernel combination
		   unworkable => since we don't use the extra information
		   (sctp_authentication_event) added in newer version, we can
		   try with different sizes) */
#ifdef __OS_linux
		/* 1. lksctp 1.0.9 with kernel < 2.6.26 -> kernel expects 
		      the structure without the authentication event member */
		if (setsockopt(s, IPPROTO_SCTP, SCTP_EVENTS, ev_s, sizeof(*ev_s)-1)==0)
			goto ev_success;
		/* 2. lksctp < 1.0.9? with kernel >= 2.6.26: the sctp.h structure
		   does not have the authentication member, but the newer kernels 
		   check only for optlen > sizeof(...) => we should never reach
		   this point. */
		/* 3. just to be foolproof if we reached this point, try
		    with a bigger size before giving up  (out of desperation) */
		if (setsockopt(s, IPPROTO_SCTP, SCTP_EVENTS, ev_s, sizeof(es))==0)
			goto ev_success;

#endif
		LOG(L_ERR, "ERROR: sctp_init_sock_opt_common: setsockopt: "
				"SCTP_EVENTS: %s\n", strerror(errno));
		sctp_err++;
		goto error; /* critical */
	}
#ifdef __OS_linux
ev_success:
#endif
#else
#error no sctp lib support for SCTP_EVENTS, consider upgrading
#endif /* SCTP_EVENTS */
	
	if (sctp_err){
		LOG(L_ERR, "ERROR: sctp: setting some sctp sockopts failed, "
					"consider upgrading your kernel\n");
	}
	return 0;
error:
	return -1;
}



/* bind all addresses from sock (sockaddr_unions)
   returns 0 on success, .1 on error */
static int sctp_bind_sock(struct socket_info* sock_info)
{
	struct addr_info* ai;
	union sockaddr_union* addr;
	
	addr=&sock_info->su;
	/* bind the addresses*/
	if (bind(sock_info->socket,  &addr->s, sockaddru_len(*addr))==-1){
		LOG(L_ERR, "ERROR: sctp_bind_sock: bind(%x, %p, %d) on %s: %s\n",
				sock_info->socket, &addr->s, 
				(unsigned)sockaddru_len(*addr),
				sock_info->address_str.s,
				strerror(errno));
		if (addr->s.sa_family==AF_INET6)
			LOG(L_ERR, "ERROR: sctp_bind_sock: might be caused by using a "
							"link local address, try site local or global\n");
		goto error;
	}
	for (ai=sock_info->addr_info_lst; ai; ai=ai->next)
		if (sctp_bindx(sock_info->socket, &ai->su.s, 1, SCTP_BINDX_ADD_ADDR)
					==-1){
			LOG(L_ERR, "ERROR: sctp_bind_sock: sctp_bindx(%x, %.*s:%d, 1, ...)"
						" on %s:%d : [%d] %s (trying to continue)\n",
						sock_info->socket,
						ai->address_str.len, ai->address_str.s, 
						sock_info->port_no,
						sock_info->address_str.s, sock_info->port_no,
						errno, strerror(errno));
			if (ai->su.s.sa_family==AF_INET6)
				LOG(L_ERR, "ERROR: sctp_bind_sock: might be caused by using a "
							"link local address, try site local or global\n");
			/* try to continue, a secondary address bind failure is not 
			 * critical */
		}
	return 0;
error:
	return -1;
}



/* init, bind & start listening on the corresp. sctp socket
   returns 0 on success, -1 on error */
int sctp_init_sock(struct socket_info* sock_info)
{
	union sockaddr_union* addr;
	
	sock_info->proto=PROTO_SCTP;
	addr=&sock_info->su;
	if (sctp_init_su(sock_info)!=0)
		goto error;
	sock_info->socket = socket(AF2PF(addr->s.sa_family), SOCK_SEQPACKET, 
								IPPROTO_SCTP);
	if (sock_info->socket==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock: socket: %s\n", strerror(errno));
		goto error;
	}
	INFO("sctp: socket %d initialized (%p)\n", sock_info->socket, sock_info);
	/* make socket non-blocking */
#if 0
	/* recvmsg must block so use blocking sockets
	 * and send with MSG_DONTWAIT */
	optval=fcntl(sock_info->socket, F_GETFL);
	if (optval==-1){
		LOG(L_ERR, "ERROR: init_sctp: fnctl failed: (%d) %s\n",
				errno, strerror(errno));
		goto error;
	}
	if (fcntl(sock_info->socket, F_SETFL, optval|O_NONBLOCK)==-1){
		LOG(L_ERR, "ERROR: init_sctp: fcntl: set non-blocking failed:"
				" (%d) %s\n", errno, strerror(errno));
		goto error;
	}
#endif

	/* set sock opts */
	if (sctp_init_sock_opt_common(sock_info->socket, sock_info->address.af)!=0)
		goto error;
	/* SCTP_EVENTS for send dried out -> present in the draft not yet
	 * present in linux (might help to detect when we could send again to
	 * some peer, kind of poor's man poll on write, based on received
	 * SCTP_SENDER_DRY_EVENTs */
	
	if (sctp_bind_sock(sock_info)<0)
		goto error;
	if (listen(sock_info->socket, 1)<0){
		LOG(L_ERR, "ERROR: sctp_init_sock: listen(%x, 1) on %s: %s\n",
					sock_info->socket, sock_info->address_str.s,
					strerror(errno));
		goto error;
	}
	return 0;
error:
	return -1;
}


#define USE_SCTP_OO

#ifdef USE_SCTP_OO

/* init, bind & start listening on the corresp. sctp socket, using
   sctp one-to-one mode
   returns 0 on success, -1 on error */
int sctp_init_sock_oo(struct socket_info* sock_info)
{
	union sockaddr_union* addr;
	int optval;
	
	sock_info->proto=PROTO_SCTP;
	addr=&sock_info->su;
	if (sctp_init_su(sock_info)!=0)
		goto error;
	sock_info->socket = socket(AF2PF(addr->s.sa_family), SOCK_STREAM, 
								IPPROTO_SCTP);
	if (sock_info->socket==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock_oo: socket: %s\n", strerror(errno));
		goto error;
	}
	INFO("sctp:oo socket %d initialized (%p)\n", sock_info->socket, sock_info);
	/* make socket non-blocking */
	optval=fcntl(sock_info->socket, F_GETFL);
	if (optval==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock_oo: fnctl failed: (%d) %s\n",
				errno, strerror(errno));
		goto error;
	}
	if (fcntl(sock_info->socket, F_SETFL, optval|O_NONBLOCK)==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock_oo: fcntl: set non-blocking failed:"
				" (%d) %s\n", errno, strerror(errno));
		goto error;
	}
	
	/* set sock opts */
	if (sctp_init_sock_opt_common(sock_info->socket, sock_info->address.af)!=0)
		goto error;
	
#ifdef SCTP_REUSE_PORT
	/* set reuse port */
	optval=1;
	if (setsockopt(sock_info->socket, IPPROTO_SCTP, SCTP_REUSE_PORT ,
					(void*)&optval, sizeof(optval)) ==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock_oo: setsockopt: "
					"SCTP_REUSE_PORT: %s\n", strerror(errno));
		goto error;
	}
#endif /* SCTP_REUSE_PORT */
	
	if (sctp_bind_sock(sock_info)<0)
		goto error;
	if (listen(sock_info->socket, 1)<0){
		LOG(L_ERR, "ERROR: sctp_init_sock_oo: listen(%x, 1) on %s: %s\n",
					sock_info->socket, sock_info->address_str.s,
					strerror(errno));
		goto error;
	}
	return 0;
error:
	return -1;
}

#endif /* USE_SCTP_OO */


#ifdef SCTP_CONN_REUSE

/* we  need SCTP_ADDR_HASH for being able to make inquires related to existing
   sctp association to a particular address  (optional) */
/*#define SCTP_ADDR_HASH*/

#define SCTP_ID_HASH_SIZE 1024 /* must be 2^k */
#define SCTP_ASSOC_HASH_SIZE 1024 /* must be 2^k */
#define SCTP_ADDR_HASH_SIZE 1024 /* must be 2^k */

/* lock method */
#ifdef GEN_LOCK_T_UNLIMITED
#define SCTP_HASH_LOCK_PER_BUCKET
#elif defined GEN_LOCK_SET_T_UNLIMITED
#define SCTP_HASH_LOCK_SET
#else
#define SCTP_HASH_ONE_LOCK
#endif


#ifdef SCTP_HASH_LOCK_PER_BUCKET
/* lock included in the hash bucket */
#define LOCK_SCTP_ID_H(h)		lock_get(&sctp_con_id_hash[(h)].lock)
#define UNLOCK_SCTP_ID_H(h)		lock_release(&sctp_con_id_hash[(h)].lock)
#define LOCK_SCTP_ASSOC_H(h)	lock_get(&sctp_con_assoc_hash[(h)].lock)
#define UNLOCK_SCTP_ASSOC_H(h)	lock_release(&sctp_con_assoc_hash[(h)].lock)
#define LOCK_SCTP_ADDR_H(h)		lock_get(&sctp_con_addr_hash[(h)].lock)
#define UNLOCK_SCTP_ADDR_H(h)	lock_release(&sctp_con_addr_hash[(h)].lock)
#elif defined SCTP_HASH_LOCK_SET
static gen_lock_set_t* sctp_con_id_h_lock_set=0;
static gen_lock_set_t* sctp_con_assoc_h_lock_set=0;
static gen_lock_set_t* sctp_con_addr_h_lock_set=0;
#define LOCK_SCTP_ID_H(h)		lock_set_get(sctp_con_id_h_lock_set, (h))
#define UNLOCK_SCTP_ID_H(h)		lock_set_release(sctp_con_id_h_lock_set, (h))
#define LOCK_SCTP_ASSOC_H(h)	lock_set_get(sctp_con_assoc_h_lock_set, (h))
#define UNLOCK_SCTP_ASSOC_H(h)	\
	lock_set_release(sctp_con_assoc_h_lock_set, (h))
#define LOCK_SCTP_ADDR_H(h)	lock_set_get(sctp_con_addr_h_lock_set, (h))
#define UNLOCK_SCTP_ADDR_H(h)	lock_set_release(sctp_con_addr_h_lock_set, (h))
#else /* use only one lock */
static gen_lock_t* sctp_con_id_h_lock=0;
static gen_lock_t* sctp_con_assoc_h_lock=0;
static gen_lock_t* sctp_con_addr_h_lock=0;
#define LOCK_SCTP_ID_H(h)		lock_get(sctp_con_id_h_lock)
#define UNLOCK_SCTP_ID_H(h)		lock_release(sctp_con_id_hlock)
#define LOCK_SCTP_ASSOC_H(h)	lock_get(sctp_con_assoc_h_lock)
#define UNLOCK_SCTP_ASSOC_H(h)	lock_release(sctp_con_assoc_h_lock)
#define LOCK_SCTP_ADDR_H(h)	lock_get(sctp_con_addr_h_lock)
#define UNLOCK_SCTP_ADDR_H(h)	lock_release(sctp_con_addr_h_lock)
#endif /* SCTP_HASH_LOCK_PER_BUCKET */


/* sctp connection flags */
#define SCTP_CON_UP_SEEN   1
#define SCTP_CON_RCV_SEEN  2
#define SCTP_CON_DOWN_SEEN 4

struct sctp_connection{
	unsigned int id;       /**< ser unique global id */
	unsigned int assoc_id; /**< sctp assoc id (can be reused for new assocs)*/
	struct socket_info* si; /**< local socket used */
	unsigned flags; /**< internal flags UP_SEEN, RCV_SEEN, DOWN_SEEN */
	ticks_t start;
	ticks_t expire; 
	union sockaddr_union remote; /**< remote ip & port */
};

struct sctp_lst_connector{
	/* id hash */
	struct sctp_con_elem* next_id;
	struct sctp_con_elem* prev_id;
	/* assoc hash */
	struct sctp_con_elem* next_assoc;
	struct sctp_con_elem* prev_assoc;
#ifdef SCTP_ADDR_HASH
	/* addr hash */
	struct sctp_con_elem* next_addr;
	struct sctp_con_elem* prev_addr;
#endif /* SCTP_ADDR_HASH */
};

struct sctp_con_elem{
	struct sctp_lst_connector l; /* must be first */
	atomic_t refcnt;
	/* data */
	struct sctp_connection con;
};

struct sctp_con_id_hash_head{
	struct sctp_lst_connector l; /* must be first */
#ifdef SCTP_HASH_LOCK_PER_BUCKET
	gen_lock_t lock;
#endif /* SCTP_HASH_LOCK_PER_BUCKET */
};

struct sctp_con_assoc_hash_head{
	struct sctp_lst_connector l; /* must be first */
#ifdef SCTP_HASH_LOCK_PER_BUCKET
	gen_lock_t lock;
#endif /* SCTP_HASH_LOCK_PER_BUCKET */
};

#ifdef SCTP_ADDR_HASH
struct sctp_con_addr_hash_head{
	struct sctp_lst_connector l; /* must be first */
#ifdef SCTP_HASH_LOCK_PER_BUCKET
	gen_lock_t lock;
#endif /* SCTP_HASH_LOCK_PER_BUCKET */
};
#endif /* SCTP_ADDR_HASH */

static struct sctp_con_id_hash_head*     sctp_con_id_hash;
static struct sctp_con_assoc_hash_head*  sctp_con_assoc_hash;
#ifdef SCTP_ADDR_HASH
static struct sctp_con_addr_hash_head*  sctp_con_addr_hash;
#endif /* SCTP_ADDR_HASH */

static atomic_t* sctp_id;
static atomic_t* sctp_conn_tracked;


#define get_sctp_con_id_hash(id) ((id) % SCTP_ID_HASH_SIZE)
#define get_sctp_con_assoc_hash(assoc_id)  ((assoc_id) % SCTP_ASSOC_HASH_SIZE)
#ifdef SCTP_ADDR_HASH
static inline unsigned get_sctp_con_addr_hash(union sockaddr_union* remote,
											struct socket_info* si)
{
	struct ip_addr ip;
	unsigned short port;
	unsigned h;
	
	su2ip_addr(&ip, remote);
	port=su_getport(remote);
	if (likely(ip.len==4))
		h=ip.u.addr32[0]^port;
	else if (ip.len==16)
		h=ip.u.addr32[0]^ip.u.addr32[1]^ip.u.addr32[2]^ ip.u.addr32[3]^port;
	else
		h=0; /* error */
	/* make sure the first bits are influenced by all 32
	 * (the first log2(SCTP_ADDR_HASH_SIZE) bits should be a mix of all
	 *  32)*/
	h ^= h>>17;
	h ^= h>>7;
	return h & (SCTP_ADDR_HASH_SIZE-1);
}
#endif /* SCTP_ADDR_HASH */



/** destroy sctp conn hashes. */
void destroy_sctp_con_tracking()
{
	int r;
	
#ifdef SCTP_HASH_LOCK_PER_BUCKET
	if (sctp_con_id_hash)
		for(r=0; r<SCTP_ID_HASH_SIZE; r++)
			lock_destroy(&sctp_con_id_hash[r].lock);
	if (sctp_con_assoc_hash)
		for(r=0; r<SCTP_ASSOC_HASH_SIZE; r++)
			lock_destroy(&sctp_con_assoc_hash[r].lock);
#	ifdef SCTP_ADDR_HASH
	if (sctp_con_addr_hash)
		for(r=0; r<SCTP_ADDR_HASH_SIZE; r++)
			lock_destroy(&sctp_con_addr_hash[r].lock);
#	endif /* SCTP_ADDR_HASH */
#elif defined SCTP_HASH_LOCK_SET
	if (sctp_con_id_h_lock_set){
		lock_set_destroy(sctp_con_id_h_lock_set);
		lock_set_dealloc(sctp_con_id_h_lock_set);
		sctp_con_id_h_lock_set=0;
	}
	if (sctp_con_assoc_h_lock_set){
		lock_set_destroy(sctp_con_assoc_h_lock_set);
		lock_set_dealloc(sctp_con_assoc_h_lock_set);
		sctp_con_assoc_h_lock_set=0;
	}
#	ifdef SCTP_ADDR_HASH
	if (sctp_con_addr_h_lock_set){
		lock_set_destroy(sctp_con_addr_h_lock_set);
		lock_set_dealloc(sctp_con_addr_h_lock_set);
		sctp_con_addr_h_lock_set=0;
	}
#	endif /* SCTP_ADDR_HASH */
#else /* SCTP_HASH_ONE_LOCK */
	if (sctp_con_id_h_lock){
		lock_destroy(sctp_con_id_h_lock);
		lock_dealloc(sctp_con_id_h_lock);
		sctp_con_id_h_lock=0;
	}
	if (sctp_con_assoc_h_lock){
		lock_destroy(sctp_con_assoc_h_lock);
		lock_dealloc(sctp_con_assoc_h_lock);
		sctp_con_assoc_h_lock=0;
	}
#	ifdef SCTP_ADDR_HASH
	if (sctp_con_addr_h_lock){
		lock_destroy(sctp_con_addr_h_lock);
		lock_dealloc(sctp_con_addr_h_lock);
		sctp_con_addr_h_lock=0;
	}
#	endif /* SCTP_ADDR_HASH */
#endif /* SCTP_HASH_LOCK_PER_BUCKET/SCTP_HASH_LOCK_SET/one lock */
	if (sctp_con_id_hash){
		shm_free(sctp_con_id_hash);
		sctp_con_id_hash=0;
	}
	if (sctp_con_assoc_hash){
		shm_free(sctp_con_assoc_hash);
		sctp_con_assoc_hash=0;
	}
#ifdef SCTP_ADDR_HASH
	if (sctp_con_addr_hash){
		shm_free(sctp_con_addr_hash);
		sctp_con_addr_hash=0;
	}
#endif /* SCTP_ADDR_HASH */
	if (sctp_id){
		shm_free(sctp_id);
		sctp_id=0;
	}
	if (sctp_conn_tracked){
		shm_free(sctp_conn_tracked);
		sctp_conn_tracked=0;
	}
}



/** intializaze sctp_conn hashes.
  * @return 0 on success, <0 on error
  */
int init_sctp_con_tracking()
{
	int r, ret;
	
	sctp_con_id_hash=shm_malloc(SCTP_ID_HASH_SIZE*sizeof(*sctp_con_id_hash));
	sctp_con_assoc_hash=shm_malloc(SCTP_ASSOC_HASH_SIZE*
									sizeof(*sctp_con_assoc_hash));
#ifdef SCTP_ADDR_HASH
	sctp_con_addr_hash=shm_malloc(SCTP_ADDR_HASH_SIZE*
									sizeof(*sctp_con_addr_hash));
#endif /* SCTP_ADDR_HASH */
	sctp_id=shm_malloc(sizeof(*sctp_id));
	sctp_conn_tracked=shm_malloc(sizeof(*sctp_conn_tracked));
	if (sctp_con_id_hash==0 || sctp_con_assoc_hash==0 ||
#ifdef SCTP_ADDR_HASH
			sctp_con_addr_hash==0 ||
#endif /* SCTP_ADDR_HASH */
			sctp_id==0 || sctp_conn_tracked==0){
		ERR("sctp init: memory allocation error\n");
		ret=E_OUT_OF_MEM;
		goto error;
	}
	atomic_set(sctp_id, 0);
	atomic_set(sctp_conn_tracked, 0);
	for (r=0; r<SCTP_ID_HASH_SIZE; r++)
		clist_init(&sctp_con_id_hash[r], l.next_id, l.prev_id);
	for (r=0; r<SCTP_ASSOC_HASH_SIZE; r++)
		clist_init(&sctp_con_assoc_hash[r], l.next_assoc, l.prev_assoc);
#ifdef SCTP_ADDR_HASH
	for (r=0; r<SCTP_ADDR_HASH_SIZE; r++)
		clist_init(&sctp_con_addr_hash[r], l.next_addr, l.prev_addr);
#endif /* SCTP_ADDR_HASH */
#ifdef SCTP_HASH_LOCK_PER_BUCKET
	for (r=0; r<SCTP_ID_HASH_SIZE; r++){
		if (lock_init(&sctp_con_id_hash[r].lock)==0){
			ret=-1;
			ERR("sctp init: failed to initialize locks\n");
			goto error;
		}
	}
	for (r=0; r<SCTP_ASSOC_HASH_SIZE; r++){
		if (lock_init(&sctp_con_assoc_hash[r].lock)==0){
			ret=-1;
			ERR("sctp init: failed to initialize locks\n");
			goto error;
		}
	}
#	ifdef SCTP_ADDR_HASH
	for (r=0; r<SCTP_ADDR_HASH_SIZE; r++){
		if (lock_init(&sctp_con_addr_hash[r].lock)==0){
			ret=-1;
			ERR("sctp init: failed to initialize locks\n");
			goto error;
		}
	}
#	endif /* SCTP_ADDR_HASH */
#elif defined SCTP_HASH_LOCK_SET
	sctp_con_id_h_lock_set=lock_set_alloc(SCTP_ID_HASH_SIZE);
	sctp_con_assoc_h_lock_set=lock_set_alloc(SCTP_ASSOC_HASH_SIZE);
#	ifdef SCTP_ADDR_HASH
	sctp_con_addr_h_lock_set=lock_set_alloc(SCTP_ADDR_HASH_SIZE);
#	endif /* SCTP_ADDR_HASH */
	if (sctp_con_id_h_lock_set==0 || sctp_con_assoc_h_lock_set==0
#	ifdef SCTP_ADDR_HASH
			|| sctp_con_addr_h_lock_set==0
#	endif /* SCTP_ADDR_HASH */
			){
		ret=E_OUT_OF_MEM;
		ERR("sctp_init: failed to alloc lock sets\n");
		goto error;
	}
	if (lock_set_init(sctp_con_id_h_lock_set)==0){
		lock_set_dealloc(sctp_con_id_h_lock_set);
		sctp_con_id_h_lock_set=0;
		ret=-1;
		ERR("sctp init: failed to initialize lock set\n");
		goto error;
	}
	if (lock_set_init(sctp_con_assoc_h_lock_set)==0){
		lock_set_dealloc(sctp_con_assoc_h_lock_set);
		sctp_con_assoc_h_lock_set=0;
		ret=-1;
		ERR("sctp init: failed to initialize lock set\n");
		goto error;
	}
#	ifdef SCTP_ADDR_HASH
	if (lock_set_init(sctp_con_addr_h_lock_set)==0){
		lock_set_dealloc(sctp_con_addr_h_lock_set);
		sctp_con_addr_h_lock_set=0;
		ret=-1;
		ERR("sctp init: failed to initialize lock set\n");
		goto error;
	}
#	endif /* SCTP_ADDR_HASH */
#else /* SCTP_HASH_ONE_LOCK */
	sctp_con_id_h_lock=lock_alloc();
	sctp_con_assoc_h_lock=lock_alloc();
#	ifdef SCTP_ADDR_HASH
	sctp_con_addr_h_lock=lock_alloc();
#	endif /* SCTP_ADDR_HASH */
	if (sctp_con_id_h_lock==0 || sctp_con_assoc_h_lock==0
#	ifdef SCTP_ADDR_HASH
			|| sctp_con_addr_h_lock==0
#	endif /* SCTP_ADDR_HASH */
			){
		ret=E_OUT_OF_MEM;
		ERR("sctp init: failed to alloc locks\n");
		goto error;
	}
	if (lock_init(sctp_con_id_h_lock)==0){
		lock_dealloc(sctp_con_id_h_lock);
		sctp_con_id_h_lock=0;
		ret=-1;
		ERR("sctp init: failed to initialize lock\n");
		goto error;
	}
	if (lock_init(sctp_con_assoc_h_lock)==0){
		lock_dealloc(sctp_con_assoc_h_lock);
		sctp_con_assoc_h_lock=0;
		ret=-1;
		ERR("sctp init: failed to initialize lock\n");
		goto error;
	}
#	ifdef SCTP_ADDR_HASH
	if (lock_init(sctp_con_addr_h_lock)==0){
		lock_dealloc(sctp_con_addr_h_lock);
		sctp_con_addr_h_lock=0;
		ret=-1;
		ERR("sctp init: failed to initialize lock\n");
		goto error;
	}
#	endif /* SCTP_ADDR_HASH */
#endif /* SCTP_HASH_LOCK_PER_BUCKET/SCTP_HASH_LOCK_SET/one lock */
	return 0;
error:
	destroy_sctp_con_tracking();
	return ret;
}



#if 0
/** adds "e" to the hashes, safe locking version.*/
static void sctp_con_add(struct sctp_con_elem* e)
{
	unsigned hash;
	DBG("sctp_con_add(%p) ( ser id %d, assoc_id %d)\n",
			e, e->con.id, e->con.assoc_id);
	
	e->l.next_id=e->l.prev_id=0;
	e->l.next_assoc=e->l.prev_assoc=0;
#ifdef SCTP_ADDR_HASH
	e->l.next_addr=e->l.prev_addr=0;
	e->refcnt.val+=3; /* account for the 3 lists */
#else /* SCTP_ADDR_HASH */
	e->refcnt.val+=2; /* account for the 2 lists */
#endif /* SCTP_ADDR_HASH */
	hash=get_sctp_con_id_hash(e->con.id);
	DBG("adding to con id hash %d\n", hash);
	LOCK_SCTP_ID_H(hash);
		clist_insert(&sctp_con_id_hash[hash], e, l.next_id, l.prev_id);
	UNLOCK_SCTP_ID_H(hash);
	hash=get_sctp_con_assoc_hash(e->con.assoc_id);
	DBG("adding to assoc_id hash %d\n", hash);
	LOCK_SCTP_ASSOC_H(hash);
		clist_insert(&sctp_con_assoc_hash[hash], e,
						l.next_assoc, l.prev_assoc);
	UNLOCK_SCTP_ASSOC_H(hash);
#ifdef SCTP_ADDR_HASH
	hash=get_sctp_con_addr_hash(&e->con.remote, e->con.si);
	DBG("adding to addr hash %d\n", hash);
	LOCK_SCTP_ADDR_H(hash);
		clist_insert(&sctp_con_addr_hash[hash], e,
						l.next_addr, l.prev_addr);
	UNLOCK_SCTP_ADDR_H(hash);
#endif /* SCTP_ADDR_HASH */
	atomic_inc(sctp_conn_tracked);
}
#endif



/** helper internal del elem function, the id hash must be locked.
  * WARNING: the id hash(h) _must_ be locked (LOCK_SCTP_ID_H(h)).
  * @param h - id hash
  * @param e - sctp_con_elem to delete (from all the hashes)
  * @return 0 if the id hash was unlocked, 1 if it's still locked */
inline static int _sctp_con_del_id_locked(unsigned h, struct sctp_con_elem* e)
{
	unsigned assoc_id_h;
	int deref; /* delayed de-reference counter */
	int locked;
#ifdef SCTP_ADDR_HASH
	unsigned addr_h;
#endif /* SCTP_ADDR_HASH */
	
	locked=1;
	clist_rm(e, l.next_id, l.prev_id);
	e->l.next_id=e->l.prev_id=0; /* mark it as id unhashed */
	/* delay atomic dereference, so that we'll perform only one
	   atomic op. even for multiple derefs. It also has the
	   nice side-effect that the entry will be guaranteed to be
	   referenced until we perform the delayed deref. at the end,
	   so we don't need to keep some lock to prevent somebody from
	   deleting the entry from under us */
	deref=1; /* removed from one list =>  deref once */
	/* remove it from the assoc hash if needed */
	if (likely(e->l.next_assoc)){
		UNLOCK_SCTP_ID_H(h);
		locked=0; /* no longer id-locked */
		/* we haven't dec. refcnt, so it's still safe to use e */
		assoc_id_h=get_sctp_con_assoc_hash(e->con.assoc_id);
		LOCK_SCTP_ASSOC_H(assoc_id_h);
			/* make sure nobody removed it in the meantime */
			if (likely(e->l.next_assoc)){
				clist_rm(e, l.next_assoc, l.prev_assoc);
				e->l.next_assoc=e->l.prev_assoc=0; /* mark it as removed */
				deref++; /* rm'ed from the assoc list => inc. delayed deref. */
			}
		UNLOCK_SCTP_ASSOC_H(assoc_id_h);
	}
#ifdef SCTP_ADDR_HASH
	/* remove it from the addr. hash if needed */
	if (likely(e->l.next_addr)){
		if (unlikely(locked)){
			UNLOCK_SCTP_ID_H(h);
			locked=0; /* no longer id-locked */
		}
		addr_h=get_sctp_con_addr_hash(&e->con.remote, e->con.si);
		LOCK_SCTP_ADDR_H(addr_h);
			/* make sure nobody removed it in the meantime */
			if (likely(e->l.next_addr)){
				clist_rm(e, l.next_addr, l.prev_addr);
				e->l.next_addr=e->l.prev_addr=0; /* mark it as removed */
				deref++; /* rm'ed from the addr list => inc. delayed deref. */
			}
		UNLOCK_SCTP_ADDR_H(addr_h);
	}
#endif /* SCTP_ADDR_HASH */
	
	/* performed delayed de-reference */
	if (atomic_add(&e->refcnt, -deref)==0){
		atomic_dec(sctp_conn_tracked);
		shm_free(e);
	}
	else
		DBG("del assoc post-deref (kept): ser id %d, assoc_id %d,"
			" post-refcnt %d, deref %d, post-tracked %d\n",
			e->con.id, e->con.assoc_id, atomic_get(&e->refcnt), deref,
			atomic_get(sctp_conn_tracked));
	return locked;
}



/** helper internal del elem function, the assoc hash must be locked.
  * WARNING: the assoc hash(h) _must_ be locked (LOCK_SCTP_ASSOC_H(h)).
  * @param h - assoc hash
  * @param e - sctp_con_elem to delete (from all the hashes)
  * @return 0 if the assoc hash was unlocked, 1 if it's still locked */
inline static int _sctp_con_del_assoc_locked(unsigned h,
												struct sctp_con_elem* e)
{
	unsigned id_hash;
	int deref; /* delayed de-reference counter */
	int locked;
#ifdef SCTP_ADDR_HASH
	unsigned addr_h;
#endif /* SCTP_ADDR_HASH */
	
	locked=1;
	clist_rm(e, l.next_assoc, l.prev_assoc);
	e->l.next_assoc=e->l.prev_assoc=0; /* mark it as assoc unhashed */
	/* delay atomic dereference, so that we'll perform only one
	   atomic op. even for multiple derefs. It also has the
	   nice side-effect that the entry will be guaranteed to be
	   referenced until we perform the delayed deref. at the end,
	   so we don't need to keep some lock to prevent somebody from
	   deleting the entry from under us */
	deref=1; /* removed from one list =>  deref once */
	/* remove it from the id hash if needed */
	if (likely(e->l.next_id)){
		UNLOCK_SCTP_ASSOC_H(h);
		locked=0; /* no longer assoc-hash-locked */
		/* we have a ref. to it so it's still safe to use e */
		id_hash=get_sctp_con_id_hash(e->con.id);
		LOCK_SCTP_ID_H(id_hash);
			/* make sure nobody removed it in the meantime */
			if (likely(e->l.next_id)){
				clist_rm(e, l.next_id, l.prev_id);
				e->l.next_id=e->l.prev_id=0; /* mark it as removed */
				deref++; /* rm'ed from the id list => inc. delayed deref. */
			}
		UNLOCK_SCTP_ID_H(id_hash);
	}
#ifdef SCTP_ADDR_HASH
	/* remove it from the addr. hash if needed */
	if (likely(e->l.next_addr)){
		if (unlikely(locked)){
			UNLOCK_SCTP_ASSOC_H(h);
			locked=0; /* no longer id-locked */
		}
		addr_h=get_sctp_con_addr_hash(&e->con.remote, e->con.si);
		LOCK_SCTP_ADDR_H(addr_h);
			/* make sure nobody removed it in the meantime */
			if (likely(e->l.next_addr)){
				clist_rm(e, l.next_addr, l.prev_addr);
				e->l.next_addr=e->l.prev_addr=0; /* mark it as removed */
				deref++; /* rm'ed from the addr list => inc. delayed deref. */
			}
		UNLOCK_SCTP_ADDR_H(addr_h);
	}
#endif /* SCTP_ADDR_HASH */
	if (atomic_add(&e->refcnt, -deref)==0){
		atomic_dec(sctp_conn_tracked);
		shm_free(e);
	}
	else
		DBG("del assoc post-deref (kept): ser id %d, assoc_id %d,"
				" post-refcnt %d, deref %d, post-tracked %d\n",
				e->con.id, e->con.assoc_id, atomic_get(&e->refcnt), deref,
				atomic_get(sctp_conn_tracked));
	return locked;
}



#ifdef SCTP_ADDR_HASH
/** helper internal del elem function, the addr hash must be locked.
  * WARNING: the addr hash(h) _must_ be locked (LOCK_SCTP_ADDR_H(h)).
  * @param h - addr hash
  * @param e - sctp_con_elem to delete (from all the hashes)
  * @return 0 if the addr hash was unlocked, 1 if it's still locked */
inline static int _sctp_con_del_addr_locked(unsigned h,
												struct sctp_con_elem* e)
{
	unsigned id_hash;
	unsigned assoc_id_h;
	int deref; /* delayed de-reference counter */
	int locked;
	
	locked=1;
	clist_rm(e, l.next_addr, l.prev_addr);
	e->l.next_addr=e->l.prev_addr=0; /* mark it as addr unhashed */
	/* delay atomic dereference, so that we'll perform only one
	   atomic op. even for multiple derefs. It also has the
	   nice side-effect that the entry will be guaranteed to be
	   referenced until we perform the delayed deref. at the end,
	   so we don't need to keep some lock to prevent somebody from
	   deleting the entry from under us */
	deref=1; /* removed from one list =>  deref once */
	/* remove it from the id hash if needed */
	if (likely(e->l.next_id)){
		UNLOCK_SCTP_ADDR_H(h);
		locked=0; /* no longer addr-hash-locked */
		/* we have a ref. to it so it's still safe to use e */
		id_hash=get_sctp_con_id_hash(e->con.id);
		LOCK_SCTP_ID_H(id_hash);
			/* make sure nobody removed it in the meantime */
			if (likely(e->l.next_id)){
				clist_rm(e, l.next_id, l.prev_id);
				e->l.next_id=e->l.prev_id=0; /* mark it as removed */
				deref++; /* rm'ed from the id list => inc. delayed deref. */
			}
		UNLOCK_SCTP_ID_H(id_hash);
	}
	/* remove it from the assoc hash if needed */
	if (likely(e->l.next_assoc)){
		if (locked){
			UNLOCK_SCTP_ADDR_H(h);
			locked=0; /* no longer addr-hash-locked */
		}
		/* we haven't dec. refcnt, so it's still safe to use e */
		assoc_id_h=get_sctp_con_assoc_hash(e->con.assoc_id);
		LOCK_SCTP_ASSOC_H(assoc_id_h);
			/* make sure nobody removed it in the meantime */
			if (likely(e->l.next_assoc)){
				clist_rm(e, l.next_assoc, l.prev_assoc);
				e->l.next_assoc=e->l.prev_assoc=0; /* mark it as removed */
				deref++; /* rm'ed from the assoc list => inc. delayed deref. */
			}
		UNLOCK_SCTP_ASSOC_H(assoc_id_h);
	}
	if (atomic_add(&e->refcnt, -deref)==0){
		atomic_dec(sctp_conn_tracked);
		shm_free(e);
	}
	else
		DBG("del assoc post-deref (kept): ser id %d, assoc_id %d,"
				" post-refcnt %d, deref %d, post-tracked %d\n",
				e->con.id, e->con.assoc_id, atomic_get(&e->refcnt), deref,
				atomic_get(sctp_conn_tracked));
	return locked;
}
#endif /* SCTP_ADDR_HASH */



/** delete all tracked associations entries.
 */
void sctp_con_tracking_flush()
{
	unsigned h;
	struct sctp_con_elem* e;
	struct sctp_con_elem* tmp;
	
	for (h=0; h<SCTP_ID_HASH_SIZE; h++){
again:
		LOCK_SCTP_ID_H(h);
			clist_foreach_safe(&sctp_con_id_hash[h], e, tmp, l.next_id) {
				if (_sctp_con_del_id_locked(h, e)==0){
					/* unlocked, need to lock again and restart the list */
					goto again;
				}
			}
		UNLOCK_SCTP_ID_H(h);
	}
}



/** using id, get the corresponding sctp assoc & socket. 
 *  @param id - ser unique assoc id
 *  @param si  - result parameter, filled with the socket info on success
 *  @param remote - result parameter, filled with the address and port
 *  @param del - if 1 delete the entry,
 *  @return assoc_id (!=0) on success & sets si, 0 on not found
 * si and remote will not be touched on failure.
 *
 */
int sctp_con_get_assoc(unsigned int id, struct socket_info** si, 
								union sockaddr_union *remote, int del)
{
	unsigned h;
	ticks_t now; 
	struct sctp_con_elem* e;
	struct sctp_con_elem* tmp;
	int ret;
	
	ret=0;
	now=get_ticks_raw();
	h=get_sctp_con_id_hash(id);
#if 0
again:
#endif
	LOCK_SCTP_ID_H(h);
		clist_foreach_safe(&sctp_con_id_hash[h], e, tmp, l.next_id){
			if(e->con.id==id){
				ret=e->con.assoc_id;
				*si=e->con.si;
				*remote=e->con.remote;
				if (del){
					if (_sctp_con_del_id_locked(h, e)==0)
						goto skip_unlock;
				}else
					e->con.expire=now +
								S_TO_TICKS(cfg_get(sctp, sctp_cfg, autoclose));
				break;
			}
#if 0
			else if (TICKS_LT(e->con.expire, now)){
				WARN("sctp con: found expired assoc %d, id %d (%d s ago)\n",
						e->con.assoc_id, e->con.id,
						TICKS_TO_S(now-e->con.expire));
				if (_sctp_con_del_id_locked(h, e)==0)
					goto again; /* if unlocked need to restart the list */
			}
#endif
		}
	UNLOCK_SCTP_ID_H(h);
skip_unlock:
	return ret;
}



/** using the assoc_id, remote addr. & socket, get the corresp. internal id.
 *  @param assoc_id - sctp assoc id
 *  @param si  - socket on which the packet was received
 *  @param del - if 1 delete the entry,
 *  @return assoc_id (!=0) on success, 0 on not found
 */
int sctp_con_get_id(unsigned int assoc_id, union sockaddr_union* remote,
					struct socket_info* si, int del)
{
	unsigned h;
	ticks_t now; 
	struct sctp_con_elem* e;
	struct sctp_con_elem* tmp;
	int ret;
	
	ret=0;
	now=get_ticks_raw();
	h=get_sctp_con_assoc_hash(assoc_id);
#if 0
again:
#endif
	LOCK_SCTP_ASSOC_H(h);
		clist_foreach_safe(&sctp_con_assoc_hash[h], e, tmp, l.next_assoc){
			if(e->con.assoc_id==assoc_id && e->con.si==si &&
					su_cmp(remote, &e->con.remote)){
				ret=e->con.id;
				if (del){
					if (_sctp_con_del_assoc_locked(h, e)==0)
						goto skip_unlock;
				}else
					e->con.expire=now +
								S_TO_TICKS(cfg_get(sctp, sctp_cfg, autoclose));
				break;
			}
#if 0
			else if (TICKS_LT(e->con.expire, now)){
				WARN("sctp con: found expired assoc %d, id %d (%d s ago)\n",
						e->con.assoc_id, e->con.id,
						TICKS_TO_S(now-e->con.expire));
				if (_sctp_con_del_assoc_locked(h, e)==0)
					goto again; /* if unlocked need to restart the list */
			}
#endif
		}
	UNLOCK_SCTP_ASSOC_H(h);
skip_unlock:
	return ret;
}



#ifdef SCTP_ADDR_HASH
/** using the dest. & source socket, get the corresponding id and assoc_id 
 *  @param remote   - peer address & port
 *  @param si       - local source socket
 *  @param assoc_id - result, filled with the sctp assoc_id
 *  @param del - if 1 delete the entry,
 *  @return ser id (!=0) on success, 0 on not found
 */
int sctp_con_addr_get_id_assoc(union sockaddr_union* remote,
								struct socket_info* si,
								int* assoc_id, int del)
{
	unsigned h;
	ticks_t now; 
	struct sctp_con_elem* e;
	struct sctp_con_elem* tmp;
	int ret;
	
	ret=0;
	*assoc_id=0;
	now=get_ticks_raw();
	h=get_sctp_con_addr_hash(remote, si);
again:
	LOCK_SCTP_ADDR_H(h);
		clist_foreach_safe(&sctp_con_addr_hash[h], e, tmp, l.next_addr){
			if(su_cmp(remote, &e->con.remote) && e->con.si==si){
				ret=e->con.id;
				*assoc_id=e->con.assoc_id;
				if (del){
					if (_sctp_con_del_addr_locked(h, e)==0)
						goto skip_unlock;
				}else
					e->con.expire=now +
								S_TO_TICKS(cfg_get(sctp, sctp_cfg, autoclose));
				break;
			}
#if 0
			else if (TICKS_LT(e->con.expire, now)){
				WARN("sctp con: found expired assoc %d, id %d (%d s ago)\n",
						e->con.assoc_id, e->con.id,
						TICKS_TO_S(now-e->con.expire));
				if (_sctp_con_del_addr_locked(h, e)==0)
					goto again; /* if unlocked need to restart the list */
			}
#endif
		}
	UNLOCK_SCTP_ADDR_H(h);
skip_unlock:
	return ret;
}
#endif /* SCTP_ADDR_HASH */



/** del con tracking for (assod_id, si).
 * @return 0 on success, -1 on error (not found)
 */
#define sctp_con_del_assoc(assoc_id, si) \
	(-(sctp_con_get_id((assoc_id), (si), 1)==0))



/** create a new sctp con elem.
  * @param id - ser connection id
  * @param assoc_id - sctp assoc id
  * @param si - corresp. socket
  * @param remote - remote side
  * @return pointer to shm allocated sctp_con_elem on success, 0 on error
  */
struct sctp_con_elem* sctp_con_new(unsigned id, unsigned assoc_id, 
									struct socket_info* si,
									union sockaddr_union* remote)
{
	struct sctp_con_elem* e;
	
	e=shm_malloc(sizeof(*e));
	if (unlikely(e==0))
		goto error;
	e->l.next_id=e->l.prev_id=0;
	e->l.next_assoc=e->l.prev_assoc=0;
	atomic_set(&e->refcnt, 0);
	e->con.id=id;
	e->con.assoc_id=assoc_id;
	e->con.si=si;
	e->con.flags=0;
	if (likely(remote))
		e->con.remote=*remote;
	else
		memset(&e->con.remote, 0, sizeof(e->con.remote));
	e->con.start=get_ticks_raw();
	e->con.expire=e->con.start +
				S_TO_TICKS(cfg_get(sctp, sctp_cfg, autoclose));
	return e;
error:
	return 0;
}



/** handles every ev on sctp assoc_id.
  * @return ser id on success (!=0) or 0 on not found/error
  */
static int sctp_con_track(int assoc_id, struct socket_info* si,
							union sockaddr_union* remote, int ev)
{
	int id;
	unsigned hash;
	unsigned assoc_hash;
	struct sctp_con_elem* e;
	struct sctp_con_elem* tmp;
	
	id=0;
	DBG("sctp_con_track(%d, %p, %d) \n", assoc_id, si, ev);
	
	/* search for (assoc_id, si) */
	assoc_hash=get_sctp_con_assoc_hash(assoc_id);
	LOCK_SCTP_ASSOC_H(assoc_hash);
		clist_foreach_safe(&sctp_con_assoc_hash[assoc_hash], e, tmp,
								l.next_assoc){
			/* we need to use the remote side address, because at least
			   on linux assoc_id are immediately reused (even if sctp
			   autoclose is off) and so it's possible that the association
			   id we saved is already closed and assigned to another
			   association by the time we search for it) */
			if(e->con.assoc_id==assoc_id && e->con.si==si &&
					su_cmp(remote, &e->con.remote)){
				if (ev==SCTP_CON_DOWN_SEEN){
					if (e->con.flags & SCTP_CON_UP_SEEN){
						/* DOWN after UP => delete */
						id=e->con.id;
						/* do delete */
						if (_sctp_con_del_assoc_locked(assoc_hash, e)==0)
							goto found; /* skip unlock */
					}else{
						/* DOWN after DOWN => error
						   DOWN after RCV w/ no UP -> not possible
						    since we never create a tracking entry on RCV
							only */
						BUG("unexpected flags: %x for assoc_id %d, id %d"
								", sctp con %p\n", e->con.flags, assoc_id,
								e->con.id, e);
						/* do delete */
						if (_sctp_con_del_assoc_locked(assoc_hash, e)==0)
							goto found; /* skip unlock */
					}
				}else if (ev==SCTP_CON_RCV_SEEN){
					/* RCV after UP or DOWN => just mark RCV as seen */
					id=e->con.id;
					e->con.flags |= SCTP_CON_RCV_SEEN;
				}else{
					/* SCTP_CON_UP */
					if (e->con.flags & SCTP_CON_DOWN_SEEN){
						/* UP after DOWN => delete */
						id=e->con.id;
						/* do delete */
						if (_sctp_con_del_assoc_locked(assoc_hash, e)==0)
							goto found; /* skip unlock */
					}else{
						/* UP after UP or after RCVD => BUG */
						BUG("connection with same assoc_id (%d) already"
								" present, flags %x\n",
								assoc_id, e->con.flags);
					}
				}
				UNLOCK_SCTP_ASSOC_H(assoc_hash);
				goto found;
			}
		}
		/* not found */
		if (unlikely(ev!=SCTP_CON_RCV_SEEN)){
			/* UP or DOWN and no tracking entry => create new tracking entry
			   for both of them (because we can have a re-ordered DOWN before
			   the UP) */
again:
				id=atomic_add(sctp_id, 1);
				if (unlikely(id==0)){
					/* overflow  and 0 is not a valid id */
					goto again;
				}
				e=sctp_con_new(id, assoc_id, si, remote);
				if (likely(e)){
					e->con.flags=ev;
					e->l.next_id=e->l.prev_id=0;
					e->l.next_assoc=e->l.prev_assoc=0;
#ifdef SCTP_ADDR_HASH
					e->l.next_addr=e->l.prev_addr=0;
					e->refcnt.val+=3; /* account for the 3 lists */
#else /* SCTP_ADDR_HASH */
					e->refcnt.val+=2; /* account for the 2 lists */
#endif /* SCTP_ADDR_HASH */
					/* already locked */
					clist_insert(&sctp_con_assoc_hash[assoc_hash], e,
									l.next_assoc, l.prev_assoc);
					hash=get_sctp_con_id_hash(e->con.id);
					LOCK_SCTP_ID_H(hash);
						clist_insert(&sctp_con_id_hash[hash], e,
									l.next_id, l.prev_id);
					UNLOCK_SCTP_ID_H(hash);
#ifdef SCTP_ADDR_HASH
					hash=get_sctp_con_addr_hash(&e->con.remote, e->con.si);
					LOCK_SCTP_ADDR_H(hash);
						clist_insert(&sctp_con_addr_hash[hash], e,
									l.next_addr, l.prev_addr);
					UNLOCK_SCTP_ADDR_H(hash);
#endif /* SCTP_ADDR_HASH */
					atomic_inc(sctp_conn_tracked);
				}
		} /* else not found and RCV -> ignore
			 We cannot create a new entry because we don't know when to
			 delete it (we can have UP DOWN RCV which would result in a
			 tracking entry living forever). This means that if we receive
			 a msg. on an assoc. before it's UP notification we won't know
			 the id for connection reuse, but since happens very rarely it's
			 an acceptable tradeoff */
	UNLOCK_SCTP_ASSOC_H(assoc_hash);
	if (unlikely(e==0)){
		ERR("memory allocation failure\n");
		goto error;
	}
found:
	return id;
error:
	return 0;
}



#else /* SCTP_CONN_REUSE */
void sctp_con_tracking_flush() {}
#endif /* SCTP_CONN_REUSE */


int init_sctp()
{
	int ret;
	
	ret=0;
	if (INIT_SCTP_STATS()!=0){
		ERR("sctp init: failed to intialize sctp stats\n");
		goto error;
	}
	/* sctp options must be initialized before  calling this function */
	sctp_conn_no=shm_malloc(sizeof(*sctp_conn_tracked));
	if ( sctp_conn_no==0){
		ERR("sctp init: memory allocation error\n");
		ret=E_OUT_OF_MEM;
		goto error;
	}
	atomic_set(sctp_conn_no, 0);
#ifdef SCTP_CONN_REUSE
	return init_sctp_con_tracking();
#endif
error:
	return ret;
}



void destroy_sctp()
{
	if (sctp_conn_no){
		shm_free(sctp_conn_no);
		sctp_conn_no=0;
	}
#ifdef SCTP_CONN_REUSE
	destroy_sctp_con_tracking();
#endif
	DESTROY_SCTP_STATS();
}



static int sctp_msg_send_ext(struct dest_info* dst, char* buf, unsigned len,
						struct sctp_sndrcvinfo* sndrcv_info);
#define SCTP_SEND_FIRST_ASSOCID 1  /* sctp_raw_send flag */
static int sctp_raw_send(int socket, char* buf, unsigned len,
						union sockaddr_union* to,
						struct sctp_sndrcvinfo* sndrcv_info,
						int flags);



/* debugging: return a string name for SCTP_ASSOC_CHANGE state */
static char* sctp_assoc_change_state2s(short int state)
{
	char* s;
	
	switch(state){
		case SCTP_COMM_UP:
			s="SCTP_COMM_UP";
			break;
		case SCTP_COMM_LOST:
			s="SCTP_COMM_LOST";
			break;
		case SCTP_RESTART:
			s="SCTP_RESTART";
			break;
		case SCTP_SHUTDOWN_COMP:
			s="SCTP_SHUTDOWN_COMP";
			break;
		case SCTP_CANT_STR_ASSOC:
			s="SCTP_CANT_STR_ASSOC";
			break;
		default:
			s="UNKNOWN";
			break;
	};
	return s;
}



/* debugging: return a string name for a SCTP_PEER_ADDR_CHANGE state */
static char* sctp_paddr_change_state2s(unsigned int state)
{
	char* s;
	
	switch (state){
		case SCTP_ADDR_AVAILABLE:
			s="SCTP_ADDR_AVAILABLE";
			break;
		case SCTP_ADDR_UNREACHABLE:
			s="SCTP_ADDR_UNREACHABLE";
			break;
		case SCTP_ADDR_REMOVED:
			s="SCTP_ADDR_REMOVED";
			break;
		case SCTP_ADDR_ADDED:
			s="SCTP_ADDR_ADDED";
			break;
		case SCTP_ADDR_MADE_PRIM:
			s="SCTP_ADDR_MADE_PRIM";
			break;
	/* not supported by lksctp 1.0.6 
		case SCTP_ADDR_CONFIRMED:
			s="SCTP_ADDR_CONFIRMED";
			break;
	*/
		default:
			s="UNKNOWN";
			break;
	}
	return s;
}



/* handle SCTP_SEND_FAILED notifications: if packet marked for retries
 * retry the send (with 0 assoc_id)
 * returns 0 on success, -1 on failure
 */
static int sctp_handle_send_failed(struct socket_info* si,
									union sockaddr_union* su,
									char* buf, unsigned len)
{
	union sctp_notification* snp;
	struct sctp_sndrcvinfo sinfo;
	struct dest_info dst;
	char* data;
	unsigned data_len;
	int retries;
	int ret;
#ifdef HAVE_SCTP_SNDRCVINFO_PR_POLICY
	int send_ttl;
#endif
	
	ret=-1;
	SCTP_STATS_SEND_FAILED();
	snp=(union sctp_notification*) buf;
	retries=snp->sn_send_failed.ssf_info.sinfo_context;
	
	/* don't retry on explicit remote error
	 * (unfortunately we can't be more picky than this, we get no 
	 * indication in the SEND_FAILED notification for other error
	 * reasons (e.g. ABORT received, INIT timeout a.s.o)
	 */
	if (retries && (snp->sn_send_failed.ssf_error==0)) {
		DBG("sctp: RETRY-ing (%d)\n", retries);
		SCTP_STATS_SEND_FORCE_RETRY();
		retries--;
		data=(char*)snp->sn_send_failed.ssf_data;
		data_len=snp->sn_send_failed.ssf_length - 
					sizeof(struct sctp_send_failed);
		
		memset(&sinfo, 0, sizeof(sinfo));
		sinfo.sinfo_flags=SCTP_UNORDERED;
#ifdef HAVE_SCTP_SNDRCVINFO_PR_POLICY
		if ((send_ttl=cfg_get(sctp, sctp_cfg, send_ttl))){
			sinfo.sinfo_pr_policy=SCTP_PR_SCTP_TTL;
			sinfo.sinfo_pr_value=send_ttl;
		}else
			sinfo.info_pr_policy=SCTP_PR_SCTP_NONE;
#else
		sinfo.sinfo_timetolive=cfg_get(sctp, sctp_cfg, send_ttl);
#endif
		sinfo.sinfo_context=retries;
		
		dst.to=*su;
		dst.send_sock=si;
		dst.id=0;
		dst.proto=PROTO_SCTP;
#ifdef USE_COMP
		dst.comp=COMP_NONE;
#endif
		
		ret=sctp_msg_send_ext(&dst, data, data_len, &sinfo);
	}
#ifdef USE_DST_BLACKLIST
	 else if (cfg_get(sctp, sctp_cfg, send_retries)) {
		/* blacklist only if send_retries is on, if off we blacklist
		   from SCTP_ASSOC_CHANGE: SCTP_COMM_LOST/SCTP_CANT_STR_ASSOC
		   which is better (because we can tell connect errors from send
		   errors and we blacklist a failed dst only once) */
		dst_blacklist_su(BLST_ERR_SEND, PROTO_SCTP, su, 0, 0);
	}
#endif /* USE_DST_BLACKLIST */
	
	return (ret>0)?0:ret;
}



/* handle SCTP_ASOC_CHANGE notifications: map ser global sctp ids
 * to kernel asoc_ids. The global ids are needed because the kernel ones
 * might get reused after a close and so they are not unique for ser's
 * lifetime. We need a unique id to match replies to the association on
 * which we received the corresponding request (so that we can send them
 * back on the same asoc & socket if still opened).
 * returns 0 on success, -1 on failure
 */
static int sctp_handle_assoc_change(struct socket_info* si,
									union sockaddr_union* su,
									union sctp_notification* snp
									)
{
	int ret;
	int state;
	int assoc_id;
	struct sctp_sndrcvinfo sinfo;
	struct ip_addr ip; /* used only on error, for debugging */
	
	state=snp->sn_assoc_change.sac_state;
	assoc_id=snp->sn_assoc_change.sac_assoc_id;
	
	ret=-1;
	switch(state){
		case SCTP_COMM_UP:
			SCTP_STATS_ESTABLISHED();
			atomic_inc(sctp_conn_no);
#ifdef SCTP_CONN_REUSE
			/* new connection, track it */
			if (likely(cfg_get(sctp, sctp_cfg, assoc_tracking)))
					sctp_con_track(assoc_id, si, su, SCTP_CON_UP_SEEN);
#if 0
again:
			id=atomic_add(sctp_id, 1);
			if (unlikely(id==0)){
				/* overflow  and 0 is not a valid id */
				goto again;
			}
			e=sctp_con_new(id, assoc_id, si, su);
			if (unlikely(e==0)){
				ERR("memory allocation failure\n");
			}else{
				sctp_con_add(e);
				ret=0;
			}
#endif
#endif /* SCTP_CONN_REUSE */
			if (unlikely((unsigned)atomic_get(sctp_conn_no) >
							(unsigned)cfg_get(sctp, sctp_cfg, max_assocs))){
				/* maximum assoc exceeded => we'll have to immediately 
				   close it */
				memset(&sinfo, 0, sizeof(sinfo));
				sinfo.sinfo_flags=SCTP_UNORDERED | SCTP_ABORT;
				sinfo.sinfo_assoc_id=assoc_id;
				ret=sctp_raw_send(si->socket, ABORT_REASON_MAX_ASSOCS,
											sizeof(ABORT_REASON_MAX_ASSOCS)-1,
											su, &sinfo, 0);
				if (ret<0){
					su2ip_addr(&ip, su);
					WARN("failed to ABORT new sctp association %d (%s:%d):"
							" %s (%d)\n", assoc_id, ip_addr2a(&ip),
							su_getport(su), strerror(errno), errno);
				}else{
					SCTP_STATS_LOCAL_REJECT();
				}
			}
			break;
		case SCTP_COMM_LOST:
			SCTP_STATS_COMM_LOST();
#ifdef USE_DST_BLACKLIST
			/* blacklist only if send_retries is turned off (if on we don't
			   know here if we did retry or we are at the first error) */
			if (cfg_get(sctp, sctp_cfg, send_retries)==0)
						dst_blacklist_su(BLST_ERR_SEND, PROTO_SCTP, su, 0, 0);
#endif /* USE_DST_BLACKLIST */
			/* no break */
			goto comm_lost_cont;	/* do not increment counters for
									   SCTP_SHUTDOWN_COMP */
		case SCTP_SHUTDOWN_COMP:
			SCTP_STATS_ASSOC_SHUTDOWN();
comm_lost_cont:
			atomic_dec(sctp_conn_no);
#ifdef SCTP_CONN_REUSE
			/* connection down*/
			if (likely(cfg_get(sctp, sctp_cfg, assoc_tracking)))
				sctp_con_track(assoc_id, si, su, SCTP_CON_DOWN_SEEN);
#if 0
			if (unlikely(sctp_con_del_assoc(assoc_id, si)!=0))
				WARN("sctp con: tried to remove inexistent connection\n");
			else
				ret=0;
#endif
#endif /* SCTP_CONN_REUSE */
			break;
		case SCTP_RESTART:
			/* do nothing on restart */
			break;
		case SCTP_CANT_STR_ASSOC:
			SCTP_STATS_CONNECT_FAILED();
			/* do nothing when failing to start an assoc
			  (in this case we never see SCTP_COMM_UP so we never 
			  track the assoc) */
#ifdef USE_DST_BLACKLIST
			/* blacklist only if send_retries is turned off (if on we don't 
			   know here if we did retry or we are at the first error) */
			if (cfg_get(sctp, sctp_cfg, send_retries)==0)
					dst_blacklist_su(BLST_ERR_CONNECT, PROTO_SCTP, su, 0, 0);
#endif /* USE_DST_BLACKLIST */
			break;
		default:
			break;
	}
	return ret;
}



static int sctp_handle_notification(struct socket_info* si,
									union sockaddr_union* su,
									char* buf, unsigned len)
{
	union sctp_notification* snp;
	char su_buf[SU2A_MAX_STR_SIZE];
	
	#define SNOT DBG
	#define ERR_LEN_TOO_SMALL(length, val, bind_addr, from_su, text) \
		if (unlikely((length)<(val))){\
			SNOT("ERROR: sctp notification from %s on %.*s:%d: " \
						text " too short (%d bytes instead of %d bytes)\n", \
						su2a((from_su), sizeof(*(from_su))), \
						(bind_addr)->name.len, (bind_addr)->name.s, \
						(bind_addr)->port_no, (int)(length), (int)(val)); \
			goto error; \
		}

	if (len < sizeof(snp->sn_header)){
		LOG(L_ERR, "ERROR: sctp_handle_notification: invalid length %d "
					"on %.*s:%d, from %s\n",
					len, si->name.len, si->name.s, si->port_no,
					su2a(su, sizeof(*su)));
		goto error;
	}
	snp=(union sctp_notification*) buf;
	switch(snp->sn_header.sn_type){
		case SCTP_REMOTE_ERROR:
			ERR_LEN_TOO_SMALL(len, sizeof(struct sctp_remote_error), si, su,
								"SCTP_REMOTE_ERROR");
			SCTP_EV_REMOTE_ERROR(&si->address, si->port_no, su, 
									ntohs(snp->sn_remote_error.sre_error) );
			SNOT("sctp notification from %s on %.*s:%d: SCTP_REMOTE_ERROR:"
					" %d, len %d\n, assoc_id %d",
					su2a(su, sizeof(*su)), si->name.len, si->name.s,
					si->port_no,
					ntohs(snp->sn_remote_error.sre_error),
					ntohs(snp->sn_remote_error.sre_length),
					snp->sn_remote_error.sre_assoc_id
				);
			break;
		case SCTP_SEND_FAILED:
			ERR_LEN_TOO_SMALL(len, sizeof(struct sctp_send_failed), si, su,
								"SCTP_SEND_FAILED");
			SCTP_EV_SEND_FAILED(&si->address, si->port_no, su, 
									snp->sn_send_failed.ssf_error);
			SNOT("sctp notification from %s on %.*s:%d: SCTP_SEND_FAILED:"
					" error %d, assoc_id %d, flags %x\n",
					su2a(su, sizeof(*su)), si->name.len, si->name.s,
					si->port_no, snp->sn_send_failed.ssf_error,
					snp->sn_send_failed.ssf_assoc_id,
					snp->sn_send_failed.ssf_flags);
			sctp_handle_send_failed(si, su, buf, len);
			break;
		case SCTP_PEER_ADDR_CHANGE:
			ERR_LEN_TOO_SMALL(len, sizeof(struct sctp_paddr_change), si, su,
								"SCTP_PEER_ADDR_CHANGE");
			SCTP_EV_PEER_ADDR_CHANGE(&si->address, si->port_no, su, 
					sctp_paddr_change_state2s(snp->sn_paddr_change.spc_state),
					snp->sn_paddr_change.spc_state,
					&snp->sn_paddr_change.spc_aaddr);
			strcpy(su_buf, su2a((union sockaddr_union*)
									&snp->sn_paddr_change.spc_aaddr, 
									sizeof(snp->sn_paddr_change.spc_aaddr)));
			SNOT("sctp notification from %s on %.*s:%d: SCTP_PEER_ADDR_CHANGE"
					": %s: %s: assoc_id %d \n",
					su2a(su, sizeof(*su)), si->name.len, si->name.s,
					si->port_no, su_buf,
					sctp_paddr_change_state2s(snp->sn_paddr_change.spc_state),
					snp->sn_paddr_change.spc_assoc_id
					);
			break;
		case SCTP_SHUTDOWN_EVENT:
			SCTP_STATS_REMOTE_SHUTDOWN();
			ERR_LEN_TOO_SMALL(len, sizeof(struct sctp_shutdown_event), si, su,
								"SCTP_SHUTDOWN_EVENT");
			SCTP_EV_SHUTDOWN_EVENT(&si->address, si->port_no, su);
			SNOT("sctp notification from %s on %.*s:%d: SCTP_SHUTDOWN_EVENT:"
					" assoc_id %d\n",
					su2a(su, sizeof(*su)), si->name.len, si->name.s,
					si->port_no, snp->sn_shutdown_event.sse_assoc_id);
			break;
		case SCTP_ASSOC_CHANGE:
			ERR_LEN_TOO_SMALL(len, sizeof(struct sctp_assoc_change), si, su,
								"SCTP_ASSOC_CHANGE");
			SCTP_EV_ASSOC_CHANGE(&si->address, si->port_no, su, 
					sctp_assoc_change_state2s(snp->sn_assoc_change.sac_state),
					snp->sn_assoc_change.sac_state);
			SNOT("sctp notification from %s on %.*s:%d: SCTP_ASSOC_CHANGE"
					": %s: assoc_id %d, ostreams %d, istreams %d\n",
					su2a(su, sizeof(*su)), si->name.len, si->name.s,
					si->port_no,
					sctp_assoc_change_state2s(snp->sn_assoc_change.sac_state),
					snp->sn_assoc_change.sac_assoc_id,
					snp->sn_assoc_change.sac_outbound_streams,
					snp->sn_assoc_change.sac_inbound_streams
					);
			sctp_handle_assoc_change(si, su, snp);
			break;
#ifdef SCTP_ADAPTION_INDICATION
		case SCTP_ADAPTION_INDICATION:
			ERR_LEN_TOO_SMALL(len, sizeof(struct sctp_adaption_event), si, su,
								"SCTP_ADAPTION_INDICATION");
			SNOT("sctp notification from %s on %.*s:%d: "
					"SCTP_ADAPTION_INDICATION \n",
					su2a(su, sizeof(*su)), si->name.len, si->name.s,
					si->port_no);
			break;
#endif /* SCTP_ADAPTION_INDICATION */
		case SCTP_PARTIAL_DELIVERY_EVENT:
			ERR_LEN_TOO_SMALL(len, sizeof(struct sctp_pdapi_event), si, su,
								"SCTP_PARTIAL_DELIVERY_EVENT");
			ERR("sctp notification from %s on %.*s:%d: "
					"SCTP_PARTIAL_DELIVERY_EVENT not supported: %d %s,"
					"assoc_id %d\n",
					su2a(su, sizeof(*su)), si->name.len, si->name.s,
					si->port_no, snp->sn_pdapi_event.pdapi_indication,
					(snp->sn_pdapi_event.pdapi_indication==
					 	SCTP_PARTIAL_DELIVERY_ABORTED)? " PD ABORTED":"",
					snp->sn_pdapi_event.pdapi_assoc_id);
			break;
#ifdef SCTP_SENDER_DRY_EVENT /* new, not yet supported */
		case SCTP_SENDER_DRY_EVENT:
			ERR_LEN_TOO_SMALL(len, sizeof(struct sctp_sender_dry_event),
								si, su, "SCTP_SENDER_DRY_EVENT");
			SCTP_EV_REMOTE_ERROR(&si->address, si->port_no, su, 0);
			SNOT("sctp notification from %s on %.*s:%d: "
					"SCTP_SENDER_DRY_EVENT on %d\n",
					su2a(su, sizeof(*su)), si->name.len, si->name.s,
					si->port_no, snp->sn_sender_dry_event.sender_dry_assoc_id);
			break;
#endif /* SCTP_SENDER_DRY_EVENT */
		default:
			SNOT("sctp notification from %s on %.*s:%d: UNKNOWN (%d)\n",
					su2a(su, sizeof(*su)), si->name.len, si->name.s,
					si->port_no, snp->sn_header.sn_type);
	}
	return 0;
error:
	return -1;
	#undef ERR_LEN_TOO_SMALL
}



int sctp_rcv_loop()
{
	unsigned len;
	static char buf [BUF_SIZE+1];
	char *tmp;
	struct receive_info ri;
	struct sctp_sndrcvinfo* sinfo;
	struct msghdr msg;
	struct iovec iov[1];
	struct cmsghdr* cmsg;
	/* use a larger buffer then needed in case some other ancillary info
	 * is enabled */
	char cbuf[CMSG_SPACE(sizeof(*sinfo))+CMSG_SPACE(1024)];

	
	ri.bind_address=bind_address; /* this will not change */
	ri.dst_port=bind_address->port_no;
	ri.dst_ip=bind_address->address;
	ri.proto=PROTO_SCTP;
	ri.proto_reserved2=0;
	
	iov[0].iov_base=buf;
	iov[0].iov_len=BUF_SIZE;
	msg.msg_iov=iov;
	msg.msg_iovlen=1;
	msg.msg_flags=0;
	

	/* initialize the config framework */
	if (cfg_child_init()) goto error;
	
	for(;;){
		msg.msg_name=&ri.src_su.s;
		msg.msg_namelen=sockaddru_len(bind_address->su);
		msg.msg_control=cbuf;
		msg.msg_controllen=sizeof(cbuf);
		sinfo=0;

		len=recvmsg(bind_address->socket, &msg, 0);
		if (len==-1){
			if (errno==EAGAIN){
				DBG("sctp_rcv_loop: EAGAIN on sctp socket\n");
				continue;
			}
			LOG(L_ERR, "ERROR: sctp_rcv_loop: sctp_recvmsg on %d (%p):"
						"[%d] %s\n", bind_address->socket, bind_address,
						errno, strerror(errno));
			if ((errno==EINTR)||(errno==EWOULDBLOCK)|| (errno==ECONNREFUSED))
				continue; /* goto skip;*/
			else goto error;
		}
		/* update the local config */
		cfg_update();
		
		if (unlikely(msg.msg_flags & MSG_NOTIFICATION)){
			/* intercept useful notifications */
			sctp_handle_notification(bind_address, &ri.src_su, buf, len);
			continue;
		}else if (unlikely(!(msg.msg_flags & MSG_EOR))){
			LOG(L_ERR, "ERROR: sctp_rcv_loop: partial delivery not"
						"supported\n");
			continue;
		}
		
		su2ip_addr(&ri.src_ip, &ri.src_su);
		ri.src_port=su_getport(&ri.src_su);
		
		/* get ancillary data */
		for (cmsg=CMSG_FIRSTHDR(&msg); cmsg; cmsg=CMSG_NXTHDR(&msg, cmsg)){
#ifdef SCTP_EXT
			if (likely((cmsg->cmsg_level==IPPROTO_SCTP) &&
						((cmsg->cmsg_type==SCTP_SNDRCV)
						 || (cmsg->cmsg_type==SCTP_EXTRCV)
						) && (cmsg->cmsg_len>=CMSG_LEN(sizeof(*sinfo)))) )
#else  /* !SCTP_EXT -- same as above but w/o SCTP_EXTRCV */
			if (likely((cmsg->cmsg_level==IPPROTO_SCTP) &&
						((cmsg->cmsg_type==SCTP_SNDRCV)
						) && (cmsg->cmsg_len>=CMSG_LEN(sizeof(*sinfo)))) )
#endif /*SCTP_EXT */
			{
				sinfo=(struct sctp_sndrcvinfo*)CMSG_DATA(cmsg);
				DBG("sctp recv: message from %s:%d stream %d  ppid %x"
						" flags %x%s tsn %u" " cumtsn %u assoc_id %d\n",
						ip_addr2a(&ri.src_ip), ri.src_port,
						sinfo->sinfo_stream, sinfo->sinfo_ppid,
						sinfo->sinfo_flags,
						(sinfo->sinfo_flags&SCTP_UNORDERED)?
							" (SCTP_UNORDERED)":"",
						sinfo->sinfo_tsn, sinfo->sinfo_cumtsn, 
						sinfo->sinfo_assoc_id);
				break;
			}
		}
		/* we  0-term the messages for debugging */
		buf[len]=0; /* no need to save the previous char */

		/* sanity checks */
		if (len<MIN_SCTP_PACKET) {
			tmp=ip_addr2a(&ri.src_ip);
			DBG("sctp_rcv_loop: probing packet received from %s:%d\n",
					tmp, ri.src_port);
			continue;
		}
		if (ri.src_port==0){
			tmp=ip_addr2a(&ri.src_ip);
			LOG(L_INFO, "sctp_rcv_loop: dropping 0 port packet from %s:0\n",
						tmp);
			continue;
		}
#ifdef USE_COMP
		ri.comp=COMP_NONE;
#endif
#ifdef SCTP_CONN_REUSE
		if (likely(cfg_get(sctp, sctp_cfg, assoc_tracking) && sinfo)){
			ri.proto_reserved1 = sctp_con_track(sinfo->sinfo_assoc_id,
												ri.bind_address, 
												&ri.src_su,
												SCTP_CON_RCV_SEEN);
			/* debugging */
			if (unlikely(ri.proto_reserved1==0))
				DBG("no tracked assoc. found for assoc_id %d, from %s\n",
						sinfo->sinfo_assoc_id, 
						su2a(&ri.src_su, sizeof(ri.src_su)));
#if 0
			ri.proto_reserved1=
				sctp_con_get_id(sinfo->sinfo_assoc_id, ri.bind_address, 0);
#endif
		}else
			ri.proto_reserved1=0;
#else /* SCTP_CONN_REUSE */
		ri.proto_received1=0;
#endif /* SCTP_CONN_REUSE */
		receive_msg(buf, len, &ri);
	}
error:
	return -1;
}



/** low level sctp non-blocking send.
 * @param socket - sctp socket to send on.
 * @param buf   - data.
 * @param len   - lenght of the data.
 * @param to    - destination in ser sockaddr_union format.
 * @param sndrcv_info - sctp_sndrcvinfo structure pointer, pre-filled.
 * @param flags - can have one of the following values (or'ed):
 *                SCTP_SEND_FIRST_ASSOCID - try to send first to assoc_id
 *                and only if that fails use "to".
 * @return the numbers of bytes sent on success (>=0) and -1 on error.
 * On error errno is set too.
 */
static int sctp_raw_send(int socket, char* buf, unsigned len,
						union sockaddr_union* to,
						struct sctp_sndrcvinfo* sndrcv_info,
						int flags)
{
	int n;
	int tolen;
	int try_assoc_id;
#if 0
	struct ip_addr ip; /* used only on error, for debugging */
#endif
	struct msghdr msg;
	struct iovec iov[1];
	struct sctp_sndrcvinfo* sinfo;
	struct cmsghdr* cmsg;
	/* make sure msg_control will point to properly aligned data */
	union {
		struct cmsghdr cm;
		char cbuf[CMSG_SPACE(sizeof(*sinfo))];
	}ctrl_un;
	
	iov[0].iov_base=buf;
	iov[0].iov_len=len;
	msg.msg_iov=iov;
	msg.msg_iovlen=1;
	msg.msg_flags=0; /* not used on send (use instead sinfo_flags) */
	msg.msg_control=ctrl_un.cbuf;
	msg.msg_controllen=sizeof(ctrl_un.cbuf);
	cmsg=CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level=IPPROTO_SCTP;
	cmsg->cmsg_type=SCTP_SNDRCV;
	cmsg->cmsg_len=CMSG_LEN(sizeof(*sinfo));
	sinfo=(struct sctp_sndrcvinfo*)CMSG_DATA(cmsg);
	*sinfo=*sndrcv_info;
	/* some systems need msg_controllen set to the actual size and not
	 * something bigger (e.g. openbsd) */
	msg.msg_controllen=cmsg->cmsg_len;
	try_assoc_id= ((flags & SCTP_SEND_FIRST_ASSOCID) && sinfo->sinfo_assoc_id);
	/* if assoc_id is set it means we want to send on association assoc_id
	   and only if it's not opened any longer use the addresses */
	if (try_assoc_id){
		/* on linux msg->name has priority over assoc_id. To try first assoc_id
		 * and then "to", one has to call first sendmsg() with msg->name==0 and
		 * sinfo->assoc_id set. If it returns EPIPE => association is no longer
		 * open => call again sendmsg() this time with msg->name!=0.
		 * on freebsd assoc_id has priority over msg->name and moreover the
		 * send falls back automatically to the address if the assoc_id is
		 * closed, so a single call to sendmsg(msg->name, sinfo->assoc_id ) is
		 * enough.  If one tries calling with msg->name==0 and the association
		 * is no longer open send will return ENOENT.
		 * on solaris it seems one must always use a dst address (assoc_id
		 * will be ignored).
		 */
#ifdef __OS_linux
		msg.msg_name=0;
		msg.msg_namelen=0;
#elif defined __OS_freebsd
		tolen=sockaddru_len(*to);
		msg.msg_name=&to->s;
		msg.msg_namelen=tolen;
#else /* __OS_* */
		/* fallback for solaris and others, sent back to
		  the address recorded (not exactly what we want, but there's
		  no way to fallback to "to") */
		tolen=sockaddru_len(*to);
		msg.msg_name=&to->s;
		msg.msg_namelen=tolen;
#endif /* __OS_* */
	}else{
		tolen=sockaddru_len(*to);
		msg.msg_name=&to->s;
		msg.msg_namelen=tolen;
	}
	
again:
	n=sendmsg(socket, &msg, MSG_DONTWAIT);
	if (n==-1){
#ifdef __OS_linux
		if ((errno==EPIPE) && try_assoc_id){
			/* try again, this time with null assoc_id and non-null msg.name */
			DBG("sctp raw sendmsg: assoc already closed (EPIPE), retrying with"
					" assoc_id=0\n");
			tolen=sockaddru_len(*to);
			msg.msg_name=&to->s;
			msg.msg_namelen=tolen;
			sinfo->sinfo_assoc_id=0;
			try_assoc_id=0;
			goto again;
		}
#elif defined __OS_freebsd
		if (errno==ENOENT){
			/* it didn't work, no retrying */
			WARN("unexpected sendmsg() failure (ENOENT),"
					" assoc_id %d\n", sinfo->sinfo_assoc_id);
		}
#else /* __OS_* */
		if ((errno==ENOENT || errno==EPIPE) && try_assoc_id){
			/* in case the sctp stack prioritises assoc_id over msg->name,
			   try again with 0 assoc_id and msg->name set to "to" */
			WARN("unexpected ENOENT or EPIPE (assoc_id %d),"
					"trying automatic recovery... (please report along with"
					"your OS version)\n", sinfo->sinfo_assoc_id);
			tolen=sockaddru_len(*to);
			msg.msg_name=&to->s;
			msg.msg_namelen=tolen;
			sinfo->sinfo_assoc_id=0;
			try_assoc_id=0;
			goto again;
		}
#endif /* __OS_* */
#if 0
		if (errno==EINTR) goto again;
		su2ip_addr(&ip, to);
		LOG(L_ERR, "ERROR: sctp_raw_send: sendmsg(sock,%p,%d,0,%s:%d,...):"
				" %s(%d)\n", buf, len, ip_addr2a(&ip), su_getport(to),
				strerror(errno), errno);
		if (errno==EINVAL) {
			LOG(L_CRIT,"CRITICAL: invalid sendmsg parameters\n"
			"one possible reason is the server is bound to localhost and\n"
			"attempts to send to the net\n");
		}else if (errno==EAGAIN || errno==EWOULDBLOCK){
			SCTP_STATS_SENDQ_FULL();
			LOG(L_ERR, "ERROR: sctp_msg_send: failed to send, send buffers"
						" full\n");
		}
#endif
	}
	return n;
}



/* send buf:len over sctp to dst using sndrcv_info (uses send_sock,
 * to and id from dest_info)
 * returns the numbers of bytes sent on success (>=0) and -1 on error
 */
static int sctp_msg_send_ext(struct dest_info* dst, char* buf, unsigned len,
						struct sctp_sndrcvinfo* sndrcv_info)
{
	int n;
	int tolen;
	struct ip_addr ip; /* used only on error, for debugging */
	struct msghdr msg;
	struct iovec iov[1];
	struct socket_info* si;
	struct sctp_sndrcvinfo* sinfo;
	struct cmsghdr* cmsg;
	/* make sure msg_control will point to properly aligned data */
	union {
		struct cmsghdr cm;
		char cbuf[CMSG_SPACE(sizeof(*sinfo))];
	}ctrl_un;
#ifdef SCTP_CONN_REUSE
	int assoc_id;
	union sockaddr_union to;
#ifdef SCTP_ADDR_HASH
	int tmp_id, tmp_assoc_id;
#endif /* SCTP_ADDR_HASH */
#endif /* SCTP_CONN_REUSE */
	
	iov[0].iov_base=buf;
	iov[0].iov_len=len;
	msg.msg_iov=iov;
	msg.msg_iovlen=1;
	msg.msg_flags=0; /* not used on send (use instead sinfo_flags) */
	msg.msg_control=ctrl_un.cbuf;
	msg.msg_controllen=sizeof(ctrl_un.cbuf);
	cmsg=CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level=IPPROTO_SCTP;
	cmsg->cmsg_type=SCTP_SNDRCV;
	cmsg->cmsg_len=CMSG_LEN(sizeof(*sinfo));
	sinfo=(struct sctp_sndrcvinfo*)CMSG_DATA(cmsg);
	*sinfo=*sndrcv_info;
	/* some systems need msg_controllen set to the actual size and not
	 * something bigger (e.g. openbsd) */
	msg.msg_controllen=cmsg->cmsg_len;
	si=dst->send_sock;
#ifdef SCTP_CONN_REUSE
	/* if dst->id is set it means we want to send on association with
	   ser id dst->id if still opened and only if closed use dst->to */
	assoc_id=0;
	if ((dst->id) && cfg_get(sctp, sctp_cfg, assoc_reuse) &&
			cfg_get(sctp, sctp_cfg, assoc_tracking) &&
			(assoc_id=sctp_con_get_assoc(dst->id, &si, &to, 0))){
		DBG("sctp: sending on sctp assoc_id %d (ser id %d)\n",
				assoc_id, dst->id);
		sinfo->sinfo_assoc_id=assoc_id;
		/* on linux msg->name has priority over assoc_id. To try first assoc_id
		 * and then dst, one has to call first sendmsg() with msg->name==0 and
		 * sinfo->assoc_id set. If it returns EPIPE => association is no longer
		 * open => call again sendmsg() this time with msg->name!=0.
		 * on freebsd assoc_id has priority over msg->name and moreover the
		 * send falls back automatically to the address if the assoc_id is
		 * closed, so a single call to sendmsg(msg->name, sinfo->assoc_id ) is
		 * enough.  If one tries calling with msg->name==0 and the association
		 * is no longer open send will return ENOENT.
		 * on solaris it seems one must always use a dst address (assoc_id
		 * will be ignored).
		 */
#ifdef __OS_linux
		DBG("sctp: linux: trying with 0 msg_name\n");
		msg.msg_name=0;
		msg.msg_namelen=0;
#elif defined __OS_freebsd
		tolen=sockaddru_len(dst->to);
		msg.msg_name=&dst->to.s;
		msg.msg_namelen=tolen;
#else /* __OS_* */
		/* fallback for solaris and others, sent back to
		  the address recorded (not exactly what we want, but there's 
		  no way to fallback to dst->to) */
		tolen=sockaddru_len(dst->to);
		msg.msg_name=&dst->to.s;
		msg.msg_namelen=tolen;
#endif /* __OS_* */
	}else{
#ifdef SCTP_ADDR_HASH
		/* update timeout for the assoc identified  by (dst->to, dst->si) */
		if (likely(cfg_get(sctp, sctp_cfg, assoc_tracking))){
			tmp_id=sctp_con_addr_get_id_assoc(&dst->to, dst->send_sock,
												&tmp_assoc_id, 0);
			DBG("sctp send: timeout updated ser id %d, sctp assoc_id %d\n",
					tmp_id, tmp_assoc_id);
			if (tmp_id==0 /* not tracked/found */ &&
					(unsigned)atomic_get(sctp_conn_tracked) >=
						(unsigned)cfg_get(sctp, sctp_cfg, max_assocs)){
				ERR("maximum number of sctp associations exceeded\n");
				goto error;
			}
		}
#endif /* SCTP_ADDR_HASH */
		tolen=sockaddru_len(dst->to);
		msg.msg_name=&dst->to.s;
		msg.msg_namelen=tolen;
	}
#else /* SCTP_CONN_REUSE */
	tolen=sockaddru_len(dst->to);
	msg.msg_name=&dst->to.s;
	msg.msg_namelen=tolen;
#endif /* SCTP_CONN_REUSE */

again:
	n=sendmsg(si->socket, &msg, MSG_DONTWAIT);
	if (n==-1){
#ifdef SCTP_CONN_REUSE
#ifdef __OS_linux
		if ((errno==EPIPE) && assoc_id){
			/* try again, this time with null assoc_id and non-null msg.name */
			DBG("sctp sendmsg: assoc already closed (EPIPE), retrying with"
					" assoc_id=0\n");
			tolen=sockaddru_len(dst->to);
			msg.msg_name=&dst->to.s;
			msg.msg_namelen=tolen;
			sinfo->sinfo_assoc_id=0;
			goto again;
		}
#elif defined __OS_freebsd
		if (errno==ENOENT){
			/* it didn't work, no retrying */
			WARN("sctp sendmsg: unexpected sendmsg() failure (ENOENT),"
					" assoc_id %d\n", assoc_id);
		}
#else /* __OS_* */
		if ((errno==ENOENT || errno==EPIPE) && assoc_id){
			/* in case the sctp stack prioritises assoc_id over msg->name,
			   try again with 0 assoc_id and msg->name set to dst->to */
			WARN("sctp sendmsg: unexpected ENOENT or EPIPE (assoc_id %d),"
					"trying automatic recovery... (please report along with"
					"your OS version)\n", assoc_id);
			tolen=sockaddru_len(dst->to);
			msg.msg_name=&dst->to.s;
			msg.msg_namelen=tolen;
			sinfo->sinfo_assoc_id=0;
			goto again;
		}
#endif /* __OS_* */
#endif /* SCTP_CONN_REUSE */
		su2ip_addr(&ip, &dst->to);
		LOG(L_ERR, "ERROR: sctp_msg_send: sendmsg(sock,%p,%d,0,%s:%d,...):"
				" %s(%d)\n", buf, len, ip_addr2a(&ip), su_getport(&dst->to),
				strerror(errno), errno);
		if (errno==EINTR) goto again;
		if (errno==EINVAL) {
			LOG(L_CRIT,"CRITICAL: invalid sendmsg parameters\n"
			"one possible reason is the server is bound to localhost and\n"
			"attempts to send to the net\n");
		}else if (errno==EAGAIN || errno==EWOULDBLOCK){
			SCTP_STATS_SENDQ_FULL();
			LOG(L_ERR, "ERROR: sctp_msg_send: failed to send, send buffers"
						" full\n");
		}
	}
	return n;
#ifdef SCTP_CONN_REUSE
#ifdef SCTP_ADDR_HASH
error:
	return -1;
#endif /* SCTP_ADDR_HASH */
#endif /* SCTP_CONN_REUSE */
}



/* wrapper around sctp_msg_send_ext():
 * send buf:len over udp to dst (uses only the to, send_sock and id members
 * from dst)
 * returns the numbers of bytes sent on success (>=0) and -1 on error
 */
int sctp_msg_send(struct dest_info* dst, char* buf, unsigned len)
{
	struct sctp_sndrcvinfo sinfo;
#ifdef HAVE_SCTP_SNDRCVINFO_PR_POLICY
	int send_ttl;
#endif
	
	memset(&sinfo, 0, sizeof(sinfo));
	sinfo.sinfo_flags=SCTP_UNORDERED;
#ifdef HAVE_SCTP_SNDRCVINFO_PR_POLICY
	if ((send_ttl=cfg_get(sctp, sctp_cfg, send_ttl))){
		sinfo.sinfo_pr_policy=SCTP_PR_SCTP_TTL;
		sinfo.sinfo_pr_value=send_ttl;
	}else
		sinfo->sinfo_pr_policy=SCTP_PR_SCTP_NONE;
#else
		sinfo.sinfo_timetolive=cfg_get(sctp, sctp_cfg, send_ttl);
#endif
	sinfo.sinfo_context=cfg_get(sctp, sctp_cfg, send_retries);
	return sctp_msg_send_ext(dst, buf, len, &sinfo);
}



/** generic sctp info (basic stats).*/
void sctp_get_info(struct sctp_gen_info* i)
{
	if (i){
		i->sctp_connections_no=atomic_get(sctp_conn_no);
#ifdef SCTP_CONN_REUSE
		if (likely(cfg_get(sctp, sctp_cfg, assoc_tracking)))
			i->sctp_tracked_no=atomic_get(sctp_conn_tracked);
		else
			i->sctp_tracked_no=-1;
#else /* SCTP_CONN_REUSE */
		i->sctp_tracked_no=-1;
#endif /* SCTP_CONN_REUSE */
		i->sctp_total_connections=atomic_get(sctp_id);
	}
}


#endif /* USE_SCTP */
