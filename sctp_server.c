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


#include "sctp_server.h"
#include "sctp_options.h"
#include "globals.h"
#include "config.h"
#include "dprint.h"
#include "receive.h"
#include "mem/mem.h"
#include "ip_addr.h"
#include "cfg/cfg_struct.h"



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



/* set common (for one to many and one to one) sctp socket options
   tries to ignore non-critical errors (it will only log them), for
   improved portability (for example older linux kernel version support
   only a limited number of sctp socket options)
   returns 0 on success, -1 on error
   WARNING: please keep it sync'ed w/ sctp_check_compiled_sockopts() */
static int sctp_init_sock_opt_common(int s)
{
	struct sctp_event_subscribe es;
	int optval;
	socklen_t optlen;
	int sctp_err;
	
	sctp_err=0;
	/* set tos */
	optval = tos;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (void*)&optval,sizeof(optval)) ==-1){
		LOG(L_WARN, "WARNING: sctp_init_sock_opt_common: setsockopt tos: %s\n",
				strerror(errno));
		/* continue since this is not critical */
	}
	
	/* set receive buffer: SO_RCVBUF*/
	if (sctp_options.sctp_so_rcvbuf){
		optval=sctp_options.sctp_so_rcvbuf;
		if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
					(void*)&optval, sizeof(optval)) ==-1){
			LOG(L_ERR, "ERROR: sctp_init_sock_opt_common: setsockopt:"
						" SO_RCVBUF (%d): %s\n", optval, strerror(errno));
			/* continue, non-critical */
		}
	}
	
	/* set send buffer: SO_SNDBUF */
	if (sctp_options.sctp_so_sndbuf){
		optval=sctp_options.sctp_so_sndbuf;
		if (setsockopt(s, SOL_SOCKET, SO_SNDBUF,
					(void*)&optval, sizeof(optval)) ==-1){
			LOG(L_ERR, "ERROR: sctp_init_sock_opt_common: setsockopt:"
						" SO_SNDBUF (%d): %s\n", optval, strerror(errno));
			/* continue, non-critical */
		}
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
	if (setsockopt(s, IPPROTO_SCTP, SCTP_PARTIAL_DELIVERY_POINT,
					(void*)&optval, sizeof(optval)) ==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock_opt_common: setsockopt: "
						"SCTP_PARTIAL_DELIVERY_POINT (%d): %s\n",
						optval, strerror(errno));
		sctp_err++;
		/* try to continue */
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
	optval=sctp_options.sctp_autoclose;
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
	
	memset(&es, 0, sizeof(es));
	/* SCTP_EVENTS for SCTP_SNDRCV (sctp_data_io_event) -> per message
	 *  information in sctp_sndrcvinfo */
	es.sctp_data_io_event=1;
	/* enable association event notifications */
	es.sctp_association_event=1; /* SCTP_ASSOC_CHANGE */
	es.sctp_address_event=1;  /* enable address events notifications */
	es.sctp_send_failure_event=1; /* SCTP_SEND_FAILED */
	es.sctp_peer_error_event=1;   /* SCTP_REMOTE_ERROR */
	es.sctp_shutdown_event=1;     /* SCTP_SHUTDOWN_EVENT */
	es.sctp_partial_delivery_event=1; /* SCTP_PARTIAL_DELIVERY_EVENT */
	/* es.sctp_adaptation_layer_event=1; - not supported by lksctp<=1.0.6*/
	/* es.sctp_authentication_event=1; -- not supported on linux 2.6.25 */
	
	/* enable the SCTP_EVENTS */
#ifdef SCTP_EVENTS
	if (setsockopt(s, IPPROTO_SCTP, SCTP_EVENTS, &es, sizeof(es))==-1){
		LOG(L_ERR, "ERROR: sctp_init_sock_opt_common: setsockopt: "
				"SCTP_EVENTS: %s\n", strerror(errno));
		sctp_err++;
		/* non critical, try to continue */
	}
#else
#warning no sctp lib support for SCTP_EVENTS, consider upgrading
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
	#ifdef USE_IPV6
		if (addr->s.sa_family==AF_INET6)
			LOG(L_ERR, "ERROR: sctp_bind_sock: might be caused by using a "
							"link local address, try site local or global\n");
	#endif
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
		#ifdef USE_IPV6
			if (ai->su.s.sa_family==AF_INET6)
				LOG(L_ERR, "ERROR: sctp_bind_sock: might be caused by using a "
							"link local address, try site local or global\n");
		#endif
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
	if (sctp_init_sock_opt_common(sock_info->socket)!=0)
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
	if (sctp_init_sock_opt_common(sock_info->socket)!=0)
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



static int sctp_handle_notification(struct socket_info* si,
									union sockaddr_union* su,
									char* buf, unsigned len)
{
	union sctp_notification* snp;
	char su_buf[SU2A_MAX_STR_SIZE];
	
	DBG("sctp_rcv_loop: MSG_NOTIFICATION\n");
	
	#define SNOT DBG
	#define ERR_LEN_TOO_SMALL(length, val, bind_addr, from_su, text) \
		if (unlikely((length)<(val))){\
			SNOT("ERROR: sctp notification from %s on %.*s:%d: " \
						text " too short (%d bytes instead of %d bytes)\n", \
						su2a((from_su), sizeof(*(from_su))), \
						(bind_addr)->name.len, (bind_addr)->name.s, \
						(bind_addr)->port_no, (length), (val)); \
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
			SNOT("sctp notification from %s on %.*s:%d: SCTP_REMOTE_ERROR:"
					" %d, len %d\n, assoc. %d",
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
			SNOT("sctp notification from %s on %.*s:%d: SCTP_SEND_FAILED:"
					" error %d, assoc. %d, flags %x\n",
					su2a(su, sizeof(*su)), si->name.len, si->name.s,
					si->port_no, snp->sn_send_failed.ssf_error,
					snp->sn_send_failed.ssf_assoc_id,
					snp->sn_send_failed.ssf_flags);
			break;
		case SCTP_PEER_ADDR_CHANGE:
			ERR_LEN_TOO_SMALL(len, sizeof(struct sctp_paddr_change), si, su,
								"SCTP_PEER_ADDR_CHANGE");
			strcpy(su_buf, su2a((union sockaddr_union*)
									&snp->sn_paddr_change.spc_aaddr, 
									sizeof(snp->sn_paddr_change.spc_aaddr)));
			SNOT("sctp notification from %s on %.*s:%d: SCTP_PEER_ADDR_CHANGE"
					": %s: %s: assoc. %d \n",
					su2a(su, sizeof(*su)), si->name.len, si->name.s,
					si->port_no, su_buf,
					sctp_paddr_change_state2s(snp->sn_paddr_change.spc_state),
					snp->sn_paddr_change.spc_assoc_id
					);
			break;
		case SCTP_SHUTDOWN_EVENT:
			ERR_LEN_TOO_SMALL(len, sizeof(struct sctp_shutdown_event), si, su,
								"SCTP_SHUTDOWN_EVENT");
			SNOT("sctp notification from %s on %.*s:%d: SCTP_SHUTDOWN_EVENT:"
					" assoc. %d\n",
					su2a(su, sizeof(*su)), si->name.len, si->name.s,
					si->port_no, snp->sn_shutdown_event.sse_assoc_id);
			break;
		case SCTP_ASSOC_CHANGE:
			ERR_LEN_TOO_SMALL(len, sizeof(struct sctp_assoc_change), si, su,
								"SCTP_ASSOC_CHANGE");
			SNOT("sctp notification from %s on %.*s:%d: SCTP_ASSOC_CHANGE"
					": %s: assoc. %d, ostreams %d, istreams %d\n",
					su2a(su, sizeof(*su)), si->name.len, si->name.s,
					si->port_no,
					sctp_assoc_change_state2s(snp->sn_assoc_change.sac_state),
					snp->sn_assoc_change.sac_assoc_id,
					snp->sn_assoc_change.sac_outbound_streams,
					snp->sn_assoc_change.sac_inbound_streams
					);
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
			SNOT("sctp notification from %s on %.*s:%d: "
					"SCTP_PARTIAL_DELIVERY_EVENT: %d%s, assoc. %d\n",
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
	ri.proto_reserved1=ri.proto_reserved2=0;
	
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

		len=recvmsg(bind_address->socket, &msg, 0);
		/* len=sctp_recvmsg(bind_address->socket, buf, BUF_SIZE, &ri.src_su.s,
							&msg.msg_namelen, &sinfo, &msg.msg_flags); */
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
		if (unlikely(msg.msg_flags & MSG_NOTIFICATION)){
			/* intercept usefull notifications */
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
			if (likely((cmsg->cmsg_level==IPPROTO_SCTP) &&
						((cmsg->cmsg_type==SCTP_SNDRCV)
#ifdef SCTP_EXT
						 || (cmsg->cmsg_type==SCTP_EXTRCV)
#endif
						) && (cmsg->cmsg_len>=CMSG_LEN(sizeof(*sinfo)))) ){
				sinfo=(struct sctp_sndrcvinfo*)CMSG_DATA(cmsg);
				DBG("sctp recv: message from %s:%d stream %d  ppid %x"
						" flags %x%s tsn %u" " cumtsn %u associd %d\n",
						ip_addr2a(&ri.src_ip), htons(ri.src_port),
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
			DBG("sctp_rcv_loop: probing packet received from %s %d\n",
					tmp, htons(ri.src_port));
			continue;
		}
		if (ri.src_port==0){
			tmp=ip_addr2a(&ri.src_ip);
			LOG(L_INFO, "sctp_rcv_loop: dropping 0 port packet from %s\n",
						tmp);
			continue;
		}
#ifdef USE_COMP
		ri.comp=COMP_NONE;
#endif
		/* update the local config */
		cfg_update();
		receive_msg(buf, len, &ri);
	}
error:
	return -1;
}


/* send buf:len over udp to dst (uses only the to and send_sock dst members)
 * returns the numbers of bytes sent on success (>=0) and -1 on error
 */
int sctp_msg_send(struct dest_info* dst, char* buf, unsigned len)
{
	int n;
	int tolen;
	struct ip_addr ip; /* used only on error, for debugging */
	struct msghdr msg;
	struct iovec iov[1];
	
	tolen=sockaddru_len(dst->to);
	iov[0].iov_base=buf;
	iov[0].iov_len=len;
	msg.msg_iov=iov;
	msg.msg_iovlen=1;
	msg.msg_name=&dst->to.s;
	msg.msg_namelen=tolen;
	msg.msg_control=0;
	msg.msg_controllen=0;
	msg.msg_flags=SCTP_UNORDERED;
again:
	n=sendmsg(dst->send_sock->socket, &msg, MSG_DONTWAIT);
#if 0
	n=sctp_sendmsg(dst->send_sock->socket, buf, len, &dst->to.s, tolen,
					0 /* ppid */, SCTP_UNORDERED /* | SCTP_EOR */ /* flags */,
					0 /* stream */, sctp_options.sctp_send_ttl /* ttl */,
					0 /* context */);
#endif
	if (n==-1){
		su2ip_addr(&ip, &dst->to);
		LOG(L_ERR, "ERROR: sctp_msg_send: sendmsg(sock,%p,%d,0,%s:%d,%d):"
				" %s(%d)\n", buf, len, ip_addr2a(&ip), su_getport(&dst->to),
				tolen, strerror(errno),errno);
		if (errno==EINTR) goto again;
		if (errno==EINVAL) {
			LOG(L_CRIT,"CRITICAL: invalid sendmsg parameters\n"
			"one possible reason is the server is bound to localhost and\n"
			"attempts to send to the net\n");
		}else if (errno==EAGAIN || errno==EWOULDBLOCK){
			LOG(L_ERR, "ERROR: sctp_msg_send: failed to send, send buffers"
						" full\n");
			/* TODO: fix blocking writes */
		}
	}
	return n;
}



void destroy_sctp()
{
}

#endif /* USE_SCTP */
