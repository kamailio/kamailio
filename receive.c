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
	char * orig;

	/* make a copy of the message */
	orig=(char*) malloc(len);
	if (orig==0){
		DPrint("ERROR: memory allocation failure\n");
		goto error1;
	}
	memcpy(orig, buf, len);
	
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
		re=route_match(  msg.first_line.u.request.method,
						 msg.first_line.u.request.uri,
						 &rlist
					  );
		if (re==0){
			/* no route found, send back error msg? */
			DPrint("WARNING: no route found!\n");
			goto skip;
		}
		re->tx++;
		/* send msg */
		DPrint(" found route to: %s\n", re->host.h_name);
		forward_request(orig, buf, len, &msg, re, src_ip);
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
		if (forward_reply(orig, buf, len, &msg)==0){
			DPrint(" reply forwarded to %s:%d\n", 
						msg.via2.host,
						(unsigned short) msg.via2.port);
		}
	}
skip:
	free(orig);
	return 0;
error:
	free(orig);
error1:
	return -1;
}

