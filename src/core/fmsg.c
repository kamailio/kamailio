/**
 * Copyright (C) 2009 Daniel-Constantin Mierla (asipto.com)
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
* \brief Faked message handling
*/

#include "dprint.h"
#include "globals.h"
#include "dset.h"

#include "fmsg.h"

#define FAKED_SIP_MSG "OPTIONS sip:you@kamailio.org SIP/2.0\r\nVia: SIP/2.0/UDP 127.0.0.1\r\nFrom: <sip:you@kamailio.org>;tag=123\r\nTo: <sip:you@kamailio.org>\r\nCall-ID: 123\r\nCSeq: 1 OPTIONS\r\nContent-Length: 0\r\n\r\n"
#define FAKED_SIP_MSG_LEN (sizeof(FAKED_SIP_MSG)-1)
static char _faked_sip_buf[BUF_SIZE];
static int _faked_sip_buf_init = 0;
static sip_msg_t _faked_msg;
static unsigned int _faked_msg_no = 0;

static unsigned int faked_msg_get_next_id(void)
{
	_faked_msg_no += ((_faked_msg_no+1)==0)?2:1;
	return _faked_msg_no;
}

static void faked_msg_buf_init(void)
{
	if(_faked_sip_buf_init!=0) {
		return;
	}
	memcpy(_faked_sip_buf, FAKED_SIP_MSG, FAKED_SIP_MSG_LEN);
	_faked_sip_buf[FAKED_SIP_MSG_LEN] = '\0';
	_faked_sip_buf_init = 1;
}

static int faked_msg_init_new(sip_msg_t *fmsg)
{
	faked_msg_buf_init();

	/* init faked sip msg */
	memset(fmsg, 0, sizeof(sip_msg_t));

	fmsg->buf=_faked_sip_buf;
	fmsg->len=FAKED_SIP_MSG_LEN;

	fmsg->set_global_address=default_global_address;
	fmsg->set_global_port=default_global_port;

	if (parse_msg(fmsg->buf, fmsg->len, fmsg)!=0) {
		LM_ERR("parse faked msg failed\n");
		return -1;
	}

	fmsg->rcv.proto = PROTO_UDP;
	fmsg->rcv.src_port = 5060;
	fmsg->rcv.src_ip.u.addr32[0] = 0x7f000001;
	fmsg->rcv.src_ip.af = AF_INET;
	fmsg->rcv.src_ip.len = 4;
	fmsg->rcv.dst_port = 5060;
	fmsg->rcv.dst_ip.u.addr32[0] = 0x7f000001;
	fmsg->rcv.dst_ip.af = AF_INET;
	fmsg->rcv.dst_ip.len = 4;

	return 0;
}

int faked_msg_init(void)
{
	if(_faked_msg_no>0) {
		return 0;
	}
	return faked_msg_init_new(&_faked_msg);
}

static inline sip_msg_t* faked_msg_build_next(int mode)
{
	_faked_msg.id = faked_msg_get_next_id();
	_faked_msg.pid = my_pid();
	memset(&_faked_msg.tval, 0, sizeof(struct timeval));
	if(mode) clear_branches();
	return &_faked_msg;
}

sip_msg_t* faked_msg_next(void)
{
	return faked_msg_build_next(0);
}

sip_msg_t* faked_msg_next_clear(void)
{
	return faked_msg_build_next(1);
}

sip_msg_t* faked_msg_get_next(void)
{
	if(faked_msg_init()<0) {
		LM_ERR("fake msg not initialized\n");
	}
	return faked_msg_next();
}

sip_msg_t* faked_msg_get_next_clear(void)
{
	if(faked_msg_init()<0) {
		LM_ERR("fake msg not initialized\n");
	}
	return faked_msg_next_clear();
}

int faked_msg_get_new(sip_msg_t *fmsg)
{
	clear_branches();
	if(faked_msg_init_new(fmsg)<0) {
		return -1;
	}
	fmsg->id = faked_msg_get_next_id();
	fmsg->pid = my_pid();
	memset(&fmsg->tval, 0, sizeof(struct timeval));

	return 0;
}

int faked_msg_match(sip_msg_t *tmsg)
{
	return ( tmsg == &_faked_msg ) ? 1 : 0;
}
