/*
 * $Id$
 *
 * Enum and E164 related functions
 *
 * Copyright (C) 2002-2003 Juha Heinanen
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include "enum.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../ut.h"
#include "../../resolve.h"
#include "../../mem/mem.h"
#include "enum_mod.h"
#include "regexp.h"


/* Checks if NAPTR record has flag u and its services field
 * e2u+[service:]sip
 */
inline int sip_match( struct naptr_rdata* naptr, str* service)
{
  if (service->len == 0) {
    return (naptr->flags_len == 1) &&
      ((naptr->flags[0] == 'u') || (naptr->flags[0] == 'U')) &&
      (naptr->services_len == 7) &&
      ((strncasecmp(naptr->services, "e2u+sip", 7) == 0) ||
       (strncasecmp(naptr->services, "sip+e2u", 7) == 0));
  } else {
    return (naptr->flags_len == 1) &&
      ((naptr->flags[0] == 'u') || (naptr->flags[0] == 'U')) &&
      (naptr->services_len == service->len + 8) &&
      (strncasecmp(naptr->services, "e2u+", 4) == 0) &&
      (strncasecmp(naptr->services + 4, service->s, service->len) == 0) &&
      (strncasecmp(naptr->services + 4 + service->len, ":sip", 4) == 0);
  }
}


/*
 * Check that From header is properly parsed and if so,
 * return pointer to parsed From header.  Otherwise return NULL.
 */
inline struct to_body *get_parsed_from_body(struct sip_msg *_msg)
{
	if (!(_msg->from)) {
		LOG(L_ERR, "get_parsed_from(): Request does not have a From header\n");
		return NULL;
	}
	if (!(_msg->from->parsed) || ((struct to_body *)_msg->from->parsed)->error != PARSE_OK) {
		LOG(L_ERR, "get_parsed_from(): From header is not properly parsed\n");
		return NULL;
	}
	return (struct to_body *)(_msg->from->parsed);
}


/* Replaces message uri with the uri given as argument.  Returns 1
 * if replacement succeeds and -1 otherwise.
 */
inline int set_uri(struct sip_msg* _msg, char* uri, int len)
{
	if (len > MAX_URI_SIZE - 1) {
		LOG(L_ERR, "ERROR: set_uri(): uri is too long\n");
		return -1;
	}
		
	if (_msg->new_uri.s) {
		pkg_free(_msg->new_uri.s);
		_msg->new_uri.len = 0;
	}
	if (_msg->parsed_uri_ok) {
		_msg->parsed_uri_ok = 0;
/*		free_uri(&_msg->parsed_uri); not needed anymore */
	}
	_msg->new_uri.s = pkg_malloc(len + 1);
	if (_msg->new_uri.s == 0) {
		LOG(L_ERR, "ERROR: set_uri(): memory allocation"
		    " failure\n");
		return -1;
	}
	memcpy(_msg->new_uri.s, uri, len);
	_msg->new_uri.s[len] = 0;
	_msg->new_uri.len = len;
	return 1;
}

/*
 * Checks if argument is an e164 number starting with +
 */
inline int is_e164(str* _user)
{
	int i;
	char c;

	if ((_user->len > 2) && (_user->len < 17) && ((_user->s)[0] == '+')) {
		for (i = 1; i <= _user->len; i++) {
			c = (_user->s)[i];
			if (c < '0' && c > '9') return -1;
		}
		return 1;
	}
	return -1;
}
				
		
/*
 * Check if from user is an e164 number
 */
int is_from_user_e164(struct sip_msg* _msg, char* _s1, char* _s2)
{
	struct to_body* body;
	struct sip_uri uri;
	int result;

	body = get_parsed_from_body(_msg);
	if (!body) return -1;

	if (parse_uri(body->uri.s, body->uri.len, &uri) < 0) {
		LOG(L_ERR, "is_from_user_e164(): Error while parsing From uri\n");
		return -1;
	}

	result = is_e164(&(uri.user));
	return result;
}

/* Parse NAPTR regexp field of the form !pattern!replacement! and return its
 * components in pattern and replacement paraneters.  Regexp field starts at
 * address first and is len characters long.
 */
inline int parse_naptr_regexp(char* first, int len, str* pattern, str* replacement)
{
	char *second, *third;

	if (len > 0) {
		if (*first == '!') {
			second = (char *)memchr((void *)(first + 1), '!', len - 1);
			if (second) {
				len = len - (second - first + 1);
				if (len > 0) {
					third = memchr(second + 1, '!', len);
					if (third) {
						pattern->len = second - first - 1;
						pattern->s = first + 1;
						replacement->len = third - second - 1;
						replacement->s = second + 1;
						return 1;
					} else {
						LOG(LOG_ERR, "parse_regexp(): third ! missing from regexp\n");
						return -1;
					}
				} else {
					LOG(LOG_ERR, "parse_regexp(): third ! missing from regexp\n");
					return -2;
				}
			} else {
				LOG(LOG_ERR, "parse_regexp(): second ! missing from regexp\n");
				return -3;
			}
		} else {
			LOG(LOG_ERR, "parse_regexp(): first ! missing from regexp\n");
			return -4;
		}
	} else {
		LOG(LOG_ERR, "parse_regexp(): regexp missing\n");
		return -5;
	}
}


/*
 * Makes enum query on user part of current request uri, which must
 * contain an international phone number, i.e, a plus (+) sign
 * followed by up to 15 decimal digits.  If query succeeds, replaces the
 * current request uri with the result and returns 1.  On failure,
 * returs -1.
 */

int enum_query(struct sip_msg* _msg, char* _service, char* _s2)
{
	char *user_s;
	int user_len, i, j, retval;
	char name[MAX_DOMAIN_SIZE];
	char uri[MAX_URI_SIZE];

	struct rdata* head;
	struct rdata* l;
	struct naptr_rdata* naptr;

	str pattern, replacement, result;
	char string[17];

	str* service;
	service = (str*)_service;

	if (parse_sip_msg_uri(_msg) < 0) {
		LOG(L_ERR, "enum_query(): uri parsing failed\n");
		return -1;
	}

	if (is_e164(&(_msg->parsed_uri.user)) == -1) {
		LOG(L_ERR, "enum_query(): uri user is not an E164 number\n");
		return -2;
	}

	user_s = _msg->parsed_uri.user.s;
	user_len = _msg->parsed_uri.user.len;

	j = 0;
	for (i = user_len - 1; i > 0; i--) {
		name[j] = user_s[i];
		name[j + 1] = '.';
		j = j + 2;
	}

	memcpy(name + j, suffix.s, suffix.len + 1);
/*	strcpy(&(name[j]), "e164.arpa."); */

	head = get_record(name, T_NAPTR);

	if (head == 0) {
		DBG("enum_query(): No NAPTR record found for %s.\n", name);
		return -3;
	}

	for (l = head; l; l = l->next) {

		if (l->type != T_NAPTR) continue; /*should never happen*/
		naptr = (struct naptr_rdata*)l->rdata;
		if (naptr == 0) {
			LOG(L_CRIT, "enum_query: BUG: null rdata\n");
			free_rdata_list(head);
			return -4;
		}

		DBG("enum_query(): order %u, pref %u, flen %u, flags '%.*s', slen %u, "
		    "services '%.*s', rlen %u, regexp '%.*s'\n", naptr->order, naptr->pref,
		    naptr->flags_len, (int)(naptr->flags_len), ZSW(naptr->flags), naptr->services_len,
		    (int)(naptr->services_len), ZSW(naptr->services), naptr->regexp_len,
		    (int)(naptr->regexp_len), ZSW(naptr->regexp));

		if (sip_match(naptr, service) != 0) {
			if (parse_naptr_regexp(&(naptr->regexp[0]), naptr->regexp_len,
					 &pattern, &replacement) < 0) {
				free_rdata_list(head); /*clean up*/
				LOG(L_ERR, "enum_query(): parsing of NAPTR regexp failed\n");
				return -5;
			}
			if ((pattern.len == 4) && (strncmp(pattern.s, "^.*$", 4) == 0)) {
				DBG("enum_query(): resulted in replacement: '%.*s'\n",
				    replacement.len, ZSW(replacement.s));				
				retval = set_uri(_msg, replacement.s, replacement.len);
				free_rdata_list(head); /*clean up*/
				return retval;
			}
			result.s = &(uri[0]);
			result.len = MAX_URI_SIZE;
			/* Avoid making copies of pattern and replacement */
			pattern.s[pattern.len] = (char)0;
			replacement.s[replacement.len] = (char)0;
			/* We have already checked the size of
			   _msg->parsed_uri.user.s */ 
			memcpy(&(string[0]), user_s, user_len);
			string[user_len] = (char)0;
			if (reg_replace(pattern.s, replacement.s, &(string[0]),
					&result) < 0) {
				pattern.s[pattern.len] = '!';
				replacement.s[replacement.len] = '!';
				LOG(L_ERR, "enum_query(): regexp replace failed\n");
				free_rdata_list(head); /*clean up*/
				return -6;
			}
			DBG("enum_query(): resulted in replacement: '%.*s'\n",
			    result.len, ZSW(result.s));
			retval = set_uri(_msg, result.s, result.len);
			pattern.s[pattern.len] = '!';
			replacement.s[replacement.len] = '!';
			free_rdata_list(head); /*clean up*/
			return retval;
		}
	}

	free_rdata_list(head); /*clean up*/
	return -1;
}
