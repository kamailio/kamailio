/*
 * $Id$
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
 /*
  * History:
  * -------
  * 2003-02-11  modified sl_send_reply to use the transport independend
  *              msg_send  (andrei)
  * 2003-02-18  replaced TOTAG_LEN w/ TOTAG_VALUE_LEN (it was defined twice
  *              w/ different values!)  (andrei)
  * 2003-03-06  aligned to request2response use of tag bookmarks (jiri)
  * 2003-04-04  modified sl_send_reply to use src_port if rport is present
  *              in the topmost via (andrei)
  */


#include "../../globals.h"
#include "../../forward.h"
#include "../../dprint.h"
#include "../../md5utils.h"
#include "../../msg_translator.h"
#include "../../udp_server.h"
#include "../../timer.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../crc.h"
#include "../../dset.h"
#include "../../data_lump_rpl.h"
#include "../../action.h"
#include "../../config.h"
#include "../../tags.h"
#include "sl_stats.h"
#include "sl_funcs.h"


/* to-tag including pre-calculated and fixed part */
static char           sl_tag[TOTAG_VALUE_LEN];
/* from here, the variable prefix begins */
static char           *tag_suffix;
/* if we for this time did not send any stateless reply,
   we do not filter */
static unsigned int  *sl_timeout;


int sl_startup()
{

	init_tags( sl_tag, &tag_suffix,
			"SER-stateless",
			SL_TOTAG_SEPARATOR );

	/*timeout*/
	sl_timeout = (unsigned int*)shm_malloc(sizeof(unsigned int));
	if (!sl_timeout)
	{
		LOG(L_ERR,"ERROR:sl_startup: no more free memory!\n");
		return -1;
	}
	*(sl_timeout)=get_ticks();

	return 1;
}




int sl_shutdown()
{
	if (sl_timeout)
		shm_free(sl_timeout);
	return 1;
}

#ifdef _MOVED_TO_CORE
static void calc_crc_suffix( struct sip_msg *msg )
{
	int ss_nr;
	str suffix_source[3];

	ss_nr=2;
	suffix_source[0]=msg->via1->host;
	suffix_source[1]=msg->via1->port_str;
	if (msg->via1->branch) 
		suffix_source[ss_nr++]=msg->via1->branch->value;
	crcitt_string_array( tag_suffix, suffix_source, ss_nr );
}
#endif


int sl_send_reply(struct sip_msg *msg ,int code ,char *text )
{
	char               *buf;
	unsigned int       len;
	union sockaddr_union to;
	char *dset;
	struct lump_rpl *dset_lump;
	int dset_len;
	struct bookmark dummy_bm;


	if ( msg->first_line.u.request.method_value==METHOD_ACK)
	{
		LOG(L_WARN, "Warning: sl_send_reply: I won't send a reply for ACK!!\n");
		goto error;
	}

	/* family will be updated in update_sock to whatever appropriate;
	   -jiri
	to.sin_family = AF_INET; */

	if (reply_to_via) {
		if (update_sock_struct_from_via(  &(to), msg, msg->via1 )==-1)
		{
			LOG(L_ERR, "ERROR: sl_send_reply: "
				"cannot lookup reply dst: %s\n",
				msg->via1->host.s );
			goto error;
		}
	} else update_sock_struct_from_ip( &to, msg );

	/* if that is a redirection message, dump current message set to it */
	if (code>=300 && code<400) {
		dset=print_dset(msg, &dset_len);
		if (dset) {
			dset_lump=build_lump_rpl(dset, dset_len);
			add_lump_rpl(msg, dset_lump);
		}
	}

	/* add a to-tag if there is a To header field without it */
	if ( 	/* since RFC3261, we append to-tags anywhere we can, except
		 * 100 replies */
       		/* msg->first_line.u.request.method_value==METHOD_INVITE && */
		code>=180 &&
		(msg->to || (parse_headers(msg,HDR_TO, 0)!=-1 && msg->to))
		&& (get_to(msg)->tag_value.s==0 || get_to(msg)->tag_value.len==0) ) 
	{
		calc_crc_suffix( msg, tag_suffix );
		buf = build_res_buf_from_sip_req(code,text,sl_tag,TOTAG_VALUE_LEN,
											msg ,&len, &dummy_bm);
	} else {
		buf = build_res_buf_from_sip_req(code,text,0,0,msg ,&len, &dummy_bm);
	}
	if (!buf)
	{
		DBG("DEBUG: sl_send_reply: response building failed\n");
		goto error;
	}
	

	if (msg_send(0, msg->rcv.proto, &to, msg->rcv.proto_reserved1, buf, len)<0)
		goto error;
	
	*(sl_timeout) = get_ticks() + SL_RPL_WAIT_TIME;
	pkg_free(buf);

	update_sl_stats(code);
	return 1;

error:
	update_sl_failures();
	return -1;
}


int sl_reply_error(struct sip_msg *msg )
{
	char err_buf[MAX_REASON_LEN];
	int sip_error;
	int ret;

	ret=err2reason_phrase( prev_ser_error, &sip_error, 
		err_buf, sizeof(err_buf), "SL");
	if (ret>0) {
		sl_send_reply( msg, sip_error, err_buf );
		LOG(L_ERR, "ERROR: sl_reply_error used: %s\n", 
			err_buf );
		return 1;
	} else {
		LOG(L_ERR, "ERROR: sl_reply_error: err2reason failed\n");
		return -1;
	}
}



/* Returns:
    0  : ACK to a local reply
    -1 : error
    1  : is not an ACK  or a non-local ACK
*/
int sl_filter_ACK(struct sip_msg *msg, void *bar )
{
	str *tag_str;

	if (msg->first_line.u.request.method_value!=METHOD_ACK)
		goto pass_it;

	/*check the timeout value*/
	if ( *(sl_timeout)<= get_ticks() )
	{
		DBG("DEBUG : sl_filter_ACK: to late to be a local ACK!\n");
		goto pass_it;
	}

	/*force to parse to header -> we need it for tag param*/
	if (parse_headers( msg, HDR_TO, 0 )==-1)
	{
		LOG(L_ERR,"ERROR : SL_FILTER_ACK: unable to parse To header\n");
		return -1;
	}

	if (msg->to) {
		tag_str = &(get_to(msg)->tag_value);
		if ( tag_str->len==TOTAG_VALUE_LEN )
		{
			/* calculate the variable part of to-tag */	
			calc_crc_suffix(msg, tag_suffix);
			/* test whether to-tag equal now */
			if (memcmp(tag_str->s,sl_tag,TOTAG_VALUE_LEN)==0) {
				DBG("DEBUG: sl_filter_ACK : local ACK found -> dropping it! \n" );
				return 0;
			}
		}
	}

pass_it:
	return 1;
}

