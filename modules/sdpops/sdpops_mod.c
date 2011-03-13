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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mod_fix.h"
#include "../../pvar.h"
#include "../../trim.h"
#include "../../parser/sdp/sdp.h"
#include "../../data_lump.h"


MODULE_VERSION

static int w_sdp_remove_codecs_by_id(sip_msg_t* msg, char* codecs, char *bar);
static int w_sdp_with_media(sip_msg_t* msg, char* media, char *bar);
static int w_sdp_print(sip_msg_t* msg, char* level, char *bar);

static int mod_init(void);

static cmd_export_t cmds[] = {
	{"sdp_remove_codecs_by_id",    (cmd_function)w_sdp_remove_codecs_by_id,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_with_media",             (cmd_function)w_sdp_with_media,
		1, fixup_spve_null,  0, ANY_ROUTE},
	{"sdp_print",                  (cmd_function)w_sdp_print,
		1, fixup_igp_null,  0, ANY_ROUTE},
	{0, 0, 0, 0, 0}
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
int str_find_token(str *text, str *result, char delim)
{
	int i;
	if(text==NULL || result==NULL)
		return -1;
	if(text->s[0] == delim)
	{
		 text->s += 1;
		 text->len -= 1;
	}
	trim_leading(text);
	result->s = text->s;
	result->len = 0;
	for (i=0; i<text->len; i++)
	{
		if(result->s[i]==delim || result->s[i]=='\0'
				|| result->s[i]=='\r' || result->s[i]=='\n')
			return 0;
		result->len++;
	}
	return 0;
}


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

	LM_ERR("attempting to remove codecs from sdp: [%.*s]\n",
			codecs->len, codecs->s);

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
 * @brief check 'media' matches the value of any 'm=value ...' lines
 * @return -1 - error; 0 - not found; 1 - found
 */
static int sdp_with_media(sip_msg_t *msg, str *media)
{
	sdp_info_t *sdp = NULL;
	int sdp_session_num;
	int sdp_stream_num;
	sdp_session_cell_t* sdp_session;
	sdp_stream_cell_t* sdp_stream;

	if(parse_sdp(msg) < 0) {
		LM_ERR("Unable to parse sdp\n");
		return -1;
	}

	LM_ERR("attempting to search for media type: [%.*s]\n",
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
