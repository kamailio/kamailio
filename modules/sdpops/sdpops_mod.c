/*
 * $Id$
 *
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
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

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mod_fix.h"
#include "../../pvar.h"
#include "../../usr_avp.h"
#include "../../parser/sdp/sdp.h"
#include "../../parser/sdp/sdp_helpr_funcs.h"
#include "../../trim.h"
#include "../../data_lump.h"
#include "../../ut.h"

#include "api.h"
#include "sdpops_data.h"

MODULE_VERSION

static int w_sdp_remove_line_by_prefix(sip_msg_t* msg, char* prefix, char* bar);
static int w_sdp_remove_codecs_by_id(sip_msg_t* msg, char* codecs, char *bar);
static int w_sdp_remove_codecs_by_name(sip_msg_t* msg, char* codecs, char *bar);
static int w_sdp_keep_codecs_by_id(sip_msg_t* msg, char* codecs, char *bar);
static int w_sdp_keep_codecs_by_name(sip_msg_t* msg, char* codecs, char *bar);
static int w_sdp_with_media(sip_msg_t* msg, char* media, char *bar);
static int w_sdp_with_active_media(sip_msg_t* msg, char* media, char *bar);
static int w_sdp_with_transport(sip_msg_t* msg, char* transport, char *bar);
static int w_sdp_with_transport_like(sip_msg_t* msg, char* transport, char *bar);
static int w_sdp_transport(sip_msg_t* msg, char *bar);
static int w_sdp_with_codecs_by_id(sip_msg_t* msg, char* codec, char *bar);
static int w_sdp_with_codecs_by_name(sip_msg_t* msg, char* codec, char *bar);
static int w_sdp_remove_media(sip_msg_t* msg, char* media, char *bar);
static int w_sdp_remove_transport(sip_msg_t* msg, char* transport, char *bar);
static int w_sdp_print(sip_msg_t* msg, char* level, char *bar);
static int w_sdp_get(sip_msg_t* msg, char *bar);
static int w_sdp_content(sip_msg_t* msg, char* foo, char *bar);
static int w_sdp_with_ice(sip_msg_t* msg, char* foo, char *bar);
static int w_sdp_get_line_startswith(sip_msg_t* msg, char *foo, char *bar);


static int mod_init(void);

static cmd_export_t cmds[] = {
	{"sdp_remove_line_by_prefix",  (cmd_function)w_sdp_remove_line_by_prefix,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_remove_codecs_by_id",    (cmd_function)w_sdp_remove_codecs_by_id,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_remove_codecs_by_name",  (cmd_function)w_sdp_remove_codecs_by_name,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_keep_codecs_by_id",    (cmd_function)w_sdp_keep_codecs_by_id,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_keep_codecs_by_id",    (cmd_function)w_sdp_keep_codecs_by_id,
		2, fixup_spve_spve,  0, ANY_ROUTE},
	{"sdp_keep_codecs_by_name",  (cmd_function)w_sdp_keep_codecs_by_name,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_keep_codecs_by_name",  (cmd_function)w_sdp_keep_codecs_by_name,
		2, fixup_spve_spve,  0, ANY_ROUTE},
	{"sdp_with_media",             (cmd_function)w_sdp_with_media,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_with_active_media",       (cmd_function)w_sdp_with_active_media,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_remove_media",             (cmd_function)w_sdp_remove_media,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_with_transport",         (cmd_function)w_sdp_with_transport,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_with_transport_like",  (cmd_function)w_sdp_with_transport_like,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_remove_transport",       (cmd_function)w_sdp_remove_transport,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_transport",              (cmd_function)w_sdp_transport,
		1, 0,  0, ANY_ROUTE},
	{"sdp_with_codecs_by_id",      (cmd_function)w_sdp_with_codecs_by_id,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_with_codecs_by_name",    (cmd_function)w_sdp_with_codecs_by_name,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_print",                  (cmd_function)w_sdp_print,
		1, fixup_igp_null,  0, ANY_ROUTE},
	{"sdp_get",                  (cmd_function)w_sdp_get,
		1, 0,  0, ANY_ROUTE},
	{"sdp_content",                (cmd_function)w_sdp_content,
		0, 0,  0, ANY_ROUTE},
	{"sdp_with_ice",                (cmd_function)w_sdp_with_ice,
		0, 0,  0, ANY_ROUTE},
	{"sdp_get_line_startswith", (cmd_function)w_sdp_get_line_startswith,
		2, 0,  0, ANY_ROUTE},
	{"bind_sdpops",                (cmd_function)bind_sdpops,
		1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

static pv_export_t mod_pvs[] = {
#if 0
	{{"sdp", (sizeof("sdp")-1)}, /* */
		PVT_OTHER, pv_get_sdp, 0,
		0, 0, 0, 0},
#endif

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static param_export_t params[] = {
	{0, 0, 0}
};

/** module exports */
struct module_exports exports= {
	"sdpops",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,          /* exported statistics */
	0  ,        /* exported MI functions */
	mod_pvs,    /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	0,
	0           /* per-child init function */
};

/** 
 * 
 */
static int mod_init(void)
{
	LM_DBG("sdpops module loaded\n");
	return 0;
}


/**
 *
 */
int sdp_locate_line(sip_msg_t* msg, char *pos, str *aline)
{
	char *p;
	char *bend;

	p = pos;
	while(*p!='\n') p--;
	aline->s = p + 1;
	p = pos;
	bend = msg->buf+msg->len;
	while(*p!='\n' && p<bend) p++;
	aline->len = p - aline->s + 1;
	if(unlikely(p==bend)) aline->len--;

	return 0;
}

/**
 *
 */
int sdp_remove_str_codec_id_attrs(sip_msg_t* msg,
		sdp_stream_cell_t* sdp_stream, str *rm_codec)
{
	str aline = {0, 0};
	sdp_payload_attr_t *payload;
	struct lump *anchor;

	payload = sdp_stream->payload_attr;
	while (payload) {
		LM_DBG("a= ... for codec %.*s/%.*s\n",
				payload->rtp_payload.len, payload->rtp_payload.s,
				payload->rtp_enc.len, payload->rtp_enc.s);
		if(rm_codec->len==payload->rtp_payload.len
				&& strncmp(payload->rtp_payload.s, rm_codec->s,
					rm_codec->len)==0) {
			if(payload->rtp_enc.s!=NULL) {
				if(sdp_locate_line(msg, payload->rtp_enc.s, &aline)==0)
				{
					LM_DBG("removing line: %.*s", aline.len, aline.s);
					anchor = del_lump(msg, aline.s - msg->buf,
							aline.len, 0);
					if (anchor == NULL) {
						LM_ERR("failed to remove [%.*s] inside [%.*s]\n",
								rm_codec->len, rm_codec->s,
								aline.len, aline.s);
						return -1;
					}
				}
			}
			if(payload->fmtp_string.s!=NULL) {
				if(sdp_locate_line(msg, payload->fmtp_string.s, &aline)==0)
				{
					LM_DBG("removing line: %.*s\n", aline.len, aline.s);
					anchor = del_lump(msg, aline.s - msg->buf,
							aline.len, 0);
					if (anchor == NULL) {
						LM_ERR("failed to remove [%.*s] inside [%.*s]\n",
								rm_codec->len, rm_codec->s,
								aline.len, aline.s);
						return -1;
					}
				}
			}
		}
		payload=payload->next;
	}

	return 0;
}

/**
 *
 */
int sdp_codec_in_str(str *allcodecs, str* codec, char delim)
{
	int i;
	int cmp;

	if(allcodecs==NULL || codec==NULL
			|| allcodecs->len<=0 || codec->len<=0)
		return 0;

	cmp = 1;
	for(i=0; i<allcodecs->len; i++) {
		if(cmp==1) {
			if(codec->len <= allcodecs->len-i) {
				if(strncmp(&allcodecs->s[i], codec->s, codec->len)==0) {
					if(&allcodecs->s[i+codec->len]
							== &allcodecs->s[allcodecs->len]
							|| allcodecs->s[i+codec->len] == delim) {
						/* match */
						return 1;
					}
				}
			}
		}
		if(allcodecs->s[i]==delim)
			cmp = 1;
		else
			cmp = 0;
	}

	return 0;
}


/**
 *
 */
int sdp_remove_str_codec_id(sip_msg_t* msg, str *allcodecs, str* rmcodec)
{
	int i;
	int cmp;
	struct lump *anchor;

	if(msg==NULL || allcodecs==NULL || rmcodec==NULL
			|| allcodecs->len<=0 || rmcodec->len<=0)
		return -1;

	cmp = 1;
	for(i=0; i<allcodecs->len; i++) {
		if(cmp==1) {
			if(rmcodec->len <= allcodecs->len-i) {
				if(strncmp(&allcodecs->s[i], rmcodec->s, rmcodec->len)==0) {
					if(&allcodecs->s[i+rmcodec->len]
							== &allcodecs->s[allcodecs->len]
							|| allcodecs->s[i+rmcodec->len] == ' ') {
						/* match - remove also the space before codec id */
						LM_DBG("found codec [%.*s] inside [%.*s]\n",
								rmcodec->len, rmcodec->s,
								allcodecs->len, allcodecs->s);
						anchor = del_lump(msg, &allcodecs->s[i-1] - msg->buf,
								rmcodec->len+1, 0);
						if (anchor == NULL) {
							LM_ERR("failed to remove [%.*s] inside [%.*s]\n",
									rmcodec->len, rmcodec->s,
									allcodecs->len, allcodecs->s);
							return -1;
						}
						return 0;
					}
				}
			}
		}
		if(allcodecs->s[i]==' ')
			cmp = 1;
		else
			cmp = 0;
	}

	return 0;
}

/**
 *
 */
int sdp_remove_codecs_by_id(sip_msg_t* msg, str* codecs)
{
	sdp_info_t *sdp = NULL;
	int sdp_session_num;
	int sdp_stream_num;
	sdp_session_cell_t* sdp_session;
	sdp_stream_cell_t* sdp_stream;
	str sdp_codecs;
	str tmp_codecs;
	str rm_codec;

	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	sdp = (sdp_info_t*)msg->body;

	if(sdp==NULL) {
		LM_DBG("No sdp body\n");
		return -1;
	}

	LM_DBG("attempting to remove codecs from sdp: [%.*s]\n",
			codecs->len, codecs->s);

	sdp_session_num = 0;
	for(;;)
	{
		sdp_session = get_sdp_session(msg, sdp_session_num);
		if(!sdp_session) break;
		sdp_stream_num = 0;
		for(;;)
		{
			sdp_stream = get_sdp_stream(msg, sdp_session_num, sdp_stream_num);
			if(!sdp_stream) break;

			LM_DBG("stream %d of %d - payloads [%.*s]\n",
					sdp_stream_num, sdp_session_num, 
					sdp_stream->payloads.len, sdp_stream->payloads.s);
			sdp_codecs = sdp_stream->payloads;
			tmp_codecs = *codecs;
			while(str_find_token(&tmp_codecs, &rm_codec, ',')==0
					&& rm_codec.len>0)
			{
				tmp_codecs.len -=(int)(&rm_codec.s[rm_codec.len]-tmp_codecs.s);
				tmp_codecs.s = rm_codec.s + rm_codec.len;

				LM_DBG("codecs [%.*s] - remove [%.*s]\n",
						sdp_codecs.len, sdp_codecs.s,
						rm_codec.len, rm_codec.s);
				sdp_remove_str_codec_id(msg, &sdp_codecs, &rm_codec);
				sdp_remove_str_codec_id_attrs(msg, sdp_stream, &rm_codec);
			}
			sdp_stream_num++;
		}
		sdp_session_num++;
	}

	return 0;
}

// removes consecutive blocks of SDP lines that begin with script provided prefix
int sdp_remove_line_by_prefix(sip_msg_t* msg, str* prefix)
{
	str body = {NULL, 0};
	str remove = {NULL, 0};
	str line = {NULL, 0};
	char* del_lump_start = NULL;
	char* del_lump_end = NULL;
	int del_lump_flag = 0;
	struct lump *anchor;
	char* p = NULL;

	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	if(msg->body == NULL) {
		LM_DBG("No sdp body\n");
		return -1;
	}

	body.s = ((sdp_info_t*)msg->body)->raw_sdp.s;
	body.len = ((sdp_info_t*)msg->body)->raw_sdp.len;

	if (body.s==NULL) {
		LM_ERR("failed to get the message body\n");
		return -1;
	}
	body.len = msg->len - (body.s - msg->buf);
	if (body.len==0) {
		LM_DBG("message body has zero length\n");
		return -1;
	}

	p = find_sdp_line(body.s, body.s+body.len, prefix->s[0]);
	while (p != NULL)
	{
		if (sdp_locate_line(msg, p, &line) != 0)
		{
			LM_ERR("sdp_locate_line fail\n");
			return -1;
		}

		//LM_DBG("line.s: %.*s\n", line.len, line.s);

		if (extract_field(&line, &remove, *prefix) == 0)
		{
			//LM_DBG("line range: %d - %d\n", line.s - body.s, line.s + line.len - body.s);

			if (del_lump_start == NULL)
			{
				del_lump_start = line.s;
				del_lump_end = line.s + line.len;
				//LM_DBG("first match, prepare new lump  (len=%d)\n", line.len);
			}
			else if ( p == del_lump_end )  // current line is same as del_lump_end
			{
				del_lump_end = line.s + line.len;
				//LM_DBG("cont. match, made lump longer  (len+=%d)\n", line.len);
			}

			if (del_lump_end >= body.s + body.len)
			{
				//LM_DBG("end of buffer, delete lump\n");
				del_lump_flag = 1;
			}
			//LM_DBG("lump pos: %d - %d\n", del_lump_start - body.s, del_lump_end - body.s);
		}
		else if ( del_lump_end != NULL)
		{
			//LM_DBG("line does not start with search pattern, delete current lump\n");
			del_lump_flag = 1;
		}

		if (del_lump_flag && del_lump_start && del_lump_end)
		{
			LM_DBG("del_lump range: %d - %d  len: %d\n", (int)(del_lump_start - body.s),
					(int)(del_lump_end - body.s), (int)(del_lump_end - del_lump_start));

			anchor = del_lump(msg, del_lump_start - msg->buf, del_lump_end - del_lump_start, HDR_OTHER_T);
			if (anchor == NULL)
			{
				LM_ERR("failed to remove lump\n");
				return -1;
			}

			del_lump_start = NULL;
			del_lump_end = NULL;
			del_lump_flag = 0;
			//LM_DBG("succesful lump deletion\n");
		}

		p = find_sdp_line(line.s + line.len, body.s + body.len, prefix->s[0]);
	}
	return 0;
}

static int w_sdp_remove_line_by_prefix(sip_msg_t* msg, char* prefix, char* bar)
{
	str prfx = {NULL, 0};

	if(prefix==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if (get_str_fparam(&prfx, msg, (fparam_t*)prefix))
	{
		LM_ERR("unable to determine prefix\n");
		return -1;
	}
	LM_DBG("Removing SDP lines with prefix: %.*s\n", prfx.len, prfx.s);

	if(sdp_remove_line_by_prefix(msg, &prfx)<0)
		return -1;
	return 1;
}

/**
 *
 */
static int w_sdp_remove_codecs_by_id(sip_msg_t* msg, char* codecs, char* bar)
{
	str lcodecs = {0, 0};

	if(codecs==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)codecs, &lcodecs)!=0)
	{
		LM_ERR("unable to get the list of codecs\n");
		return -1;
	}

	if(sdp_remove_codecs_by_id(msg, &lcodecs)<0)
		return -1;
	return 1;
}

/**
 *
 */
int sdp_remove_codecs_by_name(sip_msg_t* msg, str* codecs)
{
	sdp_info_t *sdp = NULL;
	str idslist;

	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	sdp = (sdp_info_t*)msg->body;

	if(sdp==NULL) {
		LM_DBG("No sdp body\n");
		return -1;
	}

	LM_DBG("attempting to remove codecs from sdp: [%.*s]\n",
			codecs->len, codecs->s);

	if(sdpops_build_ids_list(sdp, codecs, &idslist)<0)
		return -1;

	if(sdp_remove_codecs_by_id(msg, &idslist)<0)
		return -1;

	return 0;

}

/**
 *
 */
static int w_sdp_remove_codecs_by_name(sip_msg_t* msg, char* codecs, char* bar)
{
	str lcodecs = {0, 0};

	if(codecs==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)codecs, &lcodecs)!=0)
	{
		LM_ERR("unable to get the list of codecs\n");
		return -1;
	}

	if(sdp_remove_codecs_by_name(msg, &lcodecs)<0)
		return -1;
	return 1;
}

/**
 *
 */
int sdp_keep_codecs_by_id(sip_msg_t* msg, str* codecs, str *media)
{
	sdp_info_t *sdp = NULL;
	int sdp_session_num;
	int sdp_stream_num;
	sdp_session_cell_t* sdp_session;
	sdp_stream_cell_t* sdp_stream;
	str sdp_codecs;
	str tmp_codecs;
	str rm_codec;

	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	sdp = (sdp_info_t*)msg->body;

	if(sdp==NULL) {
		LM_DBG("No sdp body\n");
		return -1;
	}

	LM_DBG("attempting to keep codecs in sdp: [%.*s]\n",
			codecs->len, codecs->s);

	sdp_session_num = 0;
	for(;;)
	{
		sdp_session = get_sdp_session(msg, sdp_session_num);
		if(!sdp_session) break;
		sdp_stream_num = 0;
		for(;;)
		{
			sdp_stream = get_sdp_stream(msg, sdp_session_num, sdp_stream_num);
			if(!sdp_stream) break;

			LM_DBG("stream %d of %d - payloads [%.*s]\n",
					sdp_stream_num, sdp_session_num,
					sdp_stream->payloads.len, sdp_stream->payloads.s);
			if((media==NULL)
					|| (media->len==sdp_stream->media.len
						&& strncasecmp(sdp_stream->media.s, media->s,
							media->len)==0))
			{
				sdp_codecs = sdp_stream->payloads;
				tmp_codecs = sdp_stream->payloads;
				while(str_find_token(&tmp_codecs, &rm_codec, ' ')==0
						&& rm_codec.len>0)
				{
					tmp_codecs.len -=(int)(&rm_codec.s[rm_codec.len]-tmp_codecs.s);
					tmp_codecs.s = rm_codec.s + rm_codec.len;

					if(sdp_codec_in_str(codecs, &rm_codec, ',')==0) {
						LM_DBG("codecs [%.*s] - remove [%.*s]\n",
								sdp_codecs.len, sdp_codecs.s,
								rm_codec.len, rm_codec.s);
						sdp_remove_str_codec_id(msg, &sdp_codecs, &rm_codec);
						sdp_remove_str_codec_id_attrs(msg, sdp_stream, &rm_codec);
					}
				}
			}
			sdp_stream_num++;
		}
		sdp_session_num++;
	}

	return 0;
}

/**
 *
 */
static int w_sdp_keep_codecs_by_id(sip_msg_t* msg, char* codecs, char* media)
{
	str lcodecs = {0, 0};
	str lmedia = {0, 0};

	if(codecs==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)codecs, &lcodecs)!=0)
	{
		LM_ERR("unable to get the list of codecs\n");
		return -1;
	}
	if(media!=NULL)
	{
		if(fixup_get_svalue(msg, (gparam_p)media, &lmedia)!=0)
		{
			LM_ERR("unable to get the media type\n");
			return -1;
		}
	}

	if(sdp_keep_codecs_by_id(msg, &lcodecs, (media)?&lmedia:NULL)<0)
		return -1;
	return 1;
}

/**
 *
 */
int sdp_keep_codecs_by_name(sip_msg_t* msg, str* codecs, str *media)
{
	sdp_info_t *sdp = NULL;
	str idslist;

	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	sdp = (sdp_info_t*)msg->body;

	if(sdp==NULL) {
		LM_DBG("No sdp body\n");
		return -1;
	}

	LM_DBG("attempting to keep codecs in sdp: [%.*s]\n",
			codecs->len, codecs->s);

	if(sdpops_build_ids_list(sdp, codecs, &idslist)<0)
		return -1;

	if(sdp_keep_codecs_by_id(msg, &idslist, media)<0)
		return -1;

	return 0;

}

/**
 *
 */
static int w_sdp_keep_codecs_by_name(sip_msg_t* msg, char* codecs, char* media)
{
	str lcodecs = {0, 0};
	str lmedia = {0, 0};

	if(codecs==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)codecs, &lcodecs)!=0)
	{
		LM_ERR("unable to get the list of codecs\n");
		return -1;
	}

	if(media!=NULL)
	{
		if(fixup_get_svalue(msg, (gparam_p)media, &lmedia)!=0)
		{
			LM_ERR("unable to get the media type\n");
			return -1;
		}
	}

	if(sdp_keep_codecs_by_name(msg, &lcodecs, (media)?&lmedia:NULL)<0)
		return -1;
	return 1;
}

/** 
 * @brief check 'media' matches the value of any 'm=value ...' lines
 * @return -1 - error; 0 - not found; 1 - found
 */
static int sdp_with_media(sip_msg_t *msg, str *media)
{
	int sdp_session_num;
	int sdp_stream_num;
	sdp_session_cell_t* sdp_session;
	sdp_stream_cell_t* sdp_stream;

	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	LM_DBG("attempting to search for media type: [%.*s]\n",
			media->len, media->s);

	sdp_session_num = 0;
	for(;;)
	{
		sdp_session = get_sdp_session(msg, sdp_session_num);
		if(!sdp_session) break;
		sdp_stream_num = 0;
		for(;;)
		{
			sdp_stream = get_sdp_stream(msg, sdp_session_num, sdp_stream_num);
			if(!sdp_stream) break;

			LM_DBG("stream %d of %d - media [%.*s]\n",
					sdp_stream_num, sdp_session_num,
					sdp_stream->media.len, sdp_stream->media.s);
			if(media->len==sdp_stream->media.len
					&& strncasecmp(sdp_stream->media.s, media->s,
						media->len)==0)
				return 1;
			sdp_stream_num++;
		}
		sdp_session_num++;
	}

	return 0;
}

/**
 *
 */
static int w_sdp_with_media(sip_msg_t* msg, char* media, char *bar)
{
	str lmedia = {0, 0};

	if(media==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)media, &lmedia)!=0)
	{
		LM_ERR("unable to get the media value\n");
		return -1;
	}

	if(sdp_with_media(msg, &lmedia)<=0)
		return -1;
	return 1;
}

/**
 * @brief check 'media' matches the value of any 'm=value ...' lines, and that line is active
 * @return -1 - error; 0 - not found or inactive; 1 - at least one sendrecv, recvonly or sendonly stream
 */
static int sdp_with_active_media(sip_msg_t *msg, str *media)
{
	int sdp_session_num;
	int sdp_stream_num;
	int port_num;
	sdp_session_cell_t* sdp_session;
	sdp_stream_cell_t* sdp_stream;

	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	LM_DBG("attempting to search for media type: [%.*s]\n",
			media->len, media->s);

	sdp_session_num = 0;
	for(;;)
	{
		sdp_session = get_sdp_session(msg, sdp_session_num);
		if(!sdp_session) break;
		sdp_stream_num = 0;
		for(;;)
		{
			sdp_stream = get_sdp_stream(msg, sdp_session_num, sdp_stream_num);
			if(!sdp_stream) break;

			LM_DBG("stream %d of %d - media [%.*s]\n",
					sdp_stream_num, sdp_session_num,
					sdp_stream->media.len, sdp_stream->media.s);
			if(media->len==sdp_stream->media.len
					&& strncasecmp(sdp_stream->media.s, media->s,
						media->len)==0) {
				port_num = atoi(sdp_stream->port.s);
				LM_DBG("Port number is %d\n", port_num);
				if (port_num != 0) {  /* Zero port number => inactive */
					LM_DBG("sendrecv_mode %.*s\n", sdp_stream->sendrecv_mode.len, sdp_stream->sendrecv_mode.s);
					if ((sdp_stream->sendrecv_mode.len == 0) || /* No send/recv mode given => sendrecv */
						(strncasecmp(sdp_stream->sendrecv_mode.s, "inactive", 8) != 0)) { /* Explicit mode is not inactive */
						/* Found an active stream for the correct media type */
						return 1;
					}
				}
			}
			sdp_stream_num++;
		}
		sdp_session_num++;
	}

	return 0;
}

/**
 *
 */
static int w_sdp_with_active_media(sip_msg_t* msg, char* media, char *bar)
{
	str lmedia = {0, 0};

	if(media==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)media, &lmedia)!=0)
	{
		LM_ERR("unable to get the media value\n");
		return -1;
	}

	if(sdp_with_active_media(msg, &lmedia)<=0)
		return -1;
	return 1;
}

/**
 * @brief remove streams matching the m='media'
 * @return -1 - error; 0 - not found; >=1 - found
 */
static int sdp_remove_media(sip_msg_t *msg, str *media)
{
	sdp_info_t *sdp = NULL;
	int sdp_session_num;
	int sdp_stream_num;
	sdp_session_cell_t* sdp_session;
	sdp_stream_cell_t* sdp_stream;
	sdp_stream_cell_t* nxt_stream;
	int ret = 0;
	char *dstart = NULL;
	int dlen = 0;
	struct lump *anchor;

	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	LM_DBG("attempting to search for media type: [%.*s]\n",
			media->len, media->s);

	sdp = (sdp_info_t*)msg->body;

	sdp_session_num = 0;
	for(;;)
	{
		sdp_session = get_sdp_session(msg, sdp_session_num);
		if(!sdp_session) break;
		sdp_stream_num = 0;
		for(;;)
		{
			sdp_stream = get_sdp_stream(msg, sdp_session_num, sdp_stream_num);
			if(!sdp_stream) break;

			LM_DBG("stream %d of %d - media [%.*s]\n",
					sdp_stream_num, sdp_session_num,
					sdp_stream->media.len, sdp_stream->media.s);
			if(media->len==sdp_stream->media.len
					&& strncasecmp(sdp_stream->media.s, media->s,
						media->len)==0)
			{
				/* found - remove */
				LM_DBG("removing media stream: %.*s", media->len, media->s);
				nxt_stream = get_sdp_stream(msg, sdp_session_num,
						sdp_stream_num+1);
				/* skip back 'm=' */
				dstart = sdp_stream->media.s - 2;
				if(!nxt_stream) {
					/* delete to end of sdp */
					dlen = (int)(sdp->text.s + sdp->text.len - dstart);
				} else {
					/* delete to start of next stream */
					dlen = (int)(nxt_stream->media.s - 2 - dstart);
				}
				anchor = del_lump(msg, dstart - msg->buf, dlen, 0);
				if (anchor == NULL) {
					LM_ERR("failed to remove media type [%.*s]\n",
							media->len, media->s);
					return -1;
				}

				ret++;
			}
			sdp_stream_num++;
		}
		sdp_session_num++;
	}

	return ret;
}


/**
 *
 */
static int w_sdp_remove_media(sip_msg_t* msg, char* media, char *bar)
{
	str lmedia = {0, 0};

	if(media==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)media, &lmedia)!=0)
	{
		LM_ERR("unable to get the media value\n");
		return -1;
	}

	if(sdp_remove_media(msg, &lmedia)<=0)
		return -1;
	return 1;
}


/** 
 * @brief check 'media' matches the value of any 'm=media port value ...' lines
 * @return -1 - error; 0 - not found; 1 - found
 */
static int sdp_with_transport(sip_msg_t *msg, str *transport, int like)
{
	int sdp_session_num;
	int sdp_stream_num;
	sdp_session_cell_t* sdp_session;
	sdp_stream_cell_t* sdp_stream;

	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	LM_DBG("attempting to search for transport type: [%.*s]\n",
			transport->len, transport->s);

	sdp_session_num = 0;
	for(;;)
	{
		sdp_session = get_sdp_session(msg, sdp_session_num);
		if(!sdp_session) break;
		sdp_stream_num = 0;
		for(;;)
		{
			sdp_stream = get_sdp_stream(msg, sdp_session_num, sdp_stream_num);
			if(!sdp_stream) break;

			LM_DBG("stream %d of %d - transport [%.*s]\n",
					sdp_stream_num, sdp_session_num,
					sdp_stream->transport.len, sdp_stream->transport.s);
			if (like == 0) {
			    if(transport->len==sdp_stream->transport.len
			       && strncasecmp(sdp_stream->transport.s, transport->s,
					      transport->len)==0)
				return 1;
			} else {
			    if (ser_memmem(sdp_stream->transport.s, transport->s,
					   sdp_stream->transport.len, transport->len)!=NULL)
				return 1;
			}
			sdp_stream_num++;
		}
		sdp_session_num++;
	}

	return 0;
}

/** 
 * @brief assigns common media transport (if any) of 'm' lines to pv argument
 * @return -1 - error; 0 - not found; 1 - found
 */
static int w_sdp_transport(sip_msg_t* msg, char *avp)
{
    int_str avp_val;
    int_str avp_name;
    static unsigned short avp_type = 0;
    str s;
    pv_spec_t *avp_spec = NULL;
    int sdp_session_num;
    int sdp_stream_num;
    sdp_session_cell_t* sdp_session;
    sdp_stream_cell_t* sdp_stream;
    str *transport;

    s.s = avp; s.len = strlen(s.s);
    if (pv_locate_name(&s) != s.len) {
	LM_ERR("invalid avp parameter %s\n", avp);
	return -1;
    }
    if (((avp_spec = pv_cache_get(&s)) == NULL)
	|| avp_spec->type!=PVT_AVP) {
	LM_ERR("malformed or non AVP %s\n", avp);
	return -1;
    }
    if (pv_get_avp_name(0, &avp_spec->pvp, &avp_name, &avp_type) != 0) {
	LM_ERR("invalid AVP definition %s\n", avp);
	return -1;
    }

    if(parse_sdp(msg) < 0) {
	LM_ERR("unable to parse sdp\n");
	return -1;
    }

    sdp_session_num = 0;
    transport = (str *)NULL;

    for (;;) {
	sdp_session = get_sdp_session(msg, sdp_session_num);
	if (!sdp_session) break;
	sdp_stream_num = 0;
	for (;;) {
	    sdp_stream = get_sdp_stream(msg, sdp_session_num,
					sdp_stream_num);
	    if (!sdp_stream) break;
	    LM_DBG("stream %d of %d - transport [%.*s]\n",
		   sdp_stream_num, sdp_session_num,
		   sdp_stream->transport.len, sdp_stream->transport.s);
	    if (transport) {
		if (transport->len != sdp_stream->transport.len
		    || strncasecmp(sdp_stream->transport.s, transport->s,
				   transport->len) != 0) {
		    LM_DBG("no common transport\n");
		    return -2;
		}
	    } else {
		transport = &sdp_stream->transport;
	    }
	    sdp_stream_num++;
	}
	sdp_session_num++;
    }
    if (transport) {
	avp_val.s.s = transport->s;
	avp_val.s.len = transport->len;
	LM_DBG("found common transport '%.*s'\n",
	       transport->len, transport->s);
	if (add_avp(AVP_VAL_STR | avp_type, avp_name, avp_val) != 0) {
	    LM_ERR("failed to add transport avp");
	    return -1;
	}
    }

    return 1;
}


/**
 *
 */
static int w_sdp_with_transport(sip_msg_t* msg, char* transport, char *bar)
{
	str ltransport = {0, 0};

	if(transport==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)transport, &ltransport)!=0)
	{
		LM_ERR("unable to get the transport value\n");
		return -1;
	}

	if(sdp_with_transport(msg, &ltransport, 0)<=0)
		return -1;
	return 1;
}

/**
 *
 */
static int w_sdp_with_transport_like(sip_msg_t* msg, char* transport, char *bar)
{
	str ltransport = {0, 0};

	if(transport==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)transport, &ltransport)!=0)
	{
		LM_ERR("unable to get the transport value\n");
		return -1;
	}

	if(sdp_with_transport(msg, &ltransport, 1)<=0)
		return -1;
	return 1;
}

/**
 * @brief remove streams matching the m=media port 'transport'
 * @return -1 - error; 0 - not found; >=1 - found
 */
static int sdp_remove_transport(sip_msg_t *msg, str *transport)
{
	sdp_info_t *sdp = NULL;
	int sdp_session_num;
	int sdp_stream_num;
	sdp_session_cell_t* sdp_session;
	sdp_stream_cell_t* sdp_stream;
	sdp_stream_cell_t* nxt_stream;
	int ret = 0;
	char *dstart = NULL;
	int dlen = 0;
	struct lump *anchor;

	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	LM_DBG("attempting to search for transport type: [%.*s]\n",
			transport->len, transport->s);

	sdp = (sdp_info_t*)msg->body;

	sdp_session_num = 0;
	for(;;)
	{
		sdp_session = get_sdp_session(msg, sdp_session_num);
		if(!sdp_session) break;
		sdp_stream_num = 0;
		for(;;)
		{
			sdp_stream = get_sdp_stream(msg, sdp_session_num, sdp_stream_num);
			if(!sdp_stream) break;

			LM_DBG("stream %d of %d - transport [%.*s]\n",
					sdp_stream_num, sdp_session_num,
					sdp_stream->transport.len, sdp_stream->transport.s);
			if(transport->len==sdp_stream->transport.len
					&& strncasecmp(sdp_stream->transport.s, transport->s,
						transport->len)==0)
			{
				/* found - remove */
				LM_DBG("removing transport stream: %.*s", transport->len, transport->s);
				nxt_stream = get_sdp_stream(msg, sdp_session_num,
						sdp_stream_num+1);
				/* skip back 'm=' */
				dstart = sdp_stream->media.s - 2;
				if(!nxt_stream) {
					/* delete to end of sdp */
					dlen = (int)(sdp->text.s + sdp->text.len - dstart);
				} else {
					/* delete to start of next stream */
					dlen = (int)(nxt_stream->media.s - 2 - dstart);
				}
				anchor = del_lump(msg, dstart - msg->buf, dlen, 0);
				if (anchor == NULL) {
					LM_ERR("failed to remove transport type [%.*s]\n",
							transport->len, transport->s);
					return -1;
				}

				ret++;
			}
			sdp_stream_num++;
		}
		sdp_session_num++;
	}

	return ret;
}


/**
 *
 */
static int w_sdp_remove_transport(sip_msg_t* msg, char* transport, char *bar)
{
	str ltransport = {0, 0};

	if(transport==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)transport, &ltransport)!=0)
	{
		LM_ERR("unable to get the transport value\n");
		return -1;
	}

	if(sdp_remove_transport(msg, &ltransport)<=0)
		return -1;
	return 1;
}

/**
 *
 */
int sdp_with_codecs_by_id(sip_msg_t* msg, str* codecs)
{
	sdp_info_t *sdp = NULL;
	int sdp_session_num;
	int sdp_stream_num;
	sdp_session_cell_t* sdp_session;
	sdp_stream_cell_t* sdp_stream;
	str sdp_codecs;
	str tmp_codecs;
	str fnd_codec;
	int foundone = 0;
	int notfound = 0;

	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	sdp = (sdp_info_t*)msg->body;

	if(sdp==NULL) {
		LM_DBG("No sdp body\n");
		return -1;
	}

	LM_DBG("attempting to search codecs in sdp: [%.*s]\n",
			codecs->len, codecs->s);

	sdp_session_num = 0;
	for(;;)
	{
		sdp_session = get_sdp_session(msg, sdp_session_num);
		if(!sdp_session) break;
		sdp_stream_num = 0;
		for(;;)
		{
			sdp_stream = get_sdp_stream(msg, sdp_session_num, sdp_stream_num);
			if(!sdp_stream) break;

			LM_DBG("stream %d of %d - payloads [%.*s]\n",
					sdp_stream_num, sdp_session_num,
					sdp_stream->payloads.len, sdp_stream->payloads.s);
			sdp_codecs = sdp_stream->payloads;
			tmp_codecs = *codecs;
			while(str_find_token(&tmp_codecs, &fnd_codec, ',')==0
					&& fnd_codec.len>0)
			{
				tmp_codecs.len -=(int)(&fnd_codec.s[fnd_codec.len]-tmp_codecs.s);
				tmp_codecs.s = fnd_codec.s + fnd_codec.len;

				if(sdp_codec_in_str(&sdp_codecs, &fnd_codec, ' ')==0) {
					LM_DBG("codecs [%.*s] - not found [%.*s]\n",
							sdp_codecs.len, sdp_codecs.s,
							fnd_codec.len, fnd_codec.s);
					notfound = 1;
				} else {
					LM_DBG("codecs [%.*s] - found [%.*s]\n",
							sdp_codecs.len, sdp_codecs.s,
							fnd_codec.len, fnd_codec.s);
					foundone = 1;
				}
			}
			sdp_stream_num++;
		}
		sdp_session_num++;
	}

	return (foundone + ((foundone)?notfound:0));
}

/**
 *
 */
static int w_sdp_with_codecs_by_id(sip_msg_t* msg, char* codecs, char *bar)
{
	str lcodecs = {0, 0};
	int ret;

	if(codecs==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)codecs, &lcodecs)!=0)
	{
		LM_ERR("unable to get the codecs\n");
		return -1;
	}

	ret = sdp_with_codecs_by_id(msg, &lcodecs);
	/* ret: -1 error; 0 not found */
	if(ret<=0)
		return (ret - 1);
	return ret;
}

/**
 *
 */
static int w_sdp_with_codecs_by_name(sip_msg_t* msg, char* codecs, char *bar)
{
	str lcodecs = {0, 0};
	str idslist;
	sdp_info_t *sdp = NULL;
	int ret;

	if(codecs==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if(fixup_get_svalue(msg, (gparam_p)codecs, &lcodecs)!=0)
	{
		LM_ERR("unable to get the codecs\n");
		return -1;
	}

	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	sdp = (sdp_info_t*)msg->body;

	if(sdp==NULL) {
		LM_DBG("No sdp body\n");
		return -1;
	}

	if(sdpops_build_ids_list(sdp, &lcodecs, &idslist)<0)
		return -1;

	ret = sdp_with_codecs_by_id(msg, &idslist);
	/* ret: -1 error; 0 not found */
	if(ret<=0)
		return (ret - 1);
	return ret;
}

/**
 *
 */
static int w_sdp_print(sip_msg_t* msg, char* level, char *bar)
{
	sdp_info_t *sdp = NULL;
	int llevel = L_DBG;

	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	if(fixup_get_ivalue(msg, (gparam_p)level, &llevel)!=0)
	{
		LM_ERR("unable to get the debug level value\n");
		return -1;
	}

	sdp = (sdp_info_t*)msg->body;

	print_sdp(sdp, llevel);
	return 1;
}


/**
 *
 */
static int w_sdp_get(sip_msg_t* msg, char *avp)
{
	sdp_info_t *sdp = NULL;
	int_str avp_val;
	int_str avp_name;
	static unsigned short avp_type = 0;
	str s;
	pv_spec_t *avp_spec = NULL;
	int sdp_missing=1;

	s.s = avp; s.len = strlen(s.s);
	if (pv_locate_name(&s) != s.len)
	{
		LM_ERR("invalid parameter\n");
		return -1;
	}
	if (((avp_spec = pv_cache_get(&s)) == NULL)
			|| avp_spec->type!=PVT_AVP) {
		LM_ERR("malformed or non AVP %s AVP definition\n", avp);
		return -1;
	}

	if(pv_get_avp_name(0, &avp_spec->pvp, &avp_name, &avp_type)!=0)
	{
		LM_ERR("[%s]- invalid AVP definition\n", avp);
		return -1;
	}

	sdp_missing = parse_sdp(msg);
	if(sdp_missing < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}
	sdp = (sdp_info_t*)msg->body;

	if (sdp_missing) {
		LM_DBG("No SDP\n");
		return -2;
	} else {
		avp_val.s.s = sdp->raw_sdp.s;
		avp_val.s.len = sdp->raw_sdp.len;
		LM_DBG("Found SDP %.*s\n", sdp->raw_sdp.len, sdp->raw_sdp.s);
	}
	if (add_avp(AVP_VAL_STR | avp_type, avp_name, avp_val) != 0)
	{
		LM_ERR("Failed to add SDP avp");
		return -1;
	}

	return 1;
}

/**
 *
 */
static int w_sdp_content(sip_msg_t* msg, char* foo, char *bar)
{
	if(parse_sdp(msg)==0 && msg->body!=NULL)
		return 1;
	return -1;
}

/**
 *
 */
static int w_sdp_with_ice(sip_msg_t* msg, char* foo, char *bar)
{
    str ice, body;

    ice.s = "a=candidate";
    ice.len = 11;

    body.s = get_body(msg);
    if (body.s == NULL) {
	LM_DBG("failed to get the message body\n");
	return -1;
    }

    body.len = msg->len -(int)(body.s - msg->buf);
    if (body.len == 0) {
	LM_DBG("message body has length zero\n");
	return -1;
    }

    if (ser_memmem(body.s, ice.s, body.len, ice.len) != NULL) {
	LM_DBG("found ice attribute\n");
	return 1;
    } else {
	LM_DBG("did't find ice attribute\n");
	return -1;
    }
}

/**
 *
 */
static int w_sdp_get_line_startswith(sip_msg_t *msg, char *avp, char *s_line)
{
	sdp_info_t *sdp = NULL;
	str body = {NULL, 0};
	str line = {NULL, 0};
	char* p = NULL;
	str s;
	str sline;
	int_str avp_val;
	int_str avp_name;
	pv_spec_t *avp_spec = NULL;
	static unsigned short avp_type = 0;
	int sdp_missing=1;

	if (s_line == NULL || strlen(s_line) <= 0)
	{
		LM_ERR("Search string is null or empty\n");
		return -1;
	}
	sline.s = s_line;
	sline.len = strlen(s_line);

	sdp_missing = parse_sdp(msg);

	if(sdp_missing < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	sdp = (sdp_info_t *)msg->body;

	if (sdp_missing || sdp == NULL)
	{
		LM_DBG("No SDP\n");
		return -2;
	}

	body.s = sdp->raw_sdp.s;
	body.len = sdp->raw_sdp.len;

	if (body.s==NULL) {
		LM_ERR("failed to get the message body\n");
		return -1;
	}

	body.len = msg->len - (body.s - msg->buf);
	if (body.len==0) {
		LM_DBG("message body has zero length\n");
		return -1;
	}

	if (avp == NULL || strlen(avp) <= 0)
	{
		LM_ERR("avp variable is null or empty\n");
		return -1;
	}

	s.s = avp;
	s.len = strlen(s.s);

	if (pv_locate_name(&s) != s.len)
	{
		LM_ERR("invalid parameter\n");
		return -1;
	}

	if (((avp_spec = pv_cache_get(&s)) == NULL)
			|| avp_spec->type!=PVT_AVP) {
		LM_ERR("malformed or non AVP %s AVP definition\n", avp);
		return -1;
	}

	if(pv_get_avp_name(0, &avp_spec->pvp, &avp_name, &avp_type)!=0)
	{
		LM_ERR("[%s]- invalid AVP definition\n", avp);
		return -1;
	}

	p = find_sdp_line(body.s, body.s+body.len, sline.s[0]);
	while (p != NULL)
	{
		if (sdp_locate_line(msg, p, &line) != 0)
		{
			LM_ERR("sdp_locate_line fail\n");
			return -1;
		}

		if (strncmp(line.s, sline.s, sline.len) == 0)
		{
			avp_val.s.s = line.s;
			avp_val.s.len = line.len;

			/* skip ending \r\n if exists */
			if (avp_val.s.s[line.len-2] == '\r' && avp_val.s.s[line.len-1] == '\n')
			{
				/* add_avp() clones to shm and adds 0-terminating char */
				avp_val.s.len -= 2;
			}

			if (add_avp(AVP_VAL_STR | avp_type, avp_name, avp_val) != 0)
			{
				LM_ERR("Failed to add SDP line avp");
				return -1;
			}

			return 1;
		}

		p = find_sdp_line(line.s + line.len, body.s + body.len, sline.s[0]);
	}

	return -1;
}

/**
 *
 */
int bind_sdpops(struct sdpops_binds *sob){
	if (sob == NULL) {
		LM_WARN("bind_sdpops: Cannot load sdpops API into a NULL pointer\n");
		return -1;
	}
	sob->sdp_with_media = sdp_with_media;
	return 0;
}
