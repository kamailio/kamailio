/**
 * $Id$
 *
 * Copyright (C) 2013 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file
 * \brief Kamailio gzcompress :: Module interface
 * \ingroup gzcompress
 * Module: \ref gzcompress
 */

/*! \defgroup gzcompress Kamailio :: compress-decompress message body with zlib
 *
 * This module compresses/decompresses SIP message body using zlib.
 * The script interpreter gets the SIP messages decoded, so all
 * existing functionality is preserved.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <zlib.h>

#include "../../sr_module.h"
#include "../../events.h"
#include "../../dprint.h"
#include "../../tcp_options.h"
#include "../../ut.h"
#include "../../forward.h"
#include "../../msg_translator.h"
#include "../../data_lump.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_from.h"

#include "../../modules/sanity/api.h"

MODULE_VERSION

/** local functions */
int gzc_msg_received(void *data);
int gzc_msg_sent(void *data);

/** module parameters */
static str _gzc_hdr_name = str_init("Content-Encoding");
static str _gzc_hdr_value = str_init("deflate");

static int _gzc_sanity_checks = 0;
static sanity_api_t scb = {0};

/** module functions */
static int mod_init(void);

static param_export_t params[]={
	{"header_name",		PARAM_STR, &_gzc_hdr_name},
	{"header_value",	PARAM_STR, &_gzc_hdr_value},
	{"sanity_checks",	PARAM_INT, &_gzc_sanity_checks},
	{0,0,0}
};


/** module exports */
struct module_exports exports= {
	"gzcompress",
	DEFAULT_DLFLAGS, /* dlopen flags */
	0,
	params,
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	mod_init,   /* module initialization function */
	0,
	0,
	0           /* per-child init function */
};

/**
 * init module function
 */
static int mod_init(void)
{
	if(_gzc_sanity_checks!=0)
	{
		if(sanity_load_api(&scb)<0)
		{
			LM_ERR("cannot bind to sanity module\n");
			goto error;
		}
	}
	
	sr_event_register_cb(SREV_NET_DATA_IN, gzc_msg_received);
	sr_event_register_cb(SREV_NET_DATA_OUT, gzc_msg_sent);
#ifdef USE_TCP
	tcp_set_clone_rcvbuf(1);
#endif
	return 0;
error:
	return -1;
}

/**
 *
 */
int gzc_prepare_msg(sip_msg_t *msg)
{
	if (parse_msg(msg->buf, msg->len, msg)!=0)
	{
		LM_DBG("outbuf buffer parsing failed!");
		return 1;
	}

	if(msg->first_line.type==SIP_REQUEST)
	{
		if(!IS_SIP(msg) && !IS_HTTP(msg))
		{
			LM_DBG("non sip or http request\n");
			return 1;
		}
	} else if(msg->first_line.type==SIP_REPLY) {
		if(!IS_SIP_REPLY(msg) && !IS_HTTP_REPLY(msg))
		{
			LM_DBG("non sip or http response\n");
			return 1;
		}
	} else {
		LM_DBG("non sip or http message\n");
		return 1;
	}

	if (parse_headers(msg, HDR_EOH_F, 0)==-1)
	{
		LM_DBG("parsing headers failed");
		return 2;
	}

	return 0;
}

/**
 *
 */
int gzc_skip_msg(sip_msg_t *msg)
{
	hdr_field_t *h;
	char *sp;

	if(_gzc_hdr_name.len<=0 || _gzc_hdr_value.len<=0)
		return -1;
	h = get_hdr_by_name(msg, _gzc_hdr_name.s, _gzc_hdr_name.len);
	if(h==NULL)
		return 1;
	
	for (sp = h->body.s; sp <= h->body.s + h->body.len - _gzc_hdr_value.len;
			sp++)
	{
        if (*sp == *_gzc_hdr_value.s
        		&& memcmp(sp, _gzc_hdr_value.s, _gzc_hdr_value.len)==0) {
        	/* found */
            return 0;
        }
    }

	return 2;
}

/**
 *
 */
char* gzc_msg_update(sip_msg_t *msg, unsigned int *olen)
{
	struct dest_info dst;

	init_dest_info(&dst);
	dst.proto = PROTO_UDP;
	return build_req_buf_from_sip_req(msg,
			olen, &dst, BUILD_NO_LOCAL_VIA|BUILD_NO_VIA1_UPDATE);
}

/**
 *
 */
int gzc_set_msg_body(sip_msg_t *msg, str *obody, str *nbody)
{
	struct lump *anchor;
	char* buf;

	/* none should be here - just for safety */
	del_nonshm_lump( &(msg->body_lumps) );
	msg->body_lumps = NULL;

	if(del_lump(msg, obody->s - msg->buf, obody->len, 0) == 0)
	{
		LM_ERR("cannot delete existing body");
		return -1;
	}

	anchor = anchor_lump(msg, obody->s - msg->buf, 0, 0);

	if (anchor == 0)
	{
		LM_ERR("failed to get body anchor\n");
		return -1;
	} 

	buf=pkg_malloc(nbody->len * sizeof(char));
	if (buf==0)
	{
		LM_ERR("out of pkg memory\n");
		return -1;
	}
	memcpy(buf, nbody->s, nbody->len);
	if (insert_new_lump_after(anchor, buf, nbody->len, 0) == 0)
	{
		LM_ERR("failed to insert body lump\n");
		pkg_free(buf);
		return -1;
	}
	return 0;
}

/* local buffer to use for compressing/decompressing */
static char _gzc_local_buffer[BUF_SIZE];

/**
 *
 */
int gzc_msg_received(void *data)
{
	sip_msg_t msg;
	str *obuf;
	char *nbuf = NULL;
	str obody;
	str nbody;
	unsigned long olen;
	unsigned long nlen;
	int ret;

	obuf = (str*)data;
	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = obuf->s;
	msg.len = obuf->len;

	if(gzc_prepare_msg(&msg)!=0)
	{
		goto done;
	}

	if(gzc_skip_msg(&msg))
	{
		goto done;
	}

	if(msg.first_line.type==SIP_REQUEST)
	{
		if(_gzc_sanity_checks!=0)
		{
			if(scb.check_defaults(&msg)<1)
			{
				LM_ERR("sanity checks failed\n");
				goto done;
			}
		}
	}

	obody.s = get_body(&msg);
	if (obody.s==NULL)
	{
		LM_DBG("no body for this SIP message\n");
		goto done;
	}
	obody.len = msg.buf + msg.len - obody.s;

	/* decompress the body */
	nbody.s = _gzc_local_buffer;
	nlen = BUF_SIZE;
	olen = obody.len;
	ret = uncompress((unsigned char*)nbody.s, &nlen,
			(unsigned char*)obody.s, olen);
	if(ret!=Z_OK)
	{
		LM_ERR("error decompressing body (%d)\n", ret);
		goto done;
	}
	nbody.len = (int)nlen;
	LM_DBG("body decompressed - old size: %d - new size: %d\n",
			obody.len, nbody.len);

	if(gzc_set_msg_body(&msg, &obody, &nbody)<0)
	{
		LM_ERR("error replacing body\n");
		goto done;
	}

	nbuf = gzc_msg_update(&msg, (unsigned int*)&obuf->len);

	if(obuf->len>=BUF_SIZE)
	{
		LM_ERR("new buffer overflow (%d)\n", obuf->len);
		pkg_free(nbuf);
		return -1;
	}
	memcpy(obuf->s, nbuf, obuf->len);
	obuf->s[obuf->len] = '\0';

done:
	if(nbuf!=NULL)
		pkg_free(nbuf);
	free_sip_msg(&msg);
	return 0;
}

/**
 *
 */
int gzc_msg_sent(void *data)
{
	sip_msg_t msg;
	str *obuf;
	str obody;
	str nbody;
	unsigned long olen;
	unsigned long nlen;
	int ret;

	obuf = (str*)data;
	memset(&msg, 0, sizeof(sip_msg_t));
	msg.buf = obuf->s;
	msg.len = obuf->len;

	if(gzc_prepare_msg(&msg)!=0)
	{
		goto done;
	}

	if(gzc_skip_msg(&msg))
	{
		goto done;
	}

	obody.s = get_body(&msg);
	if (obody.s==NULL)
	{
		LM_DBG("no body for this SIP message\n");
		goto done;
	}
	obody.len = msg.buf + msg.len - obody.s;

	/* decompress the body */
	nbody.s = _gzc_local_buffer;
	nlen = BUF_SIZE;
	olen = obody.len;
	ret = compress((unsigned char*)nbody.s, &nlen,
			(unsigned char*)obody.s, olen);
	if(ret!=Z_OK)
	{
		LM_ERR("error compressing body (%d)\n", ret);
		goto done;
	}
	nbody.len = (int)nlen;
	LM_DBG("body compressed - old size: %d - new size: %d\n",
			obody.len, nbody.len);

	if(gzc_set_msg_body(&msg, &obody, &nbody)<0)
	{
		LM_ERR("error replacing body\n");
		goto done;
	}

	obuf->s = gzc_msg_update(&msg, (unsigned int*)&obuf->len);

done:
	free_sip_msg(&msg);
	return 0;
}

