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

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>


/* ret= 0 if action -> end of lis t(e.g DROP), >0
   and >0 on error */
int do_action(struct action* a, struct sip_msg* msg)
{
	int ret;
	struct sockaddr_in* to;
	struct proxy_l* p;
	struct route_elem* re;

	ret=E_BUG;
	switch (a->type){
		case DROP_T:
				ret=0;
			break;
		case FORWARD_T:
			if ((a->p1_type!= PROXY_ST)|(a->p2_type!=NUMBER_ST)){
				LOG(L_CRIT, "BUG: do_action: bad forward() types %d, %d\n",
						a->p1_type, a->p2_type);
				ret=E_BUG;
				break;
			}
			ret=forward_request(msg, (struct proxy_l*)a->p1.data);
			if (ret>=0) ret=1;
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
			re=route_match(msg, &rlist[a->p1.number]);
			if (re==0){
				LOG(L_INFO, "WARNING: do_action: route(%d): no new route"
						" found\n", a->p1.number);
				ret=1;
				break;
			}
			ret=((ret=run_actions(re->actions, msg))<0)?ret:1;
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
		default:
			LOG(L_CRIT, "BUG: do_action: unknown type %d\n", a->type);
	}
	return ret;
}



/* returns: 0 on success, -1 on error */
int run_actions(struct action* a, struct sip_msg* msg)
{
	struct action* t;
	int ret;
	static int rec_lev=0;

	rec_lev++;
	if (rec_lev>ROUTE_MAX_REC_LEV){
		LOG(L_ERR, "WARNING: too many recursive routing table lookups (%d)"
					" giving up!\n", rec_lev);
		ret=E_UNSPEC;
		goto error;
	}
		
	if (a==0){
		LOG(L_ERR, "WARNING: run_actions: null action list\n");
		ret=0;
	}

	for (t=a; t!=0; t=t->next){
		ret=do_action(t, msg);
		if(ret==0) break;
		else if (ret<0){ ret=-1; goto error; }
	}
	
	rec_lev--;
	return 0;
	

error:
	rec_lev--;
	return ret;
}



