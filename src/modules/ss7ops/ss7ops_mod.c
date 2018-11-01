/*
 * ss7 module - helper module to convert SIGTRAN/ss7 to json
 *
 * Copyright (C) 2016 Holger Hans Peter Freyther (help@moiji-mobile.com)
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

#include "isup_parsed.h"
#include "../../core/sr_module.h"
#include "../../core/endianness.h"
#include "../../core/kemi.h"

MODULE_VERSION

/* hep defines */
#define HEP_M2UA		0x08
#define HEP_M2PA		0x0d

/* M2UA messages */
#define M2UA_MSG	6
#define M2UA_DATA	1
#define M2UA_IE_DATA	0x0300

/* M2PA messages */
#define M2PA_CLASS	11
#define M2PA_DATA	1

/* MTPl3 */
#define MTP_ISUP	0x05

struct mtp_level_3_hdr {
#ifdef __IS_LITTLE_ENDIAN
	uint8_t ser_ind : 4,
		spare : 2,
		ni : 2;
	uint32_t dpc : 14,
		opc : 14,
		sls : 4;
#else /* ignore middle endian */
	uint8_t ni : 2,
		spare : 2,
		ser_ind : 4;
	uint32_t sls : 4,
		opc : 14,
		dpc : 14;
#endif
	uint8_t data[0];
} __attribute__((packed));

/*! \file
 * ss7 module - helper module to convert M2UA/ISUP to JSON
 *
 */
static const char *isup_last = NULL;
static srjson_doc_t *isup_json = NULL;

static int w_isup_to_json(struct sip_msg* _m, char* param1, char* param2);
static int pv_get_isup(struct sip_msg *msg, pv_param_t *param, pv_value_t *res);
static int pv_parse_isup_name(pv_spec_p sp, str *in);

static cmd_export_t cmds[] = {
	{"isup_to_json", (cmd_function)w_isup_to_json, 1, 0, 0, ANY_ROUTE},
	{0, 0, 0, 0, 0, 0}
};

static pv_export_t mod_pvs[] = {
	{ {"isup", sizeof("isup")-1}, PVT_OTHER, pv_get_isup,
			0, pv_parse_isup_name, 0, 0, 0 },
	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static int mod_init(void)
{
	LM_DBG("ss7 module\n");
	return 0;
}

static void destroy(void)
{
	LM_DBG("Destroying ss7 module\n");
}

struct module_exports exports = {
	"ss7",      /*!< module name */
	DEFAULT_DLFLAGS, /*!< dlopen flags */
	cmds,       /*!< exported functions */
	0,          /*!< exported parameters */
	0,          /*!< exported rpc functions */
	mod_pvs,    /*!< exported pseudo-variables */
	0,          /*!< response handling function */
	mod_init,   /*!< module init function */
	0,          /*!< child init function */
	destroy     /*!< module destroy function */
};

static const uint8_t *extract_from_m2ua(const uint8_t *data, size_t *len)
{
	uint32_t data_len;

	if (*len <= 8) {
		LM_ERR("M2UA hdr too short %zu\n", *len);
		return NULL;
	}

	/* check the header */
	if (data[0] != 0x01) {
		LM_ERR("M2UA unknown version number %d\n", data[0]);
		return NULL;
	}
	if (data[1] != 0x00) {
		LM_ERR("M2UA unknown reserved fields %d\n", data[1]);
		return NULL;
	}
	if (data[2] != M2UA_MSG) {
		LM_ERR("M2UA unhandled message class %d\n", data[2]);
		return NULL;
	}
	if (data[3] != M2UA_DATA) {
		LM_ERR("M2UA not data msg but %d\n", data[3]);
		return NULL;
	}

	/* check the length */
	memcpy(&data_len, &data[4], sizeof(data_len));
	data_len = ntohl(data_len);
	if (*len < data_len) {
		LM_ERR("M2UA data can't fit %zu vs. %zu\n", *len, (size_t)data_len);
		return NULL;
	}

	/* skip the header */
	data += 8;
	data_len -= 8;
	while (data_len > 4) {
		uint16_t ie_tag, ie_len, padding;
		memcpy(&ie_tag, &data[0], sizeof(ie_tag));
		memcpy(&ie_len, &data[2], sizeof(ie_len));
		ie_tag = ntohs(ie_tag);
		ie_len = ntohs(ie_len);

		if (ie_len > data_len) {
			LM_ERR("M2UA premature end %u vs. %zu\n", ie_len, (size_t)data_len);
			return NULL;
		}

		if (ie_tag != M2UA_IE_DATA)
			goto next;

		*len = ie_len - 4;
		return &data[4];

next:
		data += ie_len;
		data_len -= ie_len;

		/* and now padding... */
                padding = (4 - (ie_len % 4)) & 0x3;
		if (data_len < padding) {
			LM_ERR("M2UA no place for padding %u vs. %zu\n", padding,
					(size_t)data_len);
			return NULL;
		}
		data += padding;
		data_len -= padding;
	}
	/* No data IE was found */
	LM_ERR("M2UA no data element found\n");
	return NULL;
}

static const uint8_t *extract_from_m2pa(const uint8_t *data, size_t *len)
{
	uint32_t data_len;

	if (*len < 8) {
		LM_ERR("M2PA hdr too short %zu\n", *len);
		return NULL;
	}

	/* check the header */
	if (data[0] != 0x01) {
		LM_ERR("M2PA unknown version number %d\n", data[0]);
		return NULL;
	}
	if (data[1] != 0x00) {
		LM_ERR("M2PA unknown reserved fields %d\n", data[1]);
		return NULL;
	}
	if (data[2] != M2PA_CLASS) {
		LM_ERR("M2PA unhandled message class %d\n", data[2]);
		return NULL;
	}
	if (data[3] != M2PA_DATA) {
		LM_ERR("M2PA not data msg but %d\n", data[3]);
		return NULL;
	}

	/* check the length */
	memcpy(&data_len, &data[4], sizeof(data_len));
	data_len = ntohl(data_len);
	if (*len < data_len) {
		LM_ERR("M2PA data can't fit %zu vs. %u\n", *len, data_len);
		return NULL;
	}

	/* skip the header */
	data += 8;
	data_len -= 8;

	/* BSN, FSN and then priority */
	if (data_len < 8) {
		LM_ERR("M2PA no space for BSN/FSN %u\n", data_len);
		return NULL;
	}
	data += 8;
	data_len -= 8;
	if (data_len == 0)
		return NULL;
	else if (data_len < 1) {
		LM_ERR("M2PA no space for prio %u\n", data_len);
		return NULL;
	}
	data += 1;
	data_len -= 1;

	*len = data_len;
	return data;
}

static const uint8_t *extract_from_mtp(const uint8_t *data, size_t *len,
		int *opc, int *dpc, int *type)
{
	struct mtp_level_3_hdr *hdr;

	*opc = INT_MAX;
	*dpc = INT_MAX;

	if (!data)
		return NULL;
	if (*len < sizeof(*hdr)) {
		LM_ERR("MTP not enough space for mtp hdr %zu vs. %zu", *len,
				sizeof(*hdr));
		return NULL;
	}

	hdr = (struct mtp_level_3_hdr *) data;
	*opc = hdr->opc;
	*dpc = hdr->dpc;
	*type = hdr->ser_ind;
	*len -= sizeof(*hdr);
	return &hdr->data[0];
}


static const uint8_t *ss7_extract_payload(const uint8_t *data, size_t *len,
		int proto, int *opc, int *dpc, int *mtp_type)
{
	switch (proto) {
	case HEP_M2UA:
		return extract_from_mtp(extract_from_m2ua(data, len), len, opc,
				dpc, mtp_type);
		break;
	case HEP_M2PA:
		return extract_from_mtp(extract_from_m2pa(data, len), len, opc,
				dpc, mtp_type);
	default:
		LM_ERR("Unknown HEP type %d/0x%c\n", proto, proto);
		return NULL;
	}
}

static uint8_t *fetch_payload(struct sip_msg *_m, char *pname, int *len)
{
	pv_spec_t *pv;
	pv_value_t pt;
	int rc;
	str s;

	s.s = pname;
	s.len = strlen(pname);
	pv = pv_cache_get(&s);
	if (!pv) {
		LM_ERR("Can't get %s\n", s.s);
		return NULL;
	}

	rc = pv->getf(_m, &pv->pvp, &pt);
	if (rc < 0) {
		LM_ERR("Can't getf rc=%d\n", rc);
		return NULL;
	}

	*len = pt.rs.len;
	return (uint8_t *) pt.rs.s;
}

static int ki_isup_to_json(sip_msg_t *_m, int proto)
{
	struct isup_state isup_state = { 0, };
	const uint8_t *data;
	int opc, dpc, mtp_type, int_len, rc;
	size_t len;

	free((char *) isup_last);
	srjson_DeleteDoc(isup_json);
	isup_last = NULL;
	isup_json = NULL;
	mtp_type = 0;

	data = fetch_payload(_m, "$var(payload)", &int_len);
	if (!data)
		return -1;

	if (int_len < 0) {
		LM_ERR("Payload length low %d\n", int_len);
		return -1;
	}
	len = int_len;

	data = ss7_extract_payload(data, &len, proto, &opc, &dpc, &mtp_type);
	if (!data)
		return -1;

	if (mtp_type != MTP_ISUP) {
		LM_DBG("Non ISUP payload %d\n", mtp_type);
		return -1;
	}

	/* parse isup... */
	isup_state.json = srjson_NewDoc(NULL);
	if (!isup_state.json) {
		LM_ERR("Failed to allocate JSON document\n");
		return -1;
	}
	isup_state.json->root = srjson_CreateObject(isup_state.json);
	if (!isup_state.json->root) {
		LM_ERR("Failed to allocate JSON object\n");
		srjson_DeleteDoc(isup_state.json);
		return -1;
	}

	rc = isup_parse(data, len, &isup_state);
	if (rc != 0) {
		srjson_DeleteDoc(isup_state.json);
		return rc;
	}
	srjson_AddNumberToObject(isup_state.json, isup_state.json->root, "opc", opc);
	srjson_AddNumberToObject(isup_state.json, isup_state.json->root, "dpc", dpc);
	isup_last = srjson_PrintUnformatted(isup_state.json, isup_state.json->root);
	isup_json = isup_state.json;
	return 1;
}

static int w_isup_to_json(struct sip_msg *_m, char *param1, char *param2)
{
		return ki_isup_to_json(_m, atoi(param1));
}

#define UINT_OR_NULL(msg, param, res, node) \
		node ? pv_get_uintval(msg, param, res, node->valuedouble) : pv_get_null(msg, param, res)

#define STR_OR_NULL(msg, param, res, tmpstr, node, out_res)	\
	if (node && node->type == srjson_String) {		\
		tmpstr.s = node->valuestring;			\
		tmpstr.len = strlen(tmpstr.s);			\
		out_res = pv_get_strval(msg, param, res, &tmpstr);	\
	} else {						\
		out_res = pv_get_null(msg, param, res);		\
	}

static int pv_get_isup(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	srjson_t *node;
	str tmpstr = { 0, };
	int out_res;

	if (!param)
		return -1;
	if (!isup_last)
		return -1;
	if (!isup_json)
		return -1;

	switch (param->pvn.u.isname.name.n) {
	case ISUP_JSON_STRING:
		tmpstr.s = (char *) isup_last;
		tmpstr.len = strlen(isup_last);
		return pv_get_strval(msg, param, res, &tmpstr);
	case ISUP_FIELD_METHOD:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "msg_name");
		STR_OR_NULL(msg, param, res, tmpstr, node, out_res);
		return out_res;
	case ISUP_FIELD_OPC:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "opc");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_DPC:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "dpc");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_CIC:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "cic");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_CALLED_INN:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "called_number");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "inn");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_CALLED_TON:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "called_number");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "ton");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_CALLED_NPI:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "called_number");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "npi");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_CALLED_NUM:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "called_number");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "num");
		STR_OR_NULL(msg, param, res, tmpstr, node, out_res);
		return out_res;
	case ISUP_FIELD_CALLING_NI:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "calling_number");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "ni");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_CALLING_RESTRICT:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "calling_number");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "restrict");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_CALLING_SCREENED:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "calling_number");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "screened");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_CALLING_TON:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "calling_number");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "ton");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_CALLING_NPI:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "calling_number");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "npi");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_CALLING_NUM:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "calling_number");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "num");
		STR_OR_NULL(msg, param, res, tmpstr, node, out_res);
		return out_res;
	case ISUP_FIELD_CALLING_CAT:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "calling_party");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "num");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_CAUSE_STD:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "cause");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "standard_num");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_CAUSE_LOC:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "cause");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "location_num");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_CAUSE_ITU_CLASS:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "cause");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "itu_class_num");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_CAUSE_ITU_NUM:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "cause");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "itu_cause_num");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_EVENT_NUM:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "event");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "event_num");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_HOP_COUNTER:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "hop_counter");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_NOC_SAT:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "nature_of_connnection");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "satellite");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_NOC_CHECK:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "nature_of_connnection");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "continuity_check");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_NOC_ECHO:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "nature_of_connnection");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "echo_device");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_FWD_INTE:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "forward_call");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "national_international_call");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_FWD_INTW:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "forward_call");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "interworking");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_FWD_EE_METH:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "forward_call");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "end_to_end_method");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_FWD_EE_INF:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "forward_call");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "end_to_end_information");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_FWD_ISUP_NUM:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "forward_call");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "isup");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_FWD_ISUP_PREF:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "forward_call");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "isup_preference");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_FWD_ISDN:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "forward_call");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "isdn_access");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_FWD_SCCP_METHOD:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "forward_call");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "sccp_method");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_MEDIUM:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "transmission_medium");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "num");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FILED_UI_CODING:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "user_information");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "coding_standard");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_UI_TRANS_CAP:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "user_information");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "transfer_capability");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_UI_TRANS_MODE:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "user_information");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "transfer_mode");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_UI_TRANS_RATE:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "user_information");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "transfer_rate");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_UI_LAYER1_IDENT:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "user_information");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "layer1_ident");
		return UINT_OR_NULL(msg, param, res, node);
	case ISUP_FIELD_UI_LAYER1_PROTOCOL:
		node = srjson_GetObjectItem(isup_json, isup_json->root, "user_information");
		if (!node)
			return pv_get_null(msg, param, res);
		node = srjson_GetObjectItem(isup_json, node, "layer1_protocol");
		return UINT_OR_NULL(msg, param, res, node);
	}

	return -1;
}

static const struct {
	const char *name;
	int index;
} pv_isup_names[] = {
	{ "method",				ISUP_FIELD_METHOD },
	{ "opc",				ISUP_FIELD_OPC },
	{ "dpc",				ISUP_FIELD_DPC },
	{ "cic",				ISUP_FIELD_CIC },
	{ "called_inn",				ISUP_FIELD_CALLED_INN },
	{ "called_ton",				ISUP_FIELD_CALLED_TON },
	{ "called_npi",				ISUP_FIELD_CALLED_NPI },
	{ "called_num",				ISUP_FIELD_CALLED_NUM },
	{ "calling_ni",				ISUP_FIELD_CALLING_NI },
	{ "calling_restrict",			ISUP_FIELD_CALLING_RESTRICT },
	{ "calling_screened",			ISUP_FIELD_CALLING_SCREENED },
	{ "calling_ton",			ISUP_FIELD_CALLING_TON },
	{ "calling_npi",			ISUP_FIELD_CALLING_NPI },
	{ "calling_num",			ISUP_FIELD_CALLING_NUM },
	{ "calling_category",			ISUP_FIELD_CALLING_CAT },
	{ "cause_standard",			ISUP_FIELD_CAUSE_STD },
	{ "cause_location",			ISUP_FIELD_CAUSE_LOC },
	{ "cause_itu_class",			ISUP_FIELD_CAUSE_ITU_CLASS },
	{ "cause_itu_num",			ISUP_FIELD_CAUSE_ITU_NUM },
	{ "event_num",				ISUP_FIELD_EVENT_NUM },
	{ "hop_counter",			ISUP_FIELD_HOP_COUNTER },
	{ "nature_of_conn_sat",			ISUP_FIELD_NOC_SAT },
	{ "nature_of_conn_con_check",		ISUP_FIELD_NOC_CHECK },
	{ "nature_of_conn_echo_device",		ISUP_FIELD_NOC_ECHO },
	{ "fwd_call_international",		ISUP_FIELD_FWD_INTE },
	{ "fwd_call_interworking",		ISUP_FIELD_FWD_INTW },
	{ "fwd_call_end_to_end_method",		ISUP_FIELD_FWD_EE_METH },
	{ "fwd_call_end_to_end_information",	ISUP_FIELD_FWD_EE_INF },
	{ "fwd_call_isup",			ISUP_FIELD_FWD_ISUP_NUM },
	{ "fwd_call_isup_preference",		ISUP_FIELD_FWD_ISUP_PREF },
	{ "fwd_call_sccp_method",		ISUP_FIELD_FWD_SCCP_METHOD },
	{ "fwd_call_isdn",			ISUP_FIELD_FWD_ISDN },
	{ "transmission_medium",		ISUP_FIELD_MEDIUM },
	{ "user_info_coding_standard",		ISUP_FILED_UI_CODING },
	{ "user_info_transfer_cap",		ISUP_FIELD_UI_TRANS_CAP },
	{ "user_info_transfer_mode",		ISUP_FIELD_UI_TRANS_MODE },
	{ "user_info_transfer_rate",		ISUP_FIELD_UI_TRANS_RATE },
	{ "user_info_layer1_ident",		ISUP_FIELD_UI_LAYER1_IDENT },
	{ "user_info_layer1_protocol",		ISUP_FIELD_UI_LAYER1_PROTOCOL },
};

static int pv_parse_isup_name(pv_spec_p sp, str *in)
{
	size_t i;
	unsigned int input, name_n;

	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	/* check strings */
	for (i = 0; i < (sizeof(pv_isup_names)/sizeof(pv_isup_names[0])); ++i) {
		if (strlen(pv_isup_names[i].name) != in->len)
			continue;
		if (strncmp(in->s, pv_isup_names[i].name, in->len) != 0)
			continue;

		sp->pvp.pvn.type = PV_NAME_INTSTR;
		sp->pvp.pvn.u.isname.type = 0;
		sp->pvp.pvn.u.isname.name.n = pv_isup_names[i].index;
		return 0;
	}

	if (str2int(in, &input) < 0)
		goto error;

	switch (input) {
	case 1:
		/* all valid input */
		name_n = ISUP_JSON_STRING;
		break;
	default:
		goto error;
	}

	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;
	sp->pvp.pvn.u.isname.name.n = name_n;

	return 0;
error:
	LM_ERR("unknown isup input %.*s\n", in->len, in->s);
	return -1;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_ss7ops_exports[] = {
	{ str_init("ss7ops"), str_init("isup_to_json"),
		SR_KEMIP_INT, ki_isup_to_json,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
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
	sr_kemi_modules_add(sr_kemi_ss7ops_exports);
	return 0;
}
