/* 
 *$Id$
 */

#include <string.h>

#include "receive.h"
#include "dprint.h"
#include "route.h"
#include "msg_parser.h"
#include "forward.h"


int receive_msg(char* buf, unsigned int len, unsigned long src_ip)
{
	struct sip_msg msg;
	struct route_elem *re;

	/* fill in msg */
	msg.buf=buf;
	msg.len=len;
	msg.src_ip=src_ip;
	/* make a copy of the message */
	msg.orig=(char*) malloc(len);
	if (msg.orig==0){
		LOG(L_ERR, "ERROR:receive_msg: memory allocation failure\n");
		goto error1;
	}
	memcpy(msg.orig, buf, len);
	
	if (parse_msg(buf,len, &msg)!=0){
		goto error;
	}
	
	if (msg.first_line.type==SIP_REQUEST){
		/* sanity checks */
		if (msg.via1.error!=VIA_PARSE_OK){
			/* no via, send back error ? */
			goto skip;
		}
		/* check if neccesarry to add receive? */
		
		/* find route */
		re=route_match( &msg, &rlist[0]);
		if (re==0){
			/* no route found, send back error msg? */
			LOG(L_WARN, "WARNING: receive_msg: no route found!\n");
			goto skip;
		}
		re->tx++;
		/* send msg */
		DBG(" found route \n");
		if (run_actions(re->actions)<0){
			LOG(L_WARN, "WARNING: receive_msg: "
					"error while trying actions\n");
			goto error;
		}
	}else if (msg.first_line.type==SIP_REPLY){
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
		
		/* send the msg */
		if (forward_reply(&msg)==0){
			DBG(" reply forwarded to %s:%d\n", 
						msg.via2.host,
						(unsigned short) msg.via2.port);
		}
	}
skip:
	free(msg.orig);
	return 0;
error:
	free(msg.orig);
error1:
	return -1;
}

