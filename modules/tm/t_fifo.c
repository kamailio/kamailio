/*
 * $Id$
 *
 * transaction maintenance functions
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
 *
 * History:
 * -------
 *  2004-02-23  created by splitting it from t_funcs (bogdan)
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/un.h>

#include "../../str.h"
#include "../../ut.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../usr_avp.h"
#include "../../parser/parser_f.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_rr.h"
#include "../../parser/parse_nameaddr.h"
#include "../../parser/contact/parse_contact.h"
#include "t_lookup.h"
#include "t_fwd.h"
#include "../../tsend.h"

int unix_send_timeout = 2; /* Default is 2 seconds */

#define TWRITE_PARAMS          21
#define TWRITE_VERSION_S       "0.2"
#define TWRITE_VERSION_LEN     (sizeof(TWRITE_VERSION_S)-1)
#define eol_line(_i_)          ( lines_eol[2*(_i_)] )

#define IDBUF_LEN              128
#define ROUTE_BUFFER_MAX       512
#define HDRS_BUFFER_MAX        512
#define CMD_BUFFER_MAX         128

#define append_str(_dest,_src,_len) \
	do{ \
		memcpy( (_dest) , (_src) , (_len) );\
		(_dest) += (_len) ;\
	}while(0);

#define append_chr(_dest,_c) \
	*((_dest)++) = _c;

#define copy_route(s,len,rs,rlen) \
	do {\
		if(rlen+len+3 >= ROUTE_BUFFER_MAX){\
			LOG(L_ERR,"vm: buffer overflow while copying new route\n");\
			goto error;\
		}\
		if(len){\
			append_chr(s,','); len++;\
		}\
		append_chr(s,'<');len++;\
		append_str(s,rs,rlen);\
		len += rlen; \
		append_chr(s,'>');len++;\
	} while(0)

static str   lines_eol[2*TWRITE_PARAMS];
static str   eol={"\n",1};

static int sock;

int init_twrite_sock(void)
{
	int flags;

	sock = socket(PF_LOCAL, SOCK_DGRAM, 0);
	if (sock == -1) {
		LOG(L_ERR, "init_twrite_sock: Unable to create socket: %s\n", strerror(errno));
		return -1;
	}

	     /* Turn non-blocking mode on */
	flags = fcntl(sock, F_GETFL);
	if (flags == -1){
		LOG(L_ERR, "init_twrite_sock: fcntl failed: %s\n",
		    strerror(errno));
		close(sock);
		return -1;
	}
		
	if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
		LOG(L_ERR, "init_twrite_sock: fcntl: set non-blocking failed:"
		    " %s\n", strerror(errno));
		close(sock);
		return -1;
	}
	return 0;
}


int init_twrite_lines()
{
	int i;

	/* init the line table */
	for(i=0;i<TWRITE_PARAMS;i++) {
		lines_eol[2*i].s = 0;
		lines_eol[2*i].len = 0;
		lines_eol[2*i+1] = eol;
	}

	/* first line is the version - fill it now */
	eol_line(0).s   = TWRITE_VERSION_S;
	eol_line(0).len = TWRITE_VERSION_LEN;

	return 0;
}


static int inline write_to_fifo(char *fifo, int cnt )
{
	int   fd_fifo;

	/* open FIFO file stream */
	if((fd_fifo = open(fifo,O_WRONLY | O_NONBLOCK)) == -1){
		switch(errno){
			case ENXIO:
				LOG(L_ERR,"ERROR:tm:t_write_req: nobody listening on "
					" [%s] fifo for reading!\n",fifo);
			default:
				LOG(L_ERR,"ERROR:tm:t_write_req: failed to open [%s] "
					"fifo : %s\n", fifo, strerror(errno));
		}
		goto error;
	}

	/* write now (unbuffered straight-down write) */
repeat:
	if (writev(fd_fifo, (struct iovec*)lines_eol, 2*cnt)<0) {
		if (errno!=EINTR) {
			LOG(L_ERR, "ERROR:tm:t_write_req: writev failed: %s\n",
				strerror(errno));
			close(fd_fifo);
			goto error;
		} else {
			goto repeat;
		}
	}
	close(fd_fifo);

	DBG("DEBUG:tm:t_write_req: write completed\n");
	return 1; /* OK */

error:
	return -1;
}


static int assemble_msg(struct sip_msg* msg, char* action)
{
	static char     id_buf[IDBUF_LEN];
	static char     route_buffer[ROUTE_BUFFER_MAX];
	static char     hdrs_buf[HDRS_BUFFER_MAX];
	static char     cmd_buf[CMD_BUFFER_MAX];
	static str      empty_param = {".",1};
	static str      email_attr = {"email",5};
	unsigned int      hash_index, label;
	contact_body_t*   cb=0;
	contact_t*        c=0;
	name_addr_t       na;
	rr_t*             record_route;
	struct hdr_field* p_hdr;
	param_hooks_t     hooks;
	struct usr_avp    *email_avp;
	int               l;
	char*             s, fproxy_lr;
	str               route, next_hop, hdrs, tmp_s, body, str_uri;

	if(msg->first_line.type != SIP_REQUEST){
		LOG(L_ERR,"assemble_msg: called for something else then"
			"a SIP request\n");
		goto error;
	}

	email_avp = 0;
	body = empty_param;

	/* parse all -- we will need every header field for a UAS */
	if ( parse_headers(msg, HDR_EOH, 0)==-1) {
		LOG(L_ERR,"assemble_msg: parse_headers failed\n");
		goto error;
	}

	/* find index and hash; (the transaction can be safely used due 
	 * to refcounting till script completes) */
	if( t_get_trans_ident(msg,&hash_index,&label) == -1 ) {
		LOG(L_ERR,"assemble_msg: t_get_trans_ident failed\n");
		goto error;
	}

	 /* parse from header */
	if (msg->from->parsed==0 && parse_from_header(msg)==-1 ) {
		LOG(L_ERR,"assemble_msg: while parsing <From:> header\n");
		goto error;
	}

	/* parse the RURI (doesn't make any malloc) */
	msg->parsed_uri_ok = 0; /* force parsing */
	if (parse_sip_msg_uri(msg)<0) {
		LOG(L_ERR,"assemble_msg: uri has not been parsed\n");
		goto error;
	}

	/* parse contact header */
	str_uri.s = 0;
	str_uri.len = 0;
	if(msg->contact) {
		if (msg->contact->parsed==0 && parse_contact(msg->contact)==-1) {
			LOG(L_ERR,"assemble_msg: error while parsing "
			    "<Contact:> header\n");
			goto error;
		}
		cb = (contact_body_t*)msg->contact->parsed;
		if(cb && (c=cb->contacts)) {
			str_uri = c->uri;
			if (find_not_quoted(&str_uri,'<')) {
				parse_nameaddr(&str_uri,&na);
				str_uri = na.uri;
			}
		}
	}

	/* str_uri is taken from caller's contact or from header
	 * for backwards compatibility with pre-3261 (from is already parsed)*/
	if(!str_uri.len || !str_uri.s)
		str_uri = get_from(msg)->uri;

	/* parse Record-Route headers */
	route.s = s = route_buffer; route.len = 0;
	fproxy_lr = 0;
	next_hop = empty_param;

	p_hdr = msg->record_route;
	if(p_hdr) {
		if (p_hdr->parsed==0 && parse_rr(p_hdr)!=0 ) {
			LOG(L_ERR,"assemble_msg: while parsing "
			    "'Record-Route:' header\n");
			goto error;
		}
		record_route = (rr_t*)p_hdr->parsed;
	} else {
		record_route = 0;
	}

	if( record_route ) {
		if ( (tmp_s.s=find_not_quoted(&record_route->nameaddr.uri,';'))!=0 &&
		tmp_s.s+1!=record_route->nameaddr.uri.s+
		record_route->nameaddr.uri.len) {
			/* Parse all parameters */
			tmp_s.len = record_route->nameaddr.uri.len - (tmp_s.s-
				record_route->nameaddr.uri.s);
			if (parse_params( &tmp_s, CLASS_URI, &hooks, 
			&record_route->params) < 0) {
				LOG(L_ERR,"assemble_msg: error while parsing "
				    "record route uri params\n");
				goto error;
			}
			fproxy_lr = (hooks.uri.lr != 0);
			DBG("assemble_msg: record_route->nameaddr.uri: %.*s\n",
				record_route->nameaddr.uri.len,record_route->nameaddr.uri.s);
			if(fproxy_lr){
				DBG("assemble_msg: first proxy has loose routing.\n");
				copy_route(s,route.len,record_route->nameaddr.uri.s,
					record_route->nameaddr.uri.len);
			}
		}
		for(p_hdr = p_hdr->next;p_hdr;p_hdr = p_hdr->next) {
			/* filter out non-RR hdr and empty hdrs */
			if( (p_hdr->type!=HDR_RECORDROUTE) || p_hdr->body.len==0)
				continue;

			if(p_hdr->parsed==0 && parse_rr(p_hdr)!=0 ){
				LOG(L_ERR,"assemble_msg: "
				    "while parsing <Record-route:> header\n");
				goto error;
			}
			for(record_route=p_hdr->parsed; record_route;
			    record_route=record_route->next){
				DBG("assemble_msg: record_route->nameaddr.uri: "
				    "<%.*s>\n", record_route->nameaddr.uri.len,
					record_route->nameaddr.uri.s);
				copy_route(s,route.len,record_route->nameaddr.uri.s,
					record_route->nameaddr.uri.len);
			}
		}

		if(!fproxy_lr){
			copy_route(s,route.len,str_uri.s,str_uri.len);
			str_uri = ((rr_t*)msg->record_route->parsed)->nameaddr.uri;
		} else {
			next_hop = ((rr_t*)msg->record_route->parsed)->nameaddr.uri;
		}
	}

	DBG("assemble_msg: calculated route: %.*s\n",
	    route.len,route.len ? route.s : "");
	DBG("assemble_msg: next r-uri: %.*s\n",
	    str_uri.len,str_uri.len ? str_uri.s : "");
	
	if( REQ_LINE(msg).method_value==METHOD_INVITE ) {
		/* get body */
		if( (body.s = get_body(msg)) == 0 ){
			LOG(L_ERR, "assemble_msg: get_body failed\n");
			goto error;
		}
		body.len = msg->len - (body.s - msg->buf);

		/* get email (if any) */
		if ( (email_avp=search_avp( &email_attr ))!=0 &&
		email_avp->val_type!=AVP_TYPE_STR ) {
			LOG(L_WARN, "assemble_msg: 'email' avp found but "
			    "not string -> ignoring it\n");
			email_avp = 0;
		}
	}

	/* additional headers */
	hdrs.s = s = hdrs_buf;
	l = sizeof(flag_t);
	if (l+12+1 >= HDRS_BUFFER_MAX) {
		LOG(L_ERR,"assemble_msg: buffer overflow "
		    "while copying optional header\n");
		goto error;
	}
	append_str(s,"P-MsgFlags: ",12); hdrs.len = 12;
	int2reverse_hex(&s, &l, (int)msg->msg_flags);
	hdrs.len += sizeof(flag_t) - l;
	append_chr(s,'\n'); hdrs.len++;

	for(p_hdr = msg->headers;p_hdr;p_hdr = p_hdr->next) {
		if( !(p_hdr->type&HDR_OTHER) )
			continue;

		if(hdrs.len+p_hdr->name.len+p_hdr->body.len+4 >= HDRS_BUFFER_MAX){
			LOG(L_ERR,"assemble_msg: buffer overflow while "
			    "copying optional header\n");
			goto error;
		}
		append_str(s,p_hdr->name.s,p_hdr->name.len);
		hdrs.len += p_hdr->name.len;
		append_chr(s,':');append_chr(s,' '); hdrs.len+=2;
		append_str(s,p_hdr->body.s,p_hdr->body.len);
		hdrs.len += p_hdr->body.len;
		if(*(s-1) != '\n'){
			append_chr(s,'\n');
			hdrs.len++;
		}
	}

	append_chr(s,'.');
	hdrs.len++;

	eol_line(1).s = s = cmd_buf;
	if(strlen(action)+12 >= CMD_BUFFER_MAX){
		LOG(L_ERR,"assemble_msg: buffer overflow while "
		    "copying command name\n");
		goto error;
	}
	append_str(s,"sip_request.",12);
	append_str(s,action,strlen(action));
	eol_line(1).len = s-eol_line(1).s;

	eol_line(2)=REQ_LINE(msg).method;     /* method type */
	eol_line(3)=msg->parsed_uri.user;     /* user from r-uri */
	eol_line(4)=email_avp?(email_avp->val.str_val):empty_param;  /* email */
	eol_line(5)=msg->parsed_uri.host;     /* domain */

	eol_line(6)=msg->rcv.bind_address->address_str; /* dst ip */

	eol_line(7)=msg->rcv.dst_port==SIP_PORT ?
			empty_param : msg->rcv.bind_address->port_no_str; /* port */

	/* r_uri ('Contact:' for next requests) */
	eol_line(8)=msg->first_line.u.request.uri;

	/* r_uri for subsequent requests */
	eol_line(9)=str_uri.len?str_uri:empty_param;

	eol_line(10)=get_from(msg)->body;		/* from */
	eol_line(11)=msg->to->body;			/* to */
	eol_line(12)=msg->callid->body;		/* callid */
	eol_line(13)=get_from(msg)->tag_value;	/* from tag */
	eol_line(14)=get_to(msg)->tag_value;	/* to tag */
	eol_line(15)=get_cseq(msg)->number;	/* cseq number */

	eol_line(16).s=id_buf;       /* hash:label */
	s = int2str(hash_index, &l);
	if (l+1>=IDBUF_LEN) {
		LOG(L_ERR, "assemble_msg: too big hash\n");
		goto error;
	}
	memcpy(id_buf, s, l);
	id_buf[l]=':';
	eol_line(16).len=l+1;
	s = int2str(label, &l);
	if (l+1+eol_line(16).len>=IDBUF_LEN) {
		LOG(L_ERR, "assemble_msg: too big label\n");
		goto error;
	}
	memcpy(id_buf+eol_line(16).len, s, l);
	eol_line(16).len+=l;

	eol_line(17) = route.len ? route : empty_param;
	eol_line(18) = next_hop;
	eol_line(19) = hdrs;
	eol_line(20) = body;

	/* success */
	return 1;
error:
	/* 0 would lead to immediate script exit -- -1 returns
	 * with 'false' to script processing */
	return -1;
}


static int write_to_unixsock(char* sockname, int cnt)
{
	int len;
	struct sockaddr_un dest;

	if (!sockname) {
		LOG(L_ERR, "write_to_unixsock: Invalid parameter\n");
		return E_UNSPEC;
	}

	len = strlen(sockname);
	if (len == 0) {
		DBG("write_to_unixsock: Error - empty socket name\n");
		return -1;
	} else if (len > 107) {
		LOG(L_ERR, "write_to_unixsock: Socket name too long\n");
		return -1;
	}
	
	memset(&dest, 0, sizeof(dest));
	dest.sun_family = PF_LOCAL;
	memcpy(dest.sun_path, sockname, len);
	
	if (connect(sock, (struct sockaddr*)&dest, SUN_LEN(&dest)) == -1) {
		LOG(L_ERR, "write_to_unixsock: Error in connect: %s\n", strerror(errno));
		return -1;
	}
	
	if (tsend_dgram_ev(sock, (struct iovec*)lines_eol, 2 * cnt, unix_send_timeout * 1000) < 0) {
		LOG(L_ERR, "write_to_unixsock: writev failed: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}


int t_write_req(struct sip_msg* msg, char* vm_fifo, char* action)
{
	if (assemble_msg(msg, action) < 0) {
		LOG(L_ERR, "ERROR:tm:t_write_req: Error int assemble_msg\n");
		return -1;
	}
		
	if (write_to_fifo(vm_fifo, TWRITE_PARAMS) == -1) {
		LOG(L_ERR, "ERROR:tm:t_write_req: write_to_fifo failed\n");
		return -1;
	}
	
	     /* make sure that if voicemail does not initiate a reply
	      * timely, a SIP timeout will be sent out */
	if (add_blind_uac() == -1) {
		LOG(L_ERR, "ERROR:tm:t_write_req: add_blind failed\n");
		return -1;
	}
	return 0;
}


int t_write_unix(struct sip_msg* msg, char* socket, char* action)
{
	if (assemble_msg(msg, action) < 0) {
		LOG(L_ERR, "ERROR:tm:t_write_unix: Error in assemble_msg\n");
		return -1;
	}

	if (write_to_unixsock(socket, TWRITE_PARAMS) == -1) {
		LOG(L_ERR, "ERROR:tm:t_write_unix: write_to_unixsock failed\n");
		return -1;
	}

	     /* make sure that if voicemail does not initiate a reply
	      * timely, a SIP timeout will be sent out */
	if (add_blind_uac() == -1) {
		LOG(L_ERR, "ERROR:tm:t_write_unix: add_blind failed\n");
		return -1;
	}
	return 0;
}
