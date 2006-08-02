/*
 * $Id$
 *
 * Options Reply Module
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * -------
 * 2003-11-11: build_lump_rpl() removed, add_lump_rpl() has flags (bogdan)
 */

#include <stdlib.h>
#include "mod_options.h"
#include "../../sr_module.h"
#include "../../config.h"
#include "../sl/sl.h"
#include "../../mem/mem.h"
#include "../../data_lump_rpl.h"
#include "../../parser/msg_parser.h"

MODULE_VERSION

static str acpt_body = STR_STATIC_INIT("*/*");
static str acpt_enc_body = STR_STATIC_INIT("");
static str acpt_lan_body = STR_STATIC_INIT("en");
static str supt_body = STR_STATIC_INIT("");

/*
 * sl_send_reply function pointer
 */
sl_api_t sl;

static int mod_init(void);
static int opt_reply(struct sip_msg* _msg, char* _foo, char* _bar);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"options_reply", (cmd_function)opt_reply, 0, 0, REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"accept",          PARAM_STR, &acpt_body},
	{"accept_encoding", PARAM_STR, &acpt_enc_body},
	{"accept_language", PARAM_STR, &acpt_lan_body},
	{"supported",       PARAM_STR, &supt_body},
	{0, 0, 0}
};

/*
 * Module description
 */
struct module_exports exports = {
	"options",       /* Module name */
	cmds,            /* Exported functions */
	0,               /* RPC methods */
	params,          /* Exported parameters */
	mod_init,        /* Initialization function */
	0,               /* Response function */
	0,               /* Destroy function */
	0,               /* OnCancel function */
	0                /* Child init function */
};

/*
 * initialize module
 */
static int mod_init(void) 
{
	bind_sl_t bind_sl;

	DBG("options initializing\n");

        bind_sl = (bind_sl_t)find_export("bind_sl", 0, 0);
	if (!bind_sl) {
		ERR("This module requires sl module\n");
		return -1;
	}
	if (bind_sl(&sl) < 0) return -1;

	return 0;
}


static int opt_reply(struct sip_msg* _msg, char* _foo, char* _bar) 
{
	str rpl_hf;
	int offset = 0;

	if ((_msg->REQ_METHOD != METHOD_OTHER) ||
	    (_msg->first_line.u.request.method.len != OPTIONS_LEN) ||
	    (strncasecmp(_msg->first_line.u.request.method.s, OPTIONS, OPTIONS_LEN) != 0)) {
		LOG(L_ERR, "options_reply(): called for non-OPTIONS request\n");
		return -1;
	}

	/* FIXME: should we additionally check if ruri == server addresses ?! */
	/* janakj: no, do it in the script */
	if (_msg->parsed_uri.user.len != 0) {
		LOG(L_ERR, "options_reply(): ruri contains username\n");
		return -1;
	}

	/* calculate the length and allocated the mem */
	rpl_hf.len = ACPT_STR_LEN + ACPT_ENC_STR_LEN + ACPT_LAN_STR_LEN +
			SUPT_STR_LEN + 4 * CRLF_LEN + acpt_body.len + acpt_enc_body.len +
			acpt_lan_body.len + supt_body.len;
	rpl_hf.s = (char*)pkg_malloc(rpl_hf.len);
	if (!rpl_hf.s) {
		LOG(L_CRIT, "options_reply(): out of memory\n");
		goto error;
	}

	/* create the header fields */
	memcpy(rpl_hf.s, ACPT_STR, ACPT_STR_LEN);
	offset = ACPT_STR_LEN;
	memcpy(rpl_hf.s + offset, acpt_body.s, acpt_body.len);
	offset += acpt_body.len;
	memcpy(rpl_hf.s + offset, CRLF, CRLF_LEN);
	offset += CRLF_LEN;

	memcpy(rpl_hf.s + offset, ACPT_ENC_STR, ACPT_ENC_STR_LEN);
	offset += ACPT_ENC_STR_LEN;
	memcpy(rpl_hf.s + offset, acpt_enc_body.s, acpt_enc_body.len);
	offset += acpt_enc_body.len;
	memcpy(rpl_hf.s + offset, CRLF, CRLF_LEN);
	offset += CRLF_LEN;

	memcpy(rpl_hf.s + offset, ACPT_LAN_STR, ACPT_LAN_STR_LEN);
	offset += ACPT_LAN_STR_LEN;
	memcpy(rpl_hf.s + offset, acpt_lan_body.s, acpt_lan_body.len);
	offset += acpt_lan_body.len;
	memcpy(rpl_hf.s + offset, CRLF, CRLF_LEN);
	offset += CRLF_LEN;

	memcpy(rpl_hf.s + offset, SUPT_STR, SUPT_STR_LEN);
	offset += SUPT_STR_LEN;
	memcpy(rpl_hf.s + offset, supt_body.s, supt_body.len);
	offset += supt_body.len;
	memcpy(rpl_hf.s + offset, CRLF, CRLF_LEN);
	offset += CRLF_LEN;

#ifdef EXTRA_DEBUG
	if (offset != rpl_hf.len) {
		LOG(L_CRIT, "options_reply(): headerlength (%i) != offset (%i)\n",
			rpl_hf.len, offset);
		abort();
	}
#endif

	if (add_lump_rpl( _msg, rpl_hf.s, rpl_hf.len,
	LUMP_RPL_HDR|LUMP_RPL_NODUP)!=0) {
		if (sl.reply(_msg, 200, "OK") == -1) {
			LOG(L_ERR, "options_reply(): failed to send 200 via send_reply\n");
			return -1;
		} else {
			return 0;
		}
	} else {
		pkg_free(rpl_hf.s);
		LOG(L_ERR, "options_reply(): add_lump_rpl failed\n");
	}

error:
	if (sl.reply(_msg, 500, "Server internal error") == -1) {
		LOG(L_ERR, "options_reply(): failed to send 500 via send_reply\n");
		return -1;
	} else {
		return 0;
	}
}
