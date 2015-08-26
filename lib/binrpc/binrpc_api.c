/*
 * Copyright (C) 2006 iptelorg GmbH
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
 
/**
 * send commands using binrpc
 *
 */

#include <sys/types.h>
#include <sys/uio.h>
#include <stdlib.h> /* realloc, rand ... */
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h> /* isprint */
#include <time.h> /* time */
/* #include <stropts.h>  - is this really needed? --andrei */

#include "../../modules/ctl/ctl_defaults.h" /* default socket & port */
#include "../../modules/ctl/init_socks.h"
#include "../../modules/ctl/binrpc.c" /* ugly hack */

#include "binrpc_api.h"


#define IOVEC_CNT 20
#define TEXT_BUFF_ALLOC_CHUNK   4096
#define FATAL_ERROR       -1

#ifndef NAME
#define NAME    "binrpc_api"
#endif

#define binrpc_malloc internal_malloc
#define binrpc_realloc internal_realloc
#define binrpc_free internal_free

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

#ifndef INT2STR_MAX_LEN
#define INT2STR_MAX_LEN  (19+1+1) /* 2^64~= 16*10^18 => 19+1 digits + \0 */
#endif

static void* (*internal_malloc)(size_t size) = malloc;
static void* (*internal_realloc)(void* ptr, size_t size) = realloc;
static void (*internal_free)(void* ptr) = free;

static char binrpc_last_errs[1024] = "";
static int verbose = 0;

char *binrpc_get_last_errs() 
{
    return binrpc_last_errs;
}

void binrpc_clear_last_err()
{
    binrpc_last_errs[0] = '\0';
}

void binrpc_set_mallocs(void* _malloc, void* _realloc, void* _free)
{
    internal_malloc = _malloc;
    internal_realloc = _realloc;
    internal_free = _free;
}

static int gen_cookie()
{
	return rand();
}

static void hexdump(unsigned char* buf, int len, int ascii)
{
	int r, i;
	
	/* dump it in hex */
	for (r=0; r<len; r++){
		if ((r) && ((r%16)==0)){
			if (ascii){
				putchar(' ');
				for (i=r-16; i<r; i++){
					if (isprint(buf[i]))
						putchar(buf[i]);
					else
						putchar('.');
				}
			}
			putchar('\n');
		}
		printf("%02x ", buf[r]);
	};
	if (ascii){
		for (i=r;i%16; i++)
			printf("   ");
		putchar(' ');
		for (i=16*(r/16); i<r; i++){
			if (isprint(buf[i]))
				putchar(buf[i]);
			else
				putchar('.');
		}
	}
	putchar('\n');
}

/* opens,  and  connects on a STREAM unix socket
 * returns socket fd or -1 on error */
static int connect_unix_sock(char* name, int type, struct sockaddr_un* mysun,
                      char* reply_socket, char* sock_dir)
{
	struct sockaddr_un ifsun;
	int s;
	int len;
	int ret;
	int retries;	
	
	retries=0;
	s=-1;
	memset(&ifsun, 0, sizeof (struct sockaddr_un));
	len=strlen(name);
	if (len>UNIX_PATH_MAX){
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1, 
				"connect_unix_sock: name too long "
				"(%d > %d): %s", len, UNIX_PATH_MAX, name);
		goto error;
	}
	ifsun.sun_family=AF_UNIX;
	memcpy(ifsun.sun_path, name, len);
#ifdef HAVE_SOCKADDR_SA_LEN
	ifsun.sun_len=len;
#endif
	s=socket(PF_UNIX, type, 0);
	if (s==-1){
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1, 
			"connect_unix_sock: cannot create unix socket"
			" %s: %s [%d]", 
			name, strerror(errno), errno);
		goto error;
	}
	if (type==SOCK_DGRAM){
		/* we must bind so that we can receive replies */
		if (reply_socket==0){
			if (sock_dir==0)
				sock_dir="/tmp";
retry:
			ret=snprintf(mysun->sun_path, UNIX_PATH_MAX, "%s/" NAME "_%d",
							sock_dir, rand()); 
			if ((ret<0) ||(ret>=UNIX_PATH_MAX)){
				snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1, 
							"connect_unix_sock: buffer overflow while trying to"
							"generate unix datagram socket name");
				goto error;
			}
		}else{
			if (strlen(reply_socket)>UNIX_PATH_MAX){
				snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1, 
							"connect_unix_sock: buffer overflow while trying to"
							"use the provided unix datagram socket name (%s)",
							reply_socket);
				goto error;
			}
			strcpy(mysun->sun_path, reply_socket);
		}
		mysun->sun_family=AF_UNIX;
		if (bind(s, (struct sockaddr*)&mysun, sizeof(struct sockaddr_un))==-1){
		//if (bind(s, mysun, sizeof(mysun))==-1){
			if (errno==EADDRINUSE && (reply_socket==0) && (retries < 10)){
				retries++;
				/* try another one */
				goto retry;
			}
			snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1, 
					"connect_unix_sock: could not bind the unix socket to"
					" %s: %s (%d)",
					mysun->sun_path, strerror(errno), errno);
			goto error;
		}
	}
	if (connect(s, (struct sockaddr *)&ifsun, sizeof(ifsun))==-1){
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1, 
				"connect_unix_sock: connect(%s): %s [%d]",
				name, strerror(errno), errno);
		goto error;
	}
	return s;
error:
	if (s!=-1) close(s);
	return FATAL_ERROR;
}

static int connect_tcpudp_socket(char* address, int port, int type)
{
	struct sockaddr_in addr;
	struct hostent* he;
	int sock;
	
	sock=-1;
	/* resolve destination */
	he=gethostbyname(address);
	if (he==0){
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1, 
				"connect_tcpudp_socket: could not resolve %s", address);
		goto error;
	}
	/* open socket*/
	addr.sin_family=he->h_addrtype;
	addr.sin_port=htons(port);
	memcpy(&addr.sin_addr.s_addr, he->h_addr_list[0], he->h_length);
	
	sock = socket(he->h_addrtype, type, 0);
	if (sock==-1){
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1, 
			"connect_tcpudp_socket: socket: %s", strerror(errno));
		goto error;
	}
	if (connect(sock, (struct sockaddr*) &addr, sizeof(struct sockaddr))!=0){
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1, 
				"connect_tcpudp_socket: connect: %s", strerror(errno));
		goto error;
	}
	return sock;
error:
	if (sock!=-1) close(sock);
	return FATAL_ERROR;
}

/* on exit cleanup */
static void cleanup(struct sockaddr_un* mysun)
{	
	if (mysun->sun_path[0] != '\0') {
		if (unlink(mysun->sun_path) < 0) {
			fprintf(stderr, "ERROR: failed to delete %s: %s\n",
					mysun->sun_path, strerror(errno));
		}
	}
}

int binrpc_open_connection(struct binrpc_handle* handle, char* name, int port, int proto,
		    char* reply_socket, char* sock_dir)
{
	struct sockaddr_un mysun;
		
	binrpc_last_errs[0] = '\0';
	binrpc_last_errs[sizeof(binrpc_last_errs)-1] = '\0';  /* snprintf safe terminator */

	handle->socket = -1;
	handle->buf = NULL;
	mysun.sun_path[0] = '\0';
	
	/* init the random number generator */
	srand(getpid()+time(0)); /* we don't need very strong random numbers */
	
	if (name == NULL) {
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
			"open_connection: invalid IP address or socket name");
		goto error;
	}
	
	handle->proto = proto;
	switch(proto) {
		case UDP_SOCK:
		case TCP_SOCK:
			if (port == 0) {
				port=DEFAULT_CTL_PORT;
			}
			
			handle->sock_type = (proto == UDP_SOCK) ? SOCK_DGRAM : SOCK_STREAM;
			if ((handle->socket = connect_tcpudp_socket(name, port, handle->sock_type)) < 0) {
				goto error;
			}
			break;
		case UNIXS_SOCK:
		case UNIXD_SOCK:
			handle->sock_type = (proto == UNIXD_SOCK) ? SOCK_DGRAM : SOCK_STREAM;
			if ((handle->socket = connect_unix_sock(name, handle->sock_type, &mysun, reply_socket, sock_dir)) < 0) {
				goto error;
			}
			break;
		case UNKNOWN_SOCK:
			snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
				"open_connection: Bad socket type for %s\n", name); /*vm zmenit*/
			goto error;
	}
	if (handle->sock_type == SOCK_DGRAM) {
		handle->buf_size = 8192;  /* max size of datagram, < SSIZE_MAX, TODO: does a platform dependent constant exist ? */
	}
	else {
		handle->buf_size = BINRPC_MAX_HDR_SIZE;	
	}
	handle->buf = binrpc_malloc(handle->buf_size);
	if (!handle->buf) {
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
			"open_connection: not enough memory to allocate buffer. Needed %d bytes", handle->buf_size);	
		binrpc_close_connection(handle);
	}
	cleanup(&mysun);
	return 0;
	
error:
	cleanup(&mysun);
	return FATAL_ERROR;
}

int binrpc_open_connection_url(struct binrpc_handle* handle, char* url) {
	static char name[100];
	char *c, *c2, *rpl_sock;
	int port, proto, i;
	handle->socket = -1;
	handle->buf = NULL;
	/* parse proto:name:port|unixd_sock */
	c = url;
	if (strncasecmp(c, "udp:", 4) == 0)
		proto = UDP_SOCK;
	else if (strncasecmp(c, "tcp:", 4) == 0)
		proto = TCP_SOCK;
	else if (strncasecmp(c, "unix:", 5) == 0 || strncasecmp(c, "unixs:", 6) == 0)
		proto = UNIXS_SOCK;
	else if (strncasecmp(c, "unixd:", 6) == 0)
		proto = UNIXD_SOCK;
	else {
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
			"open_connection_url: bad protocol in '%s'", c);
		return FATAL_ERROR;	
	}
	while (*c != ':') c++;
	c++;
	c2 = strchr(c, ':');
	if (!c2)
		c2 = c + strlen(c);
	if (c2 - c > sizeof(name)-1) {
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
			"open_connection_url: name is too long '%s'", c);
		return FATAL_ERROR;	
	}
	for (i=0; c<c2; c++, i++) {
		name[i] = *c;
	}
	name[i] = '\0';
	if (strlen(name) == 0) {
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
			"open_connection_url: name is not specified in '%s'", url);
		return FATAL_ERROR;	
	}
	c = c2;
	if (*c == ':') c++;

	port = 0;
	rpl_sock = NULL;
	switch (proto) {
		case UNIXD_SOCK:
			if (strlen(c) != 0)
				rpl_sock = c;
			break;
		case UNIXS_SOCK:
			break;
		default:
			port = atol(c);
			if (port == 0) {
				snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
					"open_connection_url: port is not specified in '%s'", url);
				return FATAL_ERROR;	
			}
			break;
	}	
	return binrpc_open_connection(handle, name, port, proto, rpl_sock, NULL);
}

void binrpc_close_connection(struct binrpc_handle* handle)
{
	if (handle->socket != -1) {
		close(handle->socket);
		handle->socket = -1;
	}
	if (handle->buf) {
		binrpc_free(handle->buf);
		handle->buf = NULL;
	}
}

/* returns: -1 on error, number of bytes written on success */
static int send_binrpc_cmd(struct binrpc_handle* handle, struct binrpc_pkt *pkt, int cookie)
{
	struct iovec v[IOVEC_CNT];
	unsigned char msg_hdr[BINRPC_MAX_HDR_SIZE];
	int n;
	
	if ((n=binrpc_build_hdr(BINRPC_REQ, binrpc_pkt_len(pkt), cookie, msg_hdr,
							BINRPC_MAX_HDR_SIZE)) < 0) {
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1, 
			"send_binrpc_cmd: build header error: %s",
			binrpc_error(n));
		return FATAL_ERROR;
	}
	v[0].iov_base=msg_hdr;
	v[0].iov_len=n;
	v[1].iov_base=pkt->body;
	v[1].iov_len=binrpc_pkt_len(pkt);
write_again:
	if ((n=writev(handle->socket, v, 2))<0){
		if (errno==EINTR)
			goto write_again;
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1, 
			"send_binrpc_cmd: send packet failed: %s (%d)", strerror(errno), errno);
		return FATAL_ERROR;
	}
	return n;
}


/* reads the whole reply
 * returns < 0 on error, reply size on success + initializes resp_handle */
static int get_reply(struct binrpc_handle *handle, 
			int cookie, 
			struct binrpc_response_handle *resp_handle)
{
	unsigned char *crt, *hdr_end;
	int n, ret, tl;
	
	ret = 0;
	resp_handle->reply_buf = NULL;
	hdr_end = crt = handle->buf;
	
	do {		
		n = read(handle->socket, crt, handle->buf_size - (crt-handle->buf));
		if (n <= 0){
			if (errno==EINTR)
				continue;
			if (n == 0)
				snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
					"get_reply: read unexpected EOF: received %d bytes"
					" of reply", (int)(long)(crt - handle->buf));
			else
				snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
					"get_reply: read reply failed: %s (%d)",
					strerror(errno), errno);
			return FATAL_ERROR;
		}
		if (verbose >= 3){
			/* dump it in hex */
			printf("received %d bytes in reply (@offset %d):\n", 
			       n, (int)(crt-handle->buf));
			hexdump(crt, n, 1);
		}
		crt += n;
		hdr_end = binrpc_parse_init(&resp_handle->in_pkt, handle->buf, crt - handle->buf, &ret);
		
	} while (ret == E_BINRPC_MORE_DATA);
	if (ret < 0){
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
			"get_reply: reply parsing error: %s",
			binrpc_error(ret));
		return FATAL_ERROR;
	}

	if (verbose>1){
		printf("new packet: type %02x, len %d, cookie %02x\n",
				resp_handle->in_pkt.type, resp_handle->in_pkt.tlen, resp_handle->in_pkt.cookie);
	}
	if (resp_handle->in_pkt.cookie!=cookie){
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
			"get_reply: reply parsing error: "
			"cookie doesn't match: sent: %02x, received: %02x", 
			cookie, resp_handle->in_pkt.cookie);
		return FATAL_ERROR;
	}

	/* we know total size and we can allocate buffer for received data */
	tl = resp_handle->in_pkt.tlen;
	
	if (handle->sock_type == SOCK_DGRAM) {
		/* we must read all datagram in one read call, otherwise unread part is truncated and lost. Read will block execution */
		if (crt - hdr_end < tl) {
			snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
				"get_reply: datagram truncated. Received: %ld, Expected: %d.",
				(long int)(crt-hdr_end), tl);
			return FATAL_ERROR;		
		}
	}
	if (crt - hdr_end > tl) {
		/* header contains probably data from next message, in case of STREAM it could be unread but it's waste of time */
		crt = hdr_end + tl;	
	}
	
	resp_handle->reply_buf = (unsigned char *) binrpc_malloc(tl);
	if (!resp_handle->reply_buf) {
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
			"get_reply: not enough memory to allocate reply buffer. %d bytes needed.",
			resp_handle->in_pkt.tlen);
		return FATAL_ERROR;
	}
	crt = resp_handle->reply_buf + (crt-hdr_end);
	memcpy(resp_handle->reply_buf, hdr_end, crt - resp_handle->reply_buf);
	tl -= crt - resp_handle->reply_buf;
	while (tl > 0) {
		n=read(handle->socket, crt, tl);
		if (n < 0){
			if (errno==EINTR)
				continue;
			snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
				"get_reply: read reply failed: %s (%d)",
				strerror(errno), errno);
			binrpc_free(resp_handle->reply_buf);
			resp_handle->reply_buf = NULL;
			return FATAL_ERROR;
		}
		if (verbose >= 3){
			/* dump it in hex */
			printf("received %d bytes in reply (@offset %d):\n", 
			       n, (int)(crt-resp_handle->reply_buf));
			hexdump(crt, n, 1);
		}
		crt += n;
		tl -= n;
	}
	
	return (int)(crt-resp_handle->reply_buf);
}

int binrpc_send_command_ex(
	struct binrpc_handle* handle, struct binrpc_pkt *pkt, 
	struct binrpc_response_handle *resp_handle)
{
	int cookie;
	
	cookie = gen_cookie();
	if (send_binrpc_cmd(handle, pkt, cookie) < 0) {
		return FATAL_ERROR;
	}
	/* read reply */
	memset(&resp_handle->in_pkt, 0, sizeof(resp_handle->in_pkt));
	if (get_reply(handle, cookie, resp_handle) < 0) {
		return FATAL_ERROR;
	}
	
	/* normal exit */
	return 0;
}

static int parse_arg(struct binrpc_val* v, char* arg)
{
	int i=0;
	double f;
	char* tmp;
	int len;

	f = 0.0;

	if (*arg)
		i=strtol(arg, &tmp, 10);
	else
		tmp = 0;
	if ((tmp==0) || (*tmp)){
		if (*arg)
			f=strtod(arg, &tmp);
		if ((tmp==0) || (*tmp)){
			/* not an int or a float => string */
			len=strlen(arg);
			if ((len>=2) && (arg[0]=='s') && (arg[1]==':')){
				tmp=&arg[2];
				len-=2;
			}else{
				tmp=arg;
			}
			v->type=BINRPC_T_STR;
			v->u.strval.s=tmp;
			v->u.strval.len=len;
		}else{ /* float */
			v->type=BINRPC_T_DOUBLE;
			v->u.fval=f;
		}
	}else{ /* int */
		v->type=BINRPC_T_INT;
		v->u.intval=i;
	}
	return 0;
}


/* parse the body into a malloc allocated,  binrpc_val array */
int binrpc_send_command(
	struct binrpc_handle* handle, char* method, char** args, int arg_count,
	struct binrpc_response_handle *resp_handle)
{
	struct binrpc_pkt req_pkt;
	struct binrpc_val v;
	int i, size, res = FATAL_ERROR, ret = 0;
	unsigned char *req_buf = NULL;

	memset(&resp_handle->in_pkt, 0, sizeof(resp_handle->in_pkt));	
	if (!method || strlen(method) == 0) {
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
			"send_command: method name not specified");
		goto fail;
	
	}
	size = BINRPC_MIN_RECORD_SIZE + 8 + strlen(method) + 1; /*max.possible optional value len */	
	for (i=0; i<arg_count; i++) {
		if (parse_arg(&v, args[i]) < 0)
			goto fail;
		switch (v.type) {
			case BINRPC_T_STR:
				size += v.u.strval.len + 1;
				break;
			case BINRPC_T_INT:
			case BINRPC_T_DOUBLE:
				size += sizeof(int);
				break;
			default:
				snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
					"BUG: send_command: unexpected value type");
				goto fail;
		}
		size +=  BINRPC_MIN_RECORD_SIZE + 8;
	}
	req_buf = binrpc_malloc(size);
	if (!req_buf) {
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
			"send_command: not enough memory to allocate buffer. Needed %d bytes", size);
		goto fail;
	}
	if ((ret = binrpc_init_pkt(&req_pkt, req_buf, size)) < 0) goto fail2;

	if ((ret = binrpc_addstr(&req_pkt, method, strlen(method))) < 0) goto fail2;

	for (i=0; i<arg_count; i++) {
		if (parse_arg(&v, args[i]) < 0)
			goto fail;
		switch (v.type) {
			case BINRPC_T_STR:
				if ((ret = binrpc_addstr(&req_pkt,  v.u.strval.s,  v.u.strval.len)) < 0) goto fail2;
				break;
			case BINRPC_T_INT:
				if ((ret = binrpc_addint(&req_pkt, v.u.intval)) < 0) goto fail2;
				break;
			case BINRPC_T_DOUBLE:
				if ((ret = binrpc_adddouble(&req_pkt, v.u.fval)) < 0) goto fail2;
				break;
			default:
				break;
		}
	}

	if (binrpc_send_command_ex(handle, &req_pkt, resp_handle) < 0) {
		goto fail;
	}
	res = 0;
fail:	
	if (req_buf) binrpc_free(req_buf);
	return res;	
fail2:
	snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
		"send_command: error when preparing params: %s", binrpc_error(ret));
	goto fail;
}

void binrpc_release_response(struct binrpc_response_handle *resp_handle) {
	if (resp_handle->reply_buf) {
		binrpc_free(resp_handle->reply_buf);
		resp_handle->reply_buf = NULL;
	}
}

int binrpc_get_response_type(struct binrpc_response_handle *resp_handle)
{
	switch(resp_handle->in_pkt.type) {
		case BINRPC_FAULT:
			return 1;
			break;
		case BINRPC_REPL:
			return 0;
			break;
		default:
			snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
				"BUG: get_response_type: not a reply");
			return FATAL_ERROR;
	}
}

/* parses strings like "bla bla %v 10%% %v\n test=%v",
 * and stops at each %v,  returning  a pointer after the %v, setting *size
 * to the string length (not including %v) and *type to the corresponding
 * BINRPC type (for now only BINRPC_T_ALL).
 * To escape a '%', use "%%", and check for type==-1 (which means skip an call
 *  again parse_fmt).
 * Usage:
 *        n="test: %v,%v,%v\n";
 *        while(*n){
 *          s=n;
 *          n=parse_fmt(n, &type, &size);
 *          printf("%.*s", size, s);
 *          if (type==-1)
 *            continue;
 *          else 
 *             printf("now we should get & print an object of type %d\n", type)
 *        }
 */
static char* parse_fmt(char* fmt, int* type, int* size)
{
	char* s;

	s=fmt;
	do{
		for(;*fmt && *fmt!='%'; fmt++);
		if (*fmt=='%'){
			switch(*(fmt+1)){
				case 'v':
					*type=BINRPC_T_ALL;
					*size=(int)(fmt-s);
					return (fmt+2);
					break;
				case '%':
					/* escaped % */
					*size=(int)(fmt-s)+1;
					*type=-1; /* skip */
					return (fmt+2);
					break;
			}
		}
	}while(*fmt);
	*type=-1; /* no value */
	*size=(fmt-s);
	return fmt;
}

static void print_binrpc_val(struct binrpc_val* v, int ident)
{
	int r;

	if ((v->type==BINRPC_T_STRUCT) && !v->u.end)
		ident--; /* fix to have strut beg. idented differently */
	for (r=0; r<ident; r++) putchar('	');
	if (v->name.s){
		printf("%.*s: ", v->name.len, v->name.s);
	}
	switch(v->type){
		case BINRPC_T_INT:
			printf("%d", v->u.intval);
			break;
		case BINRPC_T_STR:
		case BINRPC_T_BYTES:
			printf("%.*s", v->u.strval.len, v->u.strval.s);
			break;
		case BINRPC_T_ARRAY:
			printf("%c", (v->u.end)?']':'[');
			break;
		case BINRPC_T_STRUCT:
			printf("%c", (v->u.end)?'}':'{');
			break;
			default:
				printf("ERROR: unknown type %d\n", v->type);
	}
}

int binrpc_print_response(struct binrpc_response_handle *resp_handle, char* fmt)
{
	unsigned char* p;
	unsigned char* end;
	struct binrpc_val val;
	int ret;
	int rec;
	char *f;
	char* s;
	int f_size;
	int fmt_has_values;
	
	if (!resp_handle) {
		goto error;
	}
	resp_handle->in_pkt.offset = resp_handle->in_pkt.in_struct = resp_handle->in_pkt.in_array = 0;

	p=resp_handle->reply_buf;
	end=p+resp_handle->in_pkt.tlen;
	rec=0;
	f=fmt;
	fmt_has_values=0;
	/* read body */
	while(p<end){
		if (f){
					
			do{
				if (*f==0)
					f=fmt; /* reset */
				s=f;
				f=parse_fmt(f, &val.type, &f_size);
				printf("%.*s", f_size, s);
				if (val.type!=-1){
					fmt_has_values=1;
					goto read_value;
				}
			}while(*f || fmt_has_values);
			val.type=BINRPC_T_ALL;
		}else{
			val.type=BINRPC_T_ALL;
		}
read_value:
		val.name.s=0;
		val.name.len=0;
		p=binrpc_read_record(&resp_handle->in_pkt, p, end, &val, 0, &ret);
		if (ret<0){
			if (fmt)
				putchar('\n');
			/*if (ret==E_BINRPC_MORE_DATA)
				goto error_read_again;*/
			if (ret==E_BINRPC_EOP){
				printf("end of message detected\n");
				break;
			}
			snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
					"error while parsing the record %d,"
					" @%d: %02x : %s", rec,
					resp_handle->in_pkt.offset, *p, binrpc_error(ret));
			goto error;
		}
		rec++;
		if (fmt){
			print_binrpc_val(&val, 0);
		}else{
			print_binrpc_val(&val, resp_handle->in_pkt.in_struct+resp_handle->in_pkt.in_array);
			putchar('\n');
		}
	}
	if (fmt && *f){
		/* print the rest, with empty values */
		while(*f){
			s=f;
			f=parse_fmt(f, &val.type, &f_size);
			printf("%.*s", f_size, s);
		}
	}
	return 0;
error:
	return FATAL_ERROR;
/*error_read_again:
	snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
		"ERROR: more data needed");
	return -2;
	*/
}


void binrpc_free_rpc_array(struct binrpc_val* a, int size)
{
	int r;
	for (r=0; r<size; r++){
		if (a[r].name.s)
			binrpc_free(a[r].name.s);
		if ((a[r].type==BINRPC_T_STR || a[r].type==BINRPC_T_BYTES) &&
				a[r].u.strval.s){
			binrpc_free(a[r].u.strval.s);
		}
	}
	binrpc_free(a);
}


#define VAL_ARRAY_CHUNK 100
int binrpc_parse_response(struct binrpc_val** vals, int* val_count,
	struct binrpc_response_handle *resp_handle)
{
	struct binrpc_val val;
	unsigned char *p, *end;
	int ret, i;

	resp_handle->in_pkt.offset = resp_handle->in_pkt.in_struct = resp_handle->in_pkt.in_array = 0;

	i=0;
	if (*val_count==0){
		*val_count=VAL_ARRAY_CHUNK; /* start with a reasonable size */
	}
	*vals = (struct binrpc_val*) binrpc_malloc(*val_count*sizeof(**vals));
	if (*vals == 0)
		goto error_mem;
	p = resp_handle->reply_buf;
	end = p + resp_handle->in_pkt.tlen;
	
	/* read body */
	while(p < end){
		val.type = BINRPC_T_ALL;
		val.name.s = 0;
		val.name.len = 0;
		p = binrpc_read_record(&resp_handle->in_pkt, p, end, &val, 0, &ret);
		if (ret<0){
			if (ret==E_BINRPC_EOP){
				break;
			}
			snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
					"ERROR while parsing the record %d,"
					" @%d: %02x : %s", i,
					resp_handle->in_pkt.offset, *p, binrpc_error(ret));
			goto error;
		}
		if (i >= *val_count){
			struct binrpc_val *t;
			t= (struct binrpc_val*) binrpc_realloc(*vals, (VAL_ARRAY_CHUNK+(*val_count))*sizeof(**vals));
			if (t==0)
				goto error_mem;
			*vals = t;
			*val_count += VAL_ARRAY_CHUNK;
		}
		(*vals)[i] = val;
		if (val.name.s){
			if (((*vals)[i].name.s=binrpc_malloc(val.name.len+1))==0)
				goto error_mem;
			memcpy((*vals)[i].name.s, val.name.s, val.name.len);
			(*vals)[i].name.s[val.name.len]=0; /* 0-term */
		}
		if (val.u.strval.s){
			if (val.type==BINRPC_T_STR){
				if (((*vals)[i].u.strval.s=
							binrpc_malloc(val.u.strval.len+1))==0)
					goto error_mem;
				memcpy((*vals)[i].u.strval.s, val.u.strval.s,
						val.u.strval.len);
				(*vals)[i].u.strval.s[val.u.strval.len]=0; /* 0-term */
			}else if (val.type==BINRPC_T_BYTES){
				if (((*vals)[i].u.strval.s=binrpc_malloc(val.u.strval.len))==0)
					goto error_mem;
				memcpy((*vals)[i].u.strval.s, val.u.strval.s, 
						val.u.strval.len);
			}
		}
		i++;
	}
	if (i == 0) {
		binrpc_free(*vals);
		*vals = NULL;
	}
	else if (i<*val_count){
/*		do not try to save memory because it causes fragmentation when used ser mem utils and "regualar" memory leak
		struct binrpc_val *t;
		t = (struct binrpc_val*) binrpc_realloc(*vals, i*sizeof(**vals));
		if (t) *vals = t;
*/
	}
	*val_count = i;
	return 0;
error_mem:
	snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
		"parse_response: out of memory");
error:
	if (*vals){
		binrpc_free_rpc_array(*vals, i);
		*vals = NULL;
	}
	*val_count=0;
	return FATAL_ERROR;
}

int binrpc_parse_error_response(
	struct binrpc_response_handle *resp_handle,
	int *err_no,
	char **err) 
{
	struct binrpc_val val;
	unsigned char *p, *end;
	int ret;

	resp_handle->in_pkt.offset = resp_handle->in_pkt.in_struct = resp_handle->in_pkt.in_array = 0;
	p = resp_handle->reply_buf;
	end = p+resp_handle->in_pkt.tlen;
	
	val.type=BINRPC_T_INT;
	val.name.s=0;
	val.name.len=0;
	p = binrpc_read_record(&resp_handle->in_pkt, p, end, &val, 0, &ret);
	if (ret < 0) {
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
			"parse_error_response: error when parsing reply (code): %s", binrpc_error(ret)
		);
		return FATAL_ERROR;
	}
	*err_no = val.u.intval;

	val.type=BINRPC_T_STR;
	p = binrpc_read_record(&resp_handle->in_pkt, p, end, &val, 0, &ret);
	if (ret < 0) {
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
			"parse_error_response: error when parsing reply (str): %s", binrpc_error(ret)
		);
		return FATAL_ERROR;
	}																																		
	*err = val.u.strval.s;  /* it's null terminated */
	return 0;
}

/* returns a pointer to a static buffer containing l in asciiz & sets len */
static inline char* int2str_internal(unsigned int l, int* len)
{
	static char r[INT2STR_MAX_LEN];
	int i;
	
	i=INT2STR_MAX_LEN-2;
	r[INT2STR_MAX_LEN-1]=0; /* null terminate */
	do{
		r[i]=l%10+'0';
		i--;
		l/=10;
	}while(l && (i>=0));
	if (l && (i<0)){
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
			"BUG: int2str_internal: overflow");
	}
	if (len) *len=(INT2STR_MAX_LEN-2)-i;
	return &r[i+1];
}

static int realloc_buf(unsigned char** buf, int* buf_len, int req_len)
{ 
	unsigned char*	tmp_buf;
	int orig_len;
	
	orig_len = (*buf == NULL) ? 0 : strlen((char *) *buf);
	*buf_len += (TEXT_BUFF_ALLOC_CHUNK < req_len) ? TEXT_BUFF_ALLOC_CHUNK + req_len : TEXT_BUFF_ALLOC_CHUNK;
	
	if (*buf == NULL)
		tmp_buf = (unsigned char *) binrpc_malloc(orig_len + *buf_len);
	else
		tmp_buf = (unsigned char *) binrpc_realloc(*buf, orig_len + *buf_len);
	if (tmp_buf == 0) {
		snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
			"ERROR: out of memory");
		return FATAL_ERROR;
	}
	
	*buf = tmp_buf;
	(*buf)[orig_len] = '\0';

	return 0;
}

static inline int str2buffer(unsigned char** buf, int* buf_len, int* pos, 
			     char* data, int data_len)
{
	if (*buf_len < data_len) {
		if(realloc_buf(buf, buf_len, data_len) != 0) {
			return FATAL_ERROR;
		}
	}
	
	memcpy(&(*buf)[*pos], data, data_len);
	*pos += data_len;
	*buf_len -= data_len;
	
	return 0;
}

static inline int char2buffer(unsigned char** buf, int* buf_len, int* pos, 
			      char data)
{
	if (*buf_len < 1) {
		if(realloc_buf(buf, buf_len, 1) != 0) {
			return FATAL_ERROR;
		}
	}
	
	(*buf)[*pos] = data;
	++(*pos);
	--(*buf_len);
	
	return 0;
}

static int val2buffer(struct binrpc_val* v, unsigned char** buf, 
			     int *buf_len, int* pos)
{
	char *number;
	int num_len; 
	 
	if (v->name.s){
		if(str2buffer(buf, buf_len, pos, v->name.s, v->name.len) != 0) {
			return FATAL_ERROR;
		}
		if(str2buffer(buf, buf_len, pos, ": ", strlen(": ")) != 0) {  /* TODO: common format */
			return FATAL_ERROR;
		}
		
	}
	
	switch(v->type){
		case BINRPC_T_INT:
			num_len = 0;
			number = NULL;
			number = int2str_internal(v->u.intval, &num_len);
			if (number == NULL) {
				printf("ERROR: Conversion of %d into string failed.\n", v->type);
				return FATAL_ERROR;
			}
			
			if(str2buffer(buf, buf_len, pos, number, num_len) != 0) {
				return FATAL_ERROR;
			}
			break;
		case BINRPC_T_STR:
		case BINRPC_T_BYTES:
			if(str2buffer(buf, buf_len, pos, v->u.strval.s, v->u.strval.len) != 0) {
				return FATAL_ERROR;
			}
			break;
		case BINRPC_T_ARRAY:
			if(char2buffer(buf, buf_len, pos, (v->u.end) ? ']' : '[') != 0) {
				return FATAL_ERROR;
			}
			break;
		case BINRPC_T_STRUCT:
			if(char2buffer(buf, buf_len, pos, (v->u.end) ? '}' : '{') != 0) {
				return FATAL_ERROR;
			}
			break;
		default:
			printf("ERROR: unknown type %d\n", v->type);
			return FATAL_ERROR;
	};
	
	return 0;
}

int binrpc_response_to_text(
	struct binrpc_response_handle *resp_handle,
	unsigned char** txt_rsp, int* txt_rsp_len, char delimiter)
{
	unsigned char* p;
	unsigned char* end;
	struct binrpc_val val;
	int ret;
	int rec;
	int pos;
	
	pos = 0;
	
	if (!resp_handle) {
		goto error;
	}

	memset(&val, 0, sizeof(struct binrpc_val));
	resp_handle->in_pkt.offset = resp_handle->in_pkt.in_struct = resp_handle->in_pkt.in_array = 0;
	
	p=resp_handle->reply_buf;
	end=p+resp_handle->in_pkt.tlen;
	rec=0;
	
	if (*txt_rsp == NULL) {
		*txt_rsp_len = 0;
		if (realloc_buf(txt_rsp, txt_rsp_len, 0) != 0) {
			goto error;
		} 
	}
	
	/* read body */
	while(p<end){
		val.type=BINRPC_T_ALL;
		val.name.s=0;
		val.name.len=0;
		p = binrpc_read_record(&resp_handle->in_pkt, p, end, &val, 0, &ret);
		if (ret < 0) {
			if (ret == E_BINRPC_EOP) {
				printf("end of message detected\n");
				break;
			}
			snprintf(binrpc_last_errs, sizeof(binrpc_last_errs)-1,
					"ERROR while parsing the record %d,"
					" @%d: %02x : %s", rec,	resp_handle->in_pkt.offset, *p, binrpc_error(ret));
			goto error;
		}
		rec++;
		if (val2buffer(&val, txt_rsp, txt_rsp_len, &pos) != 0) {
			goto error;
		}
		
		if(char2buffer(txt_rsp, txt_rsp_len, &pos, delimiter) != 0) {
			goto error;
		}
	}
	
	/* rewrite last char - we don't need delimiter there */
	(*txt_rsp)[pos-1] = '\0';   
	/*
	if(char2buffer(txt_rsp, txt_rsp_len, &pos, '\0') != 0) {
		goto error;
	}
	*/
	return 0;
error:
	return FATAL_ERROR;
}

int main(int argc, char** argv)
{
	struct binrpc_response_handle resp_handle;
	unsigned char* txt_rsp = NULL;
	int txt_rsp_len = 0;
	struct binrpc_handle handle;
	struct binrpc_val *vals = NULL;
	int cnt, i, err_no;
	char *errs;
	
	if (argc < 2) goto err;
	
	if (binrpc_open_connection_url(&handle, argv[1]) < 0) goto err2;
	if (binrpc_send_command(&handle, argv[2], argv+3, argc-3, &resp_handle) < 0) {
		binrpc_close_connection(&handle);
		goto err2;
	}
	binrpc_close_connection(&handle);

	if (binrpc_response_to_text(&resp_handle, &txt_rsp, &txt_rsp_len, '\n') < 0) goto err3;
	fprintf(stdout, "binrpc_response_to_text():\n--------------------------\n%s\n", txt_rsp);
	
	fprintf(stdout, "\nbinrpc_print_response():\n------------------------\n");
	binrpc_print_response(&resp_handle, NULL);
	
	fprintf(stdout, "\nbinrpc_parse_response():\n------------------------\n");
	cnt = 0;
	switch (binrpc_get_response_type(&resp_handle)) {
		case 0:
			if (binrpc_parse_response(&vals, &cnt, &resp_handle) < 0) goto err3;
			fprintf(stdout, "#Records: %d\n", cnt);
			for (i = 0; i < cnt; i++) {
				fprintf(stdout, "#%.2d: type:%d name:%.*s\n", i, vals[i].type, vals[i].name.len, vals[i].name.s);
			}
			break;
		case 1:
			if (binrpc_parse_error_response(&resp_handle, &err_no, &errs) <0) goto err3;
			fprintf(stdout, "%d %s\n", err_no, errs);
			break;
		default:
			fprintf(stdout, "Unknown response type: %d\n", binrpc_get_response_type(&resp_handle));	
			break;
	}

	if (vals != NULL) {
		binrpc_free_rpc_array(vals, cnt);
	}	
	if (txt_rsp != NULL) {
		binrpc_free(txt_rsp);
	}
	binrpc_release_response(&resp_handle);
	
	return 0;
err:
	fprintf(stderr, "Usage: %s url mathod [params]\n", NAME);
	return -1;
err3:
	if (vals != NULL) {
		binrpc_free_rpc_array(vals, cnt);
	}	
	if (txt_rsp) {
		binrpc_free(txt_rsp);
	}
	binrpc_release_response(&resp_handle);
err2:
	fprintf(stderr, "ERROR: %s\n", binrpc_get_last_errs());
	return -2;
}

