/* enum.c v 0.2 2002/12/27
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
 */


#include "enum.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../resolve.h"
#include "../../mem/mem.h"

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
		free_uri(&_msg->parsed_uri);
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
	struct sip_uri uri;
	int result;

	if (!_msg->from && (parse_headers(_msg, HDR_FROM, 0) == -1)) {
		LOG(L_ERR, "is_from_user_e164(): Error while parsing message\n");
		return -1;
	}
	if (!_msg->from) {
		LOG(L_ERR, "is_from_user_e164(): From HF not found\n");
		return -1;
	}

	if (!(_msg->from->parsed)) {
		if (parse_from_header(_msg->from) == -1) {
			LOG(L_ERR, "is_from_user_e164(): Can't parse from header\n");
			return -1;
		}
	}

	if (parse_uri(get_from(_msg)->uri.s, get_from(_msg)->uri.len, &uri) < 0) {
		LOG(L_ERR, "is_from_user_e164(): Error while parsing From uri\n");
		return -1;
	}

	result = is_e164(&(uri.user));
	free_uri(&uri);
	return result;
}


/*
 * Makes enum query on user part of current request uri, which must
 * contain an international phone number, i.e, a plus (+) sign
 * followed by up to 15 decimal digits.  If query succeeds, replaces the
 * current request uri with the result and returns 1.  On failure,
 * returs -1.
 */

int enum_query(struct sip_msg* _msg, char* _s1, char* _s2)
{
	char *s, *first, *second, *third;
	int len, i, j, result;
	char name[41];

	struct rdata* head;
	struct rdata* l;
	struct naptr_rdata* naptr;

	if (parse_sip_msg_uri(_msg) < 0) {
		LOG(L_ERR, "enum_query(): uri parsing failed\n");
		return -1;
	}

	if (is_e164(&(_msg->parsed_uri.user)) == -1) {
		LOG(L_ERR, "enum_query(): uri user is not an E164 number\n");
		return -1;
	}

	s = _msg->parsed_uri.user.s;
	len = _msg->parsed_uri.user.len;

	j = 0;
	for (i = len - 1; i > 0; i--) {
		name[j] = s[i];
		name[j + 1] = '.';
		j = j + 2;
	}

	strcpy(&(name[j]), "e164.arpa.");

	head = get_record(name, T_NAPTR);

	if (head == 0) {
		DBG("enum_query(): No NAPTR record found for %s.\n", name);
		return -1;
	}

	for (l = head; l; l = l->next) {
		if (l->type != T_NAPTR) continue; /*should never happen*/
		naptr = (struct naptr_rdata*)l->rdata;
		if (naptr == 0) {
			LOG(L_CRIT, "enum_query: BUG: null rdata\n");
			free_rdata_list(head);
			return -1;
		}
		DBG("enum_query(): order %u, pref %u, flen %u, flags \'%.*s\', slen %u, "
		    "services \'%.*s\', rlen %u, regexp \'%.*s\'\n", naptr->order, naptr->pref,
		    naptr->flags_len, (int)(naptr->flags_len), naptr->flags, naptr->services_len,
		    (int)(naptr->services_len), naptr->services, naptr->regexp_len,
		    (int)(naptr->regexp_len), naptr->regexp);
		if ((naptr->flags_len == 1) &&
		    ((naptr->flags[0] == 'u') || (naptr->flags[0] == 'U')) &&
		    (naptr->services_len == 7) &&
		    (strncasecmp(naptr->services, "e2u+sip", 7) == 0)) {
			len = naptr->regexp_len;
			if (len > 0) {
				first = &(naptr->regexp[0]);
				if (*first == '!') {
					second = (char *)memchr((void *)(first + 1), '!', len - 1);
					if (second) {
						len = len - (second - first + 1);
						if (len > 0) {
							third = memchr(second + 1, '!', len);
							if (third) {
								DBG("enum_query(): resulted in uri: \'%.*s\'\n", third - second - 1, second + 1);
								result = set_uri(_msg, second + 1, third - second - 1);
								free_rdata_list(head); /*clean up*/
								return result;
							} else {
								LOG(LOG_ERR, "enum_query(): third ! missing from regexp\n");
							}
						} else {
							LOG(LOG_ERR, "enum_query(): third ! missing from regexp\n");
						}
					} else {
						LOG(LOG_ERR, "enum_query(): second ! missing from regexp\n");
					}
				} else {
					LOG(LOG_ERR, "enum_query(): first ! missing from regexp\n");
				}
			} else {
				LOG(LOG_ERR, "enum_query(): regexp missing\n");
			}
		}
	}
	free_rdata_list(head); /*clean up*/
	return -1;
}
