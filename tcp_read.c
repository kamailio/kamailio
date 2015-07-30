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

/** Kamailio core :: tcp readers processes, tcp read and pre-parse msg. functions.
 * @file tcp_read.c
 * @ingroup core
 * Module: @ref core
 */

#ifdef USE_TCP

#include <stdio.h>
#include <errno.h>
#include <string.h>


#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <unistd.h>
#include <stdlib.h> /* for abort() */


#include "dprint.h"
#include "tcp_conn.h"
#include "tcp_read.h"
#include "tcp_stats.h"
#include "tcp_ev.h"
#include "pass_fd.h"
#include "globals.h"
#include "receive.h"
#include "timer.h"
#include "local_timer.h"
#include "ut.h"
#include "trim.h"
#include "pt.h"
#include "cfg/cfg_struct.h"
#ifdef CORE_TLS
#include "tls/tls_server.h"
#else
#include "tls_hooks.h"
#endif /* CORE_TLS */
#ifdef USE_DST_BLACKLIST
#include "dst_blacklist.h"
#endif /* USE_DST_BLACKLIST */

#define HANDLE_IO_INLINE
#include "io_wait.h"
#include <fcntl.h> /* must be included after io_wait.h if SIGIO_RT is used */
#include "tsend.h"
#include "forward.h"
#include "events.h"
#include "stun.h"

#ifdef READ_HTTP11
#define HTTP11CONTINUE	"HTTP/1.1 100 Continue\r\nContent-Length: 0\r\n\r\n"
#define HTTP11CONTINUE_LEN	(sizeof(HTTP11CONTINUE)-1)
#endif

#define TCPCONN_TIMEOUT_MIN_RUN  1 /* run the timers each new tick */

/* types used in io_wait* */
enum fd_types { F_NONE, F_TCPMAIN, F_TCPCONN };

/* list of tcp connections handled by this process */
static struct tcp_connection* tcp_conn_lst=0;
static io_wait_h io_w; /* io_wait handler*/
static int tcpmain_sock=-1;

static struct local_timer tcp_reader_ltimer;
static ticks_t tcp_reader_prev_ticks;

int is_msg_complete(struct tcp_req* r);

/**
 * control cloning of TCP receive buffer
 * - needed for operations working directly inside the buffer
 *   (like msg_apply_changes())
 */
#define TCP_CLONE_RCVBUF
static int tcp_clone_rcvbuf = 0;

int tcp_set_clone_rcvbuf(int v)
{
	int r;
	r = tcp_clone_rcvbuf;
	tcp_clone_rcvbuf = v;
	return r;
}

#ifdef READ_HTTP11
static inline char *strfindcasestrz(str *haystack, char *needlez)
{
	int i,j;
	str needle;

	needle.s = needlez;
	needle.len = strlen(needlez);
	for(i=0;i<haystack->len-needle.len;i++) {
		for(j=0;j<needle.len;j++) {
			if ( !((haystack->s[i+j]==needle.s[j]) ||
					( isalpha((int)haystack->s[i+j])
						&& ((haystack->s[i+j])^(needle.s[j]))==0x20 )) )
				break;
		}
		if (j==needle.len)
			return haystack->s+i;
	}
	return 0;
}

int tcp_http11_continue(struct tcp_connection *c)
{
	struct dest_info dst;
	char *p;
	struct msg_start fline;
	int ret;
	str msg;

	ret = 0;

	msg.s = c->req.start;
	msg.len = c->req.pos - c->req.start;
#ifdef READ_MSRP
	/* skip if MSRP message */
	if(c->req.flags&F_TCP_REQ_MSRP_FRAME)
		return 0;
#endif
	p = parse_first_line(msg.s, msg.len, &fline);
	if(p==NULL)
		return 0;

	if(fline.type!=SIP_REQUEST)
		return 0;

	/* check if http request */
	if(fline.u.request.version.len < HTTP_VERSION_LEN
			|| strncasecmp(fline.u.request.version.s,
				HTTP_VERSION, HTTP_VERSION_LEN))
		return 0;

	/* check for Expect header */
	if(strfindcasestrz(&msg, "Expect: 100-continue")!=NULL)
	{
		init_dst_from_rcv(&dst, &c->rcv);
		if (tcp_send(&dst, 0, HTTP11CONTINUE, HTTP11CONTINUE_LEN) < 0) {
			LM_ERR("HTTP/1.1 continue failed\n");
		}
	}
	/* check for Transfer-Encoding header */
	if(strfindcasestrz(&msg, "Transfer-Encoding: chunked")!=NULL)
	{
		c->req.flags |= F_TCP_REQ_BCHUNKED;
		ret = 1;
	}
	/* check for HTTP Via header
	 * - HTTP Via format is different that SIP Via
	 * - workaround: replace with Hia to be ignored by SIP parser
	 */
	if((p=strfindcasestrz(&msg, "\nVia:"))!=NULL)
	{
		p++;
		*p = 'H';
		LM_DBG("replaced HTTP Via with Hia [[\n%.*s]]\n", msg.len, msg.s);
	}
	return ret;
}
#endif /* HTTP11 */


/** reads data from an existing tcp connection.
 * Side-effects: blacklisting, sets connection state to S_CONN_OK, tcp stats.
 * @param fd - connection file descriptor
 * @param c - tcp connection structure. c->state might be changed and
 *             receive info might be used for blacklisting.
 * @param buf - buffer where the received data will be stored.
 * @param b_size - buffer size.
 * @param flags - value/result - used to signal a seen or "forced" EOF on the
 *     connection (when it is known that no more data will come after the 
 *     current socket buffer is emptied )=> return/signal EOF on the first
 *     short read (=> don't use it on POLLPRI, as OOB data will cause short
 *     reads even if there are still remaining bytes in the socket buffer)
 *     input: RD_CONN_FORCE_EOF  - force EOF after the first successful read
 *                                 (bytes_read >=0 )
 *     output: RD_CONN_SHORT_READ - if the read exhausted all the bytes
 *                                  in the socket read buffer.
 *             RD_CONN_EOF - if EOF detected (0 bytes read) or forced via
 *                           RD_CONN_FORCE_EOF.
 *             RD_CONN_REPEAT_READ - the read should be repeated immediately
 *                                   (used only by the tls code for now).
 *     Note: RD_CONN_SHORT_READ & RD_CONN_EOF _are_ not cleared internally,
 *           so one should clear them before calling this function.
 * @return number of bytes read, 0 on EOF or -1 on error,
 * on EOF it also sets c->state to S_CONN_EOF.
 * (to distinguish from reads that would block which could return 0)
 * RD_CONN_SHORT_READ is also set in *flags for short reads.
 * EOF checking should be done by checking the RD_CONN_EOF flag.
 */
int tcp_read_data(int fd, struct tcp_connection *c,
					char* buf, int b_size, int* flags)
{
	int bytes_read;
	
again:
	bytes_read=read(fd, buf, b_size);
	
	if (likely(bytes_read!=b_size)){
		if(unlikely(bytes_read==-1)){
			if (errno == EWOULDBLOCK || errno == EAGAIN){
				bytes_read=0; /* nothing has been read */
			}else if (errno == EINTR) goto again;
			else{
				if (unlikely(c->state==S_CONN_CONNECT)){
					switch(errno){
						case ECONNRESET:
#ifdef USE_DST_BLACKLIST
							dst_blacklist_su(BLST_ERR_CONNECT, c->rcv.proto,
												&c->rcv.src_su,
												&c->send_flags, 0);
#endif /* USE_DST_BLACKLIST */
							TCP_EV_CONNECT_RST(errno, TCP_LADDR(c),
									TCP_LPORT(c), TCP_PSU(c), TCP_PROTO(c));
							break;
						case ETIMEDOUT:
#ifdef USE_DST_BLACKLIST
							dst_blacklist_su(BLST_ERR_CONNECT, c->rcv.proto,
												&c->rcv.src_su,
												&c->send_flags, 0);
#endif /* USE_DST_BLACKLIST */
							TCP_EV_CONNECT_TIMEOUT(errno, TCP_LADDR(c),
									TCP_LPORT(c), TCP_PSU(c), TCP_PROTO(c));
							break;
						default:
							TCP_EV_CONNECT_ERR(errno, TCP_LADDR(c),
									TCP_LPORT(c), TCP_PSU(c), TCP_PROTO(c));
					}
					TCP_STATS_CONNECT_FAILED();
				}else{
						switch(errno){
							case ECONNRESET:
								TCP_STATS_CON_RESET();
							case ETIMEDOUT:
#ifdef USE_DST_BLACKLIST
								dst_blacklist_su(BLST_ERR_SEND, c->rcv.proto,
													&c->rcv.src_su,
													&c->send_flags, 0);
#endif /* USE_DST_BLACKLIST */
								break;
						}
				}
				LOG(cfg_get(core, core_cfg, corelog),
						"error reading: %s (%d)\n", strerror(errno), errno);
				return -1;
			}
		}else if (unlikely((bytes_read==0) || 
					(*flags & RD_CONN_FORCE_EOF))){
			c->state=S_CONN_EOF;
			*flags|=RD_CONN_EOF;
			LM_DBG("EOF on %p, FD %d\n", c, fd);
		}else{
			if (unlikely(c->state==S_CONN_CONNECT || c->state==S_CONN_ACCEPT)){
				TCP_STATS_ESTABLISHED(c->state);
				c->state=S_CONN_OK;
			}
		}
		/* short read */
		*flags|=RD_CONN_SHORT_READ;
	}else{ /* else normal full read */
		if (unlikely(c->state==S_CONN_CONNECT || c->state==S_CONN_ACCEPT)){
			TCP_STATS_ESTABLISHED(c->state);
			c->state=S_CONN_OK;
		}
	}
	return bytes_read;
}



/* reads next available bytes
 *   c- tcp connection used for reading, tcp_read changes also c->state on
 *      EOF and c->req.error on read error
 *   * flags - value/result - used to signal a seen or "forced" EOF on the 
 *     connection (when it is known that no more data will come after the 
 *     current socket buffer is emptied )=> return/signal EOF on the first 
 *     short read (=> don't use it on POLLPRI, as OOB data will cause short
 *      reads even if there are still remaining bytes in the socket buffer)
 * return number of bytes read, 0 on EOF or -1 on error,
 * on EOF it also sets c->state to S_CONN_EOF.
 * (to distinguish from reads that would block which could return 0)
 * RD_CONN_SHORT_READ is also set in *flags for short reads.
 * sets also r->error */
int tcp_read(struct tcp_connection *c, int* flags)
{
	int bytes_free, bytes_read;
	struct tcp_req *r;
	int fd;

	r=&c->req;
	fd=c->fd;
	bytes_free=r->b_size- (int)(r->pos - r->buf);
	
	if (unlikely(bytes_free==0)){
		LM_ERR("buffer overrun, dropping\n");
		r->error=TCP_REQ_OVERRUN;
		return -1;
	}
	bytes_read = tcp_read_data(fd, c, r->pos, bytes_free, flags);
	if (unlikely(bytes_read < 0)){
		r->error=TCP_READ_ERROR;
		return -1;
	}
#ifdef EXTRA_DEBUG
	LM_DBG("read %d bytes:\n%.*s\n", bytes_read, bytes_read, r->pos);
#endif
	r->pos+=bytes_read;
	return bytes_read;
}



/* reads all headers (until double crlf), & parses the content-length header
 * (WARNING: inefficient, tries to reuse receive_msg but will go through
 * the headers twice [once here looking for Content-Length and for the end
 * of the headers and once in receive_msg]; a more speed efficient version will
 * result in either major code duplication or major changes to the receive code)
 * returns number of bytes read & sets r->state & r->body
 * when either r->body!=0 or r->state==H_BODY =>
 * all headers have been read. It should be called in a while loop.
 * returns < 0 if error or 0 if EOF */
int tcp_read_headers(struct tcp_connection *c, int* read_flags)
{
	int bytes, remaining;
	char *p;
	struct tcp_req* r;
	unsigned int mc;   /* magic cookie */
	unsigned short body_len;

#ifdef READ_MSRP
	char *mfline;
	str mtransid;
#endif

	#define crlf_default_skip_case \
					case '\n': \
						r->state=H_LF; \
						break; \
					default: \
						r->state=H_SKIP
	
	#define content_len_beg_case \
					case ' ': \
					case '\t': \
						if (!TCP_REQ_HAS_CLEN(r)) r->state=H_STARTWS; \
						else r->state=H_SKIP; \
							/* not interested if we already found one */ \
						break; \
					case 'C': \
					case 'c': \
						if(!TCP_REQ_HAS_CLEN(r)) r->state=H_CONT_LEN1; \
						else r->state=H_SKIP; \
						break; \
					case 'l': \
					case 'L': \
						/* short form for Content-Length */ \
						if (!TCP_REQ_HAS_CLEN(r)) r->state=H_L_COLON; \
						else r->state=H_SKIP; \
						break
						
	#define change_state(upper, lower, newstate)\
					switch(*p){ \
						case upper: \
						case lower: \
							r->state=(newstate); break; \
						crlf_default_skip_case; \
					}
	
	#define change_state_case(state0, upper, lower, newstate)\
					case state0: \
							  change_state(upper, lower, newstate); \
							  p++; \
							  break


	r=&c->req;
	/* if we still have some unparsed part, parse it first, don't do the read*/
	if (unlikely(r->parsed<r->pos)){
		bytes=0;
	}else{
#ifdef USE_TLS
		if (unlikely(c->type==PROTO_TLS))
			bytes=tls_read(c, read_flags);
		else
#endif
			bytes=tcp_read(c, read_flags);
		if (bytes<=0) return bytes;
	}
	p=r->parsed;

	while(p<r->pos && r->error==TCP_REQ_OK){
		switch((unsigned char)r->state){
			case H_BODY: /* read the body*/
				remaining=r->pos-p;
				if (remaining>r->bytes_to_go) remaining=r->bytes_to_go;
				r->bytes_to_go-=remaining;
				p+=remaining;
				if (r->bytes_to_go==0){
					r->flags|=F_TCP_REQ_COMPLETE;
					goto skip;
				}
				break;
				
			case H_SKIP:
				/* find lf, we are in this state if we are not interested
				 * in anything till end of line*/
				p=q_memchr(p, '\n', r->pos-p);
				if (p){
#ifdef READ_MSRP
					/* catch if it is MSRP or not with first '\n' */
					if(!((r->flags&F_TCP_REQ_MSRP_NO)
								|| (r->flags&F_TCP_REQ_MSRP_FRAME))) {
						if((r->pos - r->start)>5
									&& strncmp(r->start, "MSRP ", 5)==0)
						{
							r->flags |= F_TCP_REQ_MSRP_FRAME;
						} else {
							r->flags |= F_TCP_REQ_MSRP_NO;
						}
					}
#endif
					p++;
					r->state=H_LF;
				}else{
					p=r->pos;
				}
				break;
				
			case H_LF:
				/* terminate on LF CR LF or LF LF */
				switch (*p){
					case '\r':
						r->state=H_LFCR;
						break;
					case '\n':
						/* found LF LF */
						r->state=H_BODY;
#ifdef READ_HTTP11
						if (cfg_get(tcp, tcp_cfg, accept_no_cl)!=0)
							tcp_http11_continue(c);
#endif
						if (TCP_REQ_HAS_CLEN(r)){
							r->body=p+1;
							r->bytes_to_go=r->content_len;
							if (r->bytes_to_go==0){
								r->flags|=F_TCP_REQ_COMPLETE;
								p++;
								goto skip;
							}
						}else{
							if(cfg_get(tcp, tcp_cfg, accept_no_cl)!=0) {
#ifdef READ_MSRP
								/* if MSRP message */
								if(c->req.flags&F_TCP_REQ_MSRP_FRAME)
								{
									r->body=p+1;
									/* at least 3 bytes: 0\r\n */
									r->bytes_to_go=3;
									p++;
									r->content_len = 0;
									r->state=H_MSRP_BODY;
									break;
								}
#endif

#ifdef READ_HTTP11
								if(TCP_REQ_BCHUNKED(r)) {
									r->body=p+1;
									/* at least 3 bytes: 0\r\n */
									r->bytes_to_go=3;
									p++;
									r->content_len = 0;
									r->state=H_HTTP11_CHUNK_START;
									break;
								}
#endif
								r->body=p+1;
								r->bytes_to_go=0;
								r->flags|=F_TCP_REQ_COMPLETE;
								p++;
								goto skip;
							} else {
								LM_DBG("ERROR: no clen, p=%X\n", *p);
								r->error=TCP_REQ_BAD_LEN;
							}
						}
						break;
					case '-':
						r->state=H_SKIP;
#ifdef READ_MSRP
						/* catch end of MSRP frame without body
						 *     '-------sessid$\r\n'
						 * follows headers wihtout extra CRLF */
						if(r->flags&F_TCP_REQ_MSRP_FRAME) {
							p--;
							r->state=H_MSRP_BODY_END;
						}
#endif
						break;
					content_len_beg_case;
					default: 
						r->state=H_SKIP;
				}
				p++;
				break;
			case H_LFCR:
				if (*p=='\n'){
					/* found LF CR LF */
					r->state=H_BODY;
#ifdef READ_HTTP11
					if (cfg_get(tcp, tcp_cfg, accept_no_cl)!=0)
						tcp_http11_continue(c);
#endif
					if (TCP_REQ_HAS_CLEN(r)){
						r->body=p+1;
						r->bytes_to_go=r->content_len;
						if (r->bytes_to_go==0){
							r->flags|=F_TCP_REQ_COMPLETE;
							p++;
							goto skip;
						}
					}else{
						if (cfg_get(tcp, tcp_cfg, accept_no_cl)!=0) {
#ifdef READ_MSRP
							/* if MSRP message */
							if(c->req.flags&F_TCP_REQ_MSRP_FRAME)
							{
								r->body=p+1;
								/* at least 3 bytes: 0\r\n */
								r->bytes_to_go=3;
								p++;
								r->content_len = 0;
								r->state=H_MSRP_BODY;
								break;
							}
#endif

#ifdef READ_HTTP11
							if(TCP_REQ_BCHUNKED(r)) {
								r->body=p+1;
								/* at least 3 bytes: 0\r\n */
								r->bytes_to_go=3;
								p++;
								r->content_len = 0;
								r->state=H_HTTP11_CHUNK_START;
								break;
							}
#endif
							r->body=p+1;
							r->bytes_to_go=0;
							r->flags|=F_TCP_REQ_COMPLETE;
							p++;
							goto skip;
						} else {
							LM_DBG("ERROR: no clen, p=%X\n", *p);
							r->error=TCP_REQ_BAD_LEN;
						}
					}
				}else r->state=H_SKIP;
				p++;
				break;
				
			case H_STARTWS:
				switch (*p){
					content_len_beg_case;
					crlf_default_skip_case;
				}
				p++;
				break;
			case H_SKIP_EMPTY:
				switch (*p){
					case '\n':
						break;
					case '\r':
						if (cfg_get(tcp, tcp_cfg, crlf_ping)) {
							r->state=H_SKIP_EMPTY_CR_FOUND;
							r->start=p;
						}
						break;
					case ' ':
					case '\t':
						/* skip empty lines */
						break;
					case 'C': 
					case 'c': 
						r->state=H_CONT_LEN1; 
						r->start=p;
						break;
					case 'l':
					case 'L':
						/* short form for Content-Length */
						r->state=H_L_COLON;
						r->start=p;
						break;
					default:
						/* stun test */						
						if (unlikely(sr_event_enabled(SREV_STUN_IN)) && (unsigned char)*p == 0x00) {
							r->state=H_STUN_MSG;
						/* body will used as pointer to the last used byte */
							r->body=p;
							r->content_len = 0;
							LM_DBG("stun msg detected\n");
						} else {
							r->state=H_SKIP;
						}
						r->start=p;
				};
				p++;
				break;

			case H_SKIP_EMPTY_CR_FOUND:
				if (*p=='\n'){
					r->state=H_SKIP_EMPTY_CRLF_FOUND;
					p++;
				}else{
					r->state=H_SKIP_EMPTY;
				}
				break;

			case H_SKIP_EMPTY_CRLF_FOUND:
				if (*p=='\r'){
					r->state = H_SKIP_EMPTY_CRLFCR_FOUND;
					p++;
				}else{
					r->state = H_SKIP_EMPTY;
				}
				break;

			case H_SKIP_EMPTY_CRLFCR_FOUND:
				if (*p=='\n'){
					r->state = H_PING_CRLF;
					r->flags |= F_TCP_REQ_HAS_CLEN |
							F_TCP_REQ_COMPLETE; /* hack to avoid error check */
					p++;
					goto skip;
				}else{
					r->state = H_SKIP_EMPTY;
				}
				break;

			case H_STUN_MSG:
				if ((r->pos - r->body) >= sizeof(struct stun_hdr)) {
					/* copy second short from buffer where should be body 
					 * length 
					 */
					memcpy(&body_len, &r->start[sizeof(unsigned short)], 
						sizeof(unsigned short));
					
					body_len = ntohs(body_len);
					
					/* check if there is valid magic cookie */
					memcpy(&mc, &r->start[sizeof(unsigned int)], 
						sizeof(unsigned int));
					mc = ntohl(mc);
					/* using has_content_len as a flag if there should be
					 * fingerprint or no
					 */
					r->flags |= (mc == MAGIC_COOKIE) ? F_TCP_REQ_HAS_CLEN : 0;
					
					r->body += sizeof(struct stun_hdr);
					p = r->body; 
					
					if (body_len > 0) {
						r->state = H_STUN_READ_BODY;
					}
					else {
						if (is_msg_complete(r) != 0) {
							goto skip;
						}
						else {
							/* set content_len to length of fingerprint */
							body_len = sizeof(struct stun_attr) + 20;
							/* 20 is SHA_DIGEST_LENGTH from openssl/sha.h */
						}
					}
					r->content_len=body_len;
				}
				else {
					p = r->pos; 
				}
				break;
				
			case H_STUN_READ_BODY:
				/* check if the whole body was read */
				body_len=r->content_len;
				if ((r->pos - r->body) >= body_len) {
					r->body += body_len;
					p = r->body;
					if (is_msg_complete(r) != 0) {
						r->content_len=0;
						goto skip;
					}
					else {
						/* set content_len to length of fingerprint */
						body_len = sizeof(struct stun_attr) + 20;
						/* 20 is SHA_DIGEST_LENGTH from openssl/sha.h */
						r->content_len=body_len;
					}
				}
				else {
					p = r->pos;
				}
				break;
				
			case H_STUN_FP:
				/* content_len contains length of fingerprint in this place! */
				body_len=r->content_len;
				if ((r->pos - r->body) >= body_len) {
					r->body += body_len;
					p = r->body;
					r->state = H_STUN_END;
					r->flags |= F_TCP_REQ_COMPLETE |
						F_TCP_REQ_HAS_CLEN; /* hack to avoid error check */
					r->content_len=0;
					goto skip;
				}
				else {
					p = r->pos;
				}
				break;

			change_state_case(H_CONT_LEN1,  'O', 'o', H_CONT_LEN2);
			change_state_case(H_CONT_LEN2,  'N', 'n', H_CONT_LEN3);
			change_state_case(H_CONT_LEN3,  'T', 't', H_CONT_LEN4);
			change_state_case(H_CONT_LEN4,  'E', 'e', H_CONT_LEN5);
			change_state_case(H_CONT_LEN5,  'N', 'n', H_CONT_LEN6);
			change_state_case(H_CONT_LEN6,  'T', 't', H_CONT_LEN7);
			change_state_case(H_CONT_LEN7,  '-', '_', H_CONT_LEN8);
			change_state_case(H_CONT_LEN8,  'L', 'l', H_CONT_LEN9);
			change_state_case(H_CONT_LEN9,  'E', 'e', H_CONT_LEN10);
			change_state_case(H_CONT_LEN10, 'N', 'n', H_CONT_LEN11);
			change_state_case(H_CONT_LEN11, 'G', 'g', H_CONT_LEN12);
			change_state_case(H_CONT_LEN12, 'T', 't', H_CONT_LEN13);
			change_state_case(H_CONT_LEN13, 'H', 'h', H_L_COLON);

			case H_L_COLON:
				switch(*p){
					case ' ':
					case '\t':
						break; /* skip space */
					case ':':
						r->state=H_CONT_LEN_BODY;
						break;
					crlf_default_skip_case;
				};
				p++;
				break;

			case  H_CONT_LEN_BODY:
				switch(*p){
					case ' ':
					case '\t':
						break; /* eat space */
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						r->state=H_CONT_LEN_BODY_PARSE;
						r->content_len=(*p-'0');
						break;
					/*FIXME: content length on different lines ! */
					crlf_default_skip_case;
				}
				p++;
				break;

			case H_CONT_LEN_BODY_PARSE:
				switch(*p){
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						r->content_len=r->content_len*10+(*p-'0');
						break;
					case '\r':
					case ' ':
					case '\t': /* FIXME: check if line contains only WS */
						if(r->content_len<0) {
							LM_ERR("bad Content-Length header value %d in"
									" state %d\n", r->content_len, r->state);
							r->content_len=0;
							r->error=TCP_REQ_BAD_LEN;
							r->state=H_SKIP; /* skip now */
						}
						r->state=H_SKIP;
						r->flags|=F_TCP_REQ_HAS_CLEN;
						break;
					case '\n':
						/* end of line, parse successful */
						if(r->content_len<0) {
							LM_ERR("bad Content-Length header value %d in"
									" state %d\n", r->content_len, r->state);
							r->content_len=0;
							r->error=TCP_REQ_BAD_LEN;
							r->state=H_SKIP; /* skip now */
						}
						r->state=H_LF;
						r->flags|=F_TCP_REQ_HAS_CLEN;
						break;
					default:
						LM_ERR("bad Content-Length header value, unexpected "
								"char %c in state %d\n", *p, r->state);
						r->state=H_SKIP; /* try to find another?*/
				}
				p++;
				break;
			
#ifdef READ_HTTP11
			case H_HTTP11_CHUNK_START: /* start a new body chunk: SIZE\r\nBODY\r\n */
				r->chunk_size = 0;
				r->state = H_HTTP11_CHUNK_SIZE;
				break;
			case H_HTTP11_CHUNK_BODY: /* content of chunnk */
				remaining=r->pos-p;
				if (remaining>r->bytes_to_go) remaining=r->bytes_to_go;
				r->bytes_to_go-=remaining;
				p+=remaining;
				if (r->bytes_to_go==0){
					r->state = H_HTTP11_CHUNK_END;
					/* shift back body content */
					if(r->chunk_size>0 && p-r->chunk_size>r->body) {
						memmove(r->body + r->content_len, p - r->chunk_size,
								r->chunk_size);
						r->content_len += r->chunk_size;
					}
					goto skip;
				}
				break;

			case H_HTTP11_CHUNK_END:
				switch(*p){
					case '\r':
					case ' ':
					case '\t': /* skip */
						break;
					case '\n':
						r->state = H_HTTP11_CHUNK_START;
						break;
					default:
						LM_ERR("bad chunk, unexpected "
								"char %c in state %d\n", *p, r->state);
						r->state=H_SKIP; /* try to find another?*/
				}
				p++;
				break;

			case H_HTTP11_CHUNK_SIZE:
				switch(*p){
					case '0': case '1': case '2': case '3':
					case '4': case '5': case '6': case '7':
					case '8': case '9':
						r->chunk_size <<= 4;
						r->chunk_size += *p - '0';
						break;
					case 'a': case 'b': case 'c': case 'd':
					case 'e': case 'f':
						r->chunk_size <<= 4;
						r->chunk_size += *p - 'a' + 10;
						break;
					case 'A': case 'B': case 'C': case 'D':
					case 'E': case 'F':
						r->chunk_size <<= 4;
						r->chunk_size += *p - 'A' + 10;
						break;
					case '\r':
					case ' ':
					case '\t': /* skip */
						break;
					case '\n':
						/* end of line, parse successful */
						r->state=H_HTTP11_CHUNK_BODY;
						r->bytes_to_go = r->chunk_size;
						if (r->bytes_to_go==0){
							r->state=H_HTTP11_CHUNK_FINISH;
							r->flags|=F_TCP_REQ_COMPLETE;
							p++;
							goto skip;
						}
						break;
					default:
						LM_ERR("bad chunk size value, unexpected "
								"char %c in state %d\n", *p, r->state);
						r->state=H_SKIP; /* try to find another?*/
				}
				p++;
				break;
#endif
#ifdef READ_MSRP
			case H_MSRP_BODY: /* body of msrp frame */
				/* find lf, we are in this state if we are not interested
				 * in anything till end of line*/
				r->flags |= F_TCP_REQ_MSRP_BODY;
				p = q_memchr(p, '\n', r->pos-p);
				if (p) {
					p++;
					r->state=H_MSRP_BODY_LF;
				} else {
					p=r->pos;
				}
				break;
			case H_MSRP_BODY_LF: /* LF in body of msrp frame */
				switch (*p) {
					case '-':
							p--;
							r->state=H_MSRP_BODY_END;
						break;
					default:
						r->state=H_MSRP_BODY;
				}
				p++;
				break;
			case H_MSRP_BODY_END: /* end of body for msrp frame */
				/* find LF and check if it is end-line */
				p = q_memchr(p, '\n', r->pos-p);
				if (p) {
					/* check if it is end line '-------sessid$\r\n' */
					if(r->pos - r->start < 10) {
						LM_ERR("weird situation when reading MSRP frame"
								" - continue reading\n");
						/* *p=='\n' */
						r->state=H_MSRP_BODY_LF;
						p++;
						break;
					}
					if(*(p-1)!='\r') {
						/* not ending in '\r\n' - not end-line */
						/* *p=='\n' */
						r->state=H_MSRP_BODY_LF;
						p++;
						break;
					}
					/* locate transaction id in first line
					 * -- first line exists, that's why we are here */
					mfline =  q_memchr(r->start, '\n', r->pos-r->start);
					mtransid.s = q_memchr(r->start + 5 /* 'MSRP ' */, ' ',
							mfline - r->start);
					mtransid.len = mtransid.s - r->start - 5;
					mtransid.s = r->start + 5;
					trim(&mtransid);
					if(memcmp(mtransid.s,
							p - 1 /*\r*/ - 1 /* '+'|'#'|'$' */ - mtransid.len,
							mtransid.len)!=0) {
						/* no match on session id - not end-line */
						/* *p=='\n' */
						r->state=H_MSRP_BODY_LF;
						p++;
						break;
					}
					if(memcmp(p - 1 /*\r*/ - 1 /* '+'|'#'|'$' */ - mtransid.len
								- 7 /* 7 x '-' */ - 1 /* '\n' */, "\n-------",
								8)!=0) {
						/* no match on "\n-------" - not end-line */
						/* *p=='\n' */
						r->state=H_MSRP_BODY_LF;
						p++;
						break;
					}
					r->state=H_MSRP_FINISH;
					r->flags|=F_TCP_REQ_COMPLETE;
					p++;
					goto skip;

				} else {
					p=r->pos;
				}
				break;
#endif

			default:
				LM_CRIT("unexpected state %d\n", r->state);
				abort();
		}
	}
skip:
	r->parsed=p;
	return bytes;
}


#ifdef READ_MSRP
int msrp_process_msg(char* tcpbuf, unsigned int len,
		struct receive_info* rcv_info, struct tcp_connection* con)
{
	int ret;
	tcp_event_info_t tev;

	ret = 0;
	LM_DBG("MSRP Message: [[>>>\n%.*s<<<]]\n", len, tcpbuf);
	if(likely(sr_event_enabled(SREV_TCP_MSRP_FRAME))) {
		memset(&tev, 0, sizeof(tcp_event_info_t));
		tev.type = SREV_TCP_MSRP_FRAME;
		tev.buf = tcpbuf;
		tev.len = len;
		tev.rcv = rcv_info;
		tev.con = con;
		ret = sr_event_exec(SREV_TCP_MSRP_FRAME, (void*)(&tev));
	} else {
		LM_DBG("no callback registering for handling MSRP - dropping!\n");
	}
	return ret;
}
#endif

#ifdef READ_WS
static int tcp_read_ws(struct tcp_connection *c, int* read_flags)
{
	int bytes, size, pos, mask_present;
	unsigned int len;
	char *p;
	struct tcp_req *r;

	r=&c->req;
#ifdef USE_TLS
	if (unlikely(c->type == PROTO_WSS))
		bytes = tls_read(c, read_flags);
	else
#endif
		bytes = tcp_read(c, read_flags);

	if (bytes <= 0)
	{
		if (likely(r->parsed >= r->pos))
			return 0;
	}

	size = r->pos - r->parsed;

	p = r->parsed;
	pos = 0;

	/*
	 0                   1                   2                   3
	 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	+-+-+-+-+-------+-+-------------+-------------------------------+
	|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
	|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
	|N|V|V|V|       |S|             |   (if payload len==126/127)   |
	| |1|2|3|       |K|             |                               |
	+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
	|     Extended payload length continued, if payload len == 127  |
	+ - - - - - - - - - - - - - - - +-------------------------------+
	|                               |Masking-key, if MASK set to 1  |
	+-------------------------------+-------------------------------+
	| Masking-key (continued)       |          Payload Data         |
	+-------------------------------- - - - - - - - - - - - - - - - +
	:                     Payload Data continued ...                :
	+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
	|                     Payload Data continued ...                |
	+---------------------------------------------------------------+

	Do minimal parse required to make sure the full message has been
	received (websocket module will do full parse and validation).
	*/

	/* Process first two bytes */
	if (size < pos + 2)
		goto skip;
	pos++;
	mask_present = p[pos] & 0x80;
	len = (p[pos++] & 0xff) & ~0x80;

	/* Work out real length */
	if (len == 126)
	{
		if (size < pos + 2)
			goto skip;

		len =	  ((p[pos + 0] & 0xff) <<  8)
			| ((p[pos + 1] & 0xff) <<  0);
		pos += 2;
	}
	else if (len == 127)
	{
		if (size < pos + 8)
			goto skip;

		/* Only decoding the last four bytes of the length...
		   This limits the size of WebSocket messages that can be
		   handled to 2^32 - which should be plenty for SIP! */
		len =	  ((p[pos + 4] & 0xff) << 24)
			| ((p[pos + 5] & 0xff) << 16)
			| ((p[pos + 6] & 0xff) <<  8)
			| ((p[pos + 7] & 0xff) <<  0);
		pos += 8;
	}

	/* Skip mask */
	if (mask_present)
	{
		if (size < pos + 4)
			goto skip;
		pos += 4;
	}

	/* Now check the whole message has been received */
	if (size < pos + len)
		goto skip;

	pos += len;
	r->flags |= F_TCP_REQ_COMPLETE;
	r->parsed = &p[pos];

skip:
	return bytes;
}

static int ws_process_msg(char* tcpbuf, unsigned int len,
		struct receive_info* rcv_info, struct tcp_connection* con)
{
	int ret;
	tcp_event_info_t tev;

	ret = 0;
	LM_DBG("WebSocket Message: [[>>>\n%.*s<<<]]\n", len, tcpbuf);
	if(likely(sr_event_enabled(SREV_TCP_WS_FRAME_IN))) {
		memset(&tev, 0, sizeof(tcp_event_info_t));
		tev.type = SREV_TCP_WS_FRAME_IN;
		tev.buf = tcpbuf;
		tev.len = len;
		tev.rcv = rcv_info;
		tev.con = con;
		ret = sr_event_exec(SREV_TCP_WS_FRAME_IN, (void*)(&tev));
	} else {
		LM_DBG("no callback registering for handling WebSockets - dropping!\n");
	}
	return ret;
}
#endif

/**
 * @brief wrapper around receive_msg() to clone the tcpbuf content
 *
 * When receiving over TCP, tcpbuf points inside the TCP stream buffer, but during
 * processing of config, msg->buf content might be changed and may corrupt
 * the content of the stream. Safer, make a clone of buf content in a local
 * buffer and give that to receive_msg() to link to msg->buf
 */
int receive_tcp_msg(char* tcpbuf, unsigned int len,
		struct receive_info* rcv_info, struct tcp_connection* con)
{
#ifdef TCP_CLONE_RCVBUF
#ifdef DYN_BUF
	char *buf = NULL;
#else
	static char *buf = NULL;
	static unsigned int bsize = 0;
#endif
	int blen;

	/* cloning is disabled via parameter */
	if(likely(tcp_clone_rcvbuf==0)) {
#ifdef READ_MSRP
		if(unlikely(con->req.flags&F_TCP_REQ_MSRP_FRAME))
			return msrp_process_msg(tcpbuf, len, rcv_info, con);
#endif
#ifdef READ_WS
		if(unlikely(con->type == PROTO_WS || con->type == PROTO_WSS))
			return ws_process_msg(tcpbuf, len, rcv_info, con);
#endif

		return receive_msg(tcpbuf, len, rcv_info);
	}

	/* min buffer size is BUF_SIZE */
	blen = len;
	if(blen < BUF_SIZE)
		blen = BUF_SIZE;

#ifdef DYN_BUF
	buf=pkg_malloc(blen+1);
	if (buf==0) {
		LM_ERR("could not allocate receive buffer\n");
		return -1;
	}
#else
	/* allocate buffer when needed
	 * - no buffer yet
	 * - existing buffer too small (min size is BUF_SIZE - to accomodate most
	 *   of SIP messages; expected larger for HTTP/XCAP)
	 * - existing buffer too large (e.g., we got a too big message in the past,
	 *   let's free it)
	 *
	 * - also, use system memory, not to eat from PKG (same as static buffer
	 *   from PKG pov)
	 */
	if(buf==NULL || bsize < blen || blen < bsize/2) {
		if(buf!=NULL)
			free(buf);
		buf=malloc(blen+1);
		if (buf==0) {
			LM_ERR("could not allocate receive buffer\n");
			return -1;
		}
		bsize = blen;
	}
#endif

	memcpy(buf, tcpbuf, len);
	buf[len] = '\0';
#ifdef READ_MSRP
	if(unlikely(con->req.flags&F_TCP_REQ_MSRP_FRAME))
		return msrp_process_msg(buf, len, rcv_info, con);
#endif
#ifdef READ_WS
	if(unlikely(con->type == PROTO_WS || con->type == PROTO_WSS))
		return ws_process_msg(buf, len, rcv_info, con);
#endif
	return receive_msg(buf, len, rcv_info);
#else /* TCP_CLONE_RCVBUF */
#ifdef READ_MSRP
	if(unlikely(con->req.flags&F_TCP_REQ_MSRP_FRAME))
		return msrp_process_msg(tcpbuf, len, rcv_info, con);
#endif
#ifdef READ_WS
	if(unlikely(con->type == PROTO_WS || con->type == PROTO_WSS))
		return ws_process_msg(tcpbuf, len, rcv_info, con);
#endif
	return receive_msg(tcpbuf, len, rcv_info);
#endif /* TCP_CLONE_RCVBUF */
}

int tcp_read_req(struct tcp_connection* con, int* bytes_read, int* read_flags)
{
	int bytes;
	int total_bytes;
	int resp;
	long size;
	struct tcp_req* req;
	struct dest_info dst;
	char c;
	int ret;
		
		bytes=-1;
		total_bytes=0;
		resp=CONN_RELEASE;
		req=&con->req;

again:
		if (likely(req->error==TCP_REQ_OK)){
#ifdef READ_WS
			if (unlikely(con->type == PROTO_WS || con->type == PROTO_WSS))
				bytes=tcp_read_ws(con, read_flags);
			else
#endif
				bytes=tcp_read_headers(con, read_flags);
#ifdef EXTRA_DEBUG
						/* if timeout state=0; goto end__req; */
			LM_DBG("read= %d bytes, parsed=%d, state=%d, error=%d\n",
					bytes, (int)(req->parsed-req->start), req->state,
					req->error );
			LM_DBG("last char=0x%02X, parsed msg=\n%.*s\n",
					*(req->parsed-1), (int)(req->parsed-req->start),
					req->start);
#endif
			if (unlikely(bytes==-1)){
				LOG(cfg_get(core, core_cfg, corelog),
						"ERROR: tcp_read_req: error reading \n");
				resp=CONN_ERROR;
				goto end_req;
			}
			total_bytes+=bytes;
			/* eof check:
			 * is EOF if eof on fd and req.  not complete yet,
			 * if req. is complete we might have a second unparsed
			 * request after it, so postpone release_with_eof
			 */
			if (unlikely((con->state==S_CONN_EOF) && 
						(! TCP_REQ_COMPLETE(req)))) {
				LM_DBG("EOF\n");
				resp=CONN_EOF;
				goto end_req;
			}
		}
		if (unlikely(req->error!=TCP_REQ_OK)){
			LM_ERR("bad request, state=%d, error=%d buf:\n%.*s\nparsed:\n%.*s\n",
					req->state, req->error,
					(int)(req->pos-req->buf), req->buf,
					(int)(req->parsed-req->start), req->start);
			LM_DBG("received from: port %d\n", con->rcv.src_port);
			print_ip("received from: ip", &con->rcv.src_ip, "\n");
			resp=CONN_ERROR;
			goto end_req;
		}
		if (likely(TCP_REQ_COMPLETE(req))){
#ifdef EXTRA_DEBUG
			LM_DBG("end of header part\n");
			LM_DBG("received from: port %d\n", con->rcv.src_port);
			print_ip("received from: ip", &con->rcv.src_ip, "\n");
			LM_DBG("headers:\n%.*s.\n",
					(int)(req->body-req->start), req->start);
#endif
			if (likely(TCP_REQ_HAS_CLEN(req))){
				LM_DBG("content-length=%d\n", req->content_len);
#ifdef EXTRA_DEBUG
				LM_DBG("body:\n%.*s\n", req->content_len,req->body);
#endif
			}else{
				if (cfg_get(tcp, tcp_cfg, accept_no_cl)==0) {
					req->error=TCP_REQ_BAD_LEN;
					LM_ERR("content length not present or unparsable\n");
					resp=CONN_ERROR;
					goto end_req;
				}
			}
			/* if we are here everything is nice and ok*/
			resp=CONN_RELEASE;
#ifdef EXTRA_DEBUG
			LM_DBG("receiving msg(%p, %d)\n",
					req->start, (int)(req->parsed-req->start));
#endif
			/* rcv.bind_address should always be !=0 */
			bind_address=con->rcv.bind_address;
			/* just for debugging use sendipv4 as receiving socket  FIXME*/
			/*
			if (con->rcv.dst_ip.af==AF_INET6){
				bind_address=sendipv6_tcp;
			}else{
				bind_address=sendipv4_tcp;
			}
			*/
			con->rcv.proto_reserved1=con->id; /* copy the id */
			c=*req->parsed; /* ugly hack: zero term the msg & save the
							   previous char, req->parsed should be ok
							   because we always alloc BUF_SIZE+1 */
			*req->parsed=0;

			if (req->state==H_PING_CRLF) {
				init_dst_from_rcv(&dst, &con->rcv);

				if (tcp_send(&dst, 0, CRLF, CRLF_LEN) < 0) {
					LM_ERR("CRLF ping: tcp_send() failed\n");
				}
				ret = 0;
			} else if (unlikely(req->state==H_STUN_END)) {
				/* stun request */
				ret = stun_process_msg(req->start, req->parsed-req->start,
									 &con->rcv);
			} else
#ifdef READ_MSRP
			// if (unlikely(req->flags&F_TCP_REQ_MSRP_FRAME)){
			if (unlikely(req->state==H_MSRP_FINISH)){
				/* msrp frame */
				ret = receive_tcp_msg(req->start, req->parsed-req->start,
									&con->rcv, con);
			}else
#endif
#ifdef READ_HTTP11
			if (unlikely(req->state==H_HTTP11_CHUNK_FINISH)){
				/* http chunked request */
				req->body[req->content_len] = 0;
				ret = receive_tcp_msg(req->start,
						req->body + req->content_len - req->start,
						&con->rcv, con);
			}else
#endif
#ifdef READ_WS
			if (unlikely(con->type == PROTO_WS || con->type == PROTO_WSS)){
				ret = receive_tcp_msg(req->start, req->parsed-req->start,
									&con->rcv, con);
			}else
#endif
				ret = receive_tcp_msg(req->start, req->parsed-req->start,
									&con->rcv, con);
				
			if (unlikely(ret < 0)) {
				*req->parsed=c;
				resp=CONN_ERROR;
				goto end_req;
			}
			*req->parsed=c;
			
			/* prepare for next request */
			size=req->pos-req->parsed;
			req->start=req->buf;
			req->body=0;
			req->error=TCP_REQ_OK;
			req->state=H_SKIP_EMPTY;
			req->flags=0;
			req->content_len=0;
			req->bytes_to_go=0;
			req->pos=req->buf+size;
			
			if (unlikely(size)){ 
				memmove(req->buf, req->parsed, size);
				req->parsed=req->buf; /* fix req->parsed after using it */
#ifdef EXTRA_DEBUG
				LM_DBG("preparing for new request, kept %ld bytes\n", size);
#endif
				/*if we still have some unparsed bytes, try to parse them too*/
				goto again;
			} else if (unlikely(con->state==S_CONN_EOF)){
				LM_DBG("EOF after reading complete request\n");
				resp=CONN_EOF;
			}
			req->parsed=req->buf; /* fix req->parsed */
		}
		
		
	end_req:
		if (likely(bytes_read)) *bytes_read=total_bytes;
		return resp;
}



void release_tcpconn(struct tcp_connection* c, long state, int unix_sock)
{
	long response[2];
	
		LM_DBG("releasing con %p, state %ld, fd=%d, id=%d\n",
				c, state, c->fd, c->id);
		LM_DBG("extra_data %p\n", c->extra_data);
		/* release req & signal the parent */
		c->reader_pid=0; /* reset it */
		if (c->fd!=-1){
			close(c->fd);
			c->fd=-1;
		}
		/* errno==EINTR, EWOULDBLOCK a.s.o todo */
		response[0]=(long)c;
		response[1]=state;
		
		if (tsend_stream(unix_sock, (char*)response, sizeof(response), -1)<=0)
			LM_ERR("tsend_stream failed\n");
}



static ticks_t tcpconn_read_timeout(ticks_t t, struct timer_ln* tl, void* data)
{
	struct tcp_connection *c;
	
	c=(struct tcp_connection*)data; 
	/* or (struct tcp...*)(tl-offset(c->timer)) */
	
	if (likely(!(c->state<0) && TICKS_LT(t, c->timeout))){
		/* timeout extended, exit */
		return (ticks_t)(c->timeout - t);
	}
	/* if conn->state is ERROR or BAD => force timeout too */
	if (unlikely(io_watch_del(&io_w, c->fd, -1, IO_FD_CLOSING)<0)){
		LM_ERR("io_watch_del failed for %p"
					" id %d fd %d, state %d, flags %x, main fd %d\n",
					c, c->id, c->fd, c->state, c->flags, c->s);
	}
	tcpconn_listrm(tcp_conn_lst, c, c_next, c_prev);
	release_tcpconn(c, (c->state<0)?CONN_ERROR:CONN_RELEASE, tcpmain_sock);
	
	return 0;
}



/* handle io routine, based on the fd_map type
 * (it will be called from io_wait_loop* )
 * params:  fm  - pointer to a fd hash entry
 *          idx - index in the fd_array (or -1 if not known)
 * return: -1 on error, or when we are not interested any more on reads
 *            from this fd (e.g.: we are closing it )
 *          0 on EAGAIN or when by some other way it is known that no more 
 *            io events are queued on the fd (the receive buffer is empty).
 *            Usefull to detect when there are no more io events queued for
 *            sigio_rt, epoll_et, kqueue.
 *         >0 on successfull read from the fd (when there might be more io
 *            queued -- the receive buffer might still be non-empty)
 */
inline static int handle_io(struct fd_map* fm, short events, int idx)
{	
	int ret;
	int n;
	int read_flags;
	struct tcp_connection* con;
	int s;
	long resp;
	ticks_t t;
	
	/* update the local config */
	cfg_update();
	
	switch(fm->type){
		case F_TCPMAIN:
again:
			ret=n=receive_fd(fm->fd, &con, sizeof(con), &s, 0);
			LM_DBG("received n=%d con=%p, fd=%d\n", n, con, s);
			if (unlikely(n<0)){
				if (errno == EWOULDBLOCK || errno == EAGAIN){
					ret=0;
					break;
				}else if (errno == EINTR) goto again;
				else{
					LM_CRIT("read_fd: %s \n", strerror(errno));
						abort(); /* big error*/
				}
			}
			if (unlikely(n==0)){
				LM_ERR("0 bytes read\n");
				goto error;
			}
			if (unlikely(con==0)){
					LM_CRIT("null pointer\n");
					goto error;
			}
			con->fd=s;
			if (unlikely(s==-1)) {
				LM_ERR("read_fd: no fd read\n");
				goto con_error;
			}
			con->reader_pid=my_pid();
			if (unlikely(con==tcp_conn_lst)){
				LM_CRIT("duplicate connection received: %p, id %d, fd %d, refcnt %d"
							" state %d (n=%d)\n", con, con->id, con->fd,
							atomic_get(&con->refcnt), con->state, n);
				goto con_error;
				break; /* try to recover */
			}
			if (unlikely(con->state==S_CONN_BAD)){
				LM_WARN("received an already bad connection: %p id %d refcnt %d\n",
							con, con->id, atomic_get(&con->refcnt));
				goto con_error;
			}
			/* if we received the fd there is most likely data waiting to
			 * be read => process it first to avoid extra sys calls */
			read_flags=((con->flags & (F_CONN_EOF_SEEN|F_CONN_FORCE_EOF)) &&
						!(con->flags & F_CONN_OOB_DATA))? RD_CONN_FORCE_EOF
						:0;
#ifdef USE_TLS
repeat_1st_read:
#endif /* USE_TLS */
			resp=tcp_read_req(con, &n, &read_flags);
			if (unlikely(resp<0)){
				/* some error occured, but on the new fd, not on the tcp
				 * main fd, so keep the ret value */
				if (unlikely(resp!=CONN_EOF))
					con->state=S_CONN_BAD;
				release_tcpconn(con, resp, tcpmain_sock);
				break;
			}
#ifdef USE_TLS
			/* repeat read if requested (for now only tls might do this) */
			if (unlikely(read_flags & RD_CONN_REPEAT_READ))
				goto repeat_1st_read;
#endif /* USE_TLS */
			
			/* must be before io_watch_add, io_watch_add might catch some
			 * already existing events => might call handle_io and
			 * handle_io might decide to del. the new connection =>
			 * must be in the list */
			tcpconn_listadd(tcp_conn_lst, con, c_next, c_prev);
			t=get_ticks_raw();
			con->timeout=t+S_TO_TICKS(TCP_CHILD_TIMEOUT);
			/* re-activate the timer */
			con->timer.f=tcpconn_read_timeout;
			local_timer_reinit(&con->timer);
			local_timer_add(&tcp_reader_ltimer, &con->timer,
								S_TO_TICKS(TCP_CHILD_TIMEOUT), t);
			if (unlikely(io_watch_add(&io_w, s, POLLIN, F_TCPCONN, con)<0)){
				LM_CRIT("io_watch_add failed for %p id %d fd %d, state %d, flags %x,"
							" main fd %d, refcnt %d\n",
							con, con->id, con->fd, con->state, con->flags,
							con->s, atomic_get(&con->refcnt));
				tcpconn_listrm(tcp_conn_lst, con, c_next, c_prev);
				local_timer_del(&tcp_reader_ltimer, &con->timer);
				goto con_error;
			}
			break;
		case F_TCPCONN:
			con=(struct tcp_connection*)fm->data;
			if (unlikely(con->state==S_CONN_BAD)){
				resp=CONN_ERROR;
				if (!(con->send_flags.f & SND_F_CON_CLOSE))
					LM_WARN("F_TCPCONN connection marked as bad: %p id %d refcnt %d\n",
							con, con->id, atomic_get(&con->refcnt));
				goto read_error;
			}
			read_flags=((
#ifdef POLLRDHUP
						(events & POLLRDHUP) |
#endif /* POLLRDHUP */
						(events & (POLLHUP|POLLERR)) |
							(con->flags & (F_CONN_EOF_SEEN|F_CONN_FORCE_EOF)))
						&& !(events & POLLPRI))? RD_CONN_FORCE_EOF: 0;
#ifdef USE_TLS
repeat_read:
#endif /* USE_TLS */
			resp=tcp_read_req(con, &ret, &read_flags);
			if (unlikely(resp<0)){
read_error:
				ret=-1; /* some error occured */
				if (unlikely(io_watch_del(&io_w, con->fd, idx,
											IO_FD_CLOSING) < 0)){
					LM_CRIT("io_watch_del failed for %p id %d fd %d,"
							" state %d, flags %x, main fd %d, refcnt %d\n",
							con, con->id, con->fd, con->state,
							con->flags, con->s, atomic_get(&con->refcnt));
				}
				tcpconn_listrm(tcp_conn_lst, con, c_next, c_prev);
				local_timer_del(&tcp_reader_ltimer, &con->timer);
				if (unlikely(resp!=CONN_EOF))
					con->state=S_CONN_BAD;
				release_tcpconn(con, resp, tcpmain_sock);
			}else{
#ifdef USE_TLS
				if (unlikely(read_flags & RD_CONN_REPEAT_READ))
						goto repeat_read;
#endif /* USE_TLS */
				/* update timeout */
				con->timeout=get_ticks_raw()+S_TO_TICKS(TCP_CHILD_TIMEOUT);
				/* ret= 0 (read the whole socket buffer) if short read & 
				 *  !POLLPRI,  bytes read otherwise */
				ret&=(((read_flags & RD_CONN_SHORT_READ) &&
						!(events & POLLPRI)) - 1);
			}
			break;
		case F_NONE:
			LM_CRIT("empty fd map %p (%d): {%d, %d, %p}\n",
						fm, (int)(fm-io_w.fd_hash),
						fm->fd, fm->type, fm->data);
			goto error;
		default:
			LM_CRIT("uknown fd type %d\n", fm->type); 
			goto error;
	}
	
	return ret;
con_error:
	con->state=S_CONN_BAD;
	release_tcpconn(con, CONN_ERROR, tcpmain_sock);
	return ret;
error:
	return -1;
}



inline static void tcp_reader_timer_run(void)
{
	ticks_t ticks;
	
	ticks=get_ticks_raw();
	if (unlikely((ticks-tcp_reader_prev_ticks)<TCPCONN_TIMEOUT_MIN_RUN))
		return;
	tcp_reader_prev_ticks=ticks;
	local_timer_run(&tcp_reader_ltimer, ticks);
}



void tcp_receive_loop(int unix_sock)
{
	
	/* init */
	tcpmain_sock=unix_sock; /* init com. socket */
	if (init_io_wait(&io_w, get_max_open_fds(), tcp_poll_method)<0)
		goto error;
	tcp_reader_prev_ticks=get_ticks_raw();
	if (init_local_timer(&tcp_reader_ltimer, get_ticks_raw())!=0)
		goto error;
	/* add the unix socket */
	if (io_watch_add(&io_w, tcpmain_sock, POLLIN,  F_TCPMAIN, 0)<0){
		LM_CRIT("failed to add socket to the fd list\n");
		goto error;
	}

	/* initialize the config framework */
	if (cfg_child_init()) goto error;

	/* main loop */
	switch(io_w.poll_method){
		case POLL_POLL:
				while(1){
					io_wait_loop_poll(&io_w, TCP_CHILD_SELECT_TIMEOUT, 0);
					tcp_reader_timer_run();
				}
				break;
#ifdef HAVE_SELECT
		case POLL_SELECT:
			while(1){
				io_wait_loop_select(&io_w, TCP_CHILD_SELECT_TIMEOUT, 0);
				tcp_reader_timer_run();
			}
			break;
#endif
#ifdef HAVE_SIGIO_RT
		case POLL_SIGIO_RT:
			while(1){
				io_wait_loop_sigio_rt(&io_w, TCP_CHILD_SELECT_TIMEOUT);
				tcp_reader_timer_run();
			}
			break;
#endif
#ifdef HAVE_EPOLL
		case POLL_EPOLL_LT:
			while(1){
				io_wait_loop_epoll(&io_w, TCP_CHILD_SELECT_TIMEOUT, 0);
				tcp_reader_timer_run();
			}
			break;
		case POLL_EPOLL_ET:
			while(1){
				io_wait_loop_epoll(&io_w, TCP_CHILD_SELECT_TIMEOUT, 1);
				tcp_reader_timer_run();
			}
			break;
#endif
#ifdef HAVE_KQUEUE
		case POLL_KQUEUE:
			while(1){
				io_wait_loop_kqueue(&io_w, TCP_CHILD_SELECT_TIMEOUT, 0);
				tcp_reader_timer_run();
			}
			break;
#endif
#ifdef HAVE_DEVPOLL
		case POLL_DEVPOLL:
			while(1){
				io_wait_loop_devpoll(&io_w, TCP_CHILD_SELECT_TIMEOUT, 0);
				tcp_reader_timer_run();
			}
			break;
#endif
		default:
			LM_CRIT("no support for poll method %s (%d)\n", 
					poll_method_name(io_w.poll_method), io_w.poll_method);
			goto error;
	}
error:
	destroy_io_wait(&io_w);
	LM_CRIT("exiting...");
	exit(-1);
}


int is_msg_complete(struct tcp_req* r)
{
	if (TCP_REQ_HAS_CLEN(r)) {
		r->state = H_STUN_FP;
		return 0;
	}
	else {
		/* STUN message is complete */
		r->state = H_STUN_END;
		r->flags |= F_TCP_REQ_COMPLETE |
					F_TCP_REQ_HAS_CLEN; /* hack to avoid error check */
		return 1;
	}
}

#endif /* USE_TCP */
