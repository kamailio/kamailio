/*
 * $Id$
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 * History:
 * --------
 *  2006-08-15  initial version (anca)
 */

/*! \file
 * \brief Kamailio presence module :: Utility functions
 * \ref utils_func.c
 * \ingroup presence 
 */



#ifndef UTILS_FUNC_H
#define UTILS_FUNC_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_event.h"

#define LCONTACT_BUF_SIZE 1024
#define BAD_EVENT_CODE 489


#define EVENT_DIALOG_SLA(ev) \
	((ev)->type == EVENT_DIALOG \
		&& ((ev)->params.hooks.event_dialog.sla \
			|| (ev)->params.hooks.event_dialog.ma))


static inline int uandd_to_uri(str user,  str domain, str *out)
{
	int size;

	if(out==0)
		return -1;

	size = user.len + domain.len+7;
	out->s = (char*)pkg_malloc(size);

	if(out->s == NULL)
	{
		LM_ERR("no more memory\n");
		return -1;
	}
	strcpy(out->s,"sip:");
	out->len = 4;
	if(user.s!=NULL && user.len>0)
	{
		memcpy(out->s+out->len, user.s, user.len);
		out->len += user.len;
		out->s[out->len++] = '@';
	}
	memcpy(out->s + out->len, domain.s, domain.len);
	out->len += domain.len;
	out->s[out->len] = '\0';
	
	return 0;
}

static inline str* get_local_contact(struct sip_msg* msg)
{
	str ip;
	char* proto;
	int port;
	int len;
	str* contact;
	int plen;

	contact= (str*)pkg_malloc(sizeof(str));
	if(contact== NULL)
	{
		LM_ERR("No more memory\n");
		return NULL;
	}
	contact->s= (char*)pkg_malloc(LCONTACT_BUF_SIZE);
	if(contact->s== NULL)
	{
		LM_ERR("No more memory\n");
		pkg_free(contact);
		return NULL;
	}

	memset(contact->s, 0, LCONTACT_BUF_SIZE);
	contact->len= 0;
	
	plen = 3;
	if(msg->rcv.proto== PROTO_NONE || msg->rcv.proto==PROTO_UDP)
		proto= "udp";
	else
	if(msg->rcv.proto== PROTO_TLS )
			proto= "tls";
	else	
	if(msg->rcv.proto== PROTO_TCP)
		proto= "tcp";
	else	
	if(msg->rcv.proto== PROTO_SCTP) {
		proto= "sctp";
		plen = 4;
	}
	else
	{
		LM_ERR("unsupported proto\n");
		pkg_free(contact->s);
		pkg_free(contact);
		return NULL;
	}	
	
	ip.s= ip_addr2a(&msg->rcv.dst_ip);
	if(ip.s== NULL)
	{
		LM_ERR("transforming ip_addr to ascii\n");
		pkg_free(contact->s);
		pkg_free(contact);
		return NULL;
	}
	ip.len= strlen(ip.s);
	port = msg->rcv.dst_port;

	if(strncmp(ip.s, "sip:", 4)!=0)
	{
		strncpy(contact->s, "sip:", 4);
		contact->len+= 4;
	}	
	strncpy(contact->s+contact->len, ip.s, ip.len);
	contact->len += ip.len;
	if(contact->len> LCONTACT_BUF_SIZE - 21)
	{
		LM_ERR("buffer overflow\n");
		pkg_free(contact->s);
		pkg_free(contact);
		return NULL;

	}	
	len= sprintf(contact->s+contact->len, ":%d;transport=" , port);
	if(len< 0)
	{
		LM_ERR("unsuccessful sprintf\n");
		pkg_free(contact->s);
		pkg_free(contact);
		return NULL;
	}	
	contact->len+= len;
	strncpy(contact->s+ contact->len, proto, plen);
	contact->len += plen;
	
	return contact;
	
}

//str* int_to_str(long int n);

int a_to_i (char *s,int len);

void to64frombits(unsigned char *out, const unsigned char *in, int inlen);

int send_error_reply(struct sip_msg* msg, int reply_code, str reply_str);

#endif

