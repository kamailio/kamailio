/*
 * $Id$
 */



#include "action.h"
#include "config.h"
#include "error.h"
#include "dprint.h"
#include "proxy.h"

#include <netdb.h>
#include <stdlib.h>

/* ret= 0 if action -> end of lis t(e.g DROP), >0
   and >0 on error */
int do_action(struct action* a, struct sip_msg* msg)
{
	int ret;
	struct sockaddr_in* to;
	struct proxy_l* p;

	switch (a->type){
		case DROP_T:
				ret=0;
			break;
		case FORWARD_T:
			if (a->p1_type!= PROXY_ST){
				LOG(L_CRIT, "BUG: do_action: bad type %d\n", a->p1_type);
				ret=E_BUG;
				break;
			}
			ret=forward_request(msg, (struct proxy_l*)a->p1.data);
			if (ret>=0) ret=1;
			break;
		case SEND_T:
			to=(struct sockaddr_in*) malloc(sizeof(struct sockaddr));
			if (to==0){
				LOG(L_ERR, "ERROR: do_action: "
							"memory allocation failure\n");
				ret=E_OUT_OF_MEM;
				break;
			}
			if (a->p1_type!= PROXY_ST){
				LOG(L_CRIT, "BUG: do_action: bad type %d\n", a->p1_type);
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
			ret=udp_send(msg->orig, msg->len, to, sizeof(struct sockaddr));
			free(to);
			if (ret<0){
				p->errors++;
				p->ok=0;
			}else ret=1;
			
			break;
		case LOG_T:
			LOG(a->p2.number, a->p1.string);
			ret=1;
			break;
		case ERROR_T:
			LOG(L_NOTICE, "WARNING: do_action: error(\"%s\", \"%s\") "
					"not implemented yet\n", a->p1.string, a->p2.string);
			ret=1;
			break;
		case ROUTE_T:
			LOG(L_NOTICE, "WARNING: do_action: route(%d) not implemented "
							"yet\n", a->p1.number);
			break;
		case EXEC_T:
			LOG(L_NOTICE, "WARNING: exec(\"%s\") not fully implemented,",
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
	
	if (a==0){
		LOG(L_ERR, "WARNING: run_actions: null action list\n");
		ret=0;
	}

	for (t=a; t!=0; t=t->next){
		ret=do_action(t, msg);
		if(ret==0) break;
		else if (ret<0){ ret=-1; goto error; }
	}
	ret=0;

error:
	return ret;
}



