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
#include "mem.h"


#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif

#ifdef STATS
#include "stats.h"
#endif

unsigned int msg_no=0;

int receive_msg(char* buf, unsigned int len, unsigned long src_ip)
{
	struct sip_msg* msg;
#ifdef STATS
	int skipped = 1;
#endif

	msg=pkg_malloc(sizeof(struct sip_msg));
	if (msg==0) goto error1;
	msg_no++;

	memset(msg,0, sizeof(struct sip_msg)); /* init everything to 0 */
	/* fill in msg */
	msg->buf=buf;
	msg->len=len;
	msg->src_ip=src_ip;
	msg->id=msg_no;
	/* make a copy of the message */
	msg->orig=(char*) pkg_malloc(len+1);
	if (msg->orig==0){
		LOG(L_ERR, "ERROR:receive_msg: memory allocation failure\n");
		goto error1;
	}
	memcpy(msg->orig, buf, len);
	msg->orig[len]=0; /* null terminate it,good for using str* functions
						 on it*/
	
	if (parse_msg(buf,len, msg)!=0){
		goto error;
	}
	DBG("After parse_msg...\n");
	if (msg->first_line.type==SIP_REQUEST){
		DBG("msg= request\n");
		/* sanity checks */
		if ((msg->via1==0) || (msg->via1->error!=VIA_PARSE_OK)){
			/* no via, send back error ? */
			LOG(L_ERR, "ERROR: receive_msg: no via found in request\n");
			goto error;
		}
		/* check if neccesarry to add receive?->moved to forward_req */
		
		/* exec routing script */
		DBG("preparing to run routing scripts...\n");
		if (run_actions(rlist[0], msg)<0){
			LOG(L_WARN, "WARNING: receive_msg: "
					"error while trying script\n");
			goto error;
		}
#ifdef STATS
		/* jku -- update request statistics  */
		else update_received_request(msg->first_line.u.request.method_value );
#endif
	}else if (msg->first_line.type==SIP_REPLY){
		DBG("msg= reply\n");
		/* sanity checks */
		if ((msg->via1==0) || (msg->via1->error!=VIA_PARSE_OK)){
			/* no via, send back error ? */
			LOG(L_ERR, "ERROR: receive_msg: no via found in reply\n");
			goto error;
		}
		if ((msg->via2==0) || (msg->via2->error!=VIA_PARSE_OK)){
			/* no second via => error? */
			LOG(L_ERR, "ERROR: receive_msg: no 2nd via found in reply\n");
			goto error;
		}
		/* check if via1 == us */

#ifdef STATS
		/* jku -- update statistics  */
		update_received_response( msg->first_line.u.reply.statusclass );
#endif
		
		/* send the msg */
		if (forward_reply(msg)==0){
			DBG(" reply forwarded to %s:%d\n", 
						msg->via2->host.s,
						(unsigned short) msg->via2->port);
		}
	}
#ifdef STATS
	skipped = 0;
#endif
skip:
	DBG("skip:...\n");
	free_sip_msg(msg);
	pkg_free(msg);
#ifdef STATS
	if (skipped) update_received_drops;
#endif
	return 0;
error:
	DBG("error:...\n");
	free_sip_msg(msg);
	pkg_free(msg);
#ifdef STATS
	update_received_drops;
#endif
	return -1;
error1:
	if (msg) pkg_free(msg);
	pkg_free(buf);
#ifdef STATS
	update_received_drops;
#endif
	return -1;
}

