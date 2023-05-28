/**
 * Copyright (C) 2018 Daniel-Constantin Mierla (asipto.com)
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../../core/sr_module.h"
#include "../../core/mod_fix.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/lvalue.h"
#include "../../core/kemi.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_uri.h"

#include "../../core/utils/srjson.h"


MODULE_VERSION


static int sj_serialize_data(sip_msg_t *msg, srjson_doc_t *jdoc, str *smode);

static int w_sj_serialize(sip_msg_t *msg, char *mode, char *vout);

static cmd_export_t cmds[] = {
		{"sj_serialize", (cmd_function)w_sj_serialize, 2, fixup_spve_pvar,
				fixup_free_spve_pvar, ANY_ROUTE},
		{0, 0, 0, 0, 0, 0}};

static param_export_t params[] = {{0, 0, 0}};


struct module_exports exports = {
		"sipjson",		 /* module name */
		DEFAULT_DLFLAGS, /* dlopen flags */
		cmds,			 /* exported functions */
		params,			 /* exported parameters */
		0,				 /* exported rpc functions */
		0,				 /* exported pseudo-variables */
		0,				 /* module init function */
		0,				 /* response function */
		0,				 /* per child init function */
		0				 /* destroy function */
};

/**
 *
 */
static int ki_sj_serialize_helper(sip_msg_t *msg, str *smode, pv_spec_t *pvs)
{
	srjson_doc_t jdoc;
	pv_value_t val;

	if(pvs->setf == NULL) {
		LM_ERR("read only output variable\n");
		return -1;
	}

	LM_DBG("sip to json serialization...\n");
	srjson_InitDoc(&jdoc, NULL);

	if(sj_serialize_data(msg, &jdoc, smode) < 0) {
		LM_ERR("json serialization failure\n");
		goto error;
	}

	jdoc.buf.s = srjson_PrintUnformatted(&jdoc, jdoc.root);

	memset(&val, 0, sizeof(pv_value_t));
	if(jdoc.buf.s != NULL) {
		jdoc.buf.len = strlen(jdoc.buf.s);
		val.flags = PV_VAL_STR;
		val.rs = jdoc.buf;

		if(pvs->setf(msg, &pvs->pvp, (int)EQ_T, &val) < 0) {
			LM_ERR("setting string to output PV failed\n");
			goto error;
		}
		jdoc.free_fn(jdoc.buf.s);
		jdoc.buf.s = NULL;
	} else {
		val.flags = PV_VAL_NULL;
		if(pvs->setf(msg, &pvs->pvp, (int)EQ_T, &val) < 0) {
			LM_ERR("setting null to output PV failed\n");
			goto error;
		}
	}
	srjson_DestroyDoc(&jdoc);
	LM_DBG("sip to json serialization done...\n");

	return 1;

error:
	if(jdoc.buf.s != NULL) {
		jdoc.free_fn(jdoc.buf.s);
		jdoc.buf.s = NULL;
	}
	srjson_DestroyDoc(&jdoc);

	return -1;
}

/**
 * 
 */
static int ki_sj_serialize(sip_msg_t *msg, str *smode, str *pvn)
{
	pv_spec_t *pvs = NULL;

	pvs = pv_cache_get(pvn);
	if(pvs == NULL) {
		LM_ERR("cannot get pv spec for [%.*s]\n", pvn->len, pvn->s);
		return -1;
	}

	return ki_sj_serialize_helper(msg, smode, pvs);
}

/**
 * 
 */
static int w_sj_serialize(sip_msg_t *msg, char *mode, char *vout)
{
	str smode;
	pv_spec_t *dpv;

	if(fixup_get_svalue(msg, (gparam_t *)mode, &smode) != 0 || smode.len <= 0) {
		LM_ERR("no serialization mode parameter\n");
		return -1;
	}

	dpv = (pv_spec_t *)vout;

	return ki_sj_serialize_helper(msg, &smode, dpv);
}

/**
 * 
 */
static int sj_add_xuri_attr(sip_uri_t *puri, int atype, char *aname, int alen,
		srjson_doc_t *jdoc, srjson_t *jr)
{
	str s;

	if(atype == 1) /* username */ {
		if(puri->user.s == NULL || puri->user.len <= 0) {
			s.s = "";
			s.len = 0;
		} else {
			s = puri->user;
		}
		srjson_AddStrStrToObject(jdoc, jr, aname, alen, s.s, s.len);
		return 0;
	} else if(atype == 2) /* domain */ {
		if(puri->host.s == NULL || puri->host.len <= 0) {
			s.s = "";
			s.len = 0;
		} else {
			s = puri->host;
		}
		srjson_AddStrStrToObject(jdoc, jr, aname, alen, s.s, s.len);
		return 0;
	} else if(atype == 3) /* port */ {
		srjson_AddNumberToObject(jdoc, jr, aname, puri->port_no);
		return 0;
	} else if(atype == 4) /* protocol */ {
		if(puri->transport_val.s == NULL) {
			s.s = "";
			s.len = 0;
		} else {
			s = puri->transport_val;
		}
		srjson_AddStrStrToObject(jdoc, jr, aname, alen, s.s, s.len);
		return 0;
	}

	LM_DBG("unknown attribute\n");
	s.s = "";
	s.len = 0;
	srjson_AddStrStrToObject(jdoc, jr, aname, alen, s.s, s.len);

	return 0;
}

/**
 * 
 */
static int sj_serialize_data(sip_msg_t *msg, srjson_doc_t *jdoc, str *smode)
{
	int i;
	str s;
	srjson_t *jr = NULL;
	sip_uri_t *puri = NULL;

	jr = srjson_CreateObject(jdoc);
	if(jr == NULL) {
		LM_ERR("cannot create json root obj\n");
		goto error;
	}
	jdoc->root = jr;

	if(parse_headers(msg, HDR_EOH_F, 0) < 0) {
		LM_ERR("failed to parse headers\n");
		goto error;
	}

	if(msg->first_line.type == SIP_REPLY) {
		srjson_AddNumberToObject(jdoc, jr, "mt", 1);
		if(msg->cseq == NULL
				&& ((parse_headers(msg, HDR_CSEQ_F, 0) == -1)
						|| (msg->cseq == NULL))) {
			LM_ERR("no CSEQ header\n");
			s.s = "";
			s.len = 0;
		} else {
			s = get_cseq(msg)->method;
		}
	} else {
		srjson_AddNumberToObject(jdoc, jr, "mt", 2);
		s = msg->first_line.u.request.method;
	}
	srjson_AddStrStrToObject(jdoc, jr, "rm", 2, s.s, s.len);

	if(get_valid_proto_string(msg->rcv.proto, 0, 0, &s) < 0) {
		s.s = "none";
		s.len = 4;
	}
	srjson_AddStrStrToObject(jdoc, jr, "pr", 2, s.s, s.len);

	s.s = ip_addr2a(&msg->rcv.src_ip);
	s.len = strlen(s.s);
	srjson_AddStrStrToObject(jdoc, jr, "si", 2, s.s, s.len);
	srjson_AddNumberToObject(jdoc, jr, "sp", msg->rcv.src_port);

	if(msg->rcv.bind_address != NULL) {
		srjson_AddStrStrToObject(jdoc, jr, "Ri", 2,
				msg->rcv.bind_address->address_str.s,
				msg->rcv.bind_address->address_str.len);
		srjson_AddNumberToObject(
				jdoc, jr, "Rp", msg->rcv.bind_address->port_no);
	}

	if(msg->first_line.type == SIP_REPLY) {
		/* REPLY does not have a ruri */
		s.s = "";
		s.len = 0;
	} else {
		if(msg->parsed_uri_ok == 0 && parse_sip_msg_uri(msg) < 0) {
			LM_ERR("failed to parse the R-URI\n");
			s.s = "";
			s.len = 0;
		} else {
			if(msg->new_uri.s != NULL) {
				s = msg->new_uri;
			} else {
				s = msg->first_line.u.request.uri;
			}
		}
	}
	srjson_AddStrStrToObject(jdoc, jr, "ru", 2, s.s, s.len);

	if(msg->first_line.type == SIP_REPLY) {
		srjson_AddStrStrToObject(jdoc, jr, "rU", 2, "", 0);
		srjson_AddStrStrToObject(jdoc, jr, "rd", 2, "", 0);
		srjson_AddNumberToObject(jdoc, jr, "rp", 0);
		srjson_AddNumberToObject(
				jdoc, jr, "rs", msg->first_line.u.reply.statuscode);
		srjson_AddStrStrToObject(jdoc, jr, "rr", 2,
				msg->first_line.u.reply.reason.s,
				msg->first_line.u.reply.reason.len);
	} else {
		if(msg->parsed_uri_ok == 0 && parse_sip_msg_uri(msg) < 0) {
			srjson_AddStrStrToObject(jdoc, jr, "rU", 2, "", 0);
			srjson_AddStrStrToObject(jdoc, jr, "rd", 2, "", 0);
			srjson_AddNumberToObject(jdoc, jr, "rp", 0);
		} else {
			puri = &(msg->parsed_uri);
			sj_add_xuri_attr(puri, 1, "rU", 2, jdoc, jr);
			sj_add_xuri_attr(puri, 2, "rd", 2, jdoc, jr);
			sj_add_xuri_attr(puri, 3, "rp", 2, jdoc, jr);
		}
		srjson_AddNumberToObject(jdoc, jr, "rs", 0);
		srjson_AddStrStrToObject(jdoc, jr, "rr", 2, "", 0);
	}

	puri = parse_from_uri(msg);
	if(puri == NULL) {
		srjson_AddStrStrToObject(jdoc, jr, "fU", 2, "", 0);
		srjson_AddStrStrToObject(jdoc, jr, "fd", 2, "", 0);
	} else {
		sj_add_xuri_attr(puri, 1, "fU", 2, jdoc, jr);
		sj_add_xuri_attr(puri, 2, "fd", 2, jdoc, jr);
	}

	if(msg->user_agent == NULL
			&& ((parse_headers(msg, HDR_USERAGENT_F, 0) == -1)
					|| (msg->user_agent == NULL))) {
		s.s = "";
		s.len = 0;
	} else {
		s = msg->user_agent->body;
	}
	srjson_AddStrStrToObject(jdoc, jr, "ua", 2, s.s, s.len);

	if(msg->callid == NULL
			&& ((parse_headers(msg, HDR_CALLID_F, 0) == -1)
					|| (msg->callid == NULL))) {
		LM_ERR("cannot parse Call-Id header\n");
		s.s = "";
		s.len = 0;
	} else {
		s = msg->callid->body;
	}
	srjson_AddStrStrToObject(jdoc, jr, "ci", 2, s.s, s.len);

	for(i = 0; i < smode->len; i++) {
		switch(smode->s[i]) {
			case '0':
				/* default attributes added already */
				break;
			case 'B':
				s.s = get_body(msg);
				if(s.s == NULL) {
					srjson_AddStrStrToObject(jdoc, jr, "rb", 2, "", 0);
				} else {
					s.len = msg->buf + msg->len - s.s;
					srjson_AddStrStrToObject(jdoc, jr, "rb", 2, s.s, s.len);
				}
				break;
			case 'c':
				if(msg->cseq == NULL
						&& ((parse_headers(msg, HDR_CSEQ_F, 0) == -1)
								|| (msg->cseq == NULL))) {
					s.s = "";
					s.len = 0;
				} else {
					s = get_cseq(msg)->number;
				}
				srjson_AddStrStrToObject(jdoc, jr, "cs", 2, s.s, s.len);
				break;
			case 't':
				puri = parse_to_uri(msg);
				if(puri == NULL) {
					srjson_AddStrStrToObject(jdoc, jr, "tU", 2, "", 0);
					srjson_AddStrStrToObject(jdoc, jr, "td", 2, "", 0);
				} else {
					sj_add_xuri_attr(puri, 1, "tU", 2, jdoc, jr);
					sj_add_xuri_attr(puri, 2, "td", 2, jdoc, jr);
				}
				break;
		}
	}
	return 1;

error:
	LM_ERR("failed to build the json serialization document\n");
	return -1;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_sipjson_exports[] = {
	{ str_init("sipjson"), str_init("sj_serialize"),
		SR_KEMIP_INT, ki_sj_serialize,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ {0, 0}, {0, 0}, 0, NULL, { 0, 0, 0, 0, 0, 0 } }
};
/* clang-format on */

/**
 *
 */
int mod_register(char *path, int *dlflags, void *p1, void *p2)
{
	sr_kemi_modules_add(sr_kemi_sipjson_exports);
	return 0;
}
