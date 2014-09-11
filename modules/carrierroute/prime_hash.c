/*
 * $Id$
 *
 * Copyright (C) 2007 1&1 Internet AG
 *
 * This file is part of SIP-router, a free SIP server.
 *
 * SIP-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*!
 * \file
 * \brief
 * Functions for determinung a pseudo random number over a message's
 * header field, based on CRC32 or a prime number algorithm.
 */

#include "../../sr_module.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_from.h"
#include "../../crc.h"

#include <ctype.h>
#include <stdio.h> /* for snprintf */
#include <stdlib.h> /* for rand */

#include "prime_hash.h"

#define CR_RANDBUF_S 20
static char cr_randbuf[CR_RANDBUF_S];

static int determine_source(struct sip_msg *msg, enum hash_source source,
                            str *source_string);
static int validate_msg(struct sip_msg * msg);
static int determine_call_id (struct sip_msg *msg, str *source_string);
static int determine_fromto_uri (struct to_body *fromto, str *source_string);
static int determine_fromto_user (struct to_body *fromto, str *source_string);
static int determine_fromrand(str* source_string);
static int first_token (str *source_string);


int hash_func (struct sip_msg * msg,
                         enum hash_source source, int denominator) {
	int ret;
	unsigned int hash;
	str source_string;

	if(determine_source (msg, source, &source_string) == -1) {
		return -1;
	}

	crc32_uint(&source_string, &hash);

	ret = hash % denominator;
	LM_DBG("hash: %u %% %i = %i\n", hash, denominator, ret);
	return ret;
}

static int determine_source (struct sip_msg *msg, enum hash_source source,
                             str *source_string) {
	source_string->s = NULL;
	source_string->len = 0;

	if(validate_msg(msg) < 0) {
		return -1;
	}

	switch (source) {
			case shs_call_id:
			return determine_call_id (msg, source_string);
			case shs_from_uri:
			return determine_fromto_uri (get_from(msg), source_string);
			case shs_from_user:
			return determine_fromto_user (get_from(msg), source_string);
			case shs_to_uri:
			return determine_fromto_uri (get_to(msg), source_string);
			case shs_to_user:
			return determine_fromto_user (get_to(msg), source_string);
			case shs_rand:
			return determine_fromrand(source_string); /* msg is not needed */
			default:
			LM_ERR("unknown hash source %i.\n",
			     (int) source);
			return -1;
	}
}

static int validate_msg(struct sip_msg * msg) {
	if(!msg->callid && ((parse_headers(msg, HDR_CALLID_F, 0) == -1) || !msg->callid)) {
		LM_ERR("Message has no Call-ID header\n");
		return -1;
	}
	if(!msg->to && ((parse_headers(msg, HDR_TO_F, 0) == -1) || !msg->to)) {
		LM_ERR("Message has no To header\n");
		return -1;
	}
	if(!msg->from && ((parse_headers(msg, HDR_FROM_F, 0) == -1) || !msg->from)) {
		LM_ERR("Message has no From header\n");
		return -1;
	}
	//TODO it would make more sense to do the parsing just if its needed
	//     but parse_from_header is smart enough, so its probably not a huge problem
	if (parse_from_header(msg) < 0) {
		LM_ERR("Error while parsing From header field\n");
		return -1;
	}
	return 0;
}

static int determine_call_id (struct sip_msg *msg, str *source_string) {
	source_string->s = msg->callid->body.s;
	source_string->len = msg->callid->body.len;
	first_token (source_string);
	return 0;
}

static int determine_fromto_uri (struct to_body *fromto, str *source_string) {
	if (fromto == NULL) {
		LM_ERR("fromto is NULL!\n");
		return -1;
	}
	source_string->s = fromto->uri.s;
	source_string->len = fromto->uri.len;
	return 0;
}

static int determine_fromto_user (struct to_body *fromto, str *source_string) {
	struct sip_uri uri;

	if (fromto == NULL) {
		LM_ERR("fromto is NULL!\n");
		return -1;
	}
	if (parse_uri (fromto->uri.s, fromto->uri.len, &uri) < 0) {
		LM_ERR("Failed to parse From or To URI.\n");
		return -1;
	}
	source_string->s = uri.user.s;
	source_string->len = uri.user.len;
	return 0;
}

static int determine_fromrand(str* source_string){

	snprintf(&cr_randbuf[0], CR_RANDBUF_S , "%d", rand());

	LM_NOTICE("randbuf is %s\n", cr_randbuf);
	source_string->s = cr_randbuf;
	source_string->len = strlen(source_string->s);

	return 0;
}

static int first_token (str *source_string) {
	size_t len;

	if (source_string->s == NULL || source_string->len == 0) {
		return 0;
	}

	while (source_string->len > 0 && isspace (*source_string->s)) {
		++source_string->s;
		--source_string->len;
	}
	for (len = 0; len < source_string->len; ++len) {
		if (isspace (source_string->s[len])) {
			source_string->len = len;
			break;
		}
	}
	return 0;
}
