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

#ifdef USE_TCP

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/select.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>


#include "tcp_conn.h"
#include "pass_fd.h"


#define q_memchr memchr

/* reads next available bytes
 * return number of bytes read, 0 on EOF or -1 on error,
 * sets also r->error */
int tcp_read(struct tcp_req *r)
{
	int bytes_free, bytes_read;
	
	bytes_free=TCP_BUF_SIZE- (int)(r->pos - r->buf);
	
	if (bytes_free==0){
		fprintf(stderr, "buffer overrun, dropping\n");
		r->error=TCP_REQ_OVERRUN;
		return -1;
	}
again:
	bytes_read=read(r->fd, r->pos, bytes_free);

	if(bytes_read==-1){
		if (errno == EWOULDBLOCK || errno == EAGAIN){
			return 0; /* nothing has been read */
		}else if (errno == EINTR) goto again;
		else{
			fprintf(stderr, "error reading: %s\n", strerror(errno));
			r->error=TCP_READ_ERROR;
			return -1;
		}
	}
	
	r->pos+=bytes_read;
	return bytes_read;
}



/* reads all headers (until double crlf),
 * returns number of bytes read & sets r->state & r->body
 * when either r->body!=0 or r->state==H_BODY =>
 * all headers have been read. It should be called in a while loop.
 * returns < 0 if error or 0 if EOF */
int tcp_read_headers(struct tcp_req *r)
{
	int bytes;
	char *p;
	
	bytes=tcp_read(r);
	if (bytes<=0) return bytes;
	p=r->parsed;
	
	while(p<r->pos && r->state!=H_BODY){
		switch(r->state){
			case H_PARSING:
				/* find lf */
				p=q_memchr(p, '\n', r->pos-r->parsed);
				if (p){
					p++;
					r->state=H_LF;
				}else{
					p=r->pos;
				}
				break;
				
			case H_LF:
				/* terminate on LF CR LF or LF LF */
				if (*p=='\r'){
					r->state=H_LFCR;
				}else if (*p=='\n'){
					/* found LF LF */
					r->state=H_BODY;
					r->body=p+1;
				}else r->state=H_PARSING;
				p++;
				break;
			
			case H_LFCR:
				if (*p=='\n'){
					/* found LF CR LF */
					r->state=H_BODY;
					r->body=p+1;
				}else r->state=H_PARSING;
				p++;
				break;
				
			default:
				fprintf(stderr, "BUG: unexpected state %d\n", r->state);
				abort();
		}
	}
	
	r->parsed=p;
	return bytes;
}




void tcp_receive_loop(int unix_sock)
{
	struct tcp_req req;
	int bytes;
	long size;
	int n;
	long id;
	int s;
	long state;
	long response[2];
	
	
	
	
	/* listen on the unix socket for the fd */
	for(;;){
			n=receive_fd(unix_sock, &id, sizeof(id), &s);
			if (n<0){
				if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR){
					continue;
				}else{
					fprintf(stderr, "ERROR: tcp_receive_loop: read_fd: %s\n",
							strerror(errno));
					abort(); /* big error*/
				}
			}
			if (n==0){
					fprintf(stderr, 
							"WARNING: tcp_receive_loop: 0 bytes read\n");
					continue;
			}
			fprintf(stderr, "received n=%d id=%ld, fd=%d\n", n, id, s);
			if (s==-1) {
					fprintf(stderr, "ERROR: tcp_receive_loop: read_fd:"
									"no fd read\n");
					state=-1;
					goto end_req; /* ?*/
			}
		
		init_tcp_req(&req, s);
		
		
	again:
		while(req.body==0){
			bytes=tcp_read_headers(&req);
			/* if timeout state=0; goto end__req; */
			fprintf(stderr, "read= %d bytes, parsed=%d, state=%d, error=%d\n",
					bytes, req.parsed-req.buf, req.state, req.error );
			if (bytes==-1){
				fprintf(stderr, "ERROR!\n");
				state=-1;
				goto end_req;
			}
			if (bytes==0){
				fprintf(stderr, "EOF!\n");
				state=-1;
				goto end_req;
			}

		}
		fprintf(stderr, "end of header part\n");
		fprintf(stderr, "headers:\n%.*s.\n",req.body-req.buf, req.buf);

		/* just debugging*/
		state=0;
		goto end_req;
		/* parse headers ... */
		
		/* get body */
		
		/* copy request */

		/* prepare for next request */
		size=req.pos-req.body;
		if (size) memmove(req.buf, req.body, size);
		fprintf(stderr, "\npreparing for new request, kept %ld bytes\n", size);
		req.pos=req.buf+size;
		req.parsed=req.buf;
		req.body=0;
		req.error=TCP_REQ_OK;
		req.state=H_PARSING;
	
		/* process last req. */
		
		goto again;
		
	end_req:
			fprintf(stderr, "end req\n");
		/* release req & signal the parent */
		if (s!=-1) close(s);
		/* errno==EINTR, EWOULDBLOCK a.s.o todo */
		response[0]=id;
		response[1]=state;
		write(unix_sock, response, sizeof(response));
		
	
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
