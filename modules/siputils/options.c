/*
 * Options Reply Module
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*!
 * \file siputils/options.c
 * \brief SIP-utils :: Options reply modules
 * \ingroup siputils
 * Module: \ref siputils
 */

#ifdef EXTRA_DEBUG
#include <stdlib.h>   /* required by abort() */
#endif
#include "options.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../data_lump_rpl.h"
#include "../../parser/parse_uri.h"

static str opt_200_rpl = str_init("OK");
static str opt_500_rpl = str_init("Server internal error");


int opt_reply(struct sip_msg* _msg, char* _foo, char* _bar) {
	str rpl_hf;
	int offset = 0;

	/* check if it is called for an OPTIONS request */
	if (_msg->REQ_METHOD!=METHOD_OPTIONS) {
		LM_ERR("called for non-OPTIONS request\n");
		return -1;
	}
	if(_msg->parsed_uri_ok==0 && parse_sip_msg_uri(_msg)<0)
	{
		LM_ERR("ERROR while parsing the R-URI\n");
		return -1;
	}
	/* FIXME: should we additionally check if ruri == server addresses ?! */
	if (_msg->parsed_uri.user.len != 0) {
		LM_ERR("ruri contains username\n");
		return -1;
	}

	/* calculate the length and allocated the mem */
	rpl_hf.len = ACPT_STR_LEN + ACPT_ENC_STR_LEN + ACPT_LAN_STR_LEN + 
			SUPT_STR_LEN + 4*HF_SEP_STR_LEN + opt_accept.len + opt_accept_enc.len
			+ opt_accept_lang.len + opt_supported.len;
	rpl_hf.s = (char*)pkg_malloc(rpl_hf.len);
	if (!rpl_hf.s) {
		LM_CRIT("out of pkg memory\n");
		goto error;
	}

	/* create the header fields */
	memcpy(rpl_hf.s, ACPT_STR, ACPT_STR_LEN);
	offset = ACPT_STR_LEN;
	memcpy(rpl_hf.s + offset, opt_accept.s, opt_accept.len);
	offset += opt_accept.len;
	memcpy(rpl_hf.s + offset, HF_SEP_STR, HF_SEP_STR_LEN);
	offset += HF_SEP_STR_LEN;
	memcpy(rpl_hf.s + offset, ACPT_ENC_STR, ACPT_ENC_STR_LEN);
	offset += ACPT_ENC_STR_LEN;
	memcpy(rpl_hf.s + offset, opt_accept_enc.s, opt_accept_enc.len);
	offset += opt_accept_enc.len;
	memcpy(rpl_hf.s + offset, HF_SEP_STR, HF_SEP_STR_LEN);
	offset += HF_SEP_STR_LEN;
	memcpy(rpl_hf.s + offset, ACPT_LAN_STR, ACPT_LAN_STR_LEN);
	offset += ACPT_LAN_STR_LEN;
	memcpy(rpl_hf.s + offset, opt_accept_lang.s, opt_accept_lang.len);
	offset += opt_accept_lang.len;
	memcpy(rpl_hf.s + offset, HF_SEP_STR, HF_SEP_STR_LEN);
	offset += HF_SEP_STR_LEN;
	memcpy(rpl_hf.s + offset, SUPT_STR, SUPT_STR_LEN);
	offset += SUPT_STR_LEN;
	memcpy(rpl_hf.s + offset, opt_supported.s, opt_supported.len);
	offset += opt_supported.len;
	memcpy(rpl_hf.s + offset, HF_SEP_STR, HF_SEP_STR_LEN);

#ifdef EXTRA_DEBUG
	offset += HF_SEP_STR_LEN;
	if (offset != rpl_hf.len) {
		LM_CRIT("headerlength (%i) != offset (%i)\n", rpl_hf.len, offset);
		abort();
	}
#endif


	if (add_lump_rpl( _msg, rpl_hf.s, rpl_hf.len,
	LUMP_RPL_HDR|LUMP_RPL_NODUP)!=0) {
		if (opt_slb.freply(_msg, 200, &opt_200_rpl) == -1) {
			LM_ERR("failed to send 200 via send_reply\n");
			return -1;
		}
		else
			return 0;
	} else {
		pkg_free(rpl_hf.s);
		LM_ERR("add_lump_rpl failed\n");
	}

error:
	if (opt_slb.freply(_msg, 500, &opt_500_rpl) == -1) {
		LM_ERR("failed to send 500 via send_reply\n");
		return -1;
	}
	else
		return 0;
}

