/*
 * $Id$
 *
 * Copyright (C) 2012 Carlos Ruiz Díaz (caruizdiaz.com),
 *                    ConexionGroup (www.conexiongroup.com)
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

#include "../../parser/msg_parser.h"
#include "../../globals.h"

#include <sys/socket.h>

#define FAKED_SIP_MSG_FORMAT "OPTIONS sip:you@kamailio.org SIP/2.0\r\nVia: SIP/2.0/UDP 127.0.0.1\r\nFrom: <%.*s>;tag=%.*s\r\nTo: <%.*s>;tag=%.*s\r\nCall-ID: %.*s\r\nCSeq: 1 OPTIONS\r\nContent-Length: 0\r\n\r\n"

#define FAKED_SIP_MSG_BUF_LEN	1024
char _faked_sip_msg_buf[FAKED_SIP_MSG_BUF_LEN];

static struct sip_msg _faked_msg;

int faked_msg_init_with_dlg_info(str *callid, str *from_uri, str *from_tag, str *to_uri, str *to_tag, struct sip_msg **msg) {
	memset(_faked_sip_msg_buf, 0, FAKED_SIP_MSG_BUF_LEN);

	sprintf(_faked_sip_msg_buf, FAKED_SIP_MSG_FORMAT,
				from_uri->len, from_uri->s, from_tag->len, from_tag->s,
				to_uri->len, to_uri->s, to_tag->len, to_tag->s,
				callid->len, callid->s);
 
	LM_DBG("fake msg:\n%s\n", _faked_sip_msg_buf);

	_faked_msg.buf = _faked_sip_msg_buf;
	_faked_msg.len = strlen(_faked_sip_msg_buf);

	_faked_msg.set_global_address	= default_global_address;
	_faked_msg.set_global_port	= default_global_port;

	if (parse_msg(_faked_msg.buf, _faked_msg.len, &_faked_msg) != 0) {
		LM_ERR("parse_msg failed\n");
		return -1;
	}

	_faked_msg.rcv.proto = PROTO_UDP;
	_faked_msg.rcv.src_port = 5060;
	_faked_msg.rcv.src_ip.u.addr32[0] = 0x7f000001;
	_faked_msg.rcv.src_ip.af = AF_INET;
	_faked_msg.rcv.src_ip.len = 4;
	_faked_msg.rcv.dst_port = 5060;
	_faked_msg.rcv.dst_ip.u.addr32[0] = 0x7f000001;
	_faked_msg.rcv.dst_ip.af = AF_INET;
	_faked_msg.rcv.dst_ip.len = 4;

	*msg	= &_faked_msg;
	return 0;
}
