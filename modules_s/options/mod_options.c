/*
 * $Id$
 *
 * Options Reply Module
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
 * -------
 * 2003-11-11: build_lump_rpl() removed, add_lump_rpl() has flags (bogdan)
 */

#include "mod_options.h"
#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../data_lump_rpl.h"
#include "../../parser/msg_parser.h"

MODULE_VERSION

char *acpt_c, *acpt_enc_c, *acpt_lan_c, *supt_c;
str acpt_s, acpt_enc_s, acpt_lan_s, supt_s;

/*
 * sl_send_reply function pointer
 */
int (*sl_reply)(struct sip_msg* _m, char* _s1, char* _s2);

static int mod_init(void);
static int opt_reply(struct sip_msg* _msg, char* _foo, char* _bar);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"options_reply", (cmd_function)opt_reply, 0, 0, REQUEST_ROUTE},
	{0, 0, 0, 0}
};

/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"accept",     STR_PARAM, &acpt_c},
	{"accept_encoding", STR_PARAM, &acpt_enc_c},
	{"accept_language", STR_PARAM, &acpt_lan_c},
	{"support",     STR_PARAM, &supt_c},
	{0, 0, 0}
};

/*
 * Module describtion
 */
struct module_exports exports = {
	"options",       /* Module name */
	cmds,            /* Exported functions */
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
static int mod_init(void) {

	DBG("options initializing\n");

	sl_reply = find_export("sl_send_reply", 2, 0);
	if (!sl_reply) {
		LOG(L_ERR, "options: this module requires sl module\n");
		return -1;
	}

	if (acpt_c) {
		acpt_s.len = strlen(acpt_c);
		acpt_s.s = acpt_c;
	}
	else {
		acpt_s.len = ACPT_DEF_LEN;
		acpt_s.s = ACPT_DEF;
	}
	if (acpt_enc_c) {
		acpt_enc_s.len = strlen(acpt_enc_c);
		acpt_enc_s.s = acpt_enc_c;
	}
	else {
		acpt_enc_s.len = ACPT_ENC_DEF_LEN;
		acpt_enc_s.s = ACPT_ENC_DEF;
	}
	if (acpt_lan_c) {
		acpt_lan_s.len = strlen(acpt_lan_c);
		acpt_lan_s.s = acpt_lan_c;
	}
	else {
		acpt_lan_s.len = ACPT_LAN_DEF_LEN;
		acpt_lan_s.s = ACPT_LAN_DEF;
	}
	if (supt_c) {
		supt_s.len = strlen(supt_c);
		supt_s.s = supt_c;
	}
	else {
		supt_s.len = SUPT_DEF_LEN;
		supt_s.s = SUPT_DEF;
	}

	return 0;
}

static int opt_reply(struct sip_msg* _msg, char* _foo, char* _bar) {
	str rpl_hf;
	int offset = 0;

	/* check if it is called for an OPTIONS request */
	if ((_msg->REQ_METHOD==METHOD_OTHER) &&
		(_msg->first_line.u.request.method.len != 7) &&
		/* FIXME: avoid slow strncasecmp */
		(strncasecmp(_msg->first_line.u.request.method.s, "OPTIONS", 7) != 0)) {
		LOG(L_ERR, "options_reply(): called for non-OPTIONS request\n");
		return -1;
	}
	/* FIXME: should we additionaly check if ruri == server addresses ?! */
	if (_msg->parsed_uri.user.len != 0) {
		LOG(L_ERR, "options_reply(): ruri contains username\n");
		return -1;
	}

	/* calculate the length and allocated the mem */
	rpl_hf.len = ACPT_STR_LEN + ACPT_ENC_STR_LEN + ACPT_LAN_STR_LEN + 
			SUPT_STR_LEN + 4*HF_SEP_STR_LEN + acpt_s.len + acpt_enc_s.len + 
			acpt_lan_s.len + supt_s.len;
	rpl_hf.s = (char*)pkg_malloc(rpl_hf.len);
	if (!rpl_hf.s) {
		LOG(L_CRIT, "options_reply(): out of memory\n");
		goto error;
	}

	/* create the header fields */
	memcpy(rpl_hf.s, ACPT_STR, ACPT_STR_LEN);
	offset = ACPT_STR_LEN;
	memcpy(rpl_hf.s + offset, acpt_s.s, acpt_s.len);
	offset += acpt_s.len;
	memcpy(rpl_hf.s + offset, HF_SEP_STR, HF_SEP_STR_LEN);
	offset += HF_SEP_STR_LEN;
	memcpy(rpl_hf.s + offset, ACPT_ENC_STR, ACPT_ENC_STR_LEN);
	offset += ACPT_ENC_STR_LEN;
	memcpy(rpl_hf.s + offset, acpt_enc_s.s, acpt_enc_s.len);
	offset += acpt_enc_s.len;
	memcpy(rpl_hf.s + offset, HF_SEP_STR, HF_SEP_STR_LEN);
	offset += HF_SEP_STR_LEN;
	memcpy(rpl_hf.s + offset, ACPT_LAN_STR, ACPT_LAN_STR_LEN);
	offset += ACPT_LAN_STR_LEN;
	memcpy(rpl_hf.s + offset, acpt_lan_s.s, acpt_lan_s.len);
	offset += acpt_lan_s.len;
	memcpy(rpl_hf.s + offset, HF_SEP_STR, HF_SEP_STR_LEN);
	offset += HF_SEP_STR_LEN;
	memcpy(rpl_hf.s + offset, SUPT_STR, SUPT_STR_LEN);
	offset += SUPT_STR_LEN;
	memcpy(rpl_hf.s + offset, supt_s.s, supt_s.len);
	offset += supt_s.len;
	memcpy(rpl_hf.s + offset, HF_SEP_STR, HF_SEP_STR_LEN);

#ifdef EXTRA_DEBUG
	offset += HF_SEP_STR_LEN;
	if (offset != rpl_hf.len) {
		LOG(L_CRIT, "options_reply(): headerlength (%i) != offset (%i)\n", 
			rpl_hf.len, offset);
		abort();
	}
#endif


	if (add_lump_rpl( _msg, rpl_hf.s, rpl_hf.len,
	LUMP_RPL_HDR|LUMP_RPL_NODUP)!=0) {
		if (sl_reply(_msg, (char*)200, "OK") == -1) {
			LOG(L_ERR, "options_reply(): failed to send 200 via send_reply\n");
			return -1;
		}
		else
			return 0;
	} else {
		pkg_free(rpl_hf.s);
		LOG(L_ERR, "options_reply(): add_lump_rpl failed\n");
	}

error:
	if (sl_reply(_msg, (char*)500, "Server internal error") == -1) {
		LOG(L_ERR, "options_reply(): failed to send 500 via send_reply\n");
		return -1;
	}
	else
		return 0;
}

