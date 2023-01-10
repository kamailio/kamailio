/*
 * Copyright (C) 2017-2018 Julien Chavanton jchavanton@gmail.com
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "rms_sdp.h"
#include "rms_util.h"
#include "../../core/data_lump.h"
#include "../../core/parser/parse_content.h"
#include "../../core/parser/sdp/sdp.h"

// https://tools.ietf.org/html/rfc4566
// (protocol version)
const char *sdp_v = "v=0\r\n";
// (session name)
const char *sdp_s = "s=-\r\n";
// (time the session is active)
const char *sdp_t = "t=0 0\r\n";
//"a=rtpmap:101 telephone-event/8000\r\n"
//"a=fmtp:101 0-15\r\n";
//"a=rtpmap:0 PCMU/8000\r\n"
//"a=rtpmap:8 PCMA/8000\r\n"
//"a=rtpmap:96 opus/48000/2\r\n"
//"a=fmtp:96 useinbandfec=1\r\n";

int rms_get_sdp_info(rms_sdp_info_t *sdp_info, struct sip_msg *msg)
{
	sdp_session_cell_t *sdp_session;
	sdp_stream_cell_t *sdp_stream;
	str media_ip, media_port;
	int sdp_session_num = 0;
	int sdp_stream_num = get_sdp_stream_num(msg);
	if(parse_sdp(msg) < 0) {
		LM_INFO("can not parse sdp\n");
		return 0;
	}
	sdp_info_t *sdp = (sdp_info_t *)msg->body;
	if(!sdp) {
		LM_INFO("sdp null\n");
		return 0;
	}
	rms_str_dup(&sdp_info->recv_body, &sdp->text, 1);
	if(!sdp_info->recv_body.s)
		goto error;
	LM_INFO("sdp body - type[%d]\n", sdp->type);
	if(sdp_stream_num > 1 || !sdp_stream_num) {
		LM_INFO("only support one stream[%d]\n", sdp_stream_num);
	}
	sdp_stream_num = 0;
	sdp_session = get_sdp_session(msg, sdp_session_num);
	if(!sdp_session) {
		return 0;
	} else {
		int sdp_stream_num = 0;
		sdp_stream = get_sdp_stream(msg, sdp_session_num, sdp_stream_num);
		if(!sdp_stream) {
			LM_INFO("can not get the sdp stream\n");
			return 0;
		} else {
			rms_str_dup(&sdp_info->payloads, &sdp_stream->payloads, 1);
			if(!sdp_info->payloads.s)
				goto error;
		}
	}
	if(sdp_stream->ip_addr.s && sdp_stream->ip_addr.len > 0) {
		media_ip = sdp_stream->ip_addr;
	} else {
		media_ip = sdp_session->ip_addr;
	}
	rms_str_dup(&sdp_info->remote_ip, &media_ip, 1);
	if(!sdp_info->remote_ip.s)
		goto error;
	rms_str_dup(&media_port, &sdp_stream->port, 0);
	if(!media_port.s)
		goto error;
	sdp_info->remote_port = atoi(media_port.s);
	pkg_free(media_port.s);
	return 1;
error:
	rms_sdp_info_free(sdp_info);
	return 0;
}

static char *rms_sdp_get_rtpmap(str body, int type_number)
{
	char *pos = body.s;
	while((pos = strstr(pos, "a=rtpmap:"))) {
		int id;
		int sampling_rate;
		char codec[64];
		sscanf(pos, "a=rtpmap:%d %s/%d", &id, codec, &sampling_rate);
		if(id == type_number) {
			LM_INFO("[%d][%s/%d]\n", id, codec, sampling_rate);
			return rms_char_dup(codec, 1);
		}
		pos++;
	}
	return NULL;
}

void rms_sdp_info_init(rms_sdp_info_t *sdp_info)
{
	memset(sdp_info, 0, sizeof(rms_sdp_info_t));
}

void rms_sdp_info_free(rms_sdp_info_t *sdp_info)
{
	if(sdp_info->remote_ip.s) {
		shm_free(sdp_info->remote_ip.s);
		sdp_info->remote_ip.s = NULL;
	}
	if(sdp_info->payloads.s) {
		shm_free(sdp_info->payloads.s);
		sdp_info->payloads.s = NULL;
	}
	if(sdp_info->new_body.s) {
		shm_free(sdp_info->new_body.s);
		sdp_info->new_body.s = NULL;
	}
}

int rms_sdp_prepare_new_body(rms_sdp_info_t *sdp_info, PayloadType *pt)
{
	if(sdp_info->new_body.s)
		return 0;

	str *body = &sdp_info->new_body;
	body->len = strlen(sdp_v) + strlen(sdp_s) + strlen(sdp_t);

	// (originator and session identifier)
	char sdp_o[128];
	snprintf(
			sdp_o, 128, "o=- 1028316687 1 IN IP4 %s\r\n", sdp_info->local_ip.s);
	body->len += strlen(sdp_o);

	// (connection information -- not required if included in all media)
	char sdp_c[128];
	snprintf(sdp_c, 128, "c=IN IP4 %s\r\n", sdp_info->local_ip.s);
	body->len += strlen(sdp_c);

	char sdp_m[128];
	snprintf(sdp_m, 128, "m=audio %d RTP/AVP %d\r\n", sdp_info->udp_local_port, pt->type);
	body->len += strlen(sdp_m);

	char sdp_a[128];
	if (pt->type >= 96) {
		if (strcasecmp(pt->mime_type,"opus") == 0) {
			snprintf(sdp_a, 128, "a=rtpmap:%d opus/48000/2\r\n", pt->type);
		} else {
			snprintf(sdp_a, 128, "a=rtpmap:%d %s/%d/%d\r\n", pt->type, pt->mime_type, pt->clock_rate, pt->channels);
		}
		body->len += strlen(sdp_a);
	}

	body->s = shm_malloc(body->len + 1);
	if(!body->s)
		return 0;
	strcpy(body->s, sdp_v);
	strcat(body->s, sdp_o);
	strcat(body->s, sdp_s);
	strcat(body->s, sdp_c);
	strcat(body->s, sdp_t);
	strcat(body->s, sdp_m);

	if (pt->type >= 96) {
		strcat(body->s, sdp_a);
	}
	return 1;
}

PayloadType *
rms_payload_type_new() // This could be problematic as it must remain compatible with the constructor in MS2.
{
	PayloadType *newpayload = (PayloadType *)shm_malloc(sizeof(PayloadType));
	newpayload->flags |= PAYLOAD_TYPE_ALLOCATED;
	memset(newpayload, 0, sizeof(PayloadType));
	return newpayload;
}

PayloadType *rms_sdp_check_payload_type(PayloadType *pt, rms_sdp_info_t *sdp)
{
	pt->clock_rate = 8000;
	pt->channels = 1;
	pt->mime_type = NULL;
	if (pt->type > 127) {
		return NULL;
	} else if (pt->type >= 96) {
		char *rtpmap = rms_sdp_get_rtpmap(sdp->recv_body, pt->type);
		if (!rtpmap) return NULL;
		char *s_mime_type = strtok(rtpmap,"/");
		if (!s_mime_type) {
			shm_free(rtpmap);
			return NULL;
		}
		if (strcasecmp(s_mime_type,"opus") == 0) {
			int payload_type = pt->type;
			memcpy(pt, &payload_type_opus, sizeof(PayloadType));
			pt->type = payload_type;
			char * s_clock_rate = strtok(NULL,"/");
			char * s_channels = strtok(NULL,"/");
			if (s_clock_rate) {
				pt->clock_rate = atoi(s_clock_rate);
			} else {
				pt->clock_rate = 8000;
			}
			if (s_channels) {
				pt->channels = atoi(s_channels);
			} else {
				pt->channels = 2;
			}
			shm_free(rtpmap);
			LM_INFO("[%p][%d][%s|%d|%d]\n", pt, pt->type, pt->mime_type, pt->clock_rate, pt->channels);
			return pt;
		}
		shm_free(pt->mime_type);
		shm_free(rtpmap);

	} else if (pt->type == 0) {
		pt->mime_type = rms_char_dup("pcmu", 1); /* ia=rtpmap:0 PCMU/8000*/
	} else if (pt->type == 8) {
		pt->mime_type = rms_char_dup("pcma", 1);
	}
	//	} else if (pt->type == 9) {
	//		pt->mime_type=rms_char_dup("g722", 1);
	//	} else if (pt->type == 18) {
	//		pt->mime_type=rms_char_dup("g729", 1);
	//	}
	return pt;
}

PayloadType *rms_sdp_select_payload(rms_sdp_info_t *sdp)
{
	// https://tools.ietf.org/html/rfc3551
	LM_INFO("payloads[%s]\n", sdp->payloads.s); // 0 8
	PayloadType *pt = rms_payload_type_new();
	char *payloads = sdp->payloads.s;
	char *payload_type_number = strtok(payloads, " ");

	if (!pt) {
		return NULL;
	}
	while (payload_type_number) {
		pt->type = atoi(payload_type_number);
		pt = rms_sdp_check_payload_type(pt, sdp);
		if (pt->mime_type) return pt;
		payload_type_number = strtok(NULL, " ");
	}
	if (!pt->mime_type) {
		LM_INFO("unsuported codec\n");
		shm_free(pt); // payload_type_destroy(pt);
		return NULL;
	}
	LM_INFO("payload_type:%d %s/%d/%d\n", pt->type, pt->mime_type,
			pt->clock_rate, pt->channels);
	return pt;
}


int rms_sdp_set_body(struct sip_msg *msg, str *new_body)
{
	struct lump *anchor;
	char *buf;
	int len;
	char *value_s;
	int value_len;
	str body = {0, 0};
	str content_type_sdp = str_init("application/sdp");

	if(!new_body->s || new_body->len == 0) {
		LM_ERR("invalid body parameter\n");
		return -1;
	}

	body.len = 0;
	body.s = get_body(msg);
	if(body.s == 0) {
		LM_ERR("malformed sip message\n");
		return -1;
	}

	del_nonshm_lump(&(msg->body_lumps));
	msg->body_lumps = NULL;

	if(msg->content_length) {
		body.len = get_content_length(msg);
		if(body.len > 0) {
			if(body.s + body.len > msg->buf + msg->len) {
				LM_ERR("invalid content length: %d\n", body.len);
				return -1;
			}
			if(del_lump(msg, body.s - msg->buf, body.len, 0) == 0) {
				LM_ERR("cannot delete existing body");
				return -1;
			}
		}
	}

	anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
	if(!anchor) {
		LM_ERR("failed to get anchor\n");
		return -1;
	}

	if(msg->content_length == 0) {
		/* need to add Content-Length */
		len = new_body->len;
		value_s = int2str(len, &value_len);
		LM_DBG("content-length: %d (%s)\n", value_len, value_s);

		len = CONTENT_LENGTH_LEN + value_len + CRLF_LEN;
		buf = pkg_malloc(sizeof(char) * (len));
		if(!buf) {
			LM_ERR("out of pkg memory\n");
			return -1;
		}

		memcpy(buf, CONTENT_LENGTH, CONTENT_LENGTH_LEN);
		memcpy(buf + CONTENT_LENGTH_LEN, value_s, value_len);
		memcpy(buf + CONTENT_LENGTH_LEN + value_len, CRLF, CRLF_LEN);
		if(insert_new_lump_after(anchor, buf, len, 0) == 0) {
			LM_ERR("failed to insert content-length lump\n");
			pkg_free(buf);
			return -1;
		}
	}

	/* add content-type */
	if(msg->content_type == NULL
			|| msg->content_type->body.len != content_type_sdp.len
			|| strncmp(msg->content_type->body.s, content_type_sdp.s,
					   content_type_sdp.len)
					   != 0) {
		if(msg->content_type != NULL) {
			if(del_lump(msg, msg->content_type->name.s - msg->buf,
					   msg->content_type->len, 0)
					== 0) {
				LM_ERR("failed to delete content type\n");
				return -1;
			}
		}
		value_len = content_type_sdp.len;
		len = sizeof("Content-Type: ") - 1 + value_len + CRLF_LEN;
		buf = pkg_malloc(sizeof(char) * (len));
		if(!buf) {
			LM_ERR("out of pkg memory\n");
			return -1;
		}
		memcpy(buf, "Content-Type: ", sizeof("Content-Type: ") - 1);
		memcpy(buf + sizeof("Content-Type: ") - 1, content_type_sdp.s,
				value_len);
		memcpy(buf + sizeof("Content-Type: ") - 1 + value_len, CRLF, CRLF_LEN);
		if(insert_new_lump_after(anchor, buf, len, 0) == 0) {
			LM_ERR("failed to insert content-type lump\n");
			pkg_free(buf);
			return -1;
		}
	}
	anchor = anchor_lump(msg, body.s - msg->buf, 0, 0);

	if(anchor == 0) {
		LM_ERR("failed to get body anchor\n");
		return -1;
	}

	buf = pkg_malloc(sizeof(char) * (new_body->len));
	if(!buf) {
		LM_ERR("out of pkg memory\n");
		return -1;
	}
	memcpy(buf, new_body->s, new_body->len);
	if(!insert_new_lump_after(anchor, buf, new_body->len, 0)) {
		LM_ERR("failed to insert body lump\n");
		pkg_free(buf);
		return -1;
	}
	LM_DBG("new body: [%.*s]", new_body->len, new_body->s);
	return 1;
}
