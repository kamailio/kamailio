/*
 * $Id$
 */



#include "action.h"
#include "config.h"
#include "error.h"
#include "dprint.h"
#include "proxy.h"
#include "forward.h"
#include "udp_server.h"
#include "route.h"
#include "msg_parser.h"
#include "ut.h"
#include "sr_module.h"
#include "mem/mem.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
	struct sockaddr_in* to;
	struct proxy_l* p;
	char* tmp;
	char *new_uri, *end, *crt;
	int len;
	int user;
	int err;
	struct sip_uri uri;
	unsigned short port;

	ret=E_BUG;
	switch (a->type){
		case DROP_T:
				ret=0;
			break;
		case FORWARD_T:
			if (a->p1_type==URIHOST_ST){
				/*parse uri*/
				if (msg->new_uri.s){
						tmp=msg->new_uri.s;
						len=msg->new_uri.len;
				}else{
						tmp=msg->first_line.u.request.uri.s;
						len=msg->first_line.u.request.uri.len;
				}
				if (parse_uri(tmp, len, &uri)<0){
					LOG(L_ERR, "ERROR: do_action: forward: bad_uri <%s>,"
								" dropping packet\n",tmp);
					ret=E_UNSPEC;
					break;
				}
				switch (a->p2_type){
					case URIPORT_ST:
									if (uri.port.s){
									 /*port=strtol(uri.port.s,&end,10);*/
										port=str2s(uri.port.s, uri.port.len,
													&err);
										/*if ((end)&&(*end)){*/
										if (err){
											LOG(L_ERR, "ERROR: do_action: "
													"forward: bad port in "
													"uri: <%s>\n", uri.port);
											ret=E_UNSPEC;
											goto error_fwd_uri;
										}
									}else port=SIP_PORT;
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
				/* create a temporary proxy*/
				p=mk_proxy(uri.host.s, port);
				if (p==0){
					LOG(L_ERR, "ERROR:  bad host name in uri,"
							" dropping packet\n");
					ret=E_BAD_ADDRESS;
					goto error_fwd_uri;
				}
				ret=forward_request(msg, p);
				free_uri(&uri);
				free_proxy(p); /* frees only p content, not p itself */
				free(p);
				if (ret>=0) ret=1;
			}else if ((a->p1_type==PROXY_ST) && (a->p2_type==NUMBER_ST)){
				ret=forward_request(msg,(struct proxy_l*)a->p1.data);
				if (ret>=0) ret=1;
			}else{
				LOG(L_CRIT, "BUG: do_action: bad forward() types %d, %d\n",
						a->p1_type, a->p2_type);
				ret=E_BUG;
			}
			break;
		case SEND_T:
			to=(struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));
			if (to==0){
				LOG(L_ERR, "ERROR: do_action: "
							"memory allocation failure\n");
				ret=E_OUT_OF_MEM;
				break;
			}
			if ((a->p1_type!= PROXY_ST)|(a->p2_type!=NUMBER_ST)){
				LOG(L_CRIT, "BUG: do_action: bad send() types %d, %d\n",
						a->p1_type, a->p2_type);
				ret=E_BUG;
				break;
			}
			
			p=(struct proxy_l*)a->p1.data;
			
			to->sin_family = AF_INET;
			to->sin_port=(p->port)?htons(p->port):htons(SIP_PORT);
			if (p->ok==0){
				if (p->host.h_addr_list[p->addr_idx+1])
					p->addr_idx++;
				else 
					p->addr_idx=0;
				p->ok=1;
			}
			to->sin_addr.s_addr=*((long*)p->host.h_addr_list[p->addr_idx]);
			p->tx++;
			p->tx_bytes+=msg->len;
			ret=udp_send(msg->orig, msg->len, (struct sockaddr*)to,
					sizeof(struct sockaddr_in));
			free(to);
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
							"route(%d)\n", a->p1.number);
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
		case SET_HOST_T:
		case SET_HOSTPORT_T:
		case SET_USER_T:
		case SET_USERPASS_T:
		case SET_PORT_T:
		case SET_URI_T:
				user=0;
				if (a->p1_type!=STRING_ST){
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
					free_uri(&uri);
					break;
				}
				end=new_uri+MAX_URI_SIZE;
				crt=new_uri;
				/* begin copying */
				len=strlen("sip:"); if(crt+len>end) goto error_uri;
				memcpy(crt,"sip:",len);crt+=len;
				/* user */
				if ((a->type==SET_USER_T)||(a->type==SET_USERPASS_T))
					tmp=a->p1.string;
				else 
					tmp=uri.user.s;
				if (tmp){
					len=strlen(tmp); if(crt+len>end) goto error_uri;
					memcpy(crt,tmp,len);crt+=len;
					user=1; /* we have an user field so mark it */
				}
				if (a->type==SET_USERPASS_T) tmp=0;
				else tmp=uri.passwd.s;
				/* passwd */
				if (tmp){
					len=strlen(":"); if(crt+len>end) goto error_uri;
					memcpy(crt,":",len);crt+=len;
					len=strlen(tmp); if(crt+len>end) goto error_uri;
					memcpy(crt,tmp,len);crt+=len;
				}
				/* host */
				if (user || tmp){ /* add @ */
					len=strlen("@"); if(crt+len>end) goto error_uri;
					memcpy(crt,"@",len);crt+=len;
				}
				if ((a->type==SET_HOST_T) ||(a->type==SET_HOSTPORT_T))
					tmp=a->p1.string;
				else
					tmp=uri.host.s;
				if (tmp){
					len=strlen(tmp); if(crt+len>end) goto error_uri;
					memcpy(crt,tmp,len);crt+=len;
				}
				/* port */
				if (a->type==SET_HOSTPORT_T) tmp=0;
				else if (a->type==SET_PORT_T) tmp=a->p1.string;
				else tmp=uri.port.s;
				if (tmp){
					len=strlen(":"); if(crt+len>end) goto error_uri;
					memcpy(crt,":",len);crt+=len;
					len=strlen(tmp); if(crt+len>end) goto error_uri;
					memcpy(crt,tmp,len);crt+=len;
				}
				/* params */
				tmp=uri.params.s;
				if (tmp){
					len=strlen(";"); if(crt+len>end) goto error_uri;
					memcpy(crt,";",len);crt+=len;
					len=strlen(tmp); if(crt+len>end) goto error_uri;
					memcpy(crt,tmp,len);crt+=len;
				}
				/* headers */
				tmp=uri.headers.s;
				if (tmp){
					len=strlen("?"); if(crt+len>end) goto error_uri;
					memcpy(crt,"?",len);crt+=len;
					len=strlen(tmp); if(crt+len>end) goto error_uri;
					memcpy(crt,tmp,len);crt+=len;
				}
				*crt=0; /* null terminate the thing */
				/* copy it to the msg */
				if (msg->new_uri.s) pkg_free(msg->new_uri.s);
				msg->new_uri.s=new_uri;
				msg->new_uri.len=crt-new_uri;
				free_uri(&uri);
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
		default:
			LOG(L_CRIT, "BUG: do_action: unknown type %d\n", a->type);
	}
skip:
	return ret;
	
error_uri:
	LOG(L_ERR, "ERROR: do_action: set*: uri too long\n");
	free_uri(&uri);
	if (new_uri) free(new_uri);
	return E_UNSPEC;
error_fwd_uri:
	free_uri(&uri);
	return ret;
}



/* returns: 0, or 1 on success, <0 on error */
/* (0 if drop or break encountered, 1 if not ) */
int run_actions(struct action* a, struct sip_msg* msg)
{
	struct action* t;
	int ret;
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



