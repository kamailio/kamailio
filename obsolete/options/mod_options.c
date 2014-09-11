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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * -------
 * 2003-11-11: build_lump_rpl() removed, add_lump_rpl() has flags (bogdan)
 */

#include <stdlib.h>
#include "mod_options.h"
#include "../../sr_module.h"
#include "../../config.h"
#include "../../modules/sl/sl.h"
#include "../../mem/mem.h"
#include "../../data_lump_rpl.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_uri.h"

MODULE_VERSION

static str acpt_body = STR_STATIC_INIT("*/*");
static str acpt_enc_body = STR_STATIC_INIT("");
static str acpt_lan_body = STR_STATIC_INIT("en");
static str supt_body = STR_STATIC_INIT("");
static str contact = STR_STATIC_INIT("");
static str cont_param = STR_STATIC_INIT("");

#define ADD_CONT_OFF      0
#define ADD_CONT_MODPARAM 1
#define ADD_CONT_IP       2
#define ADD_CONT_RURI     3
static int add_cont = ADD_CONT_OFF;

/*
 * sl API structure for stateless reply
 */
sl_api_t slb;

static int mod_init(void);
static int opt_reply(struct sip_msg* _msg, char* _foo, char* _bar);

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"options_reply", (cmd_function)opt_reply, 0, 0,
		REQUEST_ROUTE | FAILURE_ROUTE},
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
	{"contact",         PARAM_STR, &contact},
	{"contact_param",   PARAM_STR, &cont_param},
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
	DBG("options - initializing\n");

	/* bind the SL API */
	if (sl_load_api(&slb)!=0) {
		LM_ERR("cannot bind to SL API\n");
		return -1;
	}

	if (contact.len > 0) {
		LOG(L_DBG, "contact: '%.*s'\n", contact.len, contact.s);
		add_cont = ADD_CONT_MODPARAM;
		if (strncasecmp("dstip", contact.s, contact.len) == 0) {
			contact.s = NULL;
			contact.len = 0;
			add_cont = ADD_CONT_IP;
		}
		else if (strncasecmp("ruri", contact.s, contact.len) == 0) {
			contact.s = NULL;
			contact.len = 0;
			add_cont = ADD_CONT_RURI;
		}
		/* more possible candidates here could be the To 
		 * or topmost Route URI */
	}

	if (cont_param.len > 0 && add_cont == ADD_CONT_OFF) {
		/* by default we add the IP */
		add_cont = ADD_CONT_IP;
	}

	return 0;
}


/*
 * calculates and returns the length of the Contact header to be inserted.
 */
static int contact_length(struct sip_msg* _msg)
{
	int ret = 0;

	if (add_cont == ADD_CONT_OFF) {
		return 0;
	}

	ret = CONT_STR_LEN + CRLF_LEN + 1;
	if (add_cont == ADD_CONT_MODPARAM) {
		ret += contact.len;
	}
	else if (add_cont == ADD_CONT_IP) {
		ret += _msg->rcv.bind_address->name.len;
		ret += 1;
		ret += _msg->rcv.bind_address->port_no_str.len;
		switch (_msg->rcv.bind_address->proto) {
			case PROTO_NONE:
			case PROTO_UDP:
				break;
			case PROTO_TCP:
			case PROTO_TLS:
				ret += TRANSPORT_PARAM_LEN + 3;
				break;
			case PROTO_SCTP:
				ret += TRANSPORT_PARAM_LEN + 4;
				break;
			default:
				LOG(L_CRIT, "contact_length(): unsupported proto (%d)\n",
						_msg->rcv.bind_address->proto);
		}
	}
	else if (add_cont == ADD_CONT_RURI) {
		if (parse_sip_msg_uri(_msg) != 1) {
			LOG(L_WARN, "add_contact(): failed to parse ruri\n");
		}
		if (_msg->parsed_orig_ruri_ok) {
			ret += _msg->parsed_orig_ruri.host.len;
			if (_msg->parsed_orig_ruri.port.len > 0) {
				ret += 1;
				ret += _msg->parsed_orig_ruri.port.len;
			}
		}
		else if (_msg->parsed_uri_ok){
			ret += _msg->parsed_uri.host.len;
			if (_msg->parsed_uri.port.len > 0) {
				ret += 1;
				ret += _msg->parsed_uri.port.len;
			}
		}
	}
	if (cont_param.len > 0) {
		if (*(cont_param.s) != ';') {
			ret += 1;
		}
		ret += cont_param.len;
	}

	return ret;
}

/*
 * inserts the Contact header at _dst and returns the number of written bytes
 */
static int add_contact(struct sip_msg* _msg, char* _dst)
{
	int ret = 0;

	memcpy(_dst, CONT_STR, CONT_STR_LEN);
	ret += CONT_STR_LEN;
	if (add_cont == ADD_CONT_MODPARAM) {
		memcpy(_dst + ret, contact.s, contact.len);
		ret += contact.len;
	}
	else if (add_cont == ADD_CONT_IP) {
		memcpy(_dst + ret, _msg->rcv.bind_address->name.s,
				_msg->rcv.bind_address->name.len);
		ret += _msg->rcv.bind_address->name.len;
		memcpy(_dst + ret, ":", 1);
		ret += 1;
		memcpy(_dst + ret, _msg->rcv.bind_address->port_no_str.s,
				_msg->rcv.bind_address->port_no_str.len);
		ret += _msg->rcv.bind_address->port_no_str.len;
		switch (_msg->rcv.bind_address->proto) {
			case PROTO_NONE:
			case PROTO_UDP:
				break;
			case PROTO_TCP:
				memcpy(_dst + ret, TRANSPORT_PARAM, TRANSPORT_PARAM_LEN);
				ret += TRANSPORT_PARAM_LEN;
				memcpy(_dst + ret, "tcp", 3);
				ret += 3;
				break;
			case PROTO_TLS:
				memcpy(_dst + ret, TRANSPORT_PARAM, TRANSPORT_PARAM_LEN);
				ret += TRANSPORT_PARAM_LEN;
				memcpy(_dst + ret, "tls", 3);
				ret += 3;
				break;
			case PROTO_SCTP:
				memcpy(_dst + ret, TRANSPORT_PARAM, TRANSPORT_PARAM_LEN);
				ret += TRANSPORT_PARAM_LEN;
				memcpy(_dst + ret, "sctp", 4);
				ret += 3;
				break;
			default:
				LOG(L_CRIT, "add_contact(): unknown transport protocol (%d)\n",
						_msg->rcv.bind_address->proto);
		}
	}
	else if (add_cont == ADD_CONT_RURI) {
		/* the parser was called by the contact_length function above */
		if (_msg->parsed_orig_ruri_ok) {
			memcpy(_dst + ret, _msg->parsed_orig_ruri.host.s,
					_msg->parsed_orig_ruri.host.len);
			ret += _msg->parsed_orig_ruri.host.len;
			if (_msg->parsed_orig_ruri.port.len > 0) {
				memcpy(_dst + ret, ":", 1);
				ret += 1;
				memcpy(_dst + ret, _msg->parsed_orig_ruri.port.s,
						_msg->parsed_orig_ruri.port.len);
				ret += _msg->parsed_orig_ruri.port.len;
			}
		}
		else if (_msg->parsed_uri_ok){
			memcpy(_dst + ret, _msg->parsed_uri.host.s,
					_msg->parsed_uri.host.len);
			ret += _msg->parsed_uri.host.len;
			if (_msg->parsed_uri.port.len > 0) {
				memcpy(_dst + ret, ":", 1);
				ret += 1;
				memcpy(_dst + ret, _msg->parsed_uri.port.s,
						_msg->parsed_uri.port.len);
				ret += _msg->parsed_uri.port.len;
			}
		}
	}
	if (cont_param.len > 0) {
		if (*(cont_param.s) != ';') {
			memcpy(_dst + ret, ";", 1);
			ret += 1;
		}
		memcpy(_dst + ret, cont_param.s, cont_param.len);
		ret += cont_param.len;
	}
	memcpy(_dst + ret, ">", 1);
	ret += 1;
	memcpy(_dst + ret, CRLF, CRLF_LEN);
	ret += CRLF_LEN;

	return ret;
}

/*
 * assembles and send the acctual reply
 */
static int opt_reply(struct sip_msg* _msg, char* _foo, char* _bar) 
{
	str rpl_hf;
	int ret;
	int offset = 0;
	int cont_len = 0;

	if (_msg->REQ_METHOD != METHOD_OPTIONS) {
		LOG(L_ERR, "options_reply(): called for non-OPTIONS request\n");
		return 0;
	}

	/* ruri == server address check has to be done in the script */
	if (_msg->parsed_uri_ok != 1) {
		if (parse_sip_msg_uri(_msg) != 1) {
			LOG(L_WARN, "opt_reply(): failed to parse ruri\n");
		}
	}
	if (_msg->parsed_uri.user.len != 0) {
		LOG(L_ERR, "options_reply(): wont reply because ruri contains"
				" a username\n");
		return 0;
	}

	/* calculate the length and allocated the mem */
	rpl_hf.len = ACPT_STR_LEN + ACPT_ENC_STR_LEN + ACPT_LAN_STR_LEN +
			SUPT_STR_LEN + 4 * CRLF_LEN + acpt_body.len + acpt_enc_body.len +
			acpt_lan_body.len + supt_body.len;
	if (add_cont) {
		cont_len = contact_length(_msg);
		rpl_hf.len += cont_len;
	}
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

	if (cont_len > 0) {
		ret = add_contact(_msg, rpl_hf.s + offset);
		if (ret != cont_len) {
			LOG(L_CRIT, "options_reply(): add_contact (%i) != contact_length" \
					"(%i)\n", ret, cont_len);
			goto error;
		}
		else {
			offset += cont_len;
		}
	}

#ifdef EXTRA_DEBUG
	if (offset != rpl_hf.len) {
		LOG(L_CRIT, "options_reply(): headerlength (%i) != offset (%i)\n",
			rpl_hf.len, offset);
		abort();
	}
	else {
		DBG("options_reply(): successfully build OPTIONS reply\n");
	}
#endif

	if (add_lump_rpl( _msg, rpl_hf.s, rpl_hf.len,
			LUMP_RPL_HDR|LUMP_RPL_NODUP)!=0) {
		/* the memory is freed when _msg is destroyed */
		if (slb.zreply(_msg, 200, "OK") < 0) {
			LOG(L_ERR, "options_reply(): failed to send 200 via sl reply\n");
			return 0;
		} else {
			return 1;
		}
	} else {
		LOG(L_ERR, "options_reply(): add_lump_rpl failed\n");
	}

error:
	if (rpl_hf.s) {
		pkg_free(rpl_hf.s);
	}
	if (slb.zreply(_msg, 500, "Server internal error") < 0) {
		LOG(L_ERR, "options_reply(): failed to send 500 via send_reply\n");
	}
	return 0;
}
