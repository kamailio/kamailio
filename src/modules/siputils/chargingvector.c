/*
 * Copyright (C) 2016 kamailio.org
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
 */

#include <string.h>
#include <netdb.h>

#include "../../core/mod_fix.h"
#include "../../core/data_lump.h"
#include "../../core/data_lump_rpl.h"
#include "../../core/mem/mem.h"
#include "../../core/str.h"
#include "../../core/strutils.h"
#include "chargingvector.h"

#define SIZE_CONF_ID 16
#define P_CHARGING_VECTOR "P-Charging-Vector"
#define P_CHARGING_VECTOR_HEADERNAME_LEN (sizeof(P_CHARGING_VECTOR) - 1)
#define P_CHARGING_VECTOR_PREFIX_LEN (P_CHARGING_VECTOR_HEADERNAME_LEN + 2)
#define LOOPBACK_IP 16777343

#define PCV_BUF_SIZE 256
static char _siputils_pcv_buf[PCV_BUF_SIZE];
static str _siputils_pcv = {_siputils_pcv_buf, 0};
static str _siputils_pcv_id = STR_NULL;
static str _siputils_pcv_genaddr = STR_NULL;
static str _siputils_pcv_orig = STR_NULL;
static str _siputils_pcv_term = STR_NULL;
static uint64_t _siputils_pcv_counter = 0;
static struct hdr_field *_siputils_pcv_hf_pcv = NULL;


enum PCV_Status
{
	PCV_NONE = 0,
	PCV_PARSED = 1,
	PCV_ICID_MISSING = 2,
	PCV_GENERATED = 3,
	PCV_DELETED = 4,
	PCV_REPLACED = 5,
	PCV_NOP = PCV_PARSED
};
static const char *sstatus[] = {
		"NONE", "PARSED", "ICID_MISSING", "GENERATED", "DELETED", "REPLACED"};

enum PCV_Parameter
{
	PCV_PARAM_ALL = 1,
	PCV_PARAM_GENADDR = 2,
	PCV_PARAM_ID = 3,
	PCV_PARAM_ORIG = 4,
	PCV_PARAM_TERM = 5,
	PCV_PARAM_STATUS = 6
};

static enum PCV_Status _siputils_pcv_status = PCV_NONE;
static unsigned int _siputils_pcv_current_msg_id = (unsigned int)-1;

static void sip_generate_charging_vector(char *pcv, const unsigned int maxsize)
{
	char s[PATH_MAX] = {0};
	struct hostent *host = NULL;
	int cdx = 0;
	int tdx = 0;
	int idx = 0;
	int ipx = 0;
	int pid;
	uint64_t ct = 0;
	struct in_addr *in = NULL;
	static struct in_addr ip = {0};
	unsigned char newConferenceIdentifier[SIZE_CONF_ID] = {0};
	int len = SIZE_CONF_ID;
	int i;
	char *ptr = NULL;
	char *endptr = NULL;

	/* if supplied buffer cannot carry 16 (SIZE_CONF_ID) hex characters and a null
		terminator (=33 bytes), then reduce length of generated icid-value */
	if(maxsize < (len * 2 + 1)) {
		LM_WARN("generator buffer too small for new pcv icid-value!\n");
		len = (maxsize - 1) / 2;
	}

	pid = getpid();

	if(ip.s_addr == 0) {
		if(!gethostname(s, PATH_MAX)) {
			if((host = gethostbyname(s)) != NULL) {
				int idx = 0;
				for(idx = 0; host->h_addr_list[idx] != NULL; idx++) {
					in = (struct in_addr *)host->h_addr_list[idx];
					if(in->s_addr == LOOPBACK_IP) {
						if(ip.s_addr == 0) {
							ip = *in;
						}
					} else {
						ip = *in;
					}
				}
			}
		}
	}

	ct = _siputils_pcv_counter++;
	if(_siputils_pcv_counter > 0xFFFFFFFF)
		_siputils_pcv_counter = 0;

	memset(newConferenceIdentifier, 0, SIZE_CONF_ID);
	newConferenceIdentifier[0] = 'I';
	newConferenceIdentifier[1] = 'V';
	newConferenceIdentifier[2] = 'S';
	idx = 3;
	while(idx < SIZE_CONF_ID) {
		if(idx < 7) {
			// 3-6 =IP
			newConferenceIdentifier[idx] = ((ip.s_addr >> (ipx * 8)) & 0xff);
			ipx++;
		} else if(idx < 11) {
			// 7-11 = PID
			newConferenceIdentifier[idx] = ((pid >> (cdx * 8)) & 0xff);
			cdx++;
		} else if(idx == 11) {
			time_t ts = time(NULL);
			newConferenceIdentifier[idx] = (ts & 0xff);
		} else {
			// 12-16 = COUNTER
			newConferenceIdentifier[idx] = ((ct >> (tdx * 8)) & 0xff);
			tdx++;
		}
		idx++;
	}
	LM_DBG("PCV generate\n");
	ptr = pcv;
	endptr = ptr + maxsize - 1;

	for(i = 0; i < len && ptr < endptr; i++) {
		ptr += snprintf(ptr, 3, "%02X", newConferenceIdentifier[i]);
	}
}

static unsigned int sip_param_end(const char *s, const char *end)
{
	unsigned int i;
	unsigned int len = end - s;

	for(i = 0; i < len; i++) {
		if(s[i] == '\0' || s[i] == ' ' || s[i] == ';' || s[i] == ','
				|| s[i] == '\r' || s[i] == '\n') {
			return i;
		}
	}
	return len;
}


static inline void sip_initialize_parse_buffers()
{
	_siputils_pcv_id = (str)STR_NULL;
	_siputils_pcv_genaddr = (str)STR_NULL;
	_siputils_pcv_orig = (str)STR_NULL;
	_siputils_pcv_term = (str)STR_NULL;
}

static inline void sip_initialize_pcv_buffers()
{
	memset(_siputils_pcv.s, 0, sizeof(_siputils_pcv_buf));
	_siputils_pcv.len = 0;
	sip_initialize_parse_buffers();
}

static int sip_parse_charging_vector(const char *pcv_value, unsigned int len)
{
	/* now point to each PCV component */
	LM_DBG("parsing PCV header [%.*s]\n", len, pcv_value);

	char *s = NULL;
	const char *pcv_value_end = pcv_value + len;

	s = strstr(pcv_value, "icid-value=");
	if(s != NULL) {
		_siputils_pcv_id.s = s + strlen("icid-value=");
		_siputils_pcv_id.len = sip_param_end(_siputils_pcv_id.s, pcv_value_end);
		LM_DBG("parsed P-Charging-Vector icid-value=%.*s\n",
				STR_FMT(&_siputils_pcv_id));
	} else {
		LM_WARN("mandatory icid-value not found\n");
		_siputils_pcv_id = (str)STR_NULL;
	}

	s = strstr(pcv_value, "icid-generated-at=");
	if(s != NULL) {
		_siputils_pcv_genaddr.s = s + strlen("icid-generated-at=");
		_siputils_pcv_genaddr.len =
				sip_param_end(_siputils_pcv_genaddr.s, pcv_value_end);
		LM_DBG("parsed P-Charging-Vector icid-generated-at=%.*s\n",
				STR_FMT(&_siputils_pcv_genaddr));
	} else {
		LM_DBG("icid-generated-at not found\n");
		_siputils_pcv_genaddr = (str)STR_NULL;
	}

	s = strstr(pcv_value, "orig-ioi=");
	if(s != NULL) {
		_siputils_pcv_orig.s = s + strlen("orig-ioi=");
		_siputils_pcv_orig.len =
				sip_param_end(_siputils_pcv_orig.s, pcv_value_end);
		LM_INFO("parsed P-Charging-Vector orig-ioi=%.*s\n",
				STR_FMT(&_siputils_pcv_orig));
	} else {
		_siputils_pcv_orig = (str)STR_NULL;
	}

	s = strstr(pcv_value, "term-ioi=");
	if(s != NULL) {
		_siputils_pcv_term.s = s + strlen("term-ioi=");
		_siputils_pcv_term.len =
				sip_param_end(_siputils_pcv_term.s, pcv_value_end);
		LM_INFO("parsed P-Charging-Vector term-ioi=%.*s\n",
				STR_FMT(&_siputils_pcv_term));
	} else {
		_siputils_pcv_term = (str)STR_NULL;
	}

	// only icid-value is mandatory, log anyway when missing icid-generated-at
	if(_siputils_pcv_genaddr.s == NULL && _siputils_pcv_id.s != NULL
			&& len > 0) {
		LM_WARN("icid-generated-at is missing %.*s\n", len, pcv_value);
	}

	return (_siputils_pcv_id.s != NULL);
}

static int sip_get_charging_vector(
		struct sip_msg *msg, struct hdr_field **hf_pcv)
{
	struct hdr_field *hf;
	int hf_body_len;

	str hdrname = STR_STATIC_INIT(P_CHARGING_VECTOR);

	/* we need to be sure we have parsed all headers */
	if(parse_headers(msg, HDR_EOH_F, 0) < 0) {
		LM_ERR("error parsing headers\n");
		return -1;
	}

	sip_initialize_pcv_buffers();
	_siputils_pcv_status = PCV_NONE;

	for(hf = msg->headers; hf; hf = hf->next) {
		if(hf->name.s[0] != 'P') {
			continue;
		}

		if(cmp_hdrname_str(&hf->name, &hdrname) == 0) {

			char *pcv_body = _siputils_pcv_buf;
			*hf_pcv = hf;

			if(hf->body.len > 0) {
				if(hf->body.len > sizeof(_siputils_pcv_buf)) {
					LM_WARN("received charging vector header is longer than "
							"reserved buffer - truncating.");
					hf_body_len = sizeof(_siputils_pcv_buf);
				} else
					hf_body_len = hf->body.len;

				memcpy(pcv_body, hf->body.s, hf_body_len);
				_siputils_pcv.len = hf_body_len;

				if(sip_parse_charging_vector(pcv_body, hf_body_len) == 0) {
					LM_ERR("P-Charging-Vector header found but failed to parse "
						   "value [%s].\n",
							pcv_body);
					_siputils_pcv_status = PCV_ICID_MISSING;
					sip_initialize_parse_buffers();
				} else {
					_siputils_pcv_status = PCV_PARSED;
				}
				return 2;
			} else {
				LM_WARN("P-Charging-Vector header found but has no body.\n");
				_siputils_pcv_status = PCV_ICID_MISSING;
				return 1;
			}
		}
	}
	LM_DBG("No valid P-Charging-Vector header found.\n");
	*hf_pcv = NULL;
	return 1;
}

// Remove PCV if it is in the inbound request (if it was found by sip_get_charging_vector)
static int sip_remove_charging_vector(
		struct sip_msg *msg, struct hdr_field *hf, struct lump **anchor)
{
	struct lump *l;

	if(hf != NULL) {
		l = del_lump(msg, hf->name.s - msg->buf, hf->len, 0);
		if(l == 0) {
			LM_ERR("no memory\n");
			return -1;
		}
		if(anchor != NULL) {
			*anchor = l;
		}
		return 2;
	} else {
		return 1;
	}
}

static int sip_add_charging_vector(
		struct sip_msg *msg, const str *pcv_hf, struct lump *anchor)
{
	str buf = STR_NULL;

	if(anchor == NULL) {
		LM_DBG("add pcv with new lump\n");
		anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
		if(anchor == 0) {
			LM_ERR("can't get anchor\n");
			return -1;
		}
	}

	buf.s = (char *)pkg_malloc(pcv_hf->len);
	if(!buf.s)
		goto out3;
	memcpy(buf.s, pcv_hf->s, pcv_hf->len);

	if((anchor = insert_new_lump_before(anchor, buf.s, pcv_hf->len, 0)) == 0)
		goto out1;

	if((anchor = insert_subst_lump_before(anchor, SUBST_SND_IP, 0)) == 0)
		goto out2;

	buf.s = (char *)pkg_malloc(CRLF_LEN + 1);
	if(!buf.s)
		goto out3;

	buf.len = sprintf(buf.s, CRLF);
	if((anchor = insert_new_lump_before(anchor, buf.s, buf.len, 0)) == 0)
		goto out1;

	return 1;

out1:
	pkg_free(buf.s);
out2:
	LM_ERR("can't insert lump\n");
	return -1;
out3:
	PKG_MEM_ERROR;
	return -1;
}

int sip_handle_pcv(struct sip_msg *msg, char *flags, char *str2)
{
	int generate_pcv = 0;
	int remove_pcv = 0;
	int replace_pcv = 0;
	int i;
	int action = PCV_NOP;

	str flag_str;
	struct lump *deleted_pcv_lump = NULL;

	if(get_str_fparam(&flag_str, msg, (gparam_p)flags) < 0) {
		LM_ERR("failed to retrieve parameter value\n");
		return -1;
	}

	// Process command flags
	for(i = 0; i < flag_str.len; i++) {
		switch(flag_str.s[i]) {
			case 'r':
			case 'R':
				remove_pcv = 1;
				break;

			case 'g':
			case 'G':
				generate_pcv = 1;
				break;

			case 'f':
			case 'F':
				replace_pcv = 1;
				generate_pcv = 1;
				break;

			default:
				break;
		}
	}

	if(_siputils_pcv_current_msg_id != msg->id
			|| _siputils_pcv_status == PCV_NONE) {
		if(sip_get_charging_vector(msg, &_siputils_pcv_hf_pcv) > 0) {
			_siputils_pcv_current_msg_id = msg->id;
		}
	}
	switch(_siputils_pcv_status) {
		case PCV_GENERATED:
			LM_WARN("P-Charging-Vector can't be changed after generation. "
					"Skipping command '%.*s'!",
					STR_FMT(&flag_str));
			return PCV_NOP;
		case PCV_DELETED:
			/* be consistent with the return value in this case */
			if(remove_pcv)
				return PCV_NOP;
		default:
			break;
	}

	/*
	 * We need to remove the original PCV if it was present and either
	 * we were asked to remove it or we were asked to replace it
	 * or when it was broken anyway
	 */
	if((_siputils_pcv_status == PCV_PARSED && (replace_pcv || remove_pcv))
			|| _siputils_pcv_status == PCV_ICID_MISSING) {
		i = sip_remove_charging_vector(
				msg, _siputils_pcv_hf_pcv, &deleted_pcv_lump);
		if(i <= 0)
			return (i == 0) ? -1 : i;
		sip_initialize_pcv_buffers();
		_siputils_pcv_status = PCV_DELETED;
		action = PCV_DELETED;
	}

	/* Generate PCV if
	 * - we were asked to generate it and it could not be obtained from the inbound packet
	 * - or if we were asked to replace it alltogether regardless its former value
	 */
	if(replace_pcv
			|| (generate_pcv && _siputils_pcv_status != PCV_GENERATED
					&& _siputils_pcv_status != PCV_PARSED)) {
		char generated_pcv_buf[PCV_BUF_SIZE] = {0};
		str generated_pcv = {generated_pcv_buf, 0};

		strcpy(generated_pcv_buf, P_CHARGING_VECTOR);
		strcat(generated_pcv_buf, ": ");

		char *pcv_body = generated_pcv_buf + P_CHARGING_VECTOR_PREFIX_LEN;
		int body_len = 0;
		char pcv_value[40] = {0};

		sip_generate_charging_vector(pcv_value, sizeof(pcv_value));

		body_len = snprintf(pcv_body,
				PCV_BUF_SIZE - P_CHARGING_VECTOR_PREFIX_LEN,
				"icid-value=%.*s;icid-generated-at=", (int)sizeof(pcv_value),
				pcv_value);
		generated_pcv.len = body_len + P_CHARGING_VECTOR_PREFIX_LEN;

		/* if it was generated, we need to send it out as a header */
		LM_INFO("Generated new PCV header %.*s\n", PCV_BUF_SIZE,
				generated_pcv_buf);

		i = sip_add_charging_vector(msg, &generated_pcv, deleted_pcv_lump);
		if(i <= 0) {
			LM_ERR("Failed to add P-Charging-Vector header\n");
			return (i == 0) ? -1 : i;
		}

		/* if generated and added, copy buffer and reparse it */
		sip_initialize_pcv_buffers();
		_siputils_pcv.len = body_len;
		memcpy(_siputils_pcv.s, pcv_body, _siputils_pcv.len);
		if(sip_parse_charging_vector(
				   _siputils_pcv_buf, sizeof(_siputils_pcv_buf))) {
			action = (_siputils_pcv_status == PCV_DELETED) ? PCV_REPLACED
														   : PCV_GENERATED;
			_siputils_pcv_status = PCV_GENERATED;
		}
	}

	_siputils_pcv_current_msg_id = msg->id;
	LM_DBG("Charging vector status is now %s\n", sstatus[_siputils_pcv_status]);
	return action;
}


int pv_get_charging_vector(
		struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	str pcv_pv = STR_NULL;

	if(_siputils_pcv_current_msg_id != msg->id
			|| _siputils_pcv_status == PCV_NONE) {
		if(sip_get_charging_vector(msg, &_siputils_pcv_hf_pcv) > 0) {
			_siputils_pcv_current_msg_id = msg->id;
		}
		LM_DBG("Parsed charging vector for pseudo-var, current state is %s\n",
				sstatus[_siputils_pcv_status]);
	} else {
		LM_DBG("Charging vector is in state %s for pseudo-var and buffered.",
				sstatus[_siputils_pcv_status]);
	}

	switch(param->pvn.u.isname.name.n) {
		case PCV_PARAM_TERM:
			pcv_pv = _siputils_pcv_term;
			break;
		case PCV_PARAM_ORIG:
			pcv_pv = _siputils_pcv_orig;
			break;
		case PCV_PARAM_GENADDR:
			pcv_pv = _siputils_pcv_genaddr;
			break;
		case PCV_PARAM_ID:
			pcv_pv = _siputils_pcv_id;
			break;
		case PCV_PARAM_STATUS:
			return pv_get_sintval(msg, param, res, _siputils_pcv_status);
			break;
		case PCV_PARAM_ALL:
		default:
			pcv_pv = _siputils_pcv;
			break;
	}

	if(pcv_pv.len > 0)
		return pv_get_strval(msg, param, res, &pcv_pv);
	else if(param->pvn.u.isname.name.n == PCV_PARAM_ID
			|| param->pvn.u.isname.name.n <= PCV_PARAM_ALL)
		LM_WARN("No value for pseudo-var $pcv but status was %s.\n",
				sstatus[_siputils_pcv_status]);

	return pv_get_null(msg, param, res);
}

int pv_parse_charging_vector_name(pv_spec_p sp, str *in)
{
	if(sp == NULL || in == NULL || in->len <= 0)
		return -1;

	switch(in->len) {
		case 3:
			if(strncmp(in->s, "all", 3) == 0)
				sp->pvp.pvn.u.isname.name.n = PCV_PARAM_ALL;
			else
				goto error;
			break;
		case 4:
			if(strncmp(in->s, "orig", 4) == 0)
				sp->pvp.pvn.u.isname.name.n = PCV_PARAM_ORIG;
			else if(strncmp(in->s, "term", 4) == 0)
				sp->pvp.pvn.u.isname.name.n = PCV_PARAM_TERM;
			else
				goto error;
			break;
		case 5:
			if(strncmp(in->s, "value", 5) == 0)
				sp->pvp.pvn.u.isname.name.n = PCV_PARAM_ID;
			else
				goto error;
			break;
		case 6:
			if(strncmp(in->s, "status", 6) == 0)
				sp->pvp.pvn.u.isname.name.n = PCV_PARAM_STATUS;
			else
				goto error;
			break;
		case 7:
			if(strncmp(in->s, "genaddr", 7) == 0)
				sp->pvp.pvn.u.isname.name.n = PCV_PARAM_GENADDR;
			else
				goto error;
			break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown pcv name %.*s\n", in->len, in->s);
	return -1;
}
