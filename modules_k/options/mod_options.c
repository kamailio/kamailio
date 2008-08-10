/*
 * $Id$
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * -------
 * 2003-11-11: build_lump_rpl() removed, add_lump_rpl() has flags (bogdan)
 */

/*! \file options/mod_options.c
 *  \brief Options reply modules
 *  \ingroup options
 *  Module: \ref options
 */

/*! \defgroup options OPTIONS :: Reply to OPTION pokes
 */

#ifdef EXTRA_DEBUG
#include <stdlib.h>   /* required by abort() */
#endif
#include "mod_options.h"
#include "../../sr_module.h"
#include "../../str.h"
#include "../../ut.h"
#include "../../mem/mem.h"
#include "../../data_lump_rpl.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_uri.h"
#include "../sl/sl_api.h"

MODULE_VERSION

char *acpt_c, *acpt_enc_c, *acpt_lan_c, *supt_c;
str acpt_s, acpt_enc_s, acpt_lan_s, supt_s;

/** SL binds */
struct sl_binds slb;

static str opt_200_rpl = str_init("OK");
static str opt_500_rpl = str_init("Server internal error");

static int mod_init(void);

static int opt_reply(struct sip_msg* _msg, char* _foo, char* _bar);
/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"options_reply", (cmd_function)opt_reply, 0, 0, 0, REQUEST_ROUTE},
	{0, 0, 0, 0, 0, 0}
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
 * Module description
 */
struct module_exports exports = {
	"options",       /* Module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* Exported functions */
	params,          /* Exported parameters */
	0,               /* exported statistics */
	0,               /* exported MI functions */
	0,               /* exported pseudo-variables */
	0,               /* extra processes */
	mod_init,        /* Initialization function */
	0,               /* Response function */
	0,               /* Destroy function */
	0                /* Child init function */
};

/*
 * initialize module
 */
static int mod_init(void) {

	/* load the SL API */
	if (load_sl_api(&slb)!=0) {
		LM_ERR("can't load SL API\n");
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
			SUPT_STR_LEN + 4*HF_SEP_STR_LEN + acpt_s.len + acpt_enc_s.len + 
			acpt_lan_s.len + supt_s.len;
	rpl_hf.s = (char*)pkg_malloc(rpl_hf.len);
	if (!rpl_hf.s) {
		LM_CRIT("out of pkg memory\n");
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
		LM_CRIT("headerlength (%i) != offset (%i)\n", rpl_hf.len, offset);
		abort();
	}
#endif


	if (add_lump_rpl( _msg, rpl_hf.s, rpl_hf.len,
	LUMP_RPL_HDR|LUMP_RPL_NODUP)!=0) {
		if (slb.reply(_msg, 200, &opt_200_rpl) == -1) {
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
	if (slb.reply(_msg, 500, &opt_500_rpl) == -1) {
		LM_ERR("failed to send 500 via send_reply\n");
		return -1;
	}
	else
		return 0;
}

