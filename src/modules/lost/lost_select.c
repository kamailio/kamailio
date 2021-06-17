/*
 * lost module - select interface
 *
 * Copyright (C) 2021 Eugene Sukhanov
 * NGA911
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
 * \file
 * \brief Kamailio lost :: Select interface.
 * \ingroup lost
 * Module: \ref lost
 */

#include "../../core/select.h"
#include "../../core/select_buf.h"
#include "../../core/parser/msg_parser.h"
#include "../../core/parser/hf.h"
#include "../../core/parser/parse_content.h"
#include "../../core/parser/parse_param.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_body.h"
#include "../../core/strutils.h"
#include "../../core/trim.h"
#include "../../core/msg_translator.h"


/*
@lost.nena - all Call-Info headers with “purpose” param “nena-CallId” or “nena-IncidentId”;
@lost.nena.call_id - the value of the first Call-Info header with purpose param of “nena-CallId”;
@lost.nena.incident_id - the value of the first Call-Info header with purpose param of “nena-IncidentId”;

@lost.emergency_call_data - all Call-Info headers with “purpose=EmergencyCallData”;

@lost.emergency_call_data.provider_info - all Call-Info headers with “purpose=EmergencyCallData.ProviderInfo”;
@lost.emergency_call_data.provider_info.counter - counter of present Call-Info headers with relevant emergency call data type;
@lost.emergency_call_data.provider_info[%i].uri_type - one of `http`, `https`, `cid` or `sip`;
@lost.emergency_call_data.provider_info[%i].uri - content of header inside “<>”;
@lost.emergency_call_data.provider_info[%i].raw - see details below;
@lost.emergency_call_data.provider_info[%i].body - if call-info header contains `cid` value (emergency call data passed BY_VALUE),
	then contains `body` of relevant multipart part;
@lost.emergency_call_data.provider_info[%i].header.%s[%i] - if call-info header contains `cid` value (emergency call data passed BY_VALUE),
	then provide access to multipart part headers like `Content-Type`, `Content-Disposition`;
@lost.emergency_call_data.provider_info[%i].header.%s - the short form of @lost.emergency_call_data.provider_info[%i].header.%s[1];

@lost.emergency_call_data.service_info - all Call-Info headers with “purpose=EmergencyCallData.ServiceInfo”;
@lost.emergency_call_data.service_info.counter - same as provider_info;
@lost.emergency_call_data.service_info[%i].uri_type - same as provider_info
@lost.emergency_call_data.service_info[%i].uri - content of header inside “<>”;
@lost.emergency_call_data.service_info[%i].raw - same as provider_info
@lost.emergency_call_data.service_info[%i].body - same as provider_info
@lost.emergency_call_data.service_info[%i].header.%s[%i] - if call-info header contains `cid` value (emergency call data passed BY_VALUE), then provide access to multipart part headers like `Content-Type`, `Content-Disposition`;
@lost.emergency_call_data.service_info[%i].header.%s - the short form of @lost.emergency_call_data.service_info[%i].header.%s[1];

@lost.emergency_call_data.device_info - all Call-Info headers with “purpose=EmergencyCallData.DeviceInfo”;
@lost.emergency_call_data.device_info.counter - same as provider_info;
@lost.emergency_call_data.device_info[%i].uri_type - same as provider_info
@lost.emergency_call_data.device_info[%i].uri - content of header inside “<>”;
@lost.emergency_call_data.device_info[%i].raw - same as provider_info
@lost.emergency_call_data.device_info[%i].body - same as provider_info
@lost.emergency_call_data.device_info[%i].header.%s[%i] - if call-info header contains `cid` value (emergency call data passed BY_VALUE), then provide access to multipart part headers like `Content-Type`, `Content-Disposition`;
@lost.emergency_call_data.device_info[%i].header.%s - the short form of @lost.emergency_call_data.device_info[%i].header.%s[1];

@lost.emergency_call_data.subscriber_info - all Call-Info headers with “purpose=EmergencyCallData.SubscriberInfo”;
@lost.emergency_call_data.subscriber_info.counter - same as provider_info;
@lost.emergency_call_data.subscriber_info[%i].uri_type - same as provider_info
@lost.emergency_call_data.subscriber_info[%i].uri - content of header inside “<>”;
@lost.emergency_call_data.subscriber_info[%i].raw - same as provider_info
@lost.emergency_call_data.subscriber_info[%i].body - same as provider_info
@lost.emergency_call_data.subscriber_info[%i].header.%s[%i] - if call-info header contains `cid` value (emergency call data passed BY_VALUE), then provide access to multipart part headers like `Content-Type`, `Content-Disposition`;
@lost.emergency_call_data.subscriber_info[%i].header.%s - the short form of @lost.emergency_call_data.subscriber_info[%i].header.%s[1];

@lost.emergency_call_data.comment - all Call-Info headers with “purpose=EmergencyCallData.Comment”;
@lost.emergency_call_data.comment.counter - same as provider_info;
@lost.emergency_call_data.comment[%i].uri_type - same as provider_info
@lost.emergency_call_data.comment[%i].uri - content of header inside “<>”;
@lost.emergency_call_data.comment[%i].raw - same as provider_info
@lost.emergency_call_data.comment[%i].body - same as provider_info
@lost.emergency_call_data.comment[%i].header.%s[%i] - if call-info header contains `cid` value (emergency call data passed BY_VALUE), then provide access to multipart part headers like `Content-Type`, `Content-Disposition`;
@lost.emergency_call_data.comment[%i].header.%s - the short form of @lost.emergency_call_data.comment[%i].header.%s[1];
 */

static char* strstrin(char* string, int len, char* substring, int n)
{
    const char *a, *b;
    int l;
    b = substring;
    if ((*b == 0) || (n <= 0)) return string;
    for(l = 0; (string[l] != 0) && (l < len); l++)
    {
        if (tolower(string[l]) != tolower(*b)) continue;
        a = string + l;
        while(1)
        {
            if((b - substring) >= n) return string + l;
            if(tolower(*a++) != tolower(*b++)) break;
        }
        b = substring;
    }
    return NULL;
}

static char* strstri(char* string, int len, char* substring)
{
	return strstrin(string, len, substring, strlen(substring));
}

static int sel_lost(str* res, select_t* s, struct sip_msg* msg)
{ /* dummy */
	return 0;
}

char* CheckParamHdr(str* s, char* paramname, char* paramval)
{
	str ts;
	char *pos, *respos;
	ts = *s;
	pos = strstri(ts.s, ts.len, paramname);
	if (pos)	//paramname found
	{
		respos = pos;
		ts.len = ts.len - (pos - ts.s);
		ts.s = pos;
		trim_leading(&ts);
		pos = strstri(ts.s, ts.len, "=");
		if (pos)	//"=" found
		{
			ts.len = ts.len - (pos - ts.s);
			ts.s = pos;
			trim_leading(&ts);
			pos = strstri(ts.s, ts.len, paramval);
			if (pos) return respos;	//param found, return paramname ptr
		}
	}
	return NULL;
}

char *GetHdrValueEnd(str* hs)
{
	int l, q = 0;
	for (l = 0; l < hs->len; l++)
	{
		switch (hs->s[l])
		{
		case '"': q ^= 1; break;
		case ',': if (!q) return hs->s + l;
		}
	}
	return hs->s + l;
}

int GetNextHdrValue(str* body, str* res)
{
	char *pos;

	res->s += res->len;
	res->len = body->len - (res->s - body->s);
	for(; res->len > 0; res->len--, res->s++)
	{
		if ((*(res->s) != ' ') && (*(res->s) != '\t') && (*(res->s) != '\r') && (*(res->s) != '\n') && (*(res->s) != ','))
			break;
	}
	if (!res->len) return 0;	// no more values

	pos = GetHdrValueEnd(res);
	res->len = pos - res->s;
	return 1;	// OK
}

#define _ALLOC_INC_SIZE 1024
// select all headers with purpose=prps or purpose=prps2
// return headers count or negative value on error
static int sel_hdrs(str* res, select_t* s, struct sip_msg* msg, char *prps, char *prps2)
{
	int indx = 0;
	int l, check, cnt = 0;
	str ts;
	struct hdr_field *hf;
	char *buf = NULL;
	int buf_len = _ALLOC_INC_SIZE;
	char* posarr[1024];
	res->s = NULL;
	res->len = 0;

	if(parse_headers(msg, HDR_CALLINFO_F, 0) < 0)
	{
		LM_ERR("error while parsing message headers\n");
		return -1;
	}
	buf = pkg_malloc(buf_len);
	if(!buf) {
		LM_ERR("out of memory\n");
		res->len = 0;
		return E_OUT_OF_MEM;
	}
	buf[0] = 0;

	l = 0;
	for (hf = msg->headers; hf; hf = hf->next)
	{
		if (HDR_CALLINFO_T == hf->type)
		{
			ts.s = hf->body.s;
			ts.len = 0;
			while (GetNextHdrValue(&hf->body, &ts))
			{
				check = CheckParamHdr(&ts, "purpose", prps)?1:0;
				if (prps2) check |= (CheckParamHdr(&ts, "purpose", prps2)?2:0);
				if (check)
				{
					l = strlen(buf);
					if (l + ts.len + 1 > buf_len)
					{
						buf_len = l + ts.len + _ALLOC_INC_SIZE;
						res->s = pkg_realloc(buf, buf_len);
						if (!res->s)
						{
							pkg_free(buf);
							LM_ERR("cannot realloc buffer\n");
							res->len = 0;
							return E_OUT_OF_MEM;
						}
						buf = res->s;
					}
					if (l) { strcat(buf, ","); l++; }
					posarr[cnt] = buf + l;
					memcpy(buf + l, ts.s, ts.len);
					l += ts.len;
					buf[l] = 0;
					cnt++;
				}
			}
			posarr[cnt] = buf + strlen(buf);
		}
	}
	if ((s->n > 3) && (s->params[3].type==SEL_PARAM_INT))
		indx = s->params[3].v.i;

	ts.s = buf;
	ts.len = strlen(buf);

	if (indx)
	{
		// select only data_name[indx]
		if (indx < 0)
		{	// take real positive index for negative indx
			indx = 1 + cnt + indx;
		}

		if ((indx < 1) || (indx > cnt))
		{
			ts.s = NULL;
			ts.len = 0;
		}
		else
		{
			ts.s = posarr[indx - 1];
			ts.len = posarr[indx] - posarr[indx - 1] - 1;
		}
	}

	res->len = ts.len;
	res->s = NULL;
	if (res->len > 0)
	{
		res->s = get_static_buffer(res->len);
		if(!res->s)
		{
			res->len = 0;
			LM_ERR("cannot allocate static buffer\n");
			cnt = E_OUT_OF_MEM;
		}
		else
			memcpy(res->s, ts.s, res->len);
	}
	pkg_free(buf);
	return cnt;
}

// @lost.nena
static int sel_nena(str* res, select_t* s, struct sip_msg* msg)
{
	// @lost.nena - all Call-Info headers with “purpose” param “nena-CallId” or “nena-IncidentId”;
	int err = sel_hdrs(res, s, msg, "nena-CallId", "nena-IncidentId");
	return (err < 0)?err:0;
}

// 	@lost.nena.call_id
static int sel_call_id(str* res, select_t* s, struct sip_msg* msg)
{
	str ts;
	int err = sel_hdrs(res, s, msg, "nena-CallId", NULL);
	err = (err < 0)?err:0;
	if (!err)
	{
		ts.s = res->s;
		ts.len = 0;
		if (!GetNextHdrValue(res, &ts)) return -1;
		res->s = ts.s;
		res->len = ts.len;
	}
	return err;
}

// 	@lost.nena.incident_id
static int sel_incident_id(str* res, select_t* s, struct sip_msg* msg)
{
	str ts;
	int err = sel_hdrs(res, s, msg, "nena-IncidentId", NULL);
	err = (err < 0)?err:0;
	if (!err)
	{
		ts.s = res->s;
		ts.len = 0;
		if (!GetNextHdrValue(res, &ts)) return -1;
		res->s = ts.s;
		res->len = ts.len;
	}
	return err;
}

// 	@lost.emergency_call_data
static int sel_emergency_call_data(str* res, select_t* s, struct sip_msg* msg)
{
	// “purpose=EmergencyCallData”
	int err = sel_hdrs(res, s, msg, "EmergencyCallData", NULL);
	return (err < 0)?err:0;
}

// @lost.emergency_call_data.provider_info -
static int sel_provider_info(str* res, select_t* s, struct sip_msg* msg)
{
	// all Call-Info headers with “purpose=EmergencyCallData.ProviderInfo”;
	int err = sel_hdrs(res, s, msg, "EmergencyCallData.ProviderInfo", NULL);
	return (err < 0)?err:0;
}

//@lost.emergency_call_data.provider_info.counter - counter of present Call-Info headers with relevant emergency call data type;
static int sel_provider_info_cnt(str* res, select_t* s, struct sip_msg* msg)
{
	// all Call-Info headers with “purpose=EmergencyCallData.ProviderInfo”;
	s->params[3].v.i = 0;
	int err = sel_hdrs(res, s, msg, "EmergencyCallData.ProviderInfo", NULL);
	res->s = int2str((err > 0)?err:0, &res->len);
	return (err < 0)?err:0;
}

static int sel_hdr_uri(str* res)
{
	char *pos, *pos2;
	pos = strstri(res->s, res->len, "<");
	if (!pos) return -1;
	pos2 = strstri(pos+1, res->len - (pos - res->s), ">");
	if (!pos2) return -2;

	res->len = pos2 - pos - 1;
	res->s = pos + 1;
	return 0;
}

// @lost.emergency_call_data.provider_info[%i].uri - content of header inside “<>”;
static int sel_provider_info_uri(str* res, select_t* s, struct sip_msg* msg)
{
	// @lost.emergency_call_data.provider_info[%i]
	int err = sel_provider_info(res, s, msg);
	if (!err) err = sel_hdr_uri(res);
	return err;
}

SELECT_F(select_uri_type)

//static struct sip_uri uri;

// @lost.emergency_call_data.provider_info[%i].uri_type;
static int sel_provider_info_uri_type(str* res, select_t* s, struct sip_msg* msg)
{
	// @lost.emergency_call_data.provider_info[%i]
	int err = sel_provider_info_uri(res, s, msg);
	if (!err)
	{
		// `http`, `https`, `cid` or `sip`
		char* pos = strstri(res->s, res->len, ":");
		if (pos) res->len = pos - res->s;
		else res->len = 0;
	}
	return err;
}

static int GetNextBodyPartHdr(str *bodypart, str *bhr)
{
	str ts = *bodypart;
	char *start = bodypart->s;
	int len = bodypart->len;

	trim_leading(&ts);
	start = ts.s;
	len = ts.len;
	if (bhr->len > 0)
	{
		start = bhr->s + bhr->len;
		len -= start - ts.s;
	}
	if (len > 0)
	{
		char* pos = strstri(start, len, "\r\n");
		if (pos)
		{
			len = 2 + pos - start;
			bhr->s = start;
			bhr->len = len;
			ts = *bhr;
			trim(&ts);
			return (ts.len > 0)?1:0;	// stop on void line
		}
	}
	return 0;	// no more headers
}

static int sel_uri_raw(str* res, select_t* s, struct sip_msg* msg)
{
	char* pos = strstri(res->s, res->len, "cid:");
	if (pos)
	{
		int content_length;
		char *d;
		str cid;
		char *buf;
		int l = strlen("cid:");
		
		// get Content-Id:
		cid.len = res->len - l - (pos - res->s);
		cid.s = pos + l;
		buf = pkg_malloc(cid.len + 1);
		memcpy(buf, cid.s, cid.len);
		buf[cid.len] = 0;
		
		// get body part - filter => Content-Id
		d = get_body_part_by_filter(msg, 0, 0, buf, NULL, &content_length);
		if (!d) content_length = 0;
		res->len = content_length;
		res->s = d;
		pkg_free(buf);
	}
	else res->len = 0;
	return 0;
}

// @lost.emergency_call_data.provider_info[%i].raw;
static int sel_provider_info_raw(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_provider_info_uri(res, s, msg);
	if (!err) err = sel_uri_raw(res, s, msg);
	return err;
}

static int sel_raw_body(str* res, select_t* s, struct sip_msg* msg)
{
	str bodypart;
	str bhr;
	bodypart = *res;
	bhr.len = 0;
	while (GetNextBodyPartHdr(&bodypart, &bhr))
	{
		res->s = bhr.s + bhr.len;
		res->len = bodypart.len - (res->s - bodypart.s);
	}
	return 0;
}

//@lost.emergency_call_data.provider_info[%i].body
// - if call-info header contains `cid` value (emergency call data passed BY_VALUE), then contains `body` of relevant multipart part;
static int sel_provider_info_body(str* res, select_t* s, struct sip_msg* msg)
{
	// @lost.emergency_call_data.provider_info[%i]
	int err = sel_provider_info_raw(res, s, msg);
	if (!err) err = sel_raw_body(res, s, msg);
	return err;
}

static int sel_provider_info_header(str *res, select_t *s, struct sip_msg *msg)
{ /* dummy */
	return 0;
}

static int sel_raw_header_name(str *res, select_t *s, struct sip_msg *msg)
{
	str hname = {0, 0};
	str bodypart;
	str bhr;
	int i, indx = 1, cnt;

	if(s->params[5].type == SEL_PARAM_STR)
	{
		for(i = s->params[5].v.s.len - 1; i > 0; i--)
		{
			if(s->params[5].v.s.s[i] == '_')
				s->params[5].v.s.s[i] = '-';
		}
		hname.len = s->params[5].v.s.len;
		hname.s = s->params[5].v.s.s;

	}
	if ((s->n > 6) && (s->params[6].type==SEL_PARAM_INT))
		indx = s->params[6].v.i;

	bodypart = *res;
	cnt = indx;
	if (indx < 0)
	{	// take real positive index for negative indx
		cnt = 0;
		bhr.len = 0;
		while (GetNextBodyPartHdr(&bodypart, &bhr))
			if (!strncasecmp(bhr.s, hname.s, hname.len)) cnt++;

		indx = 1 + cnt + indx;
	}

	res->s = NULL;
	res->len = 0;
	if ((indx > 0) && (indx <= cnt))
	{
		bhr.len = 0;
		cnt = 0;
		while (!res->len && GetNextBodyPartHdr(&bodypart, &bhr))
		{
			if (!strncasecmp(bhr.s, hname.s, hname.len))
			{
				cnt++;
				if (indx == cnt)
				{
					res->s = bhr.s;
					res->len = bhr.len;
					break;
				}
			}
		}
	}
	return 0;
}

//@lost.emergency_call_data.provider_info[%i].header.%s[%i] - if call-info header contains `cid` value 
// (emergency call data passed BY_VALUE), then provide access to multipart part headers like 
// `Content-Type`, `Content-Disposition`;
static int sel_provider_info_header_name(str *res, select_t *s, struct sip_msg *msg)
{
	int err = sel_provider_info_raw(res, s, msg);
	if (!err) err = sel_raw_header_name(res, s, msg);
	return err;
}


/*
 * @lost.emergency_call_data.service_info -
 */

static int sel_service_info(str* res, select_t* s, struct sip_msg* msg)
{
	// all Call-Info headers with “purpose=EmergencyCallData.ServiceInfo”;
	int err = sel_hdrs(res, s, msg, "EmergencyCallData.ServiceInfo", NULL);
	return (err < 0)?err:0;
}

static int sel_service_info_cnt(str* res, select_t* s, struct sip_msg* msg)
{
	s->params[3].v.i = 0;
	int err = sel_hdrs(res, s, msg, "EmergencyCallData.ServiceInfo", NULL);
	res->s = int2str((err > 0)?err:0, &res->len);
	return (err < 0)?err:0;
}

static int sel_service_info_uri(str* res, select_t* s, struct sip_msg* msg)
{
	// @lost.emergency_call_data.provider_info[%i]
	int err = sel_service_info(res, s, msg);
	if (!err) err = sel_hdr_uri(res);
	return err;
}

static int sel_service_info_uri_type(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_service_info_uri(res, s, msg);
	if (!err)
	{
		// `http`, `https`, `cid` or `sip`
		char* pos = strstri(res->s, res->len, ":");
		if (pos) res->len = pos - res->s;
		else res->len = 0;
	}
	return err;
}

static int sel_service_info_raw(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_service_info_uri(res, s, msg);
	if (!err) err = sel_uri_raw(res, s, msg);
	return err;
}

static int sel_service_info_body(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_service_info_raw(res, s, msg);
	if (!err) err = sel_raw_body(res, s, msg);
	return err;
}

static int sel_service_info_header(str *res, select_t *s, struct sip_msg *msg)
{ /* dummy */
	return 0;
}

static int sel_service_info_header_name(str *res, select_t *s, struct sip_msg *msg)
{
	int err = sel_service_info_raw(res, s, msg);
	if (!err) err = sel_raw_header_name(res, s, msg);
	return err;
}



/*
 * @lost.emergency_call_data.device_info -
 */

static int sel_device_info(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_hdrs(res, s, msg, "EmergencyCallData.DeviceInfo", NULL);
	return (err < 0)?err:0;
}

static int sel_device_info_cnt(str* res, select_t* s, struct sip_msg* msg)
{
	s->params[3].v.i = 0;
	int err = sel_hdrs(res, s, msg, "EmergencyCallData.DeviceInfo", NULL);
	res->s = int2str((err > 0)?err:0, &res->len);
	return (err < 0)?err:0;
}

static int sel_device_info_uri(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_device_info(res, s, msg);
	if (!err) err = sel_hdr_uri(res);
	return err;
}

static int sel_device_info_uri_type(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_device_info_uri(res, s, msg);
	if (!err)
	{
		char* pos = strstri(res->s, res->len, ":");
		if (pos) res->len = pos - res->s;
		else res->len = 0;
	}
	return err;
}

static int sel_device_info_raw(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_device_info_uri(res, s, msg);
	if (!err) err = sel_uri_raw(res, s, msg);
	return err;
}

static int sel_device_info_body(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_device_info_raw(res, s, msg);
	if (!err) err = sel_raw_body(res, s, msg);
	return err;
}

static int sel_device_info_header(str *res, select_t *s, struct sip_msg *msg)
{ /* dummy */
	return 0;
}

static int sel_device_info_header_name(str *res, select_t *s, struct sip_msg *msg)
{
	int err = sel_device_info_raw(res, s, msg);
	if (!err) err = sel_raw_header_name(res, s, msg);
	return err;
}

/*
 * @lost.emergency_call_data.subscriber_info -
 */

static int sel_subscriber_info(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_hdrs(res, s, msg, "EmergencyCallData.SubscriberInfo", NULL);
	return (err < 0)?err:0;
}

static int sel_subscriber_info_cnt(str* res, select_t* s, struct sip_msg* msg)
{
	s->params[3].v.i = 0;
	int err = sel_hdrs(res, s, msg, "EmergencyCallData.SubscriberInfo", NULL);
	res->s = int2str((err > 0)?err:0, &res->len);
	return (err < 0)?err:0;
}

static int sel_subscriber_info_uri(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_subscriber_info(res, s, msg);
	if (!err) err = sel_hdr_uri(res);
	return err;
}

static int sel_subscriber_info_uri_type(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_subscriber_info_uri(res, s, msg);
	if (!err)
	{
		char* pos = strstri(res->s, res->len, ":");
		if (pos) res->len = pos - res->s;
		else res->len = 0;
	}
	return err;
}

static int sel_subscriber_info_raw(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_subscriber_info_uri(res, s, msg);
	if (!err) err = sel_uri_raw(res, s, msg);
	return err;
}

static int sel_subscriber_info_body(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_subscriber_info_raw(res, s, msg);
	if (!err) err = sel_raw_body(res, s, msg);
	return err;
}

static int sel_subscriber_info_header(str *res, select_t *s, struct sip_msg *msg)
{ /* dummy */
	return 0;
}

static int sel_subscriber_info_header_name(str *res, select_t *s, struct sip_msg *msg)
{
	int err = sel_subscriber_info_raw(res, s, msg);
	if (!err) err = sel_raw_header_name(res, s, msg);
	return err;
}

/*
 * @lost.emergency_call_data.comment -
 */

static int sel_comment(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_hdrs(res, s, msg, "EmergencyCallData.Comment", NULL);
	return (err < 0)?err:0;
}

static int sel_comment_cnt(str* res, select_t* s, struct sip_msg* msg)
{
	s->params[3].v.i = 0;
	int err = sel_hdrs(res, s, msg, "EmergencyCallData.Comment", NULL);
	res->s = int2str((err > 0)?err:0, &res->len);
	return (err < 0)?err:0;
}

static int sel_comment_uri(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_comment(res, s, msg);
	if (!err) err = sel_hdr_uri(res);
	return err;
}

static int sel_comment_uri_type(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_comment_uri(res, s, msg);
	if (!err)
	{
		char* pos = strstri(res->s, res->len, ":");
		if (pos) res->len = pos - res->s;
		else res->len = 0;
	}
	return err;
}

static int sel_comment_raw(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_comment_uri(res, s, msg);
	if (!err) err = sel_uri_raw(res, s, msg);
	return err;
}

static int sel_comment_body(str* res, select_t* s, struct sip_msg* msg)
{
	int err = sel_comment_raw(res, s, msg);
	if (!err) err = sel_raw_body(res, s, msg);
	return err;
}

static int sel_comment_header(str *res, select_t *s, struct sip_msg *msg)
{ /* dummy */
	return 0;
}

static int sel_comment_header_name(str *res, select_t *s, struct sip_msg *msg)
{
	int err = sel_comment_raw(res, s, msg);
	if (!err) err = sel_raw_header_name(res, s, msg);
	return err;
}

select_row_t lost_sel[] = {
	/* Current cipher parameters */
	{ NULL, SEL_PARAM_STR, STR_STATIC_INIT("lost"), sel_lost, SEL_PARAM_EXPECTED},

	{ sel_lost, SEL_PARAM_STR, STR_STATIC_INIT("nena"), sel_nena, 0},
	{ sel_nena, SEL_PARAM_STR, STR_STATIC_INIT("call_id"), sel_call_id, 0},
	{ sel_nena, SEL_PARAM_STR, STR_STATIC_INIT("incident_id"), sel_incident_id, 0},

	{ sel_lost, SEL_PARAM_STR, STR_STATIC_INIT("emergency_call_data"), sel_emergency_call_data, 0},
	{ sel_emergency_call_data, SEL_PARAM_STR, STR_STATIC_INIT("provider_info"), sel_provider_info, CONSUME_NEXT_INT | OPTIONAL},
	{ sel_provider_info, SEL_PARAM_STR, STR_STATIC_INIT("counter"), sel_provider_info_cnt, 0},
	{ sel_provider_info, SEL_PARAM_STR, STR_STATIC_INIT("urib"), sel_provider_info_uri, 0},
	{ sel_provider_info, SEL_PARAM_STR, STR_STATIC_INIT("uri_type"), sel_provider_info_uri_type, 0},
	{ sel_provider_info, SEL_PARAM_STR, STR_STATIC_INIT("raw"), sel_provider_info_raw, 0},
	{ sel_provider_info, SEL_PARAM_STR, STR_STATIC_INIT("body"), sel_provider_info_body, 0},
	{ sel_provider_info, SEL_PARAM_STR, STR_STATIC_INIT("header"), sel_provider_info_header, SEL_PARAM_EXPECTED},
	{ sel_provider_info_header, SEL_PARAM_STR, STR_NULL, sel_provider_info_header_name, CONSUME_NEXT_INT | OPTIONAL },

	{ sel_emergency_call_data, SEL_PARAM_STR, STR_STATIC_INIT("service_info"), sel_service_info, CONSUME_NEXT_INT | OPTIONAL},
	{ sel_service_info, SEL_PARAM_STR, STR_STATIC_INIT("counter"), sel_service_info_cnt, 0},
	{ sel_service_info, SEL_PARAM_STR, STR_STATIC_INIT("urib"), sel_service_info_uri, 0},
	{ sel_service_info, SEL_PARAM_STR, STR_STATIC_INIT("uri_type"), sel_service_info_uri_type, 0},
	{ sel_service_info, SEL_PARAM_STR, STR_STATIC_INIT("raw"), sel_service_info_raw, 0},
	{ sel_service_info, SEL_PARAM_STR, STR_STATIC_INIT("body"), sel_service_info_body, 0},
	{ sel_service_info, SEL_PARAM_STR, STR_STATIC_INIT("header"), sel_service_info_header, SEL_PARAM_EXPECTED},
	{ sel_service_info_header, SEL_PARAM_STR, STR_NULL, sel_service_info_header_name, CONSUME_NEXT_INT | OPTIONAL },

	{ sel_emergency_call_data, SEL_PARAM_STR, STR_STATIC_INIT("device_info"), sel_device_info, CONSUME_NEXT_INT | OPTIONAL},
	{ sel_device_info, SEL_PARAM_STR, STR_STATIC_INIT("counter"), sel_device_info_cnt, 0},
	{ sel_device_info, SEL_PARAM_STR, STR_STATIC_INIT("urib"), sel_device_info_uri, 0},
	{ sel_device_info, SEL_PARAM_STR, STR_STATIC_INIT("uri_type"), sel_device_info_uri_type, 0},
	{ sel_device_info, SEL_PARAM_STR, STR_STATIC_INIT("raw"), sel_device_info_raw, 0},
	{ sel_device_info, SEL_PARAM_STR, STR_STATIC_INIT("body"), sel_device_info_body, 0},
	{ sel_device_info, SEL_PARAM_STR, STR_STATIC_INIT("header"), sel_device_info_header, SEL_PARAM_EXPECTED},
	{ sel_device_info_header, SEL_PARAM_STR, STR_NULL, sel_device_info_header_name, CONSUME_NEXT_INT | OPTIONAL },

	{ sel_emergency_call_data, SEL_PARAM_STR, STR_STATIC_INIT("subscriber_info"), sel_subscriber_info, CONSUME_NEXT_INT | OPTIONAL},
	{ sel_subscriber_info, SEL_PARAM_STR, STR_STATIC_INIT("counter"), sel_subscriber_info_cnt, 0},
	{ sel_subscriber_info, SEL_PARAM_STR, STR_STATIC_INIT("urib"), sel_subscriber_info_uri, 0},
	{ sel_subscriber_info, SEL_PARAM_STR, STR_STATIC_INIT("uri_type"), sel_subscriber_info_uri_type, 0},
	{ sel_subscriber_info, SEL_PARAM_STR, STR_STATIC_INIT("raw"), sel_subscriber_info_raw, 0},
	{ sel_subscriber_info, SEL_PARAM_STR, STR_STATIC_INIT("body"), sel_subscriber_info_body, 0},
	{ sel_subscriber_info, SEL_PARAM_STR, STR_STATIC_INIT("header"), sel_subscriber_info_header, SEL_PARAM_EXPECTED},
	{ sel_subscriber_info_header, SEL_PARAM_STR, STR_NULL, sel_subscriber_info_header_name, CONSUME_NEXT_INT | OPTIONAL },

	{ sel_emergency_call_data, SEL_PARAM_STR, STR_STATIC_INIT("comment"), sel_comment, CONSUME_NEXT_INT | OPTIONAL},
	{ sel_comment, SEL_PARAM_STR, STR_STATIC_INIT("counter"), sel_comment_cnt, 0},
	{ sel_comment, SEL_PARAM_STR, STR_STATIC_INIT("urib"), sel_comment_uri, 0},
	{ sel_comment, SEL_PARAM_STR, STR_STATIC_INIT("uri_type"), sel_comment_uri_type, 0},
	{ sel_comment, SEL_PARAM_STR, STR_STATIC_INIT("raw"), sel_comment_raw, 0},
	{ sel_comment, SEL_PARAM_STR, STR_STATIC_INIT("body"), sel_comment_body, 0},
	{ sel_comment, SEL_PARAM_STR, STR_STATIC_INIT("header"), sel_comment_header, SEL_PARAM_EXPECTED},
	{ sel_comment_header, SEL_PARAM_STR, STR_NULL, sel_comment_header_name, CONSUME_NEXT_INT | OPTIONAL },

	{ NULL, SEL_PARAM_INT, STR_NULL, NULL, 0}
};
