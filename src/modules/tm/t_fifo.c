/*
 * transaction maintenance functions
 *
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
 *
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
#include "../../mem/mem.h"
#include "../../usr_avp.h"
#include "../../parser/parser_f.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_rr.h"
#include "../../parser/parse_nameaddr.h"
#include "../../parser/parse_hname2.h"
#include "../../parser/contact/parse_contact.h"
#include "../../tsend.h"
#include "config.h"
#include "t_lookup.h"
#include "t_fwd.h"
#include "t_funcs.h"
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


#define TWRITE_PARAMS          20
#define TWRITE_VERSION_S       "0.3"
#define TWRITE_VERSION_LEN     (sizeof(TWRITE_VERSION_S)-1)

#define eol_line_s(_i_)        ( iov_lines_eol[2*(_i_)].iov_base )
#define eol_line_len(_i_)      ( iov_lines_eol[2*(_i_)].iov_len )

#define eol_line(_i_,_s_)      { eol_line_s(_i_) = (_s_).s; \
                                 eol_line_len(_i_) = (_s_).len; }

#define IDBUF_LEN              128
#define ROUTE_BUFFER_MAX       512
#define APPEND_BUFFER_MAX      4096
#define CMD_BUFFER_MAX         128

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

static struct iovec iov_lines_eol[2*TWRITE_PARAMS];
static struct iovec eol={"\n",1};

static int sock;

struct hdr_avp {
	str title;
	int type;
	str sval;
	int ival;
	struct hdr_avp *next;
};

struct tw_append {
	str name;
	int add_body;
	struct hdr_avp *elems;
	struct tw_append *next;
};

struct tw_info {
	str action;
	struct tw_append *append;
};

#define ELEM_TYPE_AVP      "avp"
#define ELEM_TYPE_AVP_LEN  (sizeof(ELEM_TYPE_AVP)-1)
#define ELEM_TYPE_HDR      "hdr"
#define ELEM_TYPE_HDR_LEN  (sizeof(ELEM_TYPE_HDR)-1)
#define ELEM_TYPE_MSG      "msg"
#define ELEM_TYPE_MSG_LEN  (sizeof(ELEM_TYPE_MSG)-1)
#define ELEM_IS_AVP        (1<<0)
#define ELEM_IS_HDR        (1<<1)
#define ELEM_IS_MSG        (1<<2)

#define ELEM_VAL_BODY      "body"
#define ELEM_VAL_BODY_LEN  (sizeof(ELEM_VAL_BODY)-1)


static struct tw_append *tw_appends;


static void print_tw_append( struct tw_append *append)
{
	struct hdr_avp *ha;

	if (!append)
		return;

	DBG("DEBUG:tm:print_tw_append: tw_append name=<%.*s>\n",
		append->name.len,append->name.s);
	for( ha=append->elems ; ha ; ha=ha->next ) {
		DBG("\ttitle=<%.*s>\n",ha->title.len,ha->title.s);
		DBG("\t\tttype=<%d>\n",ha->type);
		DBG("\t\tsval=<%.*s>\n",ha->sval.len,ha->sval.s);
		DBG("\t\tival=<%d>\n",ha->ival);
	}
}


/* tw_append syntax:
 * tw_append = name:element[;element]
 * element   = [title=]value 
 * value     = avp[avp_spec] | hdr[hdr_name] | msg[body] */
int parse_tw_append( modparam_t type, void* val)
{
	struct hdr_field hdr;
	struct hdr_avp *last;
	struct hdr_avp *ha;
	struct tw_append *app;
	int_str avp_name;
	char *s;
	char bar;
	str foo;
	int n;
	int index;
	
	if (val==0 || ((char*)val)[0]==0)
		return 0;
	s = (char*)val;

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
	while( *s && isspace((int)*s) )  s++;
	if ( !*s )
		goto parse_error;

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
	app->name.s = (char*)pkg_malloc( foo.len+1 );
	if (app->name.s==0) {
		LOG(L_ERR,"ERROR:tm:parse_tw_append: no more pkg memory\n");
		goto error;
	}
	memcpy( app->name.s, foo.s, foo.len);
	app->name.len = foo.len;
	app->name.s[app->name.len] = 0;
	last = app->elems = 0;
	app->next = tw_appends;
	tw_appends = app;

	/* parse the elements */
	while (*s) {
		/* parse element title or element type */
		foo.s = s;
		while( *s && *s!='[' && *s!='=' && *s!=';' && !isspace((int)*s) ) s++;
		if ( !*s || foo.s==s)
			goto parse_error;
		foo.len = s - foo.s;
		/* new hdr_avp structure */
		ha = (struct hdr_avp*)pkg_malloc( sizeof(struct hdr_avp) );
		if (ha==0) {
			LOG(L_ERR,"ERROR:tm:parse_tw_append: no more pkg memory\n");
			goto error;
		}
		memset( ha, 0, sizeof(struct hdr_avp));
		if (*s!='[') {
			/* foo must by title or some error -> parse separator */
			while( *s && isspace((int)*s) )  s++;
			if ( !*s || *s!='=')
				goto parse_error;
			s++;
			while( *s && isspace((int)*s) )  s++;
			if ( !*s )
				goto parse_error;
			/* set the title */
			ha->title.s = (char*)pkg_malloc( foo.len+1 );
			if (ha->title.s==0) {
				LOG(L_ERR,"ERROR:tm:parse_tw_append: no more pkg memory\n");
				goto error;
			}
			memcpy( ha->title.s, foo.s, foo.len);
			ha->title.len = foo.len;
			ha->title.s[ha->title.len] = 0;
			/* parse the type now */
			foo.s = s;
			while( *s && *s!='[' && *s!=']' && *s!=';' && !isspace((int)*s) )
				s++;
			if ( *s!='[' || foo.s==s)
				goto parse_error;
			foo.len = s - foo.s;
		}
		/* foo containes the elemet type */
		if ( foo.len==ELEM_TYPE_AVP_LEN &&
		!strncasecmp( foo.s, ELEM_TYPE_AVP, foo.len) ) {
			ha->type = ELEM_IS_AVP;
		} else if ( foo.len==ELEM_TYPE_HDR_LEN &&
		!strncasecmp( foo.s, ELEM_TYPE_HDR, foo.len) ) {
			ha->type = ELEM_IS_HDR;
		} else if ( foo.len==ELEM_TYPE_MSG_LEN &&
		!strncasecmp( foo.s, ELEM_TYPE_MSG, foo.len) ) {
			ha->type = ELEM_IS_MSG;
		} else {
			LOG(L_ERR,"ERROR:tm:parse_tw_append: unknown type <%.*s>\n",
				foo.len, foo.s);
			goto error;
		}
		/* parse the element name */
		s++;
		foo.s = s;
		while( *s && *s!=']' && *s!=';' && !isspace((int)*s) ) s++;
		if ( *s!=']' || foo.s==s )
			goto parse_error;
		foo.len = s - foo.s;
		s++;
		/* process and optimize the element name */
		if (ha->type==ELEM_IS_AVP) {
			/* element is AVP */
			if ( parse_avp_spec( &foo, &n, &avp_name, &index)!=0 ) {
				LOG(L_ERR,"ERROR:tm:parse_tw_append: bad alias spec "
					"<%.*s>\n",foo.len, foo.s);
				goto error;
			}
			if (n&AVP_NAME_STR) {
				/* string name */
				ha->sval.s = (char*)pkg_malloc(avp_name.s.len+1);
				if (ha->sval.s==0) {
					LOG(L_ERR,"ERROR:tm:parse_tw_append: no more pkg mem\n");
					goto error;
				}
				memcpy( ha->sval.s, avp_name.s.s, avp_name.s.len);
				ha->sval.len = avp_name.s.len;
				ha->sval.s[ha->sval.len] = 0;
				if (ha->title.s==0)
					ha->title = ha->sval;
			} else {
				/* ID name - if title is missing, convert the ID to
				 * string and us it a title */
				ha->ival = avp_name.n;
				if (ha->title.s==0) {
					foo.s=int2str((unsigned long)ha->ival, &foo.len);
					ha->title.s = (char*)pkg_malloc( n+1 );
					if (ha->title.s==0) {
						LOG(L_ERR,"ERROR:tm:parse_tw_append: no more pkg "
							"memory\n");
						goto error;
					}
					memcpy( ha->title.s, foo.s, foo.len);
					ha->title.len = foo.len;
					ha->title.s[ha->title.len] = 0;
				}
			}
		} else if (ha->type==ELEM_IS_HDR) {
			/* element is HDR -  try to get it's coded type if defined */
			bar = foo.s[foo.len];
			foo.s[foo.len] = ':';
			/* parse header name */
			if (parse_hname2( foo.s, foo.s+foo.len+1, &hdr)==0) {
				LOG(L_ERR,"BUG:tm_parse_tw_append: parse header failed\n");
				goto error;
			}
			foo.s[foo.len] = bar;
			ha->ival = hdr.type;
			if (hdr.type==HDR_OTHER_T || ha->title.s==0) {
				/* duplicate hdr name */
				ha->sval.s = (char*)pkg_malloc(foo.len+1);
				if (ha->sval.s==0) {
					LOG(L_ERR,"ERROR:tm:parse_tw_append: no more pkg mem\n");
					goto error;
				}
				memcpy( ha->sval.s, foo.s, foo.len);
				ha->sval.len = foo.len;
				ha->sval.s[ha->sval.len] = 0;
				if (ha->title.s==0)
					ha->title = ha->sval;
			}
		} else {
			/* element is MSG */
			if ( !(foo.len==ELEM_VAL_BODY_LEN &&
			!strncasecmp(ELEM_VAL_BODY,foo.s,foo.len)) ) {
				LOG(L_ERR,"ERROR:tm:parse_tw_append: unsupported value <%.*s>"
					" for msg type\n",foo.len,foo.s);
				goto error;
			}
			app->add_body = 1;
			pkg_free( ha );
			ha = 0;
		}

		/* parse the element separator, if present */
		while( *s && isspace((int)*s) )  s++;
		if ( *s && *s!=';')
			goto parse_error;
		if (*s==';') {
			s++;
			while( *s && isspace((int)*s) )  s++;
			if (!*s)
				goto parse_error;
		}

		/* link the element to tw_append structure */
		if (ha) {
			if (last==0) {
				last = app->elems = ha;
			} else {
				last->next = ha;
				last = ha;
			}
		}

	} /* end while */

	print_tw_append( app );
	/* free the old string */
	pkg_free(val);
	return 0;
parse_error:
	LOG(L_ERR,"ERROR:tm:parse_tw_append: parse error in <%s> around "
		"position %ld\n", (char*)val, (long)(s-(char*)val));
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
		iov_lines_eol[2*i].iov_base = 0;
		iov_lines_eol[2*i].iov_len = 0;
		iov_lines_eol[2*i+1] = eol;
	}

	/* first line is the version - fill it now */
	eol_line_s(0)   = TWRITE_VERSION_S;
	eol_line_len(0) = TWRITE_VERSION_LEN;

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
	if (writev(fd_fifo, iov_lines_eol, 2*cnt)<0) {
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


static inline char* add2buf(char *buf, char *end, char *title, int title_len,
			    char *value , int value_len)
{
	if (buf+title_len+value_len+2+1>=end)
		return 0;
	memcpy( buf, title, title_len);
	buf += title_len;
	*(buf++) = ':';
	*(buf++) = ' ';
	memcpy( buf, value, value_len);
	buf += value_len;
	*(buf++) = '\n';
	return buf;
}


static inline char* append2buf( char *buf, int len, struct sip_msg *req, 
				struct hdr_avp *ha)
{
	struct hdr_field *hdr;
	struct usr_avp   *avp;
	int_str          avp_val;
	int_str          avp_name;
	char  *end;
	str   foo;
	int   msg_parsed;

	end = buf+len;
	msg_parsed = 0;

	while(ha) {
		if (ha->type==ELEM_IS_AVP) {
			/* search for the AVP */
			if (ha->sval.s) {
				avp_name.s=ha->sval;
				avp = search_first_avp( AVP_NAME_STR, avp_name, &avp_val, 0);
				DBG("AVP <%.*s>: %p\n",avp_name.s.len, avp_name.s.s, avp);
			} else {
				avp_name.n=ha->ival;
				avp = search_first_avp( 0, avp_name, &avp_val, 0);
				DBG("AVP <%i>: %p\n",avp_name.n,avp);
			}
			if (avp) {
				if (avp->flags&AVP_VAL_STR) {
					buf=add2buf( buf, end, ha->title.s, ha->title.len,
						avp_val.s.s , avp_val.s.len);
					if (!buf)
						goto overflow_err;
				} else {
					foo.s=int2str( (unsigned long)avp_val.n, &foo.len);
					buf=add2buf( buf, end, ha->title.s, ha->title.len,
						foo.s , foo.len);
					if (!buf)
						goto overflow_err;
				}
			}
		} else if (ha->type==ELEM_IS_HDR) {
			/* parse the HDRs */
			if (!msg_parsed) {
				if (parse_headers( req, HDR_EOH_F, 0)!=0) {
					LOG(L_ERR,"ERROR:tm:append2buf: parsing hdrs failed\n");
					goto error;
				}
				msg_parsed = 1;
			}
			/* search the HDR */
			if (ha->ival==HDR_OTHER_T) {
				for(hdr=req->headers;hdr;hdr=hdr->next)
					if (ha->sval.len==hdr->name.len &&
					strncasecmp( ha->sval.s, hdr->name.s, hdr->name.len)==0)
						break;
			} else {
				for(hdr=req->headers;hdr;hdr=hdr->next)
					if (ha->ival==hdr->type)
						break;
			}
			if (hdr) {
				trim_len( foo.len, foo.s, hdr->body);
				buf=add2buf( buf, end, ha->title.s, ha->title.len,
					foo.s , foo.len);
				if (!buf)
					goto overflow_err;
			}
		} else {
			LOG(L_ERR,"BUG:tm:append2buf: unknown element type %d\n",
				ha->type);
			goto error;
		}

		ha = ha->next;
	}

	return buf;
overflow_err:
	LOG(L_ERR,"ERROR:tm:append2buf: overflow -> append exceeded %d len\n",len);
error:
	return 0;
}


static int assemble_msg(struct sip_msg* msg, struct tw_info *twi)
{
	static char     id_buf[IDBUF_LEN];
	static char     route_buffer[ROUTE_BUFFER_MAX];
	static char     append_buf[APPEND_BUFFER_MAX];
	static char     cmd_buf[CMD_BUFFER_MAX];
	static str      empty_param = STR_STATIC_INIT(".");
	unsigned int      hash_index, label;
	contact_body_t*   cb=0;
	contact_t*        c=0;
	name_addr_t       na;
	rr_t*             record_route;
	struct hdr_field* p_hdr;
	param_hooks_t     hooks;
	int               l;
	char*             s, fproxy_lr;
	int		  first_rr;
	str               route, next_hop, append, tmp_s, body, str_uri;

	if(msg->first_line.type != SIP_REQUEST){
		LOG(L_ERR,"assemble_msg: called for something else then"
			"a SIP request\n");
		goto error;
	}

	/* parse all -- we will need every header field for a UAS */
	if ( parse_headers(msg, HDR_EOH_F, 0)==-1) {
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
	if (msg->from==0 || (msg->from->parsed==0 && parse_from_header(msg)==-1)) {
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

	first_rr = 1;

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
				first_rr = 0;
				DBG("assemble_msg: first proxy has loose routing.\n");
				copy_route(s,route.len,record_route->nameaddr.uri.s,
					record_route->nameaddr.uri.len);
			}
		}
		for(;p_hdr;p_hdr = p_hdr->next) {
			/* filter out non-RR hdr and empty hdrs */
			if( (p_hdr->type!=HDR_RECORDROUTE_T) || p_hdr->body.len==0)
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
				if (!first_rr) { 
 				  copy_route(s,route.len,record_route->nameaddr.uri.s,
 					     record_route->nameaddr.uri.len);
 				} 
 				first_rr=0;
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
	
	if ( REQ_LINE(msg).method_value==METHOD_INVITE || 
	     REQ_LINE(msg).method_value==METHOD_INFO ||
			(twi->append && twi->append->add_body) ) {
		/* get body */
		if( (body.s = get_body(msg)) == 0 ){
			LOG(L_ERR, "assemble_msg: get_body failed\n");
			goto error;
		}
		body.len = msg->len - (body.s - msg->buf);
	} else {
		body = empty_param;
	}

	/* additional headers */
	append.s = s = append_buf;
	if (sizeof(flag_t)*2+12+1 >= APPEND_BUFFER_MAX) {
		LOG(L_ERR,"assemble_msg: buffer overflow "
		    "while copying optional header\n");
		goto error;
	}
	append_str(s,"P-MsgFlags: ",12);
	l = APPEND_BUFFER_MAX - (12+1); /* include trailing `\n'*/

	if (int2reverse_hex(&s, &l, (int)msg->msg_flags) == -1) {
		LOG(L_ERR,"assemble_msg: buffer overflow "
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

	eol_line_s(1) = s = cmd_buf;
	if(twi->action.len+12 >= CMD_BUFFER_MAX){
		LOG(L_ERR,"assemble_msg: buffer overflow while "
		    "copying command name\n");
		goto error;
	}
	append_str(s,"sip_request.",12);
	append_str(s,twi->action.s, twi->action.len);
	eol_line_len(1) = s - (char*)eol_line_s(1);

	eol_line(2,REQ_LINE(msg).method);     /* method type */
	eol_line(3,msg->parsed_uri.user);     /* user from r-uri */
	eol_line(4,msg->parsed_uri.host);     /* domain */

	eol_line(5,msg->rcv.bind_address->address_str); /* dst ip */

	eol_line(6,msg->rcv.dst_port==SIP_PORT ?
			empty_param : msg->rcv.bind_address->port_no_str); /* port */

	/* r_uri ('Contact:' for next requests) */
	eol_line(7,msg->first_line.u.request.uri);

	/* r_uri for subsequent requests */
	eol_line(8,str_uri.len?str_uri:empty_param);

	eol_line(9,get_from(msg)->body);		/* from */
	eol_line(10,msg->to->body);			/* to */
	eol_line(11,msg->callid->body);		/* callid */
	eol_line(12,get_from(msg)->tag_value);	/* from tag */
	eol_line(13,get_to(msg)->tag_value);	/* to tag */
	eol_line(14,get_cseq(msg)->number);	/* cseq number */

	eol_line_s(15)=id_buf;       /* hash:label */
	s = int2str(hash_index, &l);
	if (l+1>=IDBUF_LEN) {
		LOG(L_ERR, "assemble_msg: too big hash\n");
		goto error;
	}
	memcpy(id_buf, s, l);
	id_buf[l]=':';
	eol_line_len(15)=l+1;
	s = int2str(label, &l);
	if (l+1+eol_line_len(15)>=IDBUF_LEN) {
		LOG(L_ERR, "assemble_msg: too big label\n");
		goto error;
	}
	memcpy(id_buf+eol_line_len(15), s, l);
	eol_line_len(15) += l;

	eol_line(16, route.len ? route : empty_param );
	eol_line(17, next_hop );
	eol_line(18, append );
	eol_line(19, body );

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
		LOG(L_ERR, "write_to_unixsock: Error in connect: %s\n", strerror(errno));
		return -1;
	}

	if (tsend_dgram_ev(sock, iov_lines_eol, 2 * cnt,
			cfg_get(tm, tm_cfg, tm_unix_tx_timeout)) < 0) {
		LOG(L_ERR, "write_to_unixsock: writev failed: %s\n", strerror(errno));
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
