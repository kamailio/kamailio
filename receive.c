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
 *
 * History:
 * ---------
 * 2003-02-28 scratchpad compatibility abandoned (jiri)
 * 2003-01-29 transport-independent message zero-termination in
 *            receive_msg (jiri)
 * 2003-02-07 undoed jiri's zero term. changes (they break tcp) (andrei)
 * 2003-02-10 moved zero-term in the calling functions (udp_receive &
 *            tcp_read_req)
 * 2003-08-13 fixed exec_pre_cb returning 0 (backported from stable) (andrei)
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
/* address preset vars */
str default_global_address={0,0};
str default_global_port={0,0};
str default_via_address={0,0};
str default_via_port={0,0};



/* WARNING: buf must be 0 terminated (buf[len]=0) or some things might 
 * break (e.g.: modules/textops)
 */
int receive_msg(char* buf, unsigned int len, struct receive_info* rcv_info) 
{
	struct sip_msg* msg;
	int ret;
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
	msg->rcv=*rcv_info;
	msg->id=msg_no;
	msg->set_global_address=default_global_address;
	msg->set_global_port=default_global_port;
	
	if (parse_msg(buf,len, msg)!=0){
		LOG(L_ERR, "ERROR: receive_msg: parse_msg failed\n");
		goto error02;
	}
	DBG("After parse_msg...\n");

	/* execute pre-script callbacks, if any; -jiri */
	/* if some of the callbacks said not to continue with
	   script processing, don't do so
	*/
	ret=exec_pre_cb(msg);
	if (ret<=0){
		if (ret<0) goto error;
		else goto end; /* drop the message -- no error -- andrei */
	}

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
end:
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
	pkg_free(msg);
error00:
	STATS_RX_DROPS;
	return -1;
}

