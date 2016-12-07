/*
 *  mangler module
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */

/* TODO :decode2format unpleasant */

#ifndef CONTACT_OPS_H
#define CONTACT_OPS_H

/* if you want to parse all contacts not just de first one */



#include "../../parser/msg_parser.h"	/* struct sip_msg */
#include "common.h"

#define ENCODE_ALL_CONTACTS 1
#define DECODE_ALL_CONTACTS 1

#define DEFAULT_SEPARATOR "*"


extern char *contact_flds_separator;


struct uri_format
{
	str username;
	str password;
	str ip;
	str port;
	str protocol;
	str rcv_ip;
	str rcv_port;
	str rcv_proto;
	str transport; /* used only when encoding */
	str rest; /* used only when encoding */
	int first;
	int second;
};

typedef struct uri_format contact_fields_t;


int encode_contact (struct sip_msg *msg, char *encoding_prefix,char *public_ip);
int decode_contact (struct sip_msg *msg, char *unused1,char *unused2);
int decode_contact_header (struct sip_msg *msg, char *unused1,char *unused2);
	
int encode2format (struct sip_msg* msg, str* uri, struct uri_format *format);
int decode2format (str* uri, char separator, struct uri_format *format);

int encode_uri (struct sip_msg* msg, str* uri, char *encoding_prefix,
				char *public_ip,char separator, str * result);
int decode_uri (str* uri, char separator, str * result, str* dst_uri);





#endif
