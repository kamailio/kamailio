/* 
 *$Id$
 */

#include <string.h>
#include <stdlib.h>

#include "receive.h"
#include "dprint.h"
#include "route.h"
#include "msg_parser.h"
#include "forward.h"
#include "action.h"


#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif

#ifdef STATS
#include "stats.h"
#endif

int receive_msg(char* buf, unsigned int len, unsigned long src_ip)
{
	struct sip_msg msg;

#ifdef STATS
	stats.total_rx++;	
#endif

	memset(&msg,0, sizeof(struct sip_msg)); /* init everything to 0 */
	/* fill in msg */
	msg.buf=buf;
	msg.len=len;
	msg.src_ip=src_ip;
	/* make a copy of the message */
	msg.orig=(char*) malloc(len+1);
	if (msg.orig==0){
		LOG(L_ERR, "ERROR:receive_msg: memory allocation failure\n");
		goto error1;
	}
	memcpy(msg.orig, buf, len);
	msg.orig[len]=0; /* null terminate it,good for using str* functions on it*/
	
	if (parse_msg(buf,len, &msg)!=0){
		goto error;
	}
	DBG("Ater parse_msg...\n");
	
	if (msg.first_line.type==SIP_REQUEST){
		DBG("msg= request\n");
		/* sanity checks */
		if (msg.via1.error!=VIA_PARSE_OK){
			/* no via, send back error ? */
			goto skip;
		}
		/* check if neccesarry to add receive?->moved to forward_req */
		
		/* exec routing script */
		DBG("preparing to run routing scripts...\n");
		if (run_actions(rlist[0], &msg)<0){
			LOG(L_WARN, "WARNING: receive_msg: "
					"error while trying script\n");
			goto error;
		}
#ifdef STATS
		/* jku -- update statistics  */
		else stats.ok_rx_rq++;	
#endif
	}else if (msg.first_line.type==SIP_REPLY){
		DBG("msg= reply\n");
		/* sanity checks */
		if (msg.via1.error!=VIA_PARSE_OK){
			/* no via, send back error ? */
			goto skip;
		}
		if (msg.via2.error!=VIA_PARSE_OK){
			/* no second via => error? */
			goto skip;
		}
		/* check if via1 == us */

#ifdef STATS
		/* jku -- update statistics  */
		stats.ok_rx_rs++;	
#endif
		
		/* send the msg */
		if (forward_reply(&msg)==0){
			DBG(" reply forwarded to %s:%d\n", 
						msg.via2.host,
						(unsigned short) msg.via2.port);
		}
	}
skip:
	if (msg.new_uri.s) { free(msg.new_uri.s); msg.new_uri.len=0; }
	if (msg.add_rm) free_lump_list(msg.add_rm);
	if (msg.repl_add_rm) free_lump_list(msg.repl_add_rm);
	free(msg.orig);
	return 0;
error:
	if (msg.new_uri.s) free(msg.new_uri.s);
	if (msg.add_rm) free_lump_list(msg.add_rm);
	if (msg.repl_add_rm) free_lump_list(msg.repl_add_rm);
	free(msg.orig);
error1:
	return -1;
}

