/*
 * Copyright (C) 2015 ng-voice GmbH, carsten@ng-voice.com
 * File is based on cnxcc: msg_faker, written by Carlos Ruiz Díaz (caruizdiaz.com)
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

#include "../../core/parser/msg_parser.h"
#include "../../core/globals.h"
#include "../cdp/cdp_load.h"
#include "../cdp_avp/cdp_avp_mod.h"
#include "ocs_avp_helper.h"

#include "msg_faker.h"

#include <sys/socket.h>

#define FAKED_SIP_SESSION_FORMAT                                           \
	"%.*s %.*s SIP/2.0\r\nVia: SIP/2.0/UDP 127.0.0.1\r\nFrom: "            \
	"%.*s%.*s\r\nTo: %.*s;tag=xyz\r\nCall-ID: %.*s\r\nCSeq: 1 "            \
	"%.*s\r\nContent-Length: 0\r\nP-Requested-Units: %i\r\nP-Used-Units: " \
	"%i\r\nP-Access-Network-Info: %.*s\r\nP-Service-Identifier: %i\r\n\r\n"

#define FAKED_SIP_SESSION_BUF_LEN 1024
char _faked_sip_session_buf[FAKED_SIP_SESSION_BUF_LEN];

str CC_INVITE = {"INVITE", 6};
str CC_UPDATE = {"UPDATE", 6};
str CC_BYE = {"BYE", 3};

#define RO_CC_START 1
#define RO_CC_INTERIM 2
#define RO_CC_STOP 3

static struct sip_msg _faked_msg;

int getMethod(AAAMessage *msg, str **method)
{
	str s;
	s = get_avp(msg, AVP_IMS_CCR_Type, 0, __FUNCTION__);
	if(!s.s)
		return -1;
	switch(get_4bytes(s.s)) {
		case RO_CC_START:
			*method = &CC_INVITE;
			break;
		case RO_CC_INTERIM:
			*method = &CC_UPDATE;
			break;
		case RO_CC_STOP:
			*method = &CC_BYE;
			break;
		default:
			LM_ERR("Invalid CCR-Type\n");
			return -1;
			break;
	}

	return 1;
}

int faked_aaa_msg(AAAMessage *ccr, struct sip_msg **msg)
{
	int type, size;
	str *method;
	str prefix = {0, 0};
	str from_uri = getSubscriptionId1(ccr, &type);
	str to_uri = getCalledParty(ccr);
	str callid = getSession(ccr);
	str access_network_info = getAccessNetwork(ccr);
	int used_units = 0;
	int service = 0;
	int group = 0;
	int requested_units = getUnits(ccr, &used_units, &service, &group);

	if(getMethod(ccr, &method) < 0) {
		LM_ERR("Failed to get CCR-Type\n");
		return -1;
	}

	if(type != AVP_Subscription_Id_Type_SIP_URI) {
		prefix.s = "tel:";
		prefix.len = 4;
	}


	memset(_faked_sip_session_buf, 0, FAKED_SIP_SESSION_BUF_LEN);
	memset(&_faked_msg, 0, sizeof(struct sip_msg));

	size = snprintf(_faked_sip_session_buf, FAKED_SIP_SESSION_BUF_LEN,
			FAKED_SIP_SESSION_FORMAT,
			/* First-Line METHOD sip:.... */
			method->len, method->s, to_uri.len, to_uri.s,
			/* Prefix */
			prefix.len, prefix.s,
			/* From-Header */
			from_uri.len, from_uri.s,
			/* To-Header */
			to_uri.len, to_uri.s,
			/* Call-ID */
			callid.len, callid.s,
			/* CSeq (Method) */
			method->len, method->s,
			/* Requested / Used Units */
			requested_units, used_units,
			/* P-Access-Network-Info */
			access_network_info.len, access_network_info.s,
			/* P-Access-Network-Info */
			service);

	LM_DBG("fake msg:\n%s\n", _faked_sip_session_buf);

	_faked_msg.buf = _faked_sip_session_buf;
	_faked_msg.len = size;

	_faked_msg.set_global_address = default_global_address;
	_faked_msg.set_global_port = default_global_port;

	if(parse_msg(_faked_msg.buf, _faked_msg.len, &_faked_msg) != 0) {
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

	*msg = &_faked_msg;
	return 0;
}
