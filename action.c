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
 *
 * History:
 * ---------
 *  2003-02-28  scratchpad compatibility abandoned (jiri)
 *  2003-01-29  removed scratchpad (jiri)
 *  2003-03-19  fixed set* len calculation bug & simplified a little the code
 *              (should be a little faster now) (andrei)
 *              replaced all mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-04-01  Added support for loose routing in forward (janakj)
 *  2003-04-12  FORCE_RPORT_T added (andrei)
 *  2003-04-22  strip_tail added (jiri)
 *  2003-10-02  added SET_ADV_ADDR_T & SET_ADV_PORT_T (andrei)
 *  2003-10-29  added FORCE_TCP_ALIAS_T (andrei)
 */


#include "comp_defs.h"

#include "action.h"
#include "config.h"
#include "error.h"
#include "dprint.h"
#include "proxy.h"
#include "forward.h"
#include "udp_server.h"
#include "route.h"
#include "parser/msg_parser.h"
#include "parser/parse_uri.h"
#include "ut.h"
#include "sr_module.h"
#include "mem/mem.h"
#include "globals.h"
#include "dset.h"
#include "usr_avp.h"
#ifdef USE_TCP
#include "tcp_server.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif


/* ret= 0! if action -> end of list(e.g DROP), 
      > 0 to continue processing next actions
   and <0 on error */
int do_action(struct action* a, struct sip_msg* msg)
{
	int ret;
	int v;
	union sockaddr_union* to;
	struct socket_info* send_sock;
	struct proxy_l* p;
	char* tmp;
	char *new_uri, *end, *crt;
	int len;
	int user;
	struct sip_uri uri, next_hop;
	struct sip_uri* u;
	unsigned short port;
	int proto;

	/* reset the value of error to E_UNSPEC so avoid unknowledgable
	   functions to return with errror (status<0) and not setting it
	   leaving there previous error; cache the previous value though
	   for functions which want to process it */
	prev_ser_error=ser_error;
	ser_error=E_UNSPEC;

	ret=E_BUG;
	switch ((unsigned char)a->type){
		case DROP_T:
				ret=0;
			break;
		case FORWARD_T:
#ifdef USE_TCP
		case FORWARD_TCP_T:
#endif
#ifdef USE_TLS
		case FORWARD_TLS_T:
#endif
		case FORWARD_UDP_T:

			if (a->type==FORWARD_UDP_T) proto=PROTO_UDP;
#ifdef USE_TCP
			else if (a->type==FORWARD_TCP_T) proto= PROTO_TCP;
#endif
#ifdef USE_TLS
			else if (a->type==FORWARD_TLS_T) proto= PROTO_TLS;
#endif
			else proto=msg->rcv.proto;
			if (a->p1_type==URIHOST_ST){
				/*parse uri*/

				if (msg->dst_uri.len) {
					ret = parse_uri(msg->dst_uri.s, msg->dst_uri.len, &next_hop);
					u = &next_hop;
				} else {
					ret = parse_sip_msg_uri(msg);
					u = &msg->parsed_uri;
				}

				if (ret<0) {
					LOG(L_ERR, "ERROR: do_action: forward: bad_uri "
								" dropping packet\n");
					break;
				}
				
				switch (a->p2_type){
					case URIPORT_ST:
									port=u->port_no;
									break;
					case NUMBER_ST:
									port=a->p2.number;
									break;
					default:
							LOG(L_CRIT, "BUG: do_action bad forward 2nd"
										" param type (%d)\n", a->p2_type);
							ret=E_UNSPEC;
							goto error_fwd_uri;
				}
				switch(u->proto){
					case PROTO_NONE:
						proto=PROTO_UDP;
						break;
					case PROTO_UDP:
#ifdef USE_TCP
					case PROTO_TCP:
#endif
#ifdef USE_TLS
					case PROTO_TLS:
#endif
						proto=u->proto;
						break;
					default:
						LOG(L_ERR,"ERROR: do action: forward: bad uri"
								" transport %d\n", u->proto);
						ret=E_BAD_PROTO;
						goto error_fwd_uri;
				}
#ifdef USE_TLS
				if (u->secure){
					if (u->proto==PROTO_UDP){
						LOG(L_ERR, "ERROR: do_action: forward: secure uri"
								" incompatible with transport %d\n", u->proto);
						ret=E_BAD_PROTO;
						goto error_fwd_uri;
					}
					proto=PROTO_TLS;
				}
#endif
				/* create a temporary proxy*/
				p=mk_proxy(&u->host, port, proto);
				if (p==0){
					LOG(L_ERR, "ERROR:  bad host name in uri,"
							" dropping packet\n");
					ret=E_BAD_ADDRESS;
					goto error_fwd_uri;
				}
				ret=forward_request(msg, p, proto);
				/*free_uri(&uri); -- no longer needed, in sip_msg*/
				free_proxy(p); /* frees only p content, not p itself */
				pkg_free(p);
				if (ret>=0) ret=1;
			}else if ((a->p1_type==PROXY_ST) && (a->p2_type==NUMBER_ST)){
				ret=forward_request(msg,(struct proxy_l*)a->p1.data, proto);
				if (ret>=0) ret=1;
			}else{
				LOG(L_CRIT, "BUG: do_action: bad forward() types %d, %d\n",
						a->p1_type, a->p2_type);
				ret=E_BUG;
			}
			break;
		case SEND_T:
		case SEND_TCP_T:
			if ((a->p1_type!= PROXY_ST)|(a->p2_type!=NUMBER_ST)){
				LOG(L_CRIT, "BUG: do_action: bad send() types %d, %d\n",
						a->p1_type, a->p2_type);
				ret=E_BUG;
				break;
			}
			to=(union sockaddr_union*)
					pkg_malloc(sizeof(union sockaddr_union));
			if (to==0){
				LOG(L_ERR, "ERROR: do_action: "
							"memory allocation failure\n");
				ret=E_OUT_OF_MEM;
				break;
			}
			
			p=(struct proxy_l*)a->p1.data;
			
			if (p->ok==0){
				if (p->host.h_addr_list[p->addr_idx+1])
					p->addr_idx++;
				else 
					p->addr_idx=0;
				p->ok=1;
			}
			ret=hostent2su(	to, &p->host, p->addr_idx,
						(p->port)?p->port:SIP_PORT );
			if (ret==0){
				p->tx++;
				p->tx_bytes+=msg->len;
				if (a->type==SEND_T){
					/*udp*/
					send_sock=get_send_socket(to, PROTO_UDP);
					if (send_sock!=0){
						ret=udp_send(send_sock, msg->buf, msg->len, to);
					}else{
						ret=-1;
					}
				}
#ifdef USE_TCP
					else{
					/*tcp*/
					ret=tcp_send(PROTO_TCP, msg->buf, msg->len, to, 0);
				}
#endif
			}
			pkg_free(to);
			if (ret<0){
				p->errors++;
				p->ok=0;
			}else ret=1;
			
			break;
		case LOG_T:
			if ((a->p1_type!=NUMBER_ST)|(a->p2_type!=STRING_ST)){
				LOG(L_CRIT, "BUG: do_action: bad log() types %d, %d\n",
						a->p1_type, a->p2_type);
				ret=E_BUG;
				break;
			}
			LOG(a->p1.number, a->p2.string);
			ret=1;
			break;

		/* jku -- introduce a new branch */
		case APPEND_BRANCH_T:
			if ((a->p1_type!=STRING_ST)) {
				LOG(L_CRIT, "BUG: do_action: bad append_branch_t %d\n",
					a->p1_type );
				ret=E_BUG;
				break;
			}
			ret=append_branch( msg, a->p1.string, 
				a->p1.string ? strlen(a->p1.string):0 );
			break;

		/* jku begin: is_length_greater_than */
		case LEN_GT_T:
			if (a->p1_type!=NUMBER_ST) {
				LOG(L_CRIT, "BUG: do_action: bad len_gt type %d\n",
					a->p1_type );
				ret=E_BUG;
				break;
			}
			/* DBG("XXX: message length %d, max %d\n", 
				msg->len, a->p1.number ); */
			ret = msg->len >= a->p1.number ? 1 : -1;
			break;
		/* jku end: is_length_greater_than */
			
		/* jku - begin : flag processing */

		case SETFLAG_T:
			if (a->p1_type!=NUMBER_ST) {
				LOG(L_CRIT, "BUG: do_action: bad setflag() type %d\n",
					a->p1_type );
				ret=E_BUG;
				break;
			}
			if (!flag_in_range( a->p1.number )) {
				ret=E_CFG;
				break;
			}
			setflag( msg, a->p1.number );
			ret=1;
			break;

		case RESETFLAG_T:
			if (a->p1_type!=NUMBER_ST) {
				LOG(L_CRIT, "BUG: do_action: bad resetflag() type %d\n",
					a->p1_type );
				ret=E_BUG;
				break;
			}
			if (!flag_in_range( a->p1.number )) {
				ret=E_CFG;
				break;
			}
			resetflag( msg, a->p1.number );
			ret=1;
			break;
			
		case ISFLAGSET_T:
			if (a->p1_type!=NUMBER_ST) {
				LOG(L_CRIT, "BUG: do_action: bad isflagset() type %d\n",
					a->p1_type );
				ret=E_BUG;
				break;
			}
			if (!flag_in_range( a->p1.number )) {
				ret=E_CFG;
				break;
			}
			ret=isflagset( msg, a->p1.number );
			break;
		/* jku - end : flag processing */

		case ERROR_T:
			if ((a->p1_type!=STRING_ST)|(a->p2_type!=STRING_ST)){
				LOG(L_CRIT, "BUG: do_action: bad error() types %d, %d\n",
						a->p1_type, a->p2_type);
				ret=E_BUG;
				break;
			}
			LOG(L_NOTICE, "WARNING: do_action: error(\"%s\", \"%s\") "
					"not implemented yet\n", a->p1.string, a->p2.string);
			ret=1;
			break;
		case ROUTE_T:
			if (a->p1_type!=NUMBER_ST){
				LOG(L_CRIT, "BUG: do_action: bad route() type %d\n",
						a->p1_type);
				ret=E_BUG;
				break;
			}
			if ((a->p1.number>RT_NO)||(a->p1.number<0)){
				LOG(L_ERR, "ERROR: invalid routing table number in"
							"route(%lu)\n", a->p1.number);
				ret=E_CFG;
				break;
			}
			ret=((ret=run_actions(rlist[a->p1.number], msg))<0)?ret:1;
			break;
		case EXEC_T:
			if (a->p1_type!=STRING_ST){
				LOG(L_CRIT, "BUG: do_action: bad exec() type %d\n",
						a->p1_type);
				ret=E_BUG;
				break;
			}
			LOG(L_NOTICE, "WARNING: exec(\"%s\") not fully implemented,"
						" using dumb version...\n", a->p1.string);
			ret=system(a->p1.string);
			if (ret!=0){
				LOG(L_NOTICE, "WARNING: exec() returned %d\n", ret);
			}
			ret=1;
			break;
		case REVERT_URI_T:
			if (msg->new_uri.s) {
				pkg_free(msg->new_uri.s);
				msg->new_uri.len=0;
				msg->new_uri.s=0;
				msg->parsed_uri_ok=0; /* invalidate current parsed uri*/
			};
			ret=1;
			break;
		case SET_HOST_T:
		case SET_HOSTPORT_T:
		case SET_USER_T:
		case SET_USERPASS_T:
		case SET_PORT_T:
		case SET_URI_T:
		case PREFIX_T:
		case STRIP_T:
		case STRIP_TAIL_T:
				user=0;
				if (a->type==STRIP_T || a->type==STRIP_TAIL_T) {
					if (a->p1_type!=NUMBER_ST) {
						LOG(L_CRIT, "BUG: do_action: bad set*() type %d\n",
							a->p1_type);
						break;
					}
				} else if (a->p1_type!=STRING_ST){
					LOG(L_CRIT, "BUG: do_action: bad set*() type %d\n",
							a->p1_type);
					ret=E_BUG;
					break;
				}
				if (a->type==SET_URI_T){
					if (msg->new_uri.s) {
							pkg_free(msg->new_uri.s);
							msg->new_uri.len=0;
					}
					msg->parsed_uri_ok=0;
					len=strlen(a->p1.string);
					msg->new_uri.s=pkg_malloc(len+1);
					if (msg->new_uri.s==0){
						LOG(L_ERR, "ERROR: do_action: memory allocation"
								" failure\n");
						ret=E_OUT_OF_MEM;
						break;
					}
					memcpy(msg->new_uri.s, a->p1.string, len);
					msg->new_uri.s[len]=0;
					msg->new_uri.len=len;
					
					ret=1;
					break;
				}
				if (msg->new_uri.s) {
					tmp=msg->new_uri.s;
					len=msg->new_uri.len;
				}else{
					tmp=msg->first_line.u.request.uri.s;
					len=msg->first_line.u.request.uri.len;
				}
				if (parse_uri(tmp, len, &uri)<0){
					LOG(L_ERR, "ERROR: do_action: bad uri <%s>, dropping"
								" packet\n", tmp);
					ret=E_UNSPEC;
					break;
				}
				
				new_uri=pkg_malloc(MAX_URI_SIZE);
				if (new_uri==0){
					LOG(L_ERR, "ERROR: do_action: memory allocation "
								" failure\n");
					ret=E_OUT_OF_MEM;
					break;
				}
				end=new_uri+MAX_URI_SIZE;
				crt=new_uri;
				/* begin copying */
				len=strlen("sip:"); if(crt+len>end) goto error_uri;
				memcpy(crt,"sip:",len);crt+=len;

				/* user */

				/* prefix (-jiri) */
				if (a->type==PREFIX_T) {
					tmp=a->p1.string;
					len=strlen(tmp); if(crt+len>end) goto error_uri;
					memcpy(crt,tmp,len);crt+=len;
					/* whatever we had before, with prefix we have username 
					   now */
					user=1;
				}

				if ((a->type==SET_USER_T)||(a->type==SET_USERPASS_T)) {
					tmp=a->p1.string;
					len=strlen(tmp);
				} else if (a->type==STRIP_T) {
					if (a->p1.number>uri.user.len) {
						LOG(L_WARN, "Error: too long strip asked; "
									" deleting username: %lu of <%.*s>\n",
									a->p1.number, uri.user.len, uri.user.s );
						len=0;
					} else if (a->p1.number==uri.user.len) {
						len=0;
					} else {
						tmp=uri.user.s + a->p1.number;
						len=uri.user.len - a->p1.number;
					}
				} else if (a->type==STRIP_TAIL_T) {
					if (a->p1.number>uri.user.len) {
						LOG(L_WARN, "WARNING: too long strip_tail asked; "
									" deleting username: %lu of <%.*s>\n",
									a->p1.number, uri.user.len, uri.user.s );
						len=0;
					} else if (a->p1.number==uri.user.len) {
						len=0;
					} else {
						tmp=uri.user.s;
						len=uri.user.len - a->p1.number;
					}
				} else {
					tmp=uri.user.s;
					len=uri.user.len;
				}

				if (len){
					if(crt+len>end) goto error_uri;
					memcpy(crt,tmp,len);crt+=len;
					user=1; /* we have an user field so mark it */
				}

				if (a->type==SET_USERPASS_T) tmp=0;
				else tmp=uri.passwd.s;
				/* passwd */
				if (tmp){
					len=uri.passwd.len; if(crt+len+1>end) goto error_uri;
					*crt=':'; crt++;
					memcpy(crt,tmp,len);crt+=len;
				}
				/* host */
				if (user || tmp){ /* add @ */
					if(crt+1>end) goto error_uri;
					*crt='@'; crt++;
				}
				if ((a->type==SET_HOST_T) ||(a->type==SET_HOSTPORT_T)) {
					tmp=a->p1.string;
					if (tmp) len = strlen(tmp);
					else len=0;
				} else {
					tmp=uri.host.s;
					len = uri.host.len;
				}
				if (tmp){
					if(crt+len>end) goto error_uri;
					memcpy(crt,tmp,len);crt+=len;
				}
				/* port */
				if (a->type==SET_HOSTPORT_T) tmp=0;
				else if (a->type==SET_PORT_T) {
					tmp=a->p1.string;
					if (tmp) len = strlen(tmp);
					else len = 0;
				} else {
					tmp=uri.port.s;
					len = uri.port.len;
				}
				if (tmp){
					if(crt+len+1>end) goto error_uri;
					*crt=':'; crt++;
					memcpy(crt,tmp,len);crt+=len;
				}
				/* params */
				tmp=uri.params.s;
				if (tmp){
					len=uri.params.len; if(crt+len+1>end) goto error_uri;
					*crt=';'; crt++;
					memcpy(crt,tmp,len);crt+=len;
				}
				/* headers */
				tmp=uri.headers.s;
				if (tmp){
					len=uri.headers.len; if(crt+len+1>end) goto error_uri;
					*crt='?'; crt++;
					memcpy(crt,tmp,len);crt+=len;
				}
				*crt=0; /* null terminate the thing */
				/* copy it to the msg */
				if (msg->new_uri.s) pkg_free(msg->new_uri.s);
				msg->new_uri.s=new_uri;
				msg->new_uri.len=crt-new_uri;
				msg->parsed_uri_ok=0;
				ret=1;
				break;
		case IF_T:
				/* if null expr => ignore if? */
				if ((a->p1_type==EXPR_ST)&&a->p1.data){
					v=eval_expr((struct expr*)a->p1.data, msg);
					if (v<0){
						if (v==EXPR_DROP){ /* hack to quit on DROP*/
							ret=0;
							break;
						}else{
							LOG(L_WARN,"WARNING: do_action:"
										"error in expression\n");
						}
					}
					
					ret=1;  /*default is continue */
					if (v>0) {
						if ((a->p2_type==ACTIONS_ST)&&a->p2.data){
							ret=run_actions((struct action*)a->p2.data, msg);
						}
					}else if ((a->p3_type==ACTIONS_ST)&&a->p3.data){
							ret=run_actions((struct action*)a->p3.data, msg);
					}
				}
			break;
		case MODULE_T:
			if ( ((a->p1_type==CMDF_ST)&&a->p1.data)/*&&
					((a->p2_type==STRING_ST)&&a->p2.data)*/ ){
				ret=((cmd_function)(a->p1.data))(msg, (char*)a->p2.data,
													  (char*)a->p3.data);
			}else{
				LOG(L_CRIT,"BUG: do_action: bad module call\n");
			}
			break;
		case FORCE_RPORT_T:
			msg->msg_flags|=FL_FORCE_RPORT;
			ret=1; /* continue processing */
			break;
		case SET_ADV_ADDR_T:
			if (a->p1_type!=STR_ST){
				LOG(L_CRIT, "BUG: do_action: bad set_advertised_address() "
						"type %d\n", a->p1_type);
				ret=E_BUG;
				break;
			}
			msg->set_global_address=*((str*)a->p1.data);
			ret=1; /* continue processing */
			break;
		case SET_ADV_PORT_T:
			if (a->p1_type!=STR_ST){
				LOG(L_CRIT, "BUG: do_action: bad set_advertised_port() "
						"type %d\n", a->p1_type);
				ret=E_BUG;
				break;
			}
			msg->set_global_port=*((str*)a->p1.data);
			ret=1; /* continue processing */
			break;
#ifdef USE_TCP
		case FORCE_TCP_ALIAS_T:
			if ( msg->rcv.proto==PROTO_TCP
#ifdef USE_TLS
					|| msg->rcv.proto==PROTO_TLS
#endif
			   ){
				
				if (a->p1_type==NOSUBTYPE)	port=msg->via1->port;
				else if (a->p1_type==NUMBER_ST) port=(int)a->p1.number;
				else{
					LOG(L_CRIT, "BUG: do_action: bad force_tcp_alias"
							" port type %d\n", a->p1_type);
					ret=E_BUG;
					break;
				}
						
				if (tcpconn_add_alias(msg->rcv.proto_reserved1, port,
									msg->rcv.proto)!=0){
					LOG(L_ERR, " ERROR: receive_msg: tcp alias failed\n");
					ret=E_UNSPEC;
					break;
				}
			}
#endif
			ret=1; /* continue processing */
			break;
		case LOAD_AVP_T:
			if (a->p1_type!=NUMBER_ST || a->p2_type!=STRING_ST ||
			a->p3_type!=NUMBER_ST) {
				LOG(L_CRIT,"BUG: do_action: bad load_avp(%d,%d,%d) params "
						"types\n",a->p1_type,a->p2_type,a->p3_type);
				ret=E_BUG;
				break;
			}
			/* load the attribute(s)*/
			if ( load_avp( msg, (int)a->p1.number, a->p2.string,
			(int)a->p3.number)!=0 ) {
				LOG(L_ERR,"ERROR:do_action: load avp failed\n");
				ret=E_UNSPEC;
			}
			break;
		default:
			LOG(L_CRIT, "BUG: do_action: unknown type %d\n", a->type);
	}
/*skip:*/
	return ret;
	
error_uri:
	LOG(L_ERR, "ERROR: do_action: set*: uri too long\n");
	if (new_uri) pkg_free(new_uri);
	return E_UNSPEC;
error_fwd_uri:
	/*free_uri(&uri); -- not needed anymore, using msg->parsed_uri*/
	return ret;
}



/* returns: 0, or 1 on success, <0 on error */
/* (0 if drop or break encountered, 1 if not ) */
int run_actions(struct action* a, struct sip_msg* msg)
{
	struct action* t;
	int ret=E_UNSPEC;
	static int rec_lev=0;
	struct sr_module *mod;

	rec_lev++;
	if (rec_lev>ROUTE_MAX_REC_LEV){
		LOG(L_ERR, "WARNING: too many recursive routing table lookups (%d)"
					" giving up!\n", rec_lev);
		ret=E_UNSPEC;
		goto error;
	}
		
	if (a==0){
		LOG(L_ERR, "WARNING: run_actions: null action list (rec_level=%d)\n", 
			rec_lev);
		ret=0;
	}

	for (t=a; t!=0; t=t->next){
		ret=do_action(t, msg);
		if(ret==0) break;
		/* ignore errors */
		/*else if (ret<0){ ret=-1; goto error; }*/
	}
	
	rec_lev--;
	/* process module onbreak handlers if present */
	if (rec_lev==0 && ret==0) 
		for (mod=modules;mod;mod=mod->next) 
			if (mod->exports && mod->exports->onbreak_f) {
				mod->exports->onbreak_f( msg );
				DBG("DEBUG: %s onbreak handler called\n", mod->exports->name);
			}
	return ret;
	

error:
	rec_lev--;
	return ret;
}



