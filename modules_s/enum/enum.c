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
#include "../../dset.h"
#include "../../qvalue.h"
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


/* 
 * Rewrites r-uri with the uri given as argument.  Returns 1
 * if replacement succeeds and -1 otherwise.
 */
inline int rewrite_uri(struct sip_msg* _msg, char* uri, int len)
{
	if (_msg->new_uri.s) {
		pkg_free(_msg->new_uri.s);
		_msg->new_uri.len = 0;
	}
	if (_msg->parsed_uri_ok) {
		_msg->parsed_uri_ok = 0;
	}
	_msg->new_uri.s = pkg_malloc(len + 1);
	if (_msg->new_uri.s == 0) {
		LOG(L_ERR, "ERROR: rewrite_uri(): memory allocation"
		    " failure\n");
		return -1;
	}
	memcpy(_msg->new_uri.s, uri, len);
	_msg->new_uri.s[len] = 0;
	_msg->new_uri.len = len;

	DBG("rewrite_uri(): Rewriting Request-URI with '%.*s'\n", len, uri);

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
 * components in pattern and replacement parameters.  Regexp field starts at
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
 * Add parameter to URI.
 */
int add_uri_param(str *uri, str *param, str *new_uri)
{
	struct sip_uri puri;
	char *at;

	if (parse_uri(uri->s, uri->len, &puri) < 0) {
		return 0;
	}

	/* if current uri has no headers, pad param to the end of uri */
	if (puri.headers.len == 0) {
		memcpy(uri->s + uri->len, param->s, param->len);
		uri->len = uri->len + param->len;
		new_uri->len = 0;
		return 1;
	}

	/* otherwise take the long path and create new_uri */
	at = new_uri->s;
	memcpy(at, "sip:", 4);
	at = at + 4;
	if (puri.user.len) {
		memcpy(at, puri.user.s, puri.user.len);
		at = at + puri.user.len;
		if (puri.passwd.len) {
			*at = ':';
			at = at + 1;
			memcpy(at, puri.passwd.s, puri.passwd.len);
			at = at + puri.passwd.len;
		};
		*at = '@';
		at = at + 1;
	}
	memcpy(at, puri.host.s, puri.host.len);
	at = at + puri.host.len;
	if (puri.port.len) {
		*at = ':';
		at = at + 1;
		memcpy(at, puri.port.s, puri.port.len);
		at = at + puri.port.len;
	}
	if (puri.params.len) {
		*at = ';';
		at = at + 1;
		memcpy(at, puri.params.s, puri.params.len);
		at = at + puri.params.len;
	}
	memcpy(at, param->s, param->len);
	at = at + param->len;
	*at = '?';
	at = at + 1;
	memcpy(at, puri.headers.s, puri.headers.len);
	at = at + puri.headers.len;
	new_uri->len = at - new_uri->s;
	return 1;
}

/*
 * Tests if one result record is "greater" that the other.  Non-NAPTR records
 * greater that NAPTR record.  An invalid NAPTR record is greater than a 
 * valid one.  Valid NAPTR records are compared based on their
 * (order,preference).
 */
inline int naptr_greater(struct rdata* a, struct rdata* b)
{
	struct naptr_rdata *na, *nb;

	if (a->type != T_NAPTR) return 1;
	if (b->type != T_NAPTR) return 0;

	na = (struct naptr_rdata*)a->rdata;
	if (na == 0) return 1;

	nb = (struct naptr_rdata*)b->rdata;
	if (nb == 0) return 0;
	
	return (((na->order) << 16) + na->pref) >
		(((nb->order) << 16) + nb->pref);
}
	
	
/*
 * Bubble sorts result record list according to naptr (order,preference).
 */
inline void naptr_sort(struct rdata** head)
{
	struct rdata *p, *q, *r, *s, *temp, *start;

        /* r precedes p and s points to the node up to which comparisons
         are to be made */ 

	s = NULL;
	start = *head;
	while ( s != start -> next ) { 
		r = p = start ; 
		q = p -> next ;
		while ( p != s ) { 
			if ( naptr_greater(p, q) ) { 
				if ( p == start ) { 
					temp = q -> next ; 
					q -> next = p ; 
					p -> next = temp ;
					start = q ; 
					r = q ; 
				} else {
					temp = q -> next ; 
					q -> next = p ; 
					p -> next = temp ;
					r -> next = q ; 
					r = q ; 
				} 
			} else {
				r = p ; 
				p = p -> next ; 
			} 
			q = p -> next ; 
			if ( q == s ) s = p ; 
		}
	}
	*head = start;
}	

	
/*
 * Call enum_query_2 with module parameter suffix and default service.
 */
int enum_query_0(struct sip_msg* _msg, char* _str1, char* _str2)
{
	return enum_query_2(_msg, (char *)(&suffix), (char *)(&service));
}

/*
 * Call enum_query_2 with given suffix and default service.
 */
int enum_query_1(struct sip_msg* _msg, char* _suffix, char* _str2)
{
	return enum_query_2(_msg, _suffix, (char *)(&service));
}

/*
 * See documentation in README file.
 */

int enum_query_2(struct sip_msg* _msg, char* _suffix, char* _service)
{
	char *user_s;
	int user_len, i, j, first;
	char name[MAX_DOMAIN_SIZE];
	char uri[MAX_URI_SIZE];
	char new_uri[MAX_URI_SIZE];
	unsigned int priority, curr_prio;
	qvalue_t q;

	struct rdata* head;
	struct rdata* l;
	struct naptr_rdata* naptr;

	str pattern, replacement, result, new_result;

	char string[17];

	str *suffix, *service;

	suffix = (str*)_suffix;
	service = (str*)_service;

	if (parse_sip_msg_uri(_msg) < 0) {
		LOG(L_ERR, "enum_query(): uri parsing failed\n");
		return -1;
	}

	if (is_e164(&(_msg->parsed_uri.user)) == -1) {
		LOG(L_ERR, "enum_query(): uri user is not an E164 number\n");
		return -1;
	}

	user_s = _msg->parsed_uri.user.s;
	user_len = _msg->parsed_uri.user.len;

	memcpy(&(string[0]), user_s, user_len);
	string[user_len] = (char)0;

	j = 0;
	for (i = user_len - 1; i > 0; i--) {
		name[j] = user_s[i];
		name[j + 1] = '.';
		j = j + 2;
	}

	memcpy(name + j, suffix->s, suffix->len + 1);

	head = get_record(name, T_NAPTR);

	if (head == 0) {
		DBG("enum_query(): No NAPTR record found for %s.\n", name);
		return -1;
	}

	naptr_sort(&head);

	q = MAX_Q - 10;
	curr_prio = 0;
	first = 1;

	for (l = head; l; l = l->next) {

		if (l->type != T_NAPTR) continue; /*should never happen*/
		naptr = (struct naptr_rdata*)l->rdata;
		if (naptr == 0) {
			LOG(L_CRIT, "enum_query: BUG: null rdata\n");
			continue;
		}

		DBG("enum_query(): order %u, pref %u, flen %u, flags '%.*s', slen %u, "
		    "services '%.*s', rlen %u, regexp '%.*s'\n", naptr->order, naptr->pref,
		    naptr->flags_len, (int)(naptr->flags_len), ZSW(naptr->flags),
		    naptr->services_len,
		    (int)(naptr->services_len), ZSW(naptr->services), naptr->regexp_len,
		    (int)(naptr->regexp_len), ZSW(naptr->regexp));

		if (sip_match(naptr, service) == 0) continue;

		if (parse_naptr_regexp(&(naptr->regexp[0]), naptr->regexp_len,
				       &pattern, &replacement) < 0) {
			LOG(L_ERR, "enum_query(): parsing of NAPTR regexp failed\n");
			continue;
		}
		result.s = &(uri[0]);
		result.len = MAX_URI_SIZE;
		/* Avoid making copies of pattern and replacement */
		pattern.s[pattern.len] = (char)0;
		replacement.s[replacement.len] = (char)0;
		if (reg_replace(pattern.s, replacement.s, &(string[0]),
				&result) < 0) {
			pattern.s[pattern.len] = '!';
			replacement.s[replacement.len] = '!';
			LOG(L_ERR, "enum_query(): regexp replace failed\n");
			continue;
		}
		DBG("enum_query(): resulted in replacement: '%.*s'\n",
		    result.len, ZSW(result.s));
		pattern.s[pattern.len] = '!';
		replacement.s[replacement.len] = '!';
		
		if (param.len > 0) {
			if (result.len + param.len > MAX_URI_SIZE - 1) {
				LOG(L_ERR, "ERROR: enum_query(): URI is too long\n");
				continue;
			}
			new_result.s = &(new_uri[0]);
			new_result.len = MAX_URI_SIZE;
			if (add_uri_param(&result, &param, &new_result) == 0) {
				LOG(L_ERR, "ERROR: enum_query(): Parsing of URI failed\n");
				continue;
			}
			if (new_result.len > 0) {
				result = new_result;
			}
		}

		if (first) {
			if (rewrite_uri(_msg, result.s, result.len) == -1) {
				goto done;
			}
			set_ruri_q(q);
			first = 0;
			curr_prio = ((naptr->order) << 16) + naptr->pref;
		} else {
			priority = ((naptr->order) << 16) + naptr->pref;
			if (priority > curr_prio) {
				q = q - 10;
				curr_prio = priority;
			}
			if (append_branch(_msg, result.s, result.len, 0, 0, q) == -1) {
				goto done;
			}
		}
	}

done:
	free_rdata_list(head);
	return first ? -1 : 1;
}
