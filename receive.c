/* 
 *$Id$
 */

#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "receive.h"
#include "globals.h"
#include "dprint.h"
#include "route.h"
#include "parser/msg_parser.h"
#include "forward.h"
#include "action.h"
#include "mem/mem.h"
#include "stats.h"
#include "ip_addr.h"


#ifdef DEBUG_DMALLOC
#include <mem/dmalloc.h>
#endif

unsigned int msg_no=0;

int receive_msg(char* buf, unsigned int len, union sockaddr_union* src_su)
{
	struct sip_msg* msg;
#ifdef STATS
	int skipped = 1;
	struct timeval tvb, tve;	
	struct timezone tz;
	unsigned int diff;
#endif

	msg=pkg_malloc(sizeof(struct sip_msg));
	if (msg==0) goto error1;
	msg_no++;

	memset(msg,0, sizeof(struct sip_msg)); /* init everything to 0 */
	/* fill in msg */
	msg->buf=buf;
	msg->len=len;
	/* zero termination (termination of orig message bellow not that
	   useful as most of the work is done with scrath-pad; -jiri  */
	buf[len]=0;
	su2ip_addr(&msg->src_ip, src_su);
	msg->dst_ip=bind_address->address; /* won't work if listening on 0.0.0.0 */
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
		/* sanity checks */
		if ((msg->via1==0) || (msg->via1->error!=PARSE_OK)){
			/* no via, send back error ? */
			LOG(L_ERR, "ERROR: receive_msg: no via found in request\n");
			goto error;
		}
		/* check if neccesarry to add receive?->moved to forward_req */

		/* loop checks */
		if (loop_checks) {
			DBG("WARNING: receive_msg: Placeholder for loop check."
				" NOT implemented yet.\n");
		}
		
		/* exec routing script */
		DBG("preparing to run routing scripts...\n");
#ifdef  STATS
		gettimeofday( & tvb, &tz );
#endif
		if (run_actions(rlist[0], msg)<0){
			LOG(L_WARN, "WARNING: receive_msg: "
					"error while trying script\n");
			goto error;
		}
#ifdef STATS
		gettimeofday( & tve, &tz );
		diff = (tve.tv_sec-tvb.tv_sec)*1000000+(tve.tv_usec-tvb.tv_usec);
		stats->processed_requests++;
		stats->acc_req_time += diff;
		DBG("succesfully ran routing scripts...(%d usec)\n", diff);
		STATS_RX_REQUEST( msg->first_line.u.request.method_value );
#endif
	}else if (msg->first_line.type==SIP_REPLY){
		/* sanity checks */
		if ((msg->via1==0) || (msg->via1->error!=PARSE_OK)){
			/* no via, send back error ? */
			LOG(L_ERR, "ERROR: receive_msg: no via found in reply\n");
			goto error;
		}
#if 0
		if ((msg->via2==0) || (msg->via2->error!=PARSE_OK)){
			/* no second via => error? */
			LOG(L_ERR, "ERROR: receive_msg: no 2nd via found in reply\n");
			goto error;
		}
#endif
		/* check if via1 == us */

#ifdef STATS
		gettimeofday( & tvb, &tz );
		STATS_RX_RESPONSE ( msg->first_line.u.reply.statuscode / 100 );
#endif
		
		/* send the msg */
		forward_reply(msg);

#ifdef STATS
		gettimeofday( & tve, &tz );
		diff = (tve.tv_sec-tvb.tv_sec)*1000000+(tve.tv_usec-tvb.tv_usec);
		stats->processed_responses++;
		stats->acc_res_time+=diff;
		DBG("succesfully ran reply processing...(%d usec)\n", diff);
#endif
	}
#ifdef STATS
	skipped = 0;
#endif
/* jku: skip no more used
skip:
	DBG("skip:...\n");
*/
	DBG("receive_msg: cleaning up\n");
	free_sip_msg(msg);
	pkg_free(msg);
#ifdef STATS
	if (skipped) STATS_RX_DROPS;
#endif
	return 0;
error:
	DBG("error:...\n");
	free_sip_msg(msg);
	pkg_free(msg);
	STATS_RX_DROPS;
	return -1;
error1:
	if (msg) pkg_free(msg);
	pkg_free(buf);
	STATS_RX_DROPS;
	return -1;
}

