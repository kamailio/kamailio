/*
 * $Id$
 *
 * transaction maintenance functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
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
 *  2004-11-15  t_write_xxx can print whatever avp/hdr
 *  2005-07-14  t_write_xxx specification aligned to use pseudo-variables 
 *              (bogdan)
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
#include <ctype.h>
#include <string.h>

#include "../../str.h"
#include "../../ut.h"
#include "../../dprint.h"
#include "../../items.h"
#include "../../mem/mem.h"
#include "../../parser/parser_f.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_rr.h"
#include "../../parser/contact/parse_contact.h"
#include "../../tsend.h"
#include "t_lookup.h"
#include "t_fwd.h"
#include "t_fifo.h"


/* AF_LOCAL is not defined on solaris */
#if !defined(AF_LOCAL)
#define AF_LOCAL AF_UNIX
#endif
#if !defined(PF_LOCAL)
#define PF_LOCAL PF_UNIX
#endif


/* solaris doesn't have SUN_LEN */
#ifndef SUN_LEN
#define SUN_LEN(sa)	 ( strlen((sa)->sun_path) + \
					 (size_t)(((struct sockaddr_un*)0)->sun_path) )
#endif

int tm_unix_tx_timeout = 2; /* Default is 2 seconds */

#define TWRITE_PARAMS          20
#define TWRITE_VERSION_S       "0.3"
#define TWRITE_VERSION_LEN     (sizeof(TWRITE_VERSION_S)-1)
#define eol_line(_i_)          ( lines_eol[2*(_i_)] )

#define IDBUF_LEN              128
#define ROUTE_BUFFER_MAX       512
#define APPEND_BUFFER_MAX      4096
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

struct append_elem {
	str        name;       /* name / title */
	xl_spec_t  spec;       /* value's spec */
	struct append_elem *next;
};

struct tw_append {
	str name;
	int add_body;
	struct append_elem *elems;
	struct tw_append   *next;
};

struct tw_info {
	str action;
	struct tw_append *append;
};

static struct tw_append *tw_appends;


/* tw_append syntax:
 * tw_append = name:element[;element]
 * element   = (title=pseudo_variable) | msg(body)
 */
int parse_tw_append( modparam_t type, void* val)
{
	struct append_elem *last;
	struct append_elem *elem;
	struct tw_append *app;
	xl_spec_t lspec;
	char *s;
	str  foo;
	int  xl_flags;

	
	if (val==0 || ((char*)val)[0]==0)
		return 0;

	s = (char*)val;
	xl_flags = XL_THROW_ERROR | XL_DISABLE_COLORS;

	/* start parsing - first the name */
	while( *s && isspace((int)*s) )  s++;
	if ( !*s || *s==':')
		goto parse_error;
	foo.s = s;
	while ( *s && *s!=':' && !isspace((int)*s) ) s++;
	if ( !*s || foo.s==s )
		goto parse_error;
	foo.len = s - foo.s;
	/* parse separator */
	while( *s && isspace((int)*s) )  s++;
	if ( !*s || *s!=':')
		goto parse_error;
	s++;

	/* check for name duplication */
	for(app=tw_appends;app;app=app->next)
		if (app->name.len==foo.len && !strncasecmp(app->name.s,foo.s,foo.len)){
			LOG(L_ERR,"ERROR:tm:parse_tw_append: duplicated tw_append name "
				"<%.*s>\n",foo.len,foo.s);
			goto error;
		}

	/* new tw_append structure */
	app = (struct tw_append*)pkg_malloc( sizeof(struct tw_append) );
	if (app==0) {
		LOG(L_ERR,"ERROR:tm:parse_tw_append: no more pkg memory\n");
		goto error;
	}

	/* init the new append */
	app->name = foo;
	last = app->elems = 0;

	/* link the new append */
	app->next = tw_appends;
	tw_appends = app;

	/* parse the elements */
	while (*s) {

		/* skip white spaces */
		while( *s && isspace((int)*s) )  s++;
		if ( !*s )
			goto parse_error;
		/* parse element name */
		foo.s = s;
		while( *s && *s!='=' && *s!=';' && !isspace((int)*s) ) s++;
		if (foo.s==s)
			goto parse_error;
		foo.len = s - foo.s;
		/* skip spaces */
		while( *s && isspace((int)*s) )  s++;
		if ( *s && *s!='=' && *s!=';' )
			goto parse_error;

		/* short element (without name) ? */
		if (*s=='=' ) {
			/* skip '=' */
			s++;

			/* new append_elem structure */
			elem = (struct append_elem*)pkg_malloc(sizeof(struct append_elem));
			if (elem==0) {
				LOG(L_ERR,"ERROR:tm:parse_tw_append: no more pkg memory\n");
				goto error;
			}
			memset( elem, 0, sizeof(struct append_elem));

			/* set and link the element */
			elem->name = foo;
			if (last==0) {
				app->elems = elem;
			} else {
				last->next = elem;
			}
			last = elem;

			/* skip spaces */
			while (*s && isspace((int)*s))
				s++;
		} else {
			/* go back to reparse as value */
			s = foo.s;
			elem = 0;
		}

		/* get value type */
		if ( (foo.s=xl_parse_spec( s, &lspec, xl_flags))==0 )
			goto parse_error;

		/* if short element....which one? */
		if (elem==0) {
			if (lspec.type!=XL_MSG_BODY) {
			LOG(L_ERR,"ERROR:tm:parse_tw_append: short spec '%.*s' unknown"
					"(aceepted only body)\n",foo.s-s, s);
				goto error;
			}
			app->add_body = 1;
		} else {
			elem->spec = lspec;
		}

		/* continue parsing*/
		s = foo.s;

		/* skip spaces */
		while (*s && isspace((int)*s))  s++;
		if (*s && *(s++)!=';')
			goto parse_error;
	}

	/* go throught all elements and make the names null terminated */
	for( elem=app->elems ; elem ; elem=elem->next)
		elem->name.s[elem->name.len] = 0;
	/* make the append name null terminated also */
	app->name.s[app->name.len] = 0;

	return 0;
parse_error:
	LOG(L_ERR,"ERROR:tm:parse_tw_append: parse error in <%s> around "
		"position %ld(%c)\n", (char*)val, (long)(s-(char*)val),*s);
error:
	return -1;
}


static struct tw_append *search_tw_append(char *name, int len)
{
	struct tw_append * app;

	for( app=tw_appends ; app ; app=app->next )
		if (app->name.len==len && !strncasecmp(app->name.s,name,len) )
			return app;
	return 0;
}


int fixup_t_write( void** param, int param_no)
{
	struct tw_info *twi;
	char *s;

	if (param_no==2) {
		twi = (struct tw_info*)pkg_malloc( sizeof(struct tw_info) );
		if (twi==0) {
			LOG(L_ERR,"ERROR:tm:fixup_t_write: no more pkg memory\n");
			return E_OUT_OF_MEM;
		}
		memset( twi, 0 , sizeof(struct tw_info));
		s = (char*)*param;
		twi->action.s = s;
		if ( (s=strchr(s,'/'))!=0) {
			twi->action.len = s - twi->action.s;
			if (twi->action.len==0) {
				LOG(L_ERR,"ERROR:tm:fixup_t_write: empty action name\n");
				return E_CFG;
			}
			s++;
			if (*s==0) {
				LOG(L_ERR,"ERROR:tm:fixup_t_write: empty append name\n");
				return E_CFG;
			}
			twi->append = search_tw_append( s, strlen(s));
			if (twi->append==0) {
				LOG(L_ERR,"ERROR:tm:fixup_t_write: unknown append name "
					"<%s>\n",s);
				return E_CFG;
			}
		} else {
			twi->action.len = strlen(twi->action.s);
		}
		*param=(void*)twi;
	}

	return 0;
}



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
				LOG(L_ERR,"ERROR:tm:write_to_fifo: nobody listening on "
					" [%s] fifo for reading!\n",fifo);
			default:
				LOG(L_ERR,"ERROR:tm:write_to_fifo: failed to open [%s] "
					"fifo : %s\n", fifo, strerror(errno));
		}
		goto error;
	}

	/* write now (unbuffered straight-down write) */
repeat:
	if (writev(fd_fifo, (struct iovec*)lines_eol, 2*cnt)<0) {
		if (errno!=EINTR) {
			LOG(L_ERR, "ERROR:tm:write_to_fifo: writev failed: %s\n",
				strerror(errno));
			close(fd_fifo);
			goto error;
		} else {
			goto repeat;
		}
	}
	close(fd_fifo);

	DBG("DEBUG:tm:write_to_fifo: write completed\n");
	return 1; /* OK */

error:
	return -1;
}



static inline char* add2buf(char *buf, char *end, str *name, str *value)
{
	if (buf+name->len+value->len+2+1>=end)
		return 0;
	memcpy( buf, name->s, name->len);
	buf += name->len;
	*(buf++) = ':';
	*(buf++) = ' ';
	memcpy( buf, value->s, value->len);
	buf += value->len;
	*(buf++) = '\n';
	return buf;
}



static inline char* append2buf( char *buf, int len, struct sip_msg *req, 
				struct append_elem *elem)
{
	xl_value_t value;
	char *end;

	end = buf+len;

	while (elem)
	{
		/* get the value */
		if (xl_get_spec_value(req, &elem->spec, &value)!=0)
		{
			LOG(L_ERR,"ERROR:tm:append2buf: failed to get '%.*s'\n",
				elem->name.len,elem->name.s);
		}

		/* write the value into the buffer */
		buf = add2buf( buf, end, &elem->name, &value.rs);
		if (!buf)
		{
			LOG(L_ERR,"ERROR:tm:append2buf: overflow -> append "
				"exceeded %d len\n",len);
			return 0;
		}

		elem = elem->next;
	}

	return buf;
}


static int assemble_msg(struct sip_msg* msg, struct tw_info *twi)
{
	static char     id_buf[IDBUF_LEN];
	static char     route_buffer[ROUTE_BUFFER_MAX];
	static char     append_buf[APPEND_BUFFER_MAX];
	static char     cmd_buf[CMD_BUFFER_MAX];
	static str      empty_param = {".",1};
	unsigned int      hash_index, label;
	contact_body_t*   cb=0;
	contact_t*        c=0;
	name_addr_t       na;
	rr_t*             record_route;
	struct hdr_field* p_hdr;
	param_hooks_t     hooks;
	int               l;
	char*             s, fproxy_lr;
	str               route, next_hop, append, tmp_s, body, str_uri;

	if(msg->first_line.type != SIP_REQUEST){
		LOG(L_ERR,"ERROR:tm:assemble_msg: called for something else then"
			"a SIP request\n");
		goto error;
	}

	/* parse all -- we will need every header field for a UAS */
	if ( parse_headers(msg, HDR_EOH_F, 0)==-1) {
		LOG(L_ERR,"ERROR:tm:assemble_msg: parse_headers failed\n");
		goto error;
	}

	/* find index and hash; (the transaction can be safely used due 
	 * to refcounting till script completes) */
	if( t_get_trans_ident(msg,&hash_index,&label) == -1 ) {
		LOG(L_ERR,"ERROR:tm:assemble_msg: t_get_trans_ident failed\n");
		goto error;
	}

	 /* parse from header */
	if (msg->from->parsed==0 && parse_from_header(msg)==-1 ) {
		LOG(L_ERR,"ERROR:tm:assemble_msg: while parsing <From:> header\n");
		goto error;
	}

	/* parse the RURI (doesn't make any malloc) */
	msg->parsed_uri_ok = 0; /* force parsing */
	if (parse_sip_msg_uri(msg)<0) {
		LOG(L_ERR,"ERROR:tm:assemble_msg: uri has not been parsed\n");
		goto error;
	}

	/* parse contact header */
	str_uri.s = 0;
	str_uri.len = 0;
	if(msg->contact) {
		if (msg->contact->parsed==0 && parse_contact(msg->contact)==-1) {
			LOG(L_ERR,"ERROR:tm:assemble_msg: error while parsing "
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
			LOG(L_ERR,"ERROR:tm:assemble_msg: failed to parse "
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
				LOG(L_ERR,"ERROR:tm:assemble_msg: failed to parse "
				    "record route uri params\n");
				goto error;
			}
			fproxy_lr = (hooks.uri.lr != 0);
			DBG("DEBUG:tm:assemble_msg: record_route->nameaddr.uri: %.*s\n",
				record_route->nameaddr.uri.len,record_route->nameaddr.uri.s);
			if(fproxy_lr){
				DBG("DEBUG:tm:assemble_msg: first proxy has loose routing\n");
				copy_route(s,route.len,record_route->nameaddr.uri.s,
					record_route->nameaddr.uri.len);
			}
		}
		for(p_hdr = p_hdr->next;p_hdr;p_hdr = p_hdr->next) {
			/* filter out non-RR hdr and empty hdrs */
			if( (p_hdr->type!=HDR_RECORDROUTE_T) || p_hdr->body.len==0)
				continue;

			if(p_hdr->parsed==0 && parse_rr(p_hdr)!=0 ){
				LOG(L_ERR,"ERROR:tm:assemble_msg: "
					"failed to parse <Record-route:> header\n");
				goto error;
			}
			for(record_route=p_hdr->parsed; record_route;
				record_route=record_route->next){
				DBG("DEBUG:tm:assemble_msg: record_route->nameaddr.uri: "
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

	DBG("DEBUG:tm:assemble_msg: calculated route: %.*s\n",
		route.len,route.len ? route.s : "");
	DBG("DEBUG:tm:assemble_msg: next r-uri: %.*s\n",
		str_uri.len,str_uri.len ? str_uri.s : "");

	if ( REQ_LINE(msg).method_value==METHOD_INVITE || 
	(twi->append && twi->append->add_body) ) {
		/* get body */
		if( (body.s = get_body(msg)) == 0 ){
			LOG(L_ERR, "ERROR:tm:assemble_msg: get_body failed\n");
			goto error;
		}
		body.len = msg->len - (body.s - msg->buf);
	} else {
		body = empty_param;
	}

	/* flags & additional headers */
	append.s = s = append_buf;
	if (sizeof(flag_t)*2+12+1 >= APPEND_BUFFER_MAX) {
		LOG(L_ERR,"ERROR:tm:assemble_msg: buffer overflow "
			"while copying flags\n");
		goto error;
	}
	append_str(s,"P-MsgFlags: ",12);
	l = APPEND_BUFFER_MAX - (12+1); /* include trailing `\n'*/

	if (int2reverse_hex(&s, &l, (int)msg->msg_flags) == -1) {
		LOG(L_ERR,"ERROR:tm:assemble_msg: buffer overflow "
		    "while copying optional header\n");
		goto error;
	}
	append_chr(s,'\n');

	if ( twi->append && ((s=append2buf( s, APPEND_BUFFER_MAX-(s-append.s), msg,
	twi->append->elems))==0) )
		goto error;

	/* body separator */
	append_chr(s,'.');
	append.len = s-append.s;

	eol_line(1).s = s = cmd_buf;
	if(twi->action.len+12 >= CMD_BUFFER_MAX){
		LOG(L_ERR,"ERROR:tm:assemble_msg: buffer overflow while "
		    "copying command name\n");
		goto error;
	}
	append_str(s,"sip_request.",12);
	append_str(s,twi->action.s,twi->action.len);
	eol_line(1).len = s-eol_line(1).s;

	eol_line(2)=REQ_LINE(msg).method;     /* method type */
	eol_line(3)=msg->parsed_uri.user;     /* user from r-uri */
	eol_line(4)=msg->parsed_uri.host;     /* domain */

	eol_line(5)=msg->rcv.bind_address->address_str; /* dst ip */

	eol_line(6)=msg->rcv.dst_port==SIP_PORT ?
			empty_param : msg->rcv.bind_address->port_no_str; /* port */

	/* r_uri ('Contact:' for next requests) */
	eol_line(7)=msg->first_line.u.request.uri;

	/* r_uri for subsequent requests */
	eol_line(8)=str_uri.len?str_uri:empty_param;

	eol_line(9)=get_from(msg)->body;		/* from */
	eol_line(10)=msg->to->body;			/* to */
	eol_line(11)=msg->callid->body;		/* callid */
	eol_line(12)=get_from(msg)->tag_value;	/* from tag */
	eol_line(13)=get_to(msg)->tag_value;	/* to tag */
	eol_line(14)=get_cseq(msg)->number;	/* cseq number */

	eol_line(15).s=id_buf;       /* hash:label */
	s = int2str(hash_index, &l);
	if (l+1>=IDBUF_LEN) {
		LOG(L_ERR, "ERROR:tm:assemble_msg: too big hash\n");
		goto error;
	}
	memcpy(id_buf, s, l);
	id_buf[l]=':';
	eol_line(15).len=l+1;
	s = int2str(label, &l);
	if (l+1+eol_line(15).len>=IDBUF_LEN) {
		LOG(L_ERR, "ERROR:tm:assemble_msg: too big label\n");
		goto error;
	}
	memcpy(id_buf+eol_line(15).len, s, l);
	eol_line(15).len+=l;

	eol_line(16) = route.len ? route : empty_param;
	eol_line(17) = next_hop;
	eol_line(18) = append;
	eol_line(19) = body;

	/* success */
	return 1;
error:
	/* 0 would lead to immediate script exit -- -1 returns
	 * with 'false' to script processing */
	return -1;
}


static int write_to_unixsock(char* sockname, int cnt)
{
	int len, e;
	struct sockaddr_un dest;

	if (!sockname) {
		LOG(L_ERR, "ERROR:tm:write_to_unixsock: Invalid parameter\n");
		return E_UNSPEC;
	}

	len = strlen(sockname);
	if (len == 0) {
		DBG("DEBUG:tm:write_to_unixsock: Error - empty socket name\n");
		return -1;
	} else if (len > 107) {
		LOG(L_ERR, "ERROR:tm:write_to_unixsock: Socket name too long\n");
		return -1;
	}

	memset(&dest, 0, sizeof(dest));
	dest.sun_family = PF_LOCAL;
	memcpy(dest.sun_path, sockname, len);
#ifdef HAVE_SOCKADDR_SA_LEN
	dest.sun_len = len;
#endif

	e = connect(sock, (struct sockaddr*)&dest, SUN_LEN(&dest));
#ifdef HAVE_CONNECT_ECONNRESET_BUG
	/*
	 * Workaround for a nasty bug in BSD kernels dated back
	 * to the Berkeley days, so that can be found in many modern
	 * BSD-derived kernels. Workaround should be pretty harmless since
	 * in normal conditions connect(2) can never return ECONNRESET.
	 */
	if ((e == -1) && (errno == ECONNRESET))
		e = 0;
#endif
	if (e == -1) {
		LOG(L_ERR, "ERROR:tm:write_to_unixsock: Error in connect: %s\n",
			strerror(errno));
		return -1;
	}

	if (tsend_dgram_ev(sock, (struct iovec*)lines_eol, 2 * cnt, tm_unix_tx_timeout * 1000) < 0) {
		LOG(L_ERR, "ERROR:tm:write_to_unixsock: writev failed: %s\n",
			strerror(errno));
		return -1;
	}

	return 0;
}


int t_write_req(struct sip_msg* msg, char* vm_fifo, char* info)
{
	if (assemble_msg(msg, (struct tw_info*)info) < 0) {
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
	return 1;
}


int t_write_unix(struct sip_msg* msg, char* socket, char* info)
{
	if (assemble_msg(msg, (struct tw_info*)info) < 0) {
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
	return 1;
}
