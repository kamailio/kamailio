
/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *  2004-11-30  added FORCE_SEND_SOCKET_T (andrei)
 *  2005-12-12  return & drop/exit differentiation (andrei)
 *  2005-12-19  select framework (mma)
 *  2006-04-12  updated *_send() calls to use a struct dest_info (andrei)
 *  2006-07-27  dns cache and dns based send address failover support (andrei)
 *  2006-12-06  on popular request last_retcode set also by module functions
 *              (andrei)
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
#include "onsend.h"
#include "resolve.h"
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

#define USE_LONGJMP

#ifdef USE_LONGJMP
#include <setjmp.h>
#endif

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif


struct onsend_info* p_onsend=0; /* onsend route send info */
unsigned int run_flags=0;
int last_retcode=0; /* last return from a route() */

/* ret= 0! if action -> end of list(e.g DROP),
      > 0 to continue processing next actions
   and <0 on error */
int do_action(struct action* a, struct sip_msg* msg)
{
	int ret;
	int v;
	struct dest_info dst;
	char* tmp;
	char *new_uri, *end, *crt;
	int len;
	int user;
	struct sip_uri uri, next_hop;
	struct sip_uri *u;
	unsigned short port;
	unsigned short flags;
	int_str name, value;
	str* dst_host;

	/* reset the value of error to E_UNSPEC so avoid unknowledgable
	   functions to return with error (status<0) and not setting it
	   leaving there previous error; cache the previous value though
	   for functions which want to process it */
	prev_ser_error=ser_error;
	ser_error=E_UNSPEC;

	ret=E_BUG;
	switch ((unsigned char)a->type){
		case DROP_T:
				if (a->val[0].type==RETCODE_ST)
					ret=last_retcode;
				else
					ret=(int) a->val[0].u.number;
				run_flags|=(unsigned int)a->val[1].u.number;
			break;
		case FORWARD_T:
#ifdef USE_TCP
		case FORWARD_TCP_T:
#endif
#ifdef USE_TLS
		case FORWARD_TLS_T:
#endif
		case FORWARD_UDP_T:
			/* init dst */
			init_dest_info(&dst);
			if (a->type==FORWARD_UDP_T) dst.proto=PROTO_UDP;
#ifdef USE_TCP
			else if (a->type==FORWARD_TCP_T) dst.proto= PROTO_TCP;
#endif
#ifdef USE_TLS
			else if (a->type==FORWARD_TLS_T) dst.proto= PROTO_TLS;
#endif
			else dst.proto= PROTO_NONE;
			if (a->val[0].type==URIHOST_ST){
				/*parse uri*/

				if (msg->dst_uri.len) {
					ret = parse_uri(msg->dst_uri.s, msg->dst_uri.len,
									&next_hop);
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

				switch (a->val[1].type){
					case URIPORT_ST:
									port=u->port_no;
									break;
					case NUMBER_ST:
									port=a->val[1].u.number;
									break;
					default:
							LOG(L_CRIT, "BUG: do_action bad forward 2nd"
										" param type (%d)\n", a->val[1].type);
							ret=E_UNSPEC;
							goto error_fwd_uri;
				}
				if (dst.proto == PROTO_NONE){ /* only if proto not set get it
											 from the uri */
					switch(u->proto){
						case PROTO_NONE:
							dst.proto=PROTO_UDP;
							break;
						case PROTO_UDP:
#ifdef USE_TCP
						case PROTO_TCP:
#endif
#ifdef USE_TLS
						case PROTO_TLS:
#endif
							dst.proto=u->proto;
							break;
						default:
							LOG(L_ERR,"ERROR: do action: forward: bad uri"
									" transport %d\n", u->proto);
							ret=E_BAD_PROTO;
							goto error_fwd_uri;
					}
#ifdef USE_TLS
					if (u->type==SIPS_URI_T){
						if (u->proto==PROTO_UDP){
							LOG(L_ERR, "ERROR: do_action: forward: secure uri"
									" incompatible with transport %d\n",
									u->proto);
							ret=E_BAD_PROTO;
							goto error_fwd_uri;
						}
						dst.proto=PROTO_TLS;
					}
#endif
				}

#ifdef HONOR_MADDR				
				if (u->maddr_val.s && u->maddr_val.len)
					dst_host=&u->maddr_val;
				else
#endif
					dst_host=&u->host;
#ifdef USE_COMP
				dst.comp=u->comp;
#endif
				ret=forward_request(msg, dst_host, port, &dst);
				if (ret>=0){
					ret=1;
				}
			}else if ((a->val[0].type==PROXY_ST) && (a->val[1].type==NUMBER_ST)){
				if (dst.proto==PROTO_NONE)
					dst.proto=msg->rcv.proto;
				proxy2su(&dst.to,  (struct proxy_l*)a->val[0].u.data);
				ret=forward_request(msg, 0, 0, &dst);
				if (ret>=0){
					ret=1;
					proxy_mark((struct proxy_l*)a->val[0].u.data, ret);
				}else if (ser_error!=E_OK){
					proxy_mark((struct proxy_l*)a->val[0].u.data, ret);
				}
			}else{
				LOG(L_CRIT, "BUG: do_action: bad forward() types %d, %d\n",
						a->val[0].type, a->val[1].type);
				ret=E_BUG;
			}
			break;
		case SEND_T:
		case SEND_TCP_T:
			if ((a->val[0].type!= PROXY_ST)|(a->val[1].type!=NUMBER_ST)){
				LOG(L_CRIT, "BUG: do_action: bad send() types %d, %d\n",
						a->val[0].type, a->val[1].type);
				ret=E_BUG;
				break;
			}
			/* init dst */
			init_dest_info(&dst);
			ret=proxy2su(&dst.to,  (struct proxy_l*)a->val[0].u.data);
			if (ret==0){
				if (p_onsend){
					tmp=p_onsend->buf;
					len=p_onsend->len;
				}else{
					tmp=msg->buf;
					len=msg->len;
				}
				if (a->type==SEND_T){
					/*udp*/
					dst.proto=PROTO_UDP; /* not really needed for udp_send */
					dst.send_sock=get_send_socket(msg, &dst.to, PROTO_UDP);
					if (dst.send_sock!=0){
						ret=udp_send(&dst, tmp, len);
					}else{
						ret=-1;
					}
				}
#ifdef USE_TCP
					else{
						/*tcp*/
						dst.proto=PROTO_TCP;
						dst.id=0;
						ret=tcp_send(&dst, tmp, len);
				}
#endif
			}else{
				ret=E_BUG;
				break;
			}
			proxy_mark((struct proxy_l*)a->val[0].u.data, ret);
			if (ret>=0) ret=1;

			break;
		case LOG_T:
			if ((a->val[0].type!=NUMBER_ST)|(a->val[1].type!=STRING_ST)){
				LOG(L_CRIT, "BUG: do_action: bad log() types %d, %d\n",
						a->val[0].type, a->val[1].type);
				ret=E_BUG;
				break;
			}
			LOG(a->val[0].u.number, "%s", a->val[1].u.string);
			ret=1;
			break;

		/* jku -- introduce a new branch */
		case APPEND_BRANCH_T:
			if ((a->val[0].type!=STRING_ST)) {
				LOG(L_CRIT, "BUG: do_action: bad append_branch_t %d\n",
					a->val[0].type );
				ret=E_BUG;
				break;
			}
			ret=append_branch( msg, a->val[0].u.string,
					   a->val[0].u.string ? strlen(a->val[0].u.string):0,
					   0, 0, a->val[1].u.number, 0);
			break;

		/* jku begin: is_length_greater_than */
		case LEN_GT_T:
			if (a->val[0].type!=NUMBER_ST) {
				LOG(L_CRIT, "BUG: do_action: bad len_gt type %d\n",
					a->val[0].type );
				ret=E_BUG;
				break;
			}
			/* DBG("XXX: message length %d, max %d\n",
				msg->len, a->val[0].u.number ); */
			ret = msg->len >= a->val[0].u.number ? 1 : -1;
			break;
		/* jku end: is_length_greater_than */

		/* jku - begin : flag processing */

		case SETFLAG_T:
			if (a->val[0].type!=NUMBER_ST) {
				LOG(L_CRIT, "BUG: do_action: bad setflag() type %d\n",
					a->val[0].type );
				ret=E_BUG;
				break;
			}
			if (!flag_in_range( a->val[0].u.number )) {
				ret=E_CFG;
				break;
			}
			setflag( msg, a->val[0].u.number );
			ret=1;
			break;

		case RESETFLAG_T:
			if (a->val[0].type!=NUMBER_ST) {
				LOG(L_CRIT, "BUG: do_action: bad resetflag() type %d\n",
					a->val[0].type );
				ret=E_BUG;
				break;
			}
			if (!flag_in_range( a->val[0].u.number )) {
				ret=E_CFG;
				break;
			}
			resetflag( msg, a->val[0].u.number );
			ret=1;
			break;

		case ISFLAGSET_T:
			if (a->val[0].type!=NUMBER_ST) {
				LOG(L_CRIT, "BUG: do_action: bad isflagset() type %d\n",
					a->val[0].type );
				ret=E_BUG;
				break;
			}
			if (!flag_in_range( a->val[0].u.number )) {
				ret=E_CFG;
				break;
			}
			ret=isflagset( msg, a->val[0].u.number );
			break;
		/* jku - end : flag processing */

		case AVPFLAG_OPER_T:  {
			struct search_state st;
			avp_t* avp;
			int flag;
			ret = 0;
			flag = a->val[1].u.number;
			if ((a->val[0].u.attr->type & AVP_INDEX_ALL) == AVP_INDEX_ALL || (a->val[0].u.attr->type & AVP_NAME_RE)!=0) {
				for (avp=search_first_avp(a->val[0].u.attr->type, a->val[0].u.attr->name, NULL, &st); avp; avp = search_next_avp(&st, NULL)) {
					switch (a->val[2].u.number) {   /* oper: 0..reset, 1..set, -1..no change */
						case 0:
							avp->flags &= ~(avp_flags_t)a->val[1].u.number;
							break;
						case 1:
							avp->flags |= (avp_flags_t)a->val[1].u.number;
							break;
						default:;
					}
					ret = ret || ((avp->flags & (avp_flags_t)a->val[1].u.number) != 0);
				}
			}
			else {
				avp = search_avp_by_index(a->val[0].u.attr->type, a->val[0].u.attr->name, NULL, a->val[0].u.attr->index);
				if (avp) {
					switch (a->val[2].u.number) {   /* oper: 0..reset, 1..set, -1..no change */
						case 0:
							avp->flags &= ~(avp_flags_t)a->val[1].u.number;
							break;
						case 1:
							avp->flags |= (avp_flags_t)a->val[1].u.number;
							break;
						default:;
					}
					ret = (avp->flags & (avp_flags_t)a->val[1].u.number) != 0;
				}
			}
			if (ret==0)
				ret = -1;
			break;
		}
		case ERROR_T:
			if ((a->val[0].type!=STRING_ST)|(a->val[1].type!=STRING_ST)){
				LOG(L_CRIT, "BUG: do_action: bad error() types %d, %d\n",
						a->val[0].type, a->val[1].type);
				ret=E_BUG;
				break;
			}
			LOG(L_NOTICE, "WARNING: do_action: error(\"%s\", \"%s\") "
					"not implemented yet\n", a->val[0].u.string, a->val[1].u.string);
			ret=1;
			break;
		case ROUTE_T:
			if (a->val[0].type!=NUMBER_ST){
				LOG(L_CRIT, "BUG: do_action: bad route() type %d\n",
						a->val[0].type);
				ret=E_BUG;
				break;
			}
			if ((a->val[0].u.number>=main_rt.idx)||(a->val[0].u.number<0)){
				LOG(L_ERR, "ERROR: invalid routing table number in"
							"route(%lu)\n", a->val[0].u.number);
				ret=E_CFG;
				break;
			}
			/*ret=((ret=run_actions(rlist[a->val[0].u.number], msg))<0)?ret:1;*/
			ret=run_actions(main_rt.rlist[a->val[0].u.number], msg);
			last_retcode=ret;
			run_flags&=~RETURN_R_F; /* absorb returns */
			break;
		case EXEC_T:
			if (a->val[0].type!=STRING_ST){
				LOG(L_CRIT, "BUG: do_action: bad exec() type %d\n",
						a->val[0].type);
				ret=E_BUG;
				break;
			}
			LOG(L_NOTICE, "WARNING: exec(\"%s\") not fully implemented,"
						" using dumb version...\n", a->val[0].u.string);
			ret=system(a->val[0].u.string);
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
					if (a->val[0].type!=NUMBER_ST) {
						LOG(L_CRIT, "BUG: do_action: bad set*() type %d\n",
							a->val[0].type);
						break;
					}
				} else if (a->val[0].type!=STRING_ST){
					LOG(L_CRIT, "BUG: do_action: bad set*() type %d\n",
							a->val[0].type);
					ret=E_BUG;
					break;
				}
				if (a->type==SET_URI_T){
					if (msg->new_uri.s) {
							pkg_free(msg->new_uri.s);
							msg->new_uri.len=0;
					}
					msg->parsed_uri_ok=0;
					len=strlen(a->val[0].u.string);
					msg->new_uri.s=pkg_malloc(len+1);
					if (msg->new_uri.s==0){
						LOG(L_ERR, "ERROR: do_action: memory allocation"
								" failure\n");
						ret=E_OUT_OF_MEM;
						break;
					}
					memcpy(msg->new_uri.s, a->val[0].u.string, len);
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
					tmp=a->val[0].u.string;
					len=strlen(tmp); if(crt+len>end) goto error_uri;
					memcpy(crt,tmp,len);crt+=len;
					/* whatever we had before, with prefix we have username
					   now */
					user=1;
				}

				if ((a->type==SET_USER_T)||(a->type==SET_USERPASS_T)) {
					tmp=a->val[0].u.string;
					len=strlen(tmp);
				} else if (a->type==STRIP_T) {
					if (a->val[0].u.number>uri.user.len) {
						LOG(L_WARN, "Error: too long strip asked; "
									" deleting username: %lu of <%.*s>\n",
									a->val[0].u.number, uri.user.len, uri.user.s );
						len=0;
					} else if (a->val[0].u.number==uri.user.len) {
						len=0;
					} else {
						tmp=uri.user.s + a->val[0].u.number;
						len=uri.user.len - a->val[0].u.number;
					}
				} else if (a->type==STRIP_TAIL_T) {
					if (a->val[0].u.number>uri.user.len) {
						LOG(L_WARN, "WARNING: too long strip_tail asked; "
									" deleting username: %lu of <%.*s>\n",
									a->val[0].u.number, uri.user.len, uri.user.s );
						len=0;
					} else if (a->val[0].u.number==uri.user.len) {
						len=0;
					} else {
						tmp=uri.user.s;
						len=uri.user.len - a->val[0].u.number;
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
					tmp=a->val[0].u.string;
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
					tmp=a->val[0].u.string;
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
				if ((a->val[0].type==EXPR_ST)&&a->val[0].u.data){
					v=eval_expr((struct expr*)a->val[0].u.data, msg);
#if 0
					if (v<0){
						if (v==EXPR_DROP){ /* hack to quit on DROP*/
							ret=0;
							break;
						}else{
							LOG(L_WARN,"WARNING: do_action:"
										"error in expression\n");
						}
					}
#endif
					if (run_flags & EXIT_R_F){
						ret=0;
						break;
					}
					run_flags &= ~RETURN_R_F; /* catch returns in expr */
					ret=1;  /*default is continue */
					if (v>0) {
						if ((a->val[1].type==ACTIONS_ST)&&a->val[1].u.data){
							ret=run_actions((struct action*)a->val[1].u.data, msg);
						}
					}else if ((a->val[2].type==ACTIONS_ST)&&a->val[2].u.data){
							ret=run_actions((struct action*)a->val[2].u.data, msg);
					}
				}
			break;
		case MODULE_T:
			if ( a->val[0].type==MODEXP_ST && a->val[0].u.data && ((cmd_export_t*)a->val[0].u.data)->function ){
				ret=((cmd_export_t*)a->val[0].u.data)->function(msg,
					(char*)a->val[2].u.data,
					(char*)a->val[3].u.data
				);
				if (ret==0) run_flags|=EXIT_R_F;
				last_retcode=ret;
			} else {
				LOG(L_CRIT,"BUG: do_action: bad module call\n");
			}
			break;
		case FORCE_RPORT_T:
			msg->msg_flags|=FL_FORCE_RPORT;
			ret=1; /* continue processing */
			break;
		case SET_ADV_ADDR_T:
			if (a->val[0].type!=STR_ST){
				LOG(L_CRIT, "BUG: do_action: bad set_advertised_address() "
						"type %d\n", a->val[0].type);
				ret=E_BUG;
				break;
			}
			msg->set_global_address=*((str*)a->val[0].u.data);
			ret=1; /* continue processing */
			break;
		case SET_ADV_PORT_T:
			if (a->val[0].type!=STR_ST){
				LOG(L_CRIT, "BUG: do_action: bad set_advertised_port() "
						"type %d\n", a->val[0].type);
				ret=E_BUG;
				break;
			}
			msg->set_global_port=*((str*)a->val[0].u.data);
			ret=1; /* continue processing */
			break;
#ifdef USE_TCP
		case FORCE_TCP_ALIAS_T:
			if ( msg->rcv.proto==PROTO_TCP
#ifdef USE_TLS
					|| msg->rcv.proto==PROTO_TLS
#endif
			   ){

				if (a->val[0].type==NOSUBTYPE)	port=msg->via1->port;
				else if (a->val[0].type==NUMBER_ST) port=(int)a->val[0].u.number;
				else{
					LOG(L_CRIT, "BUG: do_action: bad force_tcp_alias"
							" port type %d\n", a->val[0].type);
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
		case FORCE_SEND_SOCKET_T:
			if (a->val[0].type!=SOCKETINFO_ST){
				LOG(L_CRIT, "BUG: do_action: bad force_send_socket argument"
						" type: %d\n", a->val[0].type);
				ret=E_BUG;
				break;
			}
			msg->force_send_socket=(struct socket_info*)a->val[0].u.data;
			ret=1; /* continue processing */
			break;

	        case ADD_T:
	        case ASSIGN_T:

			/* If the left attr was specified withou indexing brackets delete
			 * existing AVPs before adding new ones
			 */
			if ((a->val[0].u.attr->type & AVP_INDEX_ALL) != AVP_INDEX_ALL) delete_avp(a->val[0].u.attr->type, a->val[0].u.attr->name);

			if (a->val[1].type == STRING_ST) {
				value.s = a->val[1].u.str;
				flags = a->val[0].u.attr->type | AVP_VAL_STR;
				name = a->val[0].u.attr->name;
				ret = 1;
			} else if (a->val[1].type == NUMBER_ST) {
				value.n = a->val[1].u.number;
				flags = a->val[0].u.attr->type;
				name = a->val[0].u.attr->name;
				ret = 1;
			} else if (a->val[1].type == ACTION_ST) {
				flags = a->val[0].u.attr->type;
				name = a->val[0].u.attr->name;
				if (a->val[1].u.data) {
					value.n = run_actions((struct action*)a->val[1].u.data, msg);
				} else {
					value.n = -1;
				}
				ret = value.n;
			} else if(a->val[1].type == EXPR_ST && a->val[1].u.data) {
				v = eval_expr((struct expr*)a->val[1].u.data, msg);
				if (v < 0) {
					if (v == EXPR_DROP){ /* hack to quit on DROP*/
						ret = 0;
						break;
					} else {
						LOG(L_WARN,"WARNING: do_action: error in expression\n");
					}
				}

				flags = a->val[0].u.attr->type;
				name = a->val[0].u.attr->name;
				value.n = v;
			} else if (a->val[1].type == AVP_ST) {
				struct search_state st;
				avp_t* avp;
				avp_t* avp_mark;

				avp_mark = NULL;
				if ((a->val[1].u.attr->type & AVP_INDEX_ALL) == AVP_INDEX_ALL) {
					avp = search_first_avp(a->val[1].u.attr->type, a->val[1].u.attr->name, &value, &st);
					while(avp) {
						     /* We take only the type of value and name from the source avp
						      * and reset class and track flags
						      */
						flags = (a->val[0].u.attr->type & ~AVP_INDEX_ALL) | (avp->flags & ~(AVP_CLASS_ALL|AVP_TRACK_ALL));

						if (add_avp_before(avp_mark, flags, a->val[0].u.attr->name, value) < 0) {
							LOG(L_CRIT, "ERROR: Failed to assign value to attribute\n");
							ret=E_UNSPEC;
							break;
						}

						/* move the mark, so the next found AVP will come before the one currently added
						 * so they will have the same order as in the source list
						 */
						if (avp_mark) {
							avp_mark=avp_mark->next;
						} else {
							avp_mark=search_first_avp(flags, a->val[0].u.attr->name, NULL, NULL);
						}

						avp = search_next_avp(&st, &value);
					}
					ret = 1;
					break;
				} else {
					avp = search_avp_by_index(a->val[1].u.attr->type, a->val[1].u.attr->name, &value, a->val[1].u.attr->index);
					if (avp) {
						flags = a->val[0].u.attr->type | (avp->flags & ~(AVP_CLASS_ALL|AVP_TRACK_ALL));
						name = a->val[0].u.attr->name;
						ret = 1;
					} else {
						ret = E_UNSPEC;
						break;
					}
				}
			} else if (a->val[1].type == SELECT_ST) {
				int r;
				r = run_select(&value.s, a->val[1].u.select, msg);
				if (r < 0) {
					ret=E_UNSPEC;
					break;
				} else if (r > 0) {
					value.s.s = "";
					value.s.len = 0;
				}

				flags = a->val[0].u.attr->type | AVP_VAL_STR;
				name = a->val[0].u.attr->name;
				ret = 1;
			} else {
				LOG(L_CRIT, "BUG: do_action: Bad right side of avp assignment\n");
				ret=E_BUG;
				break;
			}

			/* If the action is assign then remove the old avp value
			 * before adding new ones */
/*			if ((unsigned char)a->type == ASSIGN_T) delete_avp(flags, name); */
			if (add_avp(flags & ~AVP_INDEX_ALL, name, value) < 0) {
				LOG(L_CRIT, "ERROR: Failed to assign value to attribute\n");
				ret=E_UNSPEC;
				break;
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
	int ret;
	static int rec_lev=0;
	static jmp_buf jmp_env;
	struct sr_module *mod;

	ret=E_UNSPEC;
	rec_lev++;
	if (rec_lev>ROUTE_MAX_REC_LEV){
		LOG(L_ERR, "WARNING: too many recursive routing table lookups (%d)"
					" giving up!\n", rec_lev);
		ret=E_UNSPEC;
		goto error;
	}
	if (rec_lev==1){
		run_flags=0;
		last_retcode=0;
		if (setjmp(jmp_env)){
			rec_lev=0;
			ret=last_retcode;
			goto end;
		}
	}

	if (a==0){
		DBG("DEBUG: run_actions: null action list (rec_level=%d)\n",
			rec_lev);
		ret=1;
	}

	for (t=a; t!=0; t=t->next){
		ret=do_action(t, msg);
		if (run_flags & (RETURN_R_F|EXIT_R_F)){
			if (run_flags & EXIT_R_F){
				last_retcode=ret;
				longjmp(jmp_env, ret);
			}
			break;
		}
		/* ignore error returns */
	}

	rec_lev--;
end:
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



