/*
 * $Id$
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
 * 2002-12-??  created by andrei.
 * 2003-02-10  zero term before calling receive_msg & undo afterwards (andrei)
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
#include "pass_fd.h"
#include "globals.h"
#include "receive.h"
#include "timer.h"
#include "ut.h"



/* reads next available bytes
 * return number of bytes read, 0 on EOF or -1 on error,
 * sets also r->error */
int tcp_read(struct tcp_req *r, int fd)
{
	int bytes_free, bytes_read;
	
	bytes_free=TCP_BUF_SIZE- (int)(r->pos - r->buf);
	
	if (bytes_free==0){
		LOG(L_ERR, "ERROR: tcp_read: buffer overrun, dropping\n");
		r->error=TCP_REQ_OVERRUN;
		return -1;
	}
again:
	bytes_read=read(fd, r->pos, bytes_free);

	if(bytes_read==-1){
		if (errno == EWOULDBLOCK || errno == EAGAIN){
			return 0; /* nothing has been read */
		}else if (errno == EINTR) goto again;
		else{
			LOG(L_ERR, "ERROR: tcp_read: error reading: %s\n",strerror(errno));
			r->error=TCP_READ_ERROR;
			return -1;
		}
	}
	DBG("tcp_read: read %d bytes:\n%.*s\n", bytes_read, bytes_read, r->pos);
	
	r->pos+=bytes_read;
	return bytes_read;
}



/* reads all headers (until double crlf), & parses the content-length header
 * (WARNING: highly ineficient, tries to reuse receive_msg but will parse
 *  all the header names twice [once here & once in receive_msg]; a more
 *  speed eficient version will result in either major code duplication or
 *  major changes to the receive code - TODO)
 * returns number of bytes read & sets r->state & r->body
 * when either r->body!=0 or r->state==H_BODY =>
 * all headers have been read. It should be called in a while loop.
 * returns < 0 if error or 0 if EOF */
int tcp_read_headers(struct tcp_req *r, int fd)
{
	int bytes, remaining;
	char *p;
	
	#define crlf_default_skip_case \
					case '\n': \
						r->state=H_LF; \
						break; \
					default: \
						r->state=H_SKIP
	
	#define content_len_beg_case \
					case ' ': \
					case '\t': \
						if (!r->has_content_len) r->state=H_STARTWS; \
						else r->state=H_SKIP; \
							/* not interested if we already found one */ \
						break; \
					case 'C': \
					case 'c': \
						if(!r->has_content_len) r->state=H_CONT_LEN1; \
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


	/* if we still have some unparsed part, parse it first, don't do the read*/
	if (r->parsed<r->pos){
		bytes=0;
	}else{
		bytes=tcp_read(r, fd);
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
					r->complete=1;
					goto skip;
				}
				break;
				
			case H_SKIP:
				/* find lf, we are in this state if we are not interested
				 * in anything till end of line*/
				p=q_memchr(p, '\n', r->pos-p);
				if (p){
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
						DBG("tcp_read_headers: switching to H_BODY (lflf)\n");
						if (r->has_content_len){
							r->body=p+1;
							r->bytes_to_go=r->content_len;
							if (r->bytes_to_go==0){
								r->complete=1;
								p++;
								goto skip;
							}
						}else{
							DBG("tcp_read_headers: ERROR: no clen, p=%X\n",
									*p);
							r->error=TCP_REQ_BAD_LEN;
						}
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
					DBG("tcp_read_headers: switching to H_BODY (lfcrlf)\n");
					if (r->has_content_len){
						r->body=p+1;
						r->bytes_to_go=r->content_len;
						if (r->bytes_to_go==0){
							r->complete=1;
							p++;
							goto skip;
						}
					}else{
						DBG("tcp_read_headers: ERROR: no clen, p=%X\n",
									*p);
						r->error=TCP_REQ_BAD_LEN;
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
					case '\r':
					case ' ':
					case '\t':
						/* skip empty lines */
						break;
					case 'C': 
					case 'c': 
						r->state=H_CONT_LEN1; 
						r->start=p;
						break;
					default:
						r->state=H_SKIP;
						r->start=p;
				};
				p++;
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
					/*FIXME: content lenght on different lines ! */
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
						r->state=H_SKIP;
						r->has_content_len=1;
						break;
					case '\n':
						/* end of line, parse succesfull */
						r->state=H_LF;
						r->has_content_len=1;
						break;
					default:
						LOG(L_ERR, "ERROR: tcp_read_headers: bad "
								"Content-Length header value, unexpected "
								"char %c in state %d\n", *p, r->state);
						r->state=H_SKIP; /* try to find another?*/
				}
				p++;
				break;
			
			default:
				LOG(L_CRIT, "BUG: tcp_read_headers: unexpected state %d\n",
						r->state);
				abort();
		}
	}
skip:
	r->parsed=p;
	return bytes;
}



int tcp_read_req(struct tcp_connection* con)
{
	int bytes;
	int resp;
	long size;
	struct tcp_req* req;
	int s;
	char c;
		
		resp=CONN_RELEASE;
		s=con->fd;
		req=&con->req;
		size=0;
again:
		if(req->complete==0 && req->error==TCP_REQ_OK){
			bytes=tcp_read_headers(req, s);
						/* if timeout state=0; goto end__req; */
			DBG("read= %d bytes, parsed=%d, state=%d, error=%d\n",
					bytes, (int)(req->parsed-req->start), req->state,
					req->error );
			DBG("tcp_read_req: last char=%X, parsed msg=\n%.*s\n",
					*(req->parsed-1), (int)(req->parsed-req->start),
					req->start);
			if (bytes==-1){
				LOG(L_ERR, "ERROR: tcp_read_req: error reading \n");
				resp=CONN_ERROR;
				goto end_req;
			}
			if ((size==0) && (bytes==0)){
				DBG( "tcp_read_req: EOF\n");
				resp=CONN_EOF;
				goto end_req;
			}
		
		}
		if (req->error!=TCP_REQ_OK){
			LOG(L_ERR,"ERROR: tcp_read_req: bad request, state=%d, error=%d "
					  "buf:\n%.*s\nparsed:\n%.*s\n", req->state, req->error,
					  (int)(req->pos-req->buf), req->buf,
					  (int)(req->parsed-req->start), req->start);
			DBG("- received from: port %d, ip -", ntohs(con->rcv.src_port));
			print_ip(&con->rcv.src_ip); DBG("-\n");
			resp=CONN_ERROR;
			goto end_req;
		}
		if (req->complete){
			DBG("tcp_read_req: end of header part\n");
			DBG("- received from: port %d, ip - ", ntohs(con->rcv.src_port));
			print_ip(&con->rcv.src_ip); DBG("-\n");
			DBG("tcp_read_req: headers:\n%.*s.\n",
					(int)(req->body-req->start), req->start);
			if (req->has_content_len){
				DBG("tcp_read_req: content-length= %d\n", req->content_len);
				DBG("tcp_read_req: body:\n%.*s\n", req->content_len,req->body);
			}else{
				req->error=TCP_REQ_BAD_LEN;
				LOG(L_ERR, "ERROR: tcp_read_req: content length not present or"
						" unparsable\n");
				resp=CONN_ERROR;
				goto end_req;
			}
			/* if we are here everything is nice and ok*/
			resp=CONN_RELEASE;
			/* just for debugging use sendipv4 as receiving socket */
			DBG("calling receive_msg(%p, %d, )\n",
					req->start, (int)(req->parsed-req->start));
			bind_address=sendipv4; /*&tcp_info[con->sock_idx];*/
			con->rcv.proto_reserved1=con->id; /* copy the id */
			c=*req->parsed; /* ugly hack: zero term the msg & save the
							   previous char, req->parsed should be ok
							   because we always alloc BUF_SIZE+1 */
			*req->parsed=0;
			if (receive_msg(req->start, req->parsed-req->start, &con->rcv)<0){
				*req->parsed=c;
				resp=CONN_ERROR;
				goto end_req;
			}
			*req->parsed=c;
			
			/* prepare for next request */
			size=req->pos-req->parsed;
			if (size) memmove(req->buf, req->parsed, size);
			DBG("tcp_read_req: preparing for new request, kept %ld bytes\n",
					size);
			req->pos=req->buf+size;
			req->parsed=req->buf;
			req->start=req->buf;
			req->body=0;
			req->error=TCP_REQ_OK;
			req->state=H_SKIP_EMPTY;
			req->complete=req->content_len=req->has_content_len=0;
			req->bytes_to_go=0;
			/* if we still have some unparsed bytes, try to  parse them too*/
			if (size) goto again;
			
		}
		
		
	end_req:
		return resp;
}



void release_tcpconn(struct tcp_connection* c, long state, int unix_sock)
{
	long response[2];
	
		DBG( "releasing con %p, state %ld, fd=%d, id=%d\n",
				c, state, c->fd, c->id);
		/* release req & signal the parent */
		if (c->fd!=-1) close(c->fd);
		/* errno==EINTR, EWOULDBLOCK a.s.o todo */
		response[0]=(long)c;
		response[1]=state;
		write(unix_sock, response, sizeof(response));
}



void tcp_receive_loop(int unix_sock)
{
	struct tcp_connection* list; /* list with connections in use */
	struct tcp_connection* con;
	struct tcp_connection* c_next;
	int n;
	int nfds;
	int s;
	long resp;
	fd_set master_set;
	fd_set sel_set;
	int maxfd;
	struct timeval timeout;
	int ticks;
	
	
	/* init */
	list=con=0;
	FD_ZERO(&master_set);
	FD_SET(unix_sock, &master_set);
	maxfd=unix_sock;
	
	/* listen on the unix socket for the fd */
	for(;;){
			timeout.tv_sec=TCP_CHILD_SELECT_TIMEOUT;
			timeout.tv_usec=0;
			sel_set=master_set;
			nfds=select(maxfd+1, &sel_set, 0 , 0 , &timeout);
#ifdef EXTRA_DEBUG
			for (n=0; n<maxfd; n++){
				if (FD_ISSET(n, &sel_set)) 
					DBG("tcp receive: FD %d is set\n", n);
			}
#endif
			if (nfds<0){
				if (errno==EINTR) continue; /* just a signal */
				/* errors */
				LOG(L_ERR, "ERROR: tcp_receive_loop: select:(%d) %s\n", errno,
					strerror(errno));
				continue;
			}
			if (FD_ISSET(unix_sock, &sel_set)){
				DBG("tcp receive: receiving fd from from 'main'\n");
				nfds--;
				/* a new conn from "main" */
				n=receive_fd(unix_sock, &con, sizeof(con), &s);
				if (n<0){
					if (errno == EWOULDBLOCK || errno == EAGAIN ||
							errno == EINTR){
						continue;
					}else{
						LOG(L_CRIT,"BUG: tcp_receive_loop: read_fd: %s\n",
							strerror(errno));
						abort(); /* big error*/
					}
				}
				if (n==0){
					LOG(L_ERR, "WARNING: tcp_receive_loop: 0 bytes read\n");
					continue;
				}
				con->fd=s;
				DBG("received n=%d con=%p, fd=%d\n", n, con, s);
				if (s==-1) {
					LOG(L_ERR, "ERROR: tcp_receive_loop: read_fd:"
									"no fd read\n");
					resp=CONN_ERROR;
					con->bad=1;
					release_tcpconn(con, resp, unix_sock);
				}
				if (con==0){
					LOG(L_ERR, "ERROR: tcp_receive_loop: null pointer\n");
					resp=CONN_ERROR;
					con->bad=1;
					release_tcpconn(con, resp, unix_sock);
				}
				con->timeout=get_ticks()+TCP_CHILD_TIMEOUT;
				FD_SET(s, &master_set);
				if (maxfd<s) maxfd=s;
				tcpconn_listadd(list, con, c_next, c_prev);
			}
			ticks=get_ticks();
			for (con=list; con ; con=c_next){
				c_next=con->c_next; /* safe for removing*/
#ifdef EXTRA_DEBUG
				DBG("tcp receive: list fd=%d, id=%d, timeout=%d, refcnt=%d\n",
						con->fd, con->id, con->timeout, con->refcnt);
#endif
				if (nfds && FD_ISSET(con->fd, &sel_set)){
#ifdef EXTRA_DEBUG
					DBG("tcp receive: match, fd:isset\n");
#endif
					nfds--;
					resp=tcp_read_req(con);
					if (resp<0){
						FD_CLR(con->fd, &master_set);
						tcpconn_listrm(list, con, c_next, c_prev);
						con->bad=1;
						release_tcpconn(con, resp, unix_sock);
					}else{
						/* update timeout */
						con->timeout=ticks+TCP_CHILD_TIMEOUT;
					}
				}else{
					/* timeout */
					if (con->timeout<=ticks){
						/* expired, return to "tcp main" */
						DBG("tcp_receive_loop: %p expired (%d, %d)\n",
								con, con->timeout, ticks);
						resp=CONN_RELEASE;
						FD_CLR(con->fd, &master_set);
						tcpconn_listrm(list, con, c_next, c_prev);
						release_tcpconn(con, resp, unix_sock);
					}
				}
			}
		
	}
}


#if 0
int main(int argv, char** argc )
{
	printf("starting tests\n");
	tcp_receive_loop();
}

#endif

#endif
