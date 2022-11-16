/*
 * Copyright (C) 2011-2016 Daniel-Constantin Mierla (asipto.com)
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
#include "../../core/dprint.h"
#include "../../core/mod_fix.h"
#include "../../core/pvar.h"
#include "../../core/usr_avp.h"
#include "../../core/parser/sdp/sdp.h"
#include "../../core/parser/sdp/sdp_helpr_funcs.h"
#include "../../core/trim.h"
#include "../../core/data_lump.h"
#include "../../core/ut.h"
#include "../../core/kemi.h"
#include "../../core/parser/parse_content.h"

#include "api.h"
#include "sdpops_data.h"

MODULE_VERSION

static int w_sdp_remove_line_by_prefix(sip_msg_t* msg, char* prefix, char* media);
static int w_sdp_remove_codecs_by_id(sip_msg_t* msg, char* codecs, char *media);
static int w_sdp_remove_codecs_by_name(sip_msg_t* msg, char* codecs, char *media);
static int w_sdp_keep_codecs_by_id(sip_msg_t* msg, char* codecs, char *media);
static int w_sdp_keep_codecs_by_name(sip_msg_t* msg, char* codecs, char *media);
static int w_sdp_with_media(sip_msg_t* msg, char* media, char *bar);
static int w_sdp_with_active_media(sip_msg_t* msg, char* media, char *bar);
static int w_sdp_with_transport(sip_msg_t* msg, char* transport, char *bar);
static int w_sdp_with_transport_like(sip_msg_t* msg, char* transport, char *bar);
static int w_sdp_transport(sip_msg_t* msg, char *avp, char *p2);
static int w_sdp_with_codecs_by_id(sip_msg_t* msg, char* codec, char *bar);
static int w_sdp_with_codecs_by_name(sip_msg_t* msg, char* codec, char *bar);
static int w_sdp_remove_media(sip_msg_t* msg, char* media, char *bar);
static int w_sdp_remove_transport(sip_msg_t* msg, char* transport, char *bar);
static int w_sdp_print(sip_msg_t* msg, char* level, char *bar);
static int w_sdp_get(sip_msg_t* msg, char *avp, char *p2);
static int w_sdp_content(sip_msg_t* msg, char* foo, char *bar);
static int w_sdp_content_sloppy(sip_msg_t* msg, char* foo, char *bar);
static int w_sdp_with_ice(sip_msg_t* msg, char* foo, char *bar);
static int w_sdp_get_line_startswith(sip_msg_t* msg, char *foo, char *bar);

static int sdp_get_sess_version(sip_msg_t* msg, str* sess_version, long* sess_version_num);
static int sdp_set_sess_version(sip_msg_t* msg, str* sess_version, long* sess_version_num);
static int w_sdp_get_address_family(sip_msg_t* msg);

static int pv_get_sdp(sip_msg_t *msg, pv_param_t *param,
		pv_value_t *res);
static int pv_set_sdp(sip_msg_t *msg, pv_param_t *param,
		int op, pv_value_t *res);
static int pv_parse_sdp_name(pv_spec_p sp, str *in);

static int mod_init(void);

static cmd_export_t cmds[] = {
	{"sdp_remove_line_by_prefix",  (cmd_function)w_sdp_remove_line_by_prefix,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_remove_line_by_prefix",  (cmd_function)w_sdp_remove_line_by_prefix,
		2, fixup_spve_spve,  0, ANY_ROUTE},
	{"sdp_remove_codecs_by_id",    (cmd_function)w_sdp_remove_codecs_by_id,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_remove_codecs_by_id",    (cmd_function)w_sdp_remove_codecs_by_id,
		2, fixup_spve_spve,  0, ANY_ROUTE},
	{"sdp_remove_codecs_by_name",  (cmd_function)w_sdp_remove_codecs_by_name,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_remove_codecs_by_name",    (cmd_function)w_sdp_remove_codecs_by_name,
		2, fixup_spve_spve,  0, ANY_ROUTE},
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
	{"sdp_content",                (cmd_function)w_sdp_content_sloppy,
		1, 0,  0, ANY_ROUTE},
	{"sdp_with_ice",                (cmd_function)w_sdp_with_ice,
		0, 0,  0, ANY_ROUTE},
	{"sdp_get_line_startswith", (cmd_function)w_sdp_get_line_startswith,
		2, fixup_none_spve,  0, ANY_ROUTE},
	{"sdp_get_address_family", (cmd_function)w_sdp_get_address_family,
			0, 0,  0, ANY_ROUTE},
	{"bind_sdpops",                (cmd_function)bind_sdpops,
		1, 0, 0, 0},
	{0, 0, 0, 0, 0, 0}
};

static pv_export_t mod_pvs[] = {
	{{"sdp", (sizeof("sdp")-1)}, /* */
		PVT_OTHER, pv_get_sdp, pv_set_sdp,
		pv_parse_sdp_name, 0, 0, 0},

	{ {0, 0}, 0, 0, 0, 0, 0, 0, 0 }
};

static param_export_t params[] = {
	{0, 0, 0}
};

/** module exports */
struct module_exports exports= {
	"sdpops",        /* module name */
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,            /* cmd exports */
	params,          /* param exports */
	0,               /* RPC method exports */
	mod_pvs,         /* exported pseudo-variables */
	0,               /* response handling function */
	mod_init,        /* module initialization function */
	0,               /* per-child init function */
	0                /* module destroy function */
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
	str sdp_attrs_list[] = {
		str_init("a=rtpmap:"),
		str_init("a=fmtp:"),
		str_init("a=rtcp-fb:"),
		{0, 0}
	};
	int i;
	str aline = {0, 0};
	str raw_stream = {0, 0};
	struct lump *anchor;

	raw_stream = sdp_stream->raw_stream;
	while(raw_stream.len>5) {
		sdp_locate_line(msg, raw_stream.s, &aline);
		/* process attribute lines: a=x:c... */
		if((aline.len>5) && ((aline.s[0] | 0x20)=='a')) {
			LM_DBG("processing sdp line [%.*s]\n", aline.len, aline.s);
			for(i=0; sdp_attrs_list[i].s!=NULL; i++) {
				if(aline.len > sdp_attrs_list[i].len + rm_codec->len) {
					if(strncasecmp(aline.s, sdp_attrs_list[i].s,
								sdp_attrs_list[i].len)==0
							&& strncmp(aline.s+sdp_attrs_list[i].len, rm_codec->s,
								rm_codec->len)==0
							&& aline.s[sdp_attrs_list[i].len + rm_codec->len]==' ') {
						LM_DBG("removing line: [%.*s]\n", aline.len, aline.s);
						anchor = del_lump(msg, aline.s - msg->buf,
								aline.len, 0);
						if (anchor == NULL) {
							LM_ERR("failed to remove - id [%.*s] line [%.*s]\n",
									rm_codec->len, rm_codec->s,
									aline.len, aline.s);
							return -1;
						}
					}
				}
			}
		}
		raw_stream.s = aline.s + aline.len;
		raw_stream.len -= aline.len;
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
int sdp_remove_codecs_by_id(sip_msg_t* msg, str* codecs, str* media)
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

			if((media==NULL) || (media->len==0)
					|| (media->len==sdp_stream->media.len
						&& strncasecmp(sdp_stream->media.s, media->s,
							media->len)==0))
			{
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
			}
			sdp_stream_num++;
		}
		sdp_session_num++;
	}

	return 0;
}

int sdp_remove_line_lump_by_prefix(sip_msg_t* msg, str* body, str* prefix) {

	char *ptr = NULL;
	str line = {NULL, 0};
	str remove = {NULL, 0};
	int found = 0;
	struct lump *anchor = NULL;

	ptr = find_sdp_line(body->s, body->s + body->len, prefix->s[0]);
	while (ptr)
	{
		if (sdp_locate_line(msg, ptr, &line) != 0)
		{
			LM_ERR("sdp_locate_line() failed\n");
			return -1;
		}

		if (body->s + body->len < line.s + prefix->len) // check if strncmp would run too far
		{
			//LM_DBG("done searching, prefix string >%.*s< (%d) does not fit into remaining buffer space (%ld) \n", prefix->len, prefix->s, prefix->len, body->s + body->len - line.s);
			break;
		}

		if (strncmp(line.s, prefix->s, prefix->len ) == 0)
		{
			//LM_DBG("current remove >%.*s< (%d)\n", remove.len, remove.s, remove.len);
			if (!found) {
				//LM_DBG("first match >%.*s< (%d)\n", line.len,line.s,line.len);
				remove.s = line.s;
				remove.len = line.len;
			} else {
				//LM_DBG("cont. match >%.*s< (%d)\n", line.len,line.s,line.len);
				if (remove.s + remove.len == line.s) {
					//LM_DBG("this match is right after previous match\n");
					remove.len += line.len;
				} else {
					//LM_DBG("there is gap between this and previous match, remove now\n");
					anchor = del_lump(msg, remove.s - msg->buf, remove.len, HDR_OTHER_T);
					if (anchor==NULL)
					{
						LM_ERR("failed to remove lump\n");
						return -1;
					}
					remove.s = line.s;
					remove.len = line.len;
				}
			}
			found++;
			//LM_DBG("updated remove >%.*s< (%d)\n", remove.len, remove.s, remove.len);

		}
		ptr = find_next_sdp_line(ptr, body->s + body->len, prefix->s[0], NULL);
	}

	if (found) {
		//LM_DBG("remove >%.*s< (%d)\n", remove.len, remove.s, remove.len);
		anchor = del_lump(msg, remove.s - msg->buf, remove.len, HDR_OTHER_T);
		if (anchor==NULL)
		{
			LM_ERR("failed to remove lump\n");
			return -1;
		}
		return found;
	}

	LM_DBG("no match\n");
	return 0;
}

/**
 * @brief remove all SDP lines that begin with prefix
 * @return -1 - error; 0 - no lines found ; 1..N - N lines deleted
 */
int sdp_remove_line_by_prefix(sip_msg_t* msg, str* prefix, str* media)
{
	str body = {NULL, 0};
	int sdp_session_num = 0;
	int sdp_stream_num = 0;
	int found = 0;

	if(parse_sdp(msg) != 0) {
		LM_ERR("Unable to parse SDP\n");
		return -1;
	}

	body.s = ((sdp_info_t*)msg->body)->raw_sdp.s;
	body.len = ((sdp_info_t*)msg->body)->raw_sdp.len;

	if (body.s==NULL) {
		LM_ERR("failed to get the message body\n");
		return -1;
	}

	if (body.len==0) {
		LM_DBG("message body has zero length\n");
		return -1;
	}

	if (media->s==NULL || media->len==0 ) {
		LM_DBG("media type filter not set\n");
		found = sdp_remove_line_lump_by_prefix(msg, &body, prefix);
	} else {
		LM_DBG("using media type filter: %.*s\n",media->len, media->s);

		sdp_session_cell_t* sdp_session;
		sdp_stream_cell_t* sdp_stream;

		sdp_session_num = 0;
		for (;;) {
			sdp_session = get_sdp_session(msg, sdp_session_num);
			if(!sdp_session) break;
			sdp_stream_num = 0;
			for (;;) {
				sdp_stream = get_sdp_stream(msg, sdp_session_num, sdp_stream_num);
				if(!sdp_stream) break;
				if( sdp_stream->media.len == media->len &&
					strncasecmp(sdp_stream->media.s, media->s, media->len) == 0) {

					LM_DBG("range for media type %.*s: %ld - %ld\n",
							sdp_stream->media.len, sdp_stream->media.s,
							(long int)(sdp_stream->raw_stream.s - body.s),
							(long int)(sdp_stream->raw_stream.s
								+ sdp_stream->raw_stream.len - body.s)
					);

					found += sdp_remove_line_lump_by_prefix(msg,&(sdp_stream->raw_stream),prefix);

				}
				sdp_stream_num++;
			}
			sdp_session_num++;
		}
	}
	return found;
}

/**
 * removes all SDP lines that begin with script provided prefix
 * @return -1 - error; 1 - found
 */
static int w_sdp_remove_line_by_prefix(sip_msg_t* msg, char* prefix, char* media)
{
	str lprefix = {NULL, 0};
	str lmedia = {NULL, 0};

	if(prefix==0)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	if (get_str_fparam(&lprefix, msg, (fparam_t*)prefix))
	{
		LM_ERR("unable to determine prefix\n");
		return -1;
	}

	if (media != NULL) {
		if (get_str_fparam(&lmedia, msg, (fparam_t*)media))
		{
			LM_ERR("unable to get the media type\n");
			return -1;
		}
	}

	LM_DBG("Removing SDP lines with prefix: %.*s\n", lprefix.len, lprefix.s);

	if ( sdp_remove_line_by_prefix(msg, &lprefix, &lmedia) < 0)
		return -1;
	return 1;
}

/**
 *
 */
static int w_sdp_remove_codecs_by_id(sip_msg_t* msg, char* codecs, char* media)
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

	if(sdp_remove_codecs_by_id(msg, &lcodecs, &lmedia)<0)
		return -1;
	return 1;
}

/**
 *
 */
int sdp_remove_codecs_by_name(sip_msg_t* msg, str* codecs, str* media)
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

	if(sdp_remove_codecs_by_id(msg, &idslist, media)<0)
		return -1;

	return 0;

}

/**
 *
 */
static int w_sdp_remove_codecs_by_name(sip_msg_t* msg, char* codecs, char* media)
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

	if(sdp_remove_codecs_by_name(msg, &lcodecs, &lmedia)<0)
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
			if((media==NULL) || (media->len==0)
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
	AF_INET 2
	AF_INET6 10

it helps to extract IP adress family at c line  from sdp
	@param msg
	@return -1 for error,
			4 for  IP4,
			6 for  IP6

*/
static int w_sdp_get_address_family(sip_msg_t *msg){

	sdp_session_cell_t* session;
	int sdp_session_num;
	int result= -1;
	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp body \n");
		return -1;
	}

	sdp_session_num = 0;

	for(;;){

		session = get_sdp_session(msg, sdp_session_num);
		if(!session)
			break;

		if(session->pf==AF_INET){
			result = 4;
		}else if(session->pf==AF_INET6){
			result = 6;
		}else{
			result = -1;
		}
		sdp_session_num++;
	}

	return result;
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
				LM_DBG("removing media stream: %.*s\n", media->len, media->s);
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
static int sdp_transport_helper(sip_msg_t* msg, char *avp)
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

static int w_sdp_transport(sip_msg_t* msg, char *avp, char *p2)
{
	return sdp_transport_helper(msg, avp);
}

static int ki_sdp_transport(sip_msg_t* msg, str *avp)
{
	return sdp_transport_helper(msg, avp->s);
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
 *
 */
static int ki_sdp_with_transport(sip_msg_t* msg, str* transport)
{
	if(sdp_with_transport(msg, transport, 0)<=0)
		return -1;
	return 1;
}

/**
 *
 */
static int ki_sdp_with_transport_like(sip_msg_t* msg, str* transport)
{
	if(sdp_with_transport(msg, transport, 1)<=0)
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
int sdp_with_codecs_by_name(sip_msg_t* msg, str* codecs)
{
	str idslist;
	sdp_info_t *sdp = NULL;
	int ret;

	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	sdp = (sdp_info_t*)msg->body;

	if(sdp==NULL) {
		LM_DBG("No sdp body\n");
		return -1;
	}

	if(sdpops_build_ids_list(sdp, codecs, &idslist)<0)
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
static int w_sdp_with_codecs_by_name(sip_msg_t* msg, char* codecs, char *bar)
{
	str lcodecs = {0, 0};

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

	return sdp_with_codecs_by_name(msg, &lcodecs);
}

/**
 *
 */
static int ki_sdp_print(sip_msg_t* msg, int llevel)
{
	sdp_info_t *sdp = NULL;

	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	print_sdp(sdp, llevel);
	return 1;
}


/**
 *
 */
static int w_sdp_print(sip_msg_t* msg, char* level, char *bar)
{
	int llevel = L_DBG;

	if(fixup_get_ivalue(msg, (gparam_p)level, &llevel)!=0) {
		LM_ERR("unable to get the debug level value\n");
		return -1;
	}

	return ki_sdp_print(msg, llevel);
}

/**
 *
 */
static int sdp_get_helper(sip_msg_t* msg, char *avp)
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

	if (sdp==NULL) {
		LM_DBG("No SDP\n");
		return -2;
	}

	avp_val.s.s = sdp->raw_sdp.s;
	avp_val.s.len = sdp->raw_sdp.len;
	LM_DBG("Found SDP %.*s\n", sdp->raw_sdp.len, sdp->raw_sdp.s);

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
static int w_sdp_get(sip_msg_t* msg, char *avp, char *p2)
{
	return sdp_get_helper(msg, avp);
}

static int ki_sdp_get(sip_msg_t* msg, str *avp)
{
	return sdp_get_helper(msg, avp->s);
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
static int ki_sdp_content(sip_msg_t* msg)
{
	if(parse_sdp(msg)==0 && msg->body!=NULL)
		return 1;
	return -1;
}

/**
 *
 */
static int ki_sdp_content_flags(sip_msg_t *msg, int flags)
{
	str body;
	int mime;

	if(flags==0) {
		return ki_sdp_content(msg);
	}

	body.s = get_body(msg);
	if(body.s == NULL)
		return -1;
	body.len = msg->len - (int)(body.s - msg->buf);
	if(body.len == 0)
		return -1;

	mime = parse_content_type_hdr(msg);
	if(mime < 0)
		return -1; /* error */
	if(mime == 0)
		return 1; /* default is application/sdp */

	switch(((unsigned int)mime) >> 16) {
		case TYPE_APPLICATION:
			if((mime & 0x00ff) == SUBTYPE_SDP)
				return 1;
			else
				return -1;
		case TYPE_MULTIPART:
			if((mime & 0x00ff) == SUBTYPE_MIXED) {
				if(_strnistr(body.s, "application/sdp", body.len) == NULL) {
					return -1;
				} else {
					return 1;
				}
			} else {
				return -1;
			}
		default:
			return -1;
	}
}

/**
 *
 */
static int w_sdp_content_sloppy(sip_msg_t *msg, char *foo, char *bar)
{
	return ki_sdp_content_flags(msg, 1);
}
/**
 *
 */
int sdp_with_ice(sip_msg_t* msg)
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
		LM_DBG("didn't find ice attribute\n");
		return -1;
	}
}

/**
 *
 */
static int w_sdp_with_ice(sip_msg_t* msg, char* foo, char *bar)
{
	return sdp_with_ice(msg);
}

/**
 *
 */
static int ki_sdp_get_line_startswith(sip_msg_t *msg, str *aname, str *sline)
{
	sdp_info_t *sdp = NULL;
	str body = {NULL, 0};
	str line = {NULL, 0};
	char* p = NULL;
	int_str avp_val;
	int_str avp_name;
	pv_spec_t *avp_spec = NULL;
	static unsigned short avp_type = 0;
	int sdp_missing=1;

	if (sline == NULL || sline->len <= 0) {
		LM_ERR("Search string is null or empty\n");
		return -1;
	}
	if (aname == NULL || aname->len <= 0) {
		LM_ERR("avp variable name is null or empty\n");
		return -1;
	}
	sdp_missing = parse_sdp(msg);

	if(sdp_missing < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	sdp = (sdp_info_t *)msg->body;

	if (sdp_missing || sdp == NULL) {
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

	if (pv_locate_name(aname) != aname->len) {
		LM_ERR("invalid parameter - cannot locate pv name\n");
		return -1;
	}

	if (((avp_spec = pv_cache_get(aname)) == NULL)
			|| avp_spec->type!=PVT_AVP) {
		LM_ERR("malformed or non AVP %.*s AVP definition\n",
				aname->len, aname->s);
		return -1;
	}

	if(pv_get_avp_name(0, &avp_spec->pvp, &avp_name, &avp_type)!=0) {
		LM_ERR("[%.*s]- invalid AVP definition\n", aname->len, aname->s);
		return -1;
	}

	p = find_sdp_line(body.s, body.s+body.len, sline->s[0]);
	while (p != NULL) {
		if (sdp_locate_line(msg, p, &line) != 0) {
			LM_ERR("sdp_locate_line fail\n");
			return -1;
		}

		if (strncmp(line.s, sline->s, sline->len) == 0) {
			avp_val.s.s = line.s;
			avp_val.s.len = line.len;

			/* skip ending \r\n if exists */
			if (avp_val.s.s[line.len-2] == '\r' && avp_val.s.s[line.len-1] == '\n') {
				/* add_avp() clones to shm and adds 0-terminating char */
				avp_val.s.len -= 2;
			}

			if (add_avp(AVP_VAL_STR | avp_type, avp_name, avp_val) != 0) {
				LM_ERR("Failed to add SDP line avp");
				return -1;
			}

			return 1;
		}

		p = find_sdp_line(line.s + line.len, body.s + body.len, sline->s[0]);
	}

	return -1;
}

static int sdp_get_sess_version(sip_msg_t* msg, str* sess_version, long* sess_version_num)
{
	sdp_session_cell_t* sdp_session;
	int sdp_session_num;

	sdp_session_num = 0;
	for(;;)
	{
		sdp_session = get_sdp_session(msg, sdp_session_num);
		if(!sdp_session) break;
		LM_DBG("sdp_session_num %d sess-version: %.*s\n", sdp_session_num,sdp_session->o_sess_version.len, sdp_session->o_sess_version.s);
		*sess_version = sdp_session->o_sess_version;
		sdp_session_num++;
	}

	LM_DBG("sdp_session_num %d\n", sdp_session_num);

	if ( sdp_session_num > 0 )
	{
		if ( str2slong(sess_version, sess_version_num) != -1 )
		{
			return 1;
		}
	}
	return -1;
}

static int sdp_set_sess_version(sip_msg_t* msg, str* disabled_old_sess_version, long* new_sess_version_num)
{
	str new_sess_version = STR_NULL;
	str old_sess_version = STR_NULL;
	long old_sess_version_num = 0;
	int autoincrement = 0;

	struct lump* anchor = NULL;

	if ( new_sess_version_num == NULL )
	{
		LM_ERR("no *new_sess_version_num\n");
		return -1;
	}
	if ( *new_sess_version_num == -1 )
	{
		autoincrement = 1;
	}

	// get current value and pointer (for lump operations)
	if ( sdp_get_sess_version(msg, &old_sess_version, &old_sess_version_num) != 1 )
	{
		LM_ERR("unable to get current str pointer\n");
		return -1;
	}

	if ( autoincrement == 1 )
	{
		*new_sess_version_num = old_sess_version_num + 1;
		if ( *new_sess_version_num < old_sess_version_num )
		{
			LM_ERR("autoincrement: new(%ld) < old(%ld)\n", *new_sess_version_num, old_sess_version_num);
			return -1;
		}
		LM_DBG("old_sess_version_num: %ld -> *new_sess_version_num %ld\n", old_sess_version_num, *new_sess_version_num);
	}
	LM_DBG("old_sess_version_num: %ld  autoincrement: %d\n", old_sess_version_num,autoincrement);
	LM_DBG("*new_sess_version_num: %ld  autoincrement: %d\n", *new_sess_version_num,autoincrement);

	char *sid;
	char buf[INT2STR_MAX_LEN];
	sid = sint2strbuf(*new_sess_version_num, buf, INT2STR_MAX_LEN, &(new_sess_version.len));
	if ( sid == 0 )
	{
		LM_ERR("sint2strbuf() fail\n");
		return -1;
	}

	if ( autoincrement == 1 && ( new_sess_version.len < old_sess_version.len ) )
	{
		LM_ERR("autoincrement: new(%ld) < old(%.*s)\n", *new_sess_version_num, old_sess_version.len, old_sess_version.s);
		return -1;
	}

	new_sess_version.s = sid;
	new_sess_version.s = pkg_malloc((new_sess_version.len * sizeof(char)));
	if (new_sess_version.s == NULL)
	{
		LM_ERR("Out of pkg memory\n");
		return -1;
	}

	memcpy(new_sess_version.s, sid, new_sess_version.len);

	int offset = 0;
	offset = old_sess_version.s - msg->buf;
	anchor = del_lump(msg, offset, old_sess_version.len, 0);
	if (anchor == NULL)
	{
		LM_ERR("del_lump failed\n");
		pkg_free(new_sess_version.s);
		return -1;
	}

	if (insert_new_lump_after(anchor, new_sess_version.s, new_sess_version.len, 0) == 0)
	{
		LM_ERR("insert_new_lump_after failed\n");
		pkg_free(new_sess_version.s);
		return -1;
	}

	return 1;
}

/**
 *
 */
static int w_sdp_get_line_startswith(sip_msg_t *msg, char *avp, char *pline)
{
	str sline;
	str aname;

	if(fixup_get_svalue(msg, (gparam_t*)pline, &sline)<0) {
		LM_ERR("failed to evaluate start line parameter\n");
		return -1;
	}
	aname.s = avp;
	aname.len = strlen(aname.s);

	return ki_sdp_get_line_startswith(msg, &aname, &sline);
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
	sob->sdp_with_active_media = sdp_with_active_media;
	sob->sdp_with_transport = sdp_with_transport;
	sob->sdp_with_codecs_by_id = sdp_with_codecs_by_id;
	sob->sdp_with_codecs_by_name = sdp_with_codecs_by_name;
	sob->sdp_with_ice = sdp_with_ice;
	sob->sdp_keep_codecs_by_id = sdp_keep_codecs_by_id;
	sob->sdp_keep_codecs_by_name = sdp_keep_codecs_by_name;
	sob->sdp_remove_media = sdp_remove_media;
	sob->sdp_remove_transport = sdp_remove_transport;
	sob->sdp_remove_line_by_prefix = sdp_remove_line_by_prefix;
	sob->sdp_remove_codecs_by_id = sdp_remove_codecs_by_id;
	sob->sdp_remove_codecs_by_name = sdp_remove_codecs_by_name;
	return 0;
}

/**
 *
 */
static int pv_get_sdp(sip_msg_t *msg, pv_param_t *param,
		pv_value_t *res)
{
	sdp_info_t *sdp = NULL;
	str sess_version = STR_NULL;
	long sess_version_num = 0;

	if(msg==NULL || param==NULL)
		return -1;

	if(parse_sdp(msg) < 0) {
		LM_INFO("Unable to parse sdp\n");
		return pv_get_null(msg, param, res);
	}
	sdp = (sdp_info_t*)msg->body;

	if (sdp==NULL) {
		LM_DBG("No SDP\n");
		return pv_get_null(msg, param, res);
	}

	switch(param->pvn.u.isname.name.n)
	{

/* body */
		case 0:
			LM_DBG("param->pvn.u.isname.name.n=0\n");
			return pv_get_strval(msg, param, res, &sdp->raw_sdp);
/* sess_version */
		case 1:
			if ( sdp_get_sess_version(msg, &sess_version, &sess_version_num) == 1 )
			{
				if ( sess_version.len > 0 && sess_version.s != NULL)
				{
					return pv_get_intstrval(msg, param, res, sess_version_num, &sess_version);
				}
			}
			return pv_get_null(msg, param, res);

		default:
			return pv_get_null(msg, param, res);
	}
}

static int pv_set_sdp(sip_msg_t *msg, pv_param_t *param,
		int op, pv_value_t *res)
{
	LM_DBG("res->flags: %d\n", res->flags);
	if ( res->flags & PV_TYPE_INT ) LM_DBG("PV_TYPE_INT: %d\n",PV_TYPE_INT);
	if ( res->flags & PV_VAL_INT ) LM_DBG("PV_VAL_INT: %d\n",PV_VAL_INT);
	if ( res->flags & PV_VAL_STR ) LM_DBG("PV_VAL_STR: %d\n",PV_VAL_STR);

	LM_DBG("param.pvn.u.isname.name.n = %d\n",param->pvn.u.isname.name.n);
	if (param->pvn.u.isname.name.n == 1)
	{
	// sdp(sess_version)
		if ( !(res->flags & PV_VAL_INT) )
		{
			LM_ERR("expected integer\n");
			return -1;
		}
		LM_DBG("do $sdp(sess_version) = %ld\n", res->ri);
		return sdp_set_sess_version(msg, NULL, &res->ri);
	}
	else
	{
		LM_ERR("unknown PV\n");
		return -1;
	}
	return -1;
}


/**
 *
 */
static int pv_parse_sdp_name(pv_spec_p sp, str *in)
{
	if(sp==NULL || in==NULL || in->len<=0)
		return -1;

	switch(in->len)
	{
		case 4:
			if(strncmp(in->s, "body", 4)==0)
				sp->pvp.pvn.u.isname.name.n = 0;
			else goto error;
		break;
		case 12:
			if(strncmp(in->s, "sess_version", 12)==0)
				sp->pvp.pvn.u.isname.name.n = 1;
			else goto error;
		break;
		default:
			goto error;
	}
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 0;

	return 0;

error:
	LM_ERR("unknown PV sdp name %.*s\n", in->len, in->s);
	return -1;
}

/**
 *
 */
/* clang-format off */
static sr_kemi_t sr_kemi_sdpops_exports[] = {
	{ str_init("sdpops"), str_init("remove_codecs_by_name"),
		SR_KEMIP_INT, sdp_remove_codecs_by_name,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("remove_codecs_by_id"),
		SR_KEMIP_INT, sdp_remove_codecs_by_id,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("keep_codecs_by_name"),
		SR_KEMIP_INT, sdp_keep_codecs_by_name,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("keep_codecs_by_id"),
		SR_KEMIP_INT, sdp_keep_codecs_by_id,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("remove_line_by_prefix"),
		SR_KEMIP_INT, sdp_remove_line_by_prefix,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("remove_media"),
		SR_KEMIP_INT, sdp_remove_media,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("sdp_content"),
		SR_KEMIP_INT, ki_sdp_content,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("sdp_content_flags"),
		SR_KEMIP_INT, ki_sdp_content_flags,
		{ SR_KEMIP_INT, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("sdp_with_ice"),
		SR_KEMIP_INT, sdp_with_ice,
		{ SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("sdp_with_media"),
		SR_KEMIP_INT, sdp_with_media,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("sdp_with_active_media"),
		SR_KEMIP_INT, sdp_with_active_media,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("sdp_with_codecs_by_id"),
		SR_KEMIP_INT, sdp_with_codecs_by_id,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("sdp_with_codecs_by_name"),
		SR_KEMIP_INT, sdp_with_codecs_by_name,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("sdp_get"),
		SR_KEMIP_INT, ki_sdp_get,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("sdp_transport"),
		SR_KEMIP_INT, ki_sdp_transport,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("sdp_with_transport"),
		SR_KEMIP_INT, ki_sdp_with_transport,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("sdp_with_transport_like"),
		SR_KEMIP_INT, ki_sdp_with_transport_like,
		{ SR_KEMIP_STR, SR_KEMIP_NONE, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("sdp_get_line_startswith"),
		SR_KEMIP_INT, ki_sdp_get_line_startswith,
		{ SR_KEMIP_STR, SR_KEMIP_STR, SR_KEMIP_NONE,
			SR_KEMIP_NONE, SR_KEMIP_NONE, SR_KEMIP_NONE }
	},
	{ str_init("sdpops"), str_init("sdp_print"),
		SR_KEMIP_INT, ki_sdp_print,
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
	sr_kemi_modules_add(sr_kemi_sdpops_exports);
	return 0;
}
