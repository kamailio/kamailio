/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 * 2003-02-11  modified sl_send_reply to use the transport independent
 *              msg_send  (andrei)
 * 2003-02-18  replaced TOTAG_LEN w/ TOTAG_VALUE_LEN (it was defined twice
 *              w/ different values!)  (andrei)
 * 2003-03-06  aligned to request2response use of tag bookmarks (jiri)
 * 2003-04-04  modified sl_send_reply to use src_port if rport is present
 *              in the topmost via (andrei)
 * 2003-09-11: updated to new build_lump_rpl() interface (bogdan)
 * 2003-09-11: sl_tag converted to str to fit to the new
 *               build_res_buf_from_sip_req() interface (bogdan)
 * 2003-11-11: build_lump_rpl() removed, add_lump_rpl() has flags (bogdan)
 * 2004-10-10: use of mhomed disabled for replies (jiri)
 * 2006-03-29: callbacks for sending replies added (bogdan)
 * 2006-11-28: Moved numerical code tracking out of sl_send_reply() and into
 *             update_sl_reply_stat(). Also added more detail stat collection.
 *             (Jeffrey Magder - SOMA Networks)
 */

/*!
 * \file
 * \brief SL :: functions
 * \ingroup sl
 * - Module: \ref sl
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
#include "../../parser/parse_to.h"
#include "../../config.h"
#include "../../tags.h"
#include "sl.h"
#include "sl_funcs.h"
#include "sl_cb.h"
#include "../../usr_avp.h"
#include <string.h>
#include "../../lib/kcore/statistics.h"

/* module parameter */
extern int sl_enable_stats;

/* statistic variables */
extern stat_var *tx_1xx_rpls;
extern stat_var *tx_2xx_rpls;
extern stat_var *tx_3xx_rpls;
extern stat_var *tx_4xx_rpls;
extern stat_var *tx_5xx_rpls;
extern stat_var *tx_6xx_rpls;
extern stat_var *sent_rpls;
extern stat_var *sent_err_rpls;
extern stat_var *rcv_acks;


/* to-tag including pre-calculated and fixed part */
static char           sl_tag_buf[TOTAG_VALUE_LEN];
static str            sl_tag = {sl_tag_buf,TOTAG_VALUE_LEN};
/* from here, the variable prefix begins */
static char           *tag_suffix;
/* if we for this time did not send any stateless reply,
   we do not filter */
static unsigned int  *sl_timeout = 0;


/*! SL startup helper */
int sl_startup(void)
{

	init_tags( sl_tag.s, &tag_suffix,
			"Kamailio-stateless",
			SL_TOTAG_SEPARATOR );

	/*timeout*/
	sl_timeout = (unsigned int*)shm_malloc(sizeof(unsigned int));
	if (!sl_timeout)
	{
		LM_ERR("no more shm memory!\n");
		return -1;
	}
	*(sl_timeout)=get_ticks();

	return 0;
}


/*! SL shutdown helper */
int sl_shutdown(void)
{
	if (sl_timeout)
		shm_free(sl_timeout);
	return 1;
}


/*! Take care of the statistics associated with numerical codes and replies */
static inline void update_sl_reply_stat(int code) 
{
	stat_var *numerical_stat;

	/* If stats aren't enabled, just skip over this. */
	if (!sl_enable_stats) 
		return;

	/* Kamailio already kept track of the total number of 1xx, 2xx, replies.
	* There may be setups that still expect these variables to exist, so we
	* don't touch them */
	if (code < 200 ) {
		update_stat( tx_1xx_rpls , 1);
	} else if (code<300) {
		update_stat( tx_2xx_rpls , 1);
	} else if (code<400) {
		update_stat( tx_3xx_rpls , 1);
	} else if (code<500) {
		update_stat( tx_4xx_rpls , 1);
	} else if (code<600) {
		update_stat( tx_5xx_rpls , 1);
	} else {
		update_stat( tx_6xx_rpls , 1);
	}

	update_stat( sent_rpls , 1);

	numerical_stat = get_stat_var_from_num_code(code, 1);

	if (numerical_stat != NULL)
		update_stat(numerical_stat, 1);
}

int sl_get_reply_totag(struct sip_msg *msg, str *totag)
{
	if(msg==NULL || totag==NULL)
		return -1;
	calc_crc_suffix(msg, tag_suffix);
	*totag = sl_tag;
	return 1;
}

/*! sl_send_reply helper function */
int sl_send_reply_helper(struct sip_msg *msg ,int code, str *text, str *tag)
{
	str buf;
	char *dset;
	int dset_len;
	struct bookmark dummy_bm;
	int backup_mhomed;
	int ret;
	struct dest_info dst;
	char rbuf[256];

	if ( msg->first_line.u.request.method_value==METHOD_ACK)
		goto error;

	init_dest_info(&dst);
	if (reply_to_via) {
		if (update_sock_struct_from_via(  &dst.to, msg, msg->via1 )==-1)
		{		
			LOG(L_ERR, "ERROR: sl_send_reply: "
				"cannot lookup reply dst: %s\n",
				msg->via1->host.s );
			goto error;
		}
	} else update_sock_struct_from_ip( &dst.to, msg );

	/* if that is a redirection message, dump current message set to it */
	if (code>=300 && code<400) {
		dset=print_dset(msg, &dset_len);
		if (dset) {
			add_lump_rpl(msg, dset, dset_len, LUMP_RPL_HDR);
		}
	}

	/*sr core requres charz reason phrase */
	if(text->len>=256)
	{
		LOG(L_ERR, "ERROR: reason phrase too long\n");
		goto error;
	}
	strncpy(rbuf, text->s, text->len);
	rbuf[text->len] = '\0';

	/* add a to-tag if there is a To header field without it */
	if ( code>=180 &&
		(msg->to || (parse_headers(msg,HDR_TO_F, 0)!=-1 && msg->to))
		&& (get_to(msg)->tag_value.s==0 || get_to(msg)->tag_value.len==0) ) 
	{
		if(tag!=NULL && tag->s!=NULL) {
			buf.s = build_res_buf_from_sip_req( code, rbuf, tag,
						msg, (unsigned int*)&buf.len, &dummy_bm);
		} else {
			calc_crc_suffix( msg, tag_suffix );
			buf.s = build_res_buf_from_sip_req( code, rbuf, &sl_tag, msg,
				(unsigned int*)&buf.len, &dummy_bm);
		}
	} else {
		buf.s = build_res_buf_from_sip_req( code, rbuf, 0, msg,
			(unsigned int*)&buf.len, &dummy_bm);
	}
	if (!buf.s) {
		LM_ERR("response building failed\n");
		goto error;
	}

	run_sl_callbacks( SLCB_REPLY_OUT, msg, &buf, code, text, &dst.to );

	/* supress multhoming support when sending a reply back -- that makes sure
	   that replies will come from where requests came in; good for NATs
	   (there is no known use for mhomed for locally generated replies;
	    note: forwarded cross-interface replies do benefit of mhomed!
	*/
	backup_mhomed=mhomed;
	mhomed=0;
	/* use for sending the received interface -bogdan*/
	dst.proto=msg->rcv.proto;
	dst.send_sock=msg->rcv.bind_address;
	dst.id=msg->rcv.proto_reserved1;
#ifdef USE_COMP
	dst.comp=msg->via1->comp_no;
#endif
	dst.send_flags=msg->rpl_send_flags;
	ret = msg_send(&dst, buf.s, buf.len);
	mhomed=backup_mhomed;
	pkg_free(buf.s);

	if (ret<0)
		goto error;

	*(sl_timeout) = get_ticks() + SL_RPL_WAIT_TIME;

	update_sl_reply_stat(code);

	return 1;

error:
	return -1;
}


/*! small wrapper around sl_send_reply_helper */
int sl_send_reply(struct sip_msg *msg ,int code, str *text)
{
	return sl_send_reply_helper(msg, code, text, 0);
}

/*! small wrapper around sl_send_reply_helper */
int sl_send_reply_sz(struct sip_msg *msg ,int code, char *text)
{
	str r;
	r.s = text;
	r.len = strlen(text);
	return sl_send_reply_helper(msg, code, &r, 0);
}


/*! small wrapper around sl_send_reply_helper */
int sl_send_reply_dlg(struct sip_msg *msg ,int code, str *text, str *tag)
{
	return sl_send_reply_helper(msg, code, text, tag);
}


/*! Reply an SIP error */
int sl_reply_error(struct sip_msg *msg )
{
	char err_buf[MAX_REASON_LEN];
	int sip_error;
	str text;
	int ret;

	ret = err2reason_phrase( prev_ser_error, &sip_error, 
		err_buf, sizeof(err_buf), "SL");
	if (ret<=0) {
		LM_ERR("err2reason failed\n");
		return -1;
	}
	text.len = ret;
	text.s = err_buf;
	LM_DBG("error text is %.*s\n",text.len,text.s);

	ret = sl_send_reply_helper( msg, sip_error, &text, 0);
	if (ret==-1)
		return -1;
	if_update_stat( sl_enable_stats, sent_err_rpls , 1);
	return ret;
}



/*!
 * Filter ACKs
 * \return 0 for ACKs to a local reply, -1 on error, 1 is not an ACK or a non-local ACK
 */
int sl_filter_ACK(struct sip_msg *msg, unsigned int flags, void *bar )
{
	str *tag_str;

	if (msg->first_line.u.request.method_value!=METHOD_ACK)
		goto pass_it;

	/*check the timeout value*/
	if ( *(sl_timeout)<= get_ticks() )
	{
		LM_DBG("to late to be a local ACK!\n");
		goto pass_it;
	}

	/*force to parse to header -> we need it for tag param*/
	if (parse_headers( msg, HDR_TO_F, 0 )==-1)
	{
		LM_ERR("unable to parse To header\n");
		return -1;
	}

	if (msg->to) {
		tag_str = &(get_to(msg)->tag_value);
		if ( tag_str->len==TOTAG_VALUE_LEN )
		{
			/* calculate the variable part of to-tag */	
			calc_crc_suffix(msg, tag_suffix);
			/* test whether to-tag equal now */
			if (memcmp(tag_str->s,sl_tag.s,sl_tag.len)==0) {
				LM_DBG("local ACK found -> dropping it!\n");
				if_update_stat( sl_enable_stats, rcv_acks, 1);
				run_sl_callbacks( SLCB_ACK_IN, msg, 0, 0, 0, 0 );
				return 0;
			}
		}
	}

pass_it:
	return 1;
}
