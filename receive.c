/* 
 *$Id$
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
#include "script_cb.h"
#include "dset.h"


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
	if (msg==0) {
		LOG(L_ERR, "ERROR: receive_msg: no mem for sip_msg\n");
		goto error00;
	}
	msg_no++;
	/* number of vias parsed -- good for diagnostic info in replies */
	via_cnt=0;

	memset(msg,0, sizeof(struct sip_msg)); /* init everything to 0 */
	/* fill in msg */
	msg->buf=buf;
	msg->len=len;
	/* zero termination (termination of orig message bellow not that
	   useful as most of the work is done with scrath-pad; -jiri  */
	/* buf[len]=0; */ /* WARNING: zero term removed! */
	su2ip_addr(&msg->src_ip, src_su);
	msg->dst_ip=bind_address->address; /* won't work if listening on 0.0.0.0 */
	msg->id=msg_no;
	/* make a copy of the message */
	msg->orig=(char*) pkg_malloc(len+1);
	if (msg->orig==0){
		LOG(L_ERR, "ERROR:receive_msg: memory allocation failure\n");
		goto error01;
	}
	memcpy(msg->orig, buf, len);
	/* WARNING: zero term removed! */
	/* msg->orig[len]=0; */ /* null terminate it,good for using str* functions
						 on it*/
	
	if (parse_msg(buf,len, msg)!=0){
		LOG(L_ERR, "ERROR: receive_msg: parse_msg failed\n");
		goto error02;
	}
	DBG("After parse_msg...\n");

	/* execute pre-script callbacks, if any; -jiri */
	/* if some of the callbacks said not to continue with
	   script processing, don't do so
	*/
	if (exec_pre_cb(msg)==0) goto error;

	/* ... and clear branches from previous message */
	clear_branches();

	if (msg->first_line.type==SIP_REQUEST){
		/* sanity checks */
		if ((msg->via1==0) || (msg->via1->error!=PARSE_OK)){
			/* no via, send back error ? */
			LOG(L_ERR, "ERROR: receive_msg: no via found in request\n");
			goto error;
		}
		/* check if neccesarry to add receive?->moved to forward_req */

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
		/* check if via1 == us */
#endif

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
	/* execute post-script callbacks, if any; -jiri */
	exec_post_cb(msg);
	DBG("receive_msg: cleaning up\n");
	free_sip_msg(msg);
	pkg_free(msg);
#ifdef STATS
	if (skipped) STATS_RX_DROPS;
#endif
	return 0;
error:
	DBG("error:...\n");
	/* execute post-script callbacks, if any; -jiri */
	exec_post_cb(msg);
error02:
	free_sip_msg(msg);
error01:
	pkg_free(msg);
error00:
	STATS_RX_DROPS;
	return -1;
}

