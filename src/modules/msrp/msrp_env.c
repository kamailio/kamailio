/**
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../globals.h"
#include "../../ut.h"
#include "../../dset.h"

#include "msrp_parser.h"
#include "msrp_netio.h"
#include "msrp_env.h"

extern int msrp_param_sipmsg;

static msrp_env_t _msrp_env = {0};

/**
 *
 */
msrp_env_t *msrp_get_env(void)
{
	return &_msrp_env;
}

/**
 *
 */
void msrp_reset_env(void)
{
	memset(&_msrp_env, 0, sizeof(struct msrp_env));
}

/**
 *
 */
msrp_frame_t *msrp_get_current_frame(void)
{
	return _msrp_env.msrp;
}

/**
 *
 */
int msrp_set_current_frame(msrp_frame_t *mf)
{
	_msrp_env.msrp = mf;
	init_dst_from_rcv(&_msrp_env.srcinfo, mf->tcpinfo->rcv);
	_msrp_env.envflags |= MSRP_ENV_SRCINFO;
	return 0;
}


/**
 *
 */
int msrp_env_set_dstinfo(msrp_frame_t *mf, str *addr, str *fsock, int flags)
{
	struct socket_info *si = NULL;
	snd_flags_t sflags = {0};
	
	if(fsock!=NULL && fsock->len>0)
	{
		si = msrp_get_local_socket(fsock);
		if(si==NULL)
		{
			LM_DBG("local socket not found [%.*s] - trying to continue\n",
					fsock->len, fsock->s);
		}
	}
	sflags.f = flags;
	if(si==NULL)
	{
		sflags.f &= ~SND_F_FORCE_SOCKET;
	} else {
		sflags.f |= SND_F_FORCE_SOCKET;
	}

	sflags.f |=  _msrp_env.sndflags;
	memset(&_msrp_env.dstinfo, 0, sizeof(struct dest_info));
	if(msrp_uri_to_dstinfo(NULL, &_msrp_env.dstinfo, si, sflags, addr)==NULL)
	{
		LM_ERR("failed to set destination address [%.*s]\n",
				addr->len, addr->s);
		return -1;
	}
	_msrp_env.envflags |= MSRP_ENV_DSTINFO;
	return 0;
}

/**
 *
 */
int msrp_env_set_sndflags(msrp_frame_t *mf, int flags)
{
	_msrp_env.sndflags |= (flags & (~SND_F_FORCE_SOCKET));
	if(_msrp_env.envflags & MSRP_ENV_DSTINFO)
	{
		_msrp_env.dstinfo.send_flags.f |= _msrp_env.sndflags;
	}
	return 0;
}

/**
 *
 */
int msrp_env_set_rplflags(msrp_frame_t *mf, int flags)
{
	_msrp_env.rplflags |= (flags & (~SND_F_FORCE_SOCKET));
	if(_msrp_env.envflags & MSRP_ENV_SRCINFO)
	{
		_msrp_env.srcinfo.send_flags.f |= _msrp_env.rplflags;
	}
	return 0;
}


/**
 *
 */
#define MSRP_FAKED_SIPMSG_START "MSRP sip:a@127.0.0.1 SIP/2.0\r\nVia: SIP/2.0/UDP 127.0.0.1:9;branch=z9hG4bKa\r\nFrom: <b@127.0.0.1>;tag=a\r\nTo: <a@127.0.0.1>\r\nCall-ID: a\r\nCSeq: 1 MSRP\r\nContent-Length: 0\r\nMSRP-First-Line: "
#define MSRP_FAKED_SIPMSG_EXTRA 11240
#define MSRP_FAKED_SIPMSG_SIZE (sizeof(MSRP_FAKED_SIPMSG_START)+MSRP_FAKED_SIPMSG_EXTRA)
static char _msrp_faked_sipmsg_buf[MSRP_FAKED_SIPMSG_SIZE];
static sip_msg_t _msrp_faked_sipmsg;
static unsigned int _msrp_faked_sipmsg_no = 0;

sip_msg_t *msrp_fake_sipmsg(msrp_frame_t *mf)
{
	int len;
	if(msrp_param_sipmsg==0)
		return NULL;
	if(mf->buf.len >= MSRP_FAKED_SIPMSG_EXTRA-1)
		return NULL;

	len = sizeof(MSRP_FAKED_SIPMSG_START)-1;
	memcpy(_msrp_faked_sipmsg_buf, MSRP_FAKED_SIPMSG_START, len);
	memcpy(_msrp_faked_sipmsg_buf + len, mf->buf.s,
			mf->fline.buf.len + mf->hbody.len);
	len += mf->fline.buf.len + mf->hbody.len;
	memcpy(_msrp_faked_sipmsg_buf + len, "\r\n", 2);
	len += 2;
	_msrp_faked_sipmsg_buf[len] = '\0';

	memset(&_msrp_faked_sipmsg, 0, sizeof(sip_msg_t));

	_msrp_faked_sipmsg.buf=_msrp_faked_sipmsg_buf;
	_msrp_faked_sipmsg.len=len;

	_msrp_faked_sipmsg.set_global_address=default_global_address;
	_msrp_faked_sipmsg.set_global_port=default_global_port;

	if (parse_msg(_msrp_faked_sipmsg.buf, _msrp_faked_sipmsg.len,
				&_msrp_faked_sipmsg)!=0)
	{
		LM_ERR("parse_msg failed\n");
		return NULL;
	}

	_msrp_faked_sipmsg.id = 1 + _msrp_faked_sipmsg_no++;
	_msrp_faked_sipmsg.pid = my_pid();
	clear_branches();
	return &_msrp_faked_sipmsg;
}
