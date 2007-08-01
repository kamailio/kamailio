/*
 * $Id$
 *
 * Copyright (C) 2007 1&1 Internet AG
 *
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "../../sr_module.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_from.h"
#include "../../crc.h"

#include <ctype.h>
#include <math.h>

#include "hash.h"

MODULE_VERSION

int prime_number = 51797;

/************* Declaration of Interface Functions **************************/

static int real_hash_func (struct sip_msg*, int);
static int hash_func (struct sip_msg * msg,
                         enum hash_source source, int denominator);
static int prime_hash_func (struct sip_msg * msg,
                               enum hash_source source, int denominator);
static int calculate_hash (struct sip_msg*, char*, char*);

/************* Exports *****************************************************/

static cmd_export_t cmds[]= {
	{"real_hash", (cmd_function) real_hash_func, 0, 0, 0 },
	{"hash", (cmd_function) hash_func, 0, 0, 0 },
	{"prime_hash", (cmd_function) prime_hash_func, 0, 0, 0 },
	{"calculate_hash", calculate_hash, 0, 0,
	REQUEST_ROUTE | FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};

static param_export_t params[]={
	{"prime_number", INT_PARAM, &prime_number},
	{0,0,0}
};

struct module_exports exports = {
	"hash",
	DEFAULT_DLFLAGS, /* dlopen flags */
	cmds,
	params,
	0,          /* exported statistics */
	0,          /* exported MI functions */
	0,          /* exported pseudo-variables */
	0,          /* extra processes */
	0,          /* module initialization function */
	0,
	0,
	0           /* per-child init function */
};


/************* Declaration of Static Helpers *******************************/

static int real_calculate_hash(struct sip_msg *msg,
                               enum hash_source source);
static int determine_source(struct sip_msg *msg, enum hash_source source,
                            str *source_string);
static int validate_msg(struct sip_msg * msg);
static int determine_call_id (struct sip_msg *msg, str *source_string);
static int determine_fromto_uri (struct to_body *fromto, str *source_string);
static int determine_fromto_user (struct to_body *fromto, str *source_string);
static int first_token (str *source_string);
static int calc_prime_hash(str * source_string, int denominator);

/************* Globals *****************************************************/

/* We use globals to only calculate the hash if it is really necessary.
 */
static struct sip_msg *cur_msg;
static enum hash_source cur_source;
static uint32_t cur_hash;

/************* Interface Functions *****************************************/

static int real_hash_func(struct sip_msg *msg, int denominator) {
	return hash_func (msg, shs_call_id, denominator);
}

static int hash_func (struct sip_msg * msg,
                         enum hash_source source, int denominator) {
	int ret;
	if (real_calculate_hash (msg, source) == -1) {
		return -1;
	}
	ret = cur_hash % denominator;
	DBG ("hash: %u %% %i = %i\n", cur_hash, denominator, ret);
	return ret;
}

static int prime_hash_func(struct sip_msg * msg,
                              enum hash_source source, int denominator) {
	str source_string;
	if(source != shs_from_user && source != shs_to_user) {
		LOG(L_ERR, "prime_hash_func: chosen hash source not usable (may contain letters)\n");
		return -1;
	}
	if (determine_source (msg, source, &source_string) == -1) {
		LOG(L_ERR, "prime_hash_func: could not determine source_string\n");
		return -1;
	}
	return calc_prime_hash(&source_string, denominator);
}

static int calculate_hash (struct sip_msg *msg, char *bla, char *blubb) {
	return real_calculate_hash (msg, shs_call_id) == -1 ? -1 : 1;
}

/************* Static Helpers **********************************************/

static int real_calculate_hash (struct sip_msg *msg, enum hash_source source) {
	str source_string;
	uint32_t hash;

	if(determine_source (msg, source, &source_string) == -1) {
		return -1;
	}
	crc32_uint (&source_string, &hash);
	cur_msg = msg;
	cur_source = source;
	cur_hash = hash;
	return 0;
}

static int calc_prime_hash(str * source_string, int denominator) {
#define INT_DIGIT_LIMIT 18
	uint64_t number = 0;
	int i, j, limit = 0;
	int ret;
	char source_number_s[INT_DIGIT_LIMIT + 1];

	i=INT_DIGIT_LIMIT - 1;
	j=source_string->len - 1;
	source_number_s[INT_DIGIT_LIMIT] ='\0';
	while(i >= 0 && j >= 0) {
		if(isdigit(source_string->s[j])) {
			source_number_s[i] = source_string->s[j];
			i--;
		}
		j--;
	}
	limit = i;

	for(i=INT_DIGIT_LIMIT - 1, j=0; i>limit; i--, j++) {
		number += (source_number_s[i] - '0') * powl(10, j);
	}
	LOG(L_DBG, "calc_prime_number: source_string is %.*s, source_number_s "
	    "is: %s, number is %llu\n", source_string->len, source_string->s,
	    source_number_s + (limit + 1), number);
	ret = number % prime_number;
	ret = ret % denominator + 1;
	LOG(L_DBG, "calculated hash is: %i\n", ret);
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
			default:
			LOG (L_ERR, "BUG: hash: unknown hash source %i.\n",
			     (int) source);
			return -1;
	}
}

static int validate_msg(struct sip_msg * msg) {
	if(!msg->callid && ((parse_headers(msg, HDR_CALLID_T, 0) == -1) || !msg->callid)) {
		LOG(L_ERR, "hash: validate_msg: Message has no Call-ID header\n");
		return -1;
	}
	if(!msg->to && ((parse_headers(msg, HDR_TO_T, 0) == -1) || !msg->to)) {
		LOG(L_ERR, "hash: validate_msg: Message has no To header\n");
		return -1;
	}
	if(!msg->from && ((parse_headers(msg, HDR_FROM_T, 0) == -1) || !msg->from)) {
		LOG(L_ERR, "hash: validate_msg: Message has no From header\n");
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
		return -1;
	}
	source_string->s = fromto->uri.s;
	source_string->len = fromto->uri.len;
	return 0;
}

static int determine_fromto_user (struct to_body *fromto, str *source_string) {
	struct sip_uri uri;

	if (fromto == NULL) {
		return -1;
	}
	if (parse_uri (fromto->uri.s, fromto->uri.len, &uri) < 0) {
		LOG (L_ERR, "hash: Failed to parse From or To URI.\n");
		return -1;
	}
	source_string->s = uri.user.s;
	source_string->len = uri.user.len;
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

