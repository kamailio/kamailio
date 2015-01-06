/*
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 */


#ifndef _UAC_AUTH_H_
#define _UAC_AUTH_H_

#include "../../parser/msg_parser.h"

struct uac_credential {
	str realm;
	str user;
	str passwd;
	struct uac_credential *next;
};

struct authenticate_body {
	int flags;
	str realm;
	str domain;
	str nonce;
	str opaque;
	str qop;
	str *nc;
	str *cnonce;
};

#define AUTHENTICATE_MD5         (1<<0)
#define AUTHENTICATE_MD5SESS     (1<<1)
#define AUTHENTICATE_STALE       (1<<2)
#define QOP_AUTH                 (1<<3)
#define QOP_AUTH_INT             (1<<4)

#define HASHLEN 16
typedef char HASH[HASHLEN];

#define HASHHEXLEN 32
typedef char HASHHEX[HASHHEXLEN+1];

int has_credentials(void);

int add_credential( unsigned int type, void *val);

void destroy_credentials(void);

struct hdr_field *get_autenticate_hdr(struct sip_msg *rpl, int rpl_code);

int uac_auth( struct sip_msg *msg);

void do_uac_auth(str *method, str *uri,
		struct uac_credential *crd,
		struct authenticate_body *auth,
		HASHHEX response);

#endif
