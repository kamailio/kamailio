/*
 * $Id$
 *
 * Enum and E164 related functions
 *
 * Copyright (C) 2002-2003 Juha Heinanen
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

#include <stdlib.h>

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

/*
 * Input: E.164 number w/o leading +
 *
 * Output: number of digits in the country code
 * 	   0 on invalid number
 *
 * convention:
 *   3 digits is the default length of a country code.
 *   country codes 1 and 7 are a single digit.
 *   the following country codes are two digits: 20, 27, 30-34, 36, 39,
 *     40, 41, 43-49, 51-58, 60-66, 81, 82, 84, 86, 90-95, 98.
 */
static int cclen(const char *number)
{
	char d1,d2;

	if (!number || (strlen(number) < 3))
		return(0);

	d1 = number[0];
	d2 = number[1];
	
	if (!isdigit((int)d2)) 
		return(0);

	switch(d1) {
		case '1':
		case '7':
			return(1);
		case '2':
			if ((d2 == '0') || (d1 == '7'))
				return(2);
			break;
		case '3':
			if ((d2 >= '0') && (d1 <= '4'))
				return(2);
			if ((d2 == '6') || (d1 == '9'))
				return(2);
			break;
		case '4':
			if (d2 != '2')
				return(2);
			break;
		case '5':
			if ((d2 >= '1') && (d1 <= '8'))
				return(2);
			break;
		case '6':
			if (d1 <= '6')
				return(2);
			break;
		case '8':
			if ((d2 == '1') || (d1 == '2') || (d1 == '4') || (d1 == '6')) 
				return(2);
			break;
		case '9':
			if (d1 <= '5')
				return(2);
			if (d2 == '8')
				return(2);
			break;
		default:
			return(0);
	}

	return(3);
}



/* return the length of the string until c, if not found returns n */
static inline int findchr(char* p, int c, unsigned int size)
{
	int len=0;

	for(;len<size;p++){
		if (*p==(unsigned char)c) {
			return len;
		}
		len++;   
	}
	return len;
}


/* Parse NAPTR regexp field of the form !pattern!replacement! and return its
 * components in pattern and replacement parameters.  Regexp field starts at
 * address first and is len characters long.
 */
static inline int parse_naptr_regexp(char* first, int len, str* pattern,
										str* replacement)
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
/* Checks if NAPTR record has flag u and its services field
 * e2u+[service:]sip or
 * e2u+service[+service[+service[+...]]]
 */
static inline int sip_match( struct naptr_rdata* naptr, str* service)
{
  if (service->len == 0) {
    return (naptr->flags_len == 1) &&
      ((naptr->flags[0] == 'u') || (naptr->flags[0] == 'U')) &&
      (naptr->services_len == 7) &&
      ((strncasecmp(naptr->services, "e2u+sip", 7) == 0) ||
       (strncasecmp(naptr->services, "sip+e2u", 7) == 0));
  } else if (service->s[0] != '+') {
    return (naptr->flags_len == 1) &&
      ((naptr->flags[0] == 'u') || (naptr->flags[0] == 'U')) &&
      (naptr->services_len == service->len + 8) &&
      (strncasecmp(naptr->services, "e2u+", 4) == 0) &&
      (strncasecmp(naptr->services + 4, service->s, service->len) == 0) &&
      (strncasecmp(naptr->services + 4 + service->len, ":sip", 4) == 0);
  } else { /* handle compound NAPTRs and multiple services */
    str bakservice, baknaptr; /* we bakup the str */
    int naptrlen, len;        /* length of the extracted service */

    /* RFC 3761, NAPTR service field must start with E2U+ */
    if (strncasecmp(naptr->services, "e2u+", 4) != 0) {
      return 0;
    }
    baknaptr.s   = naptr->services + 4; /* leading 'e2u+' */
    baknaptr.len = naptr->services_len - 4;
    for (;;) { /* iterate over services in NAPTR */
      bakservice.s   = service->s + 1; /* leading '+' */
      bakservice.len = service->len - 1;
      naptrlen = findchr(baknaptr.s,'+',baknaptr.len);

      for (;;) { /* iterate over services in enum_query */
        len = findchr(bakservice.s,'+',bakservice.len);
        if ((naptrlen == len ) && !strncasecmp(baknaptr.s, bakservice.s, len)){
          return 1;
        }
        if ( (bakservice.len -= len+1) > 0) {
          bakservice.s += len+1;
          continue;
        }
        break;
      }
      if ( (baknaptr.len -= naptrlen+1) > 0) {
        baknaptr.s += naptrlen+1;
        continue;
      }
      break;
    }
    /* no matching service found */
    return 0;
  }
}


/*
 * Check that From header is properly parsed and if so,
 * return pointer to parsed From header.  Otherwise return NULL.
 */
static inline struct to_body *get_parsed_from_body(struct sip_msg *_msg)
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
 * Checks if argument is an e164 number starting with +
 */
static inline int is_e164(str* _user)
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
 * Call is_from_user_enum_2 with module parameter suffix and default service.
 */
int is_from_user_enum_0(struct sip_msg* _msg, char* _str1, char* _str2)
{
	return is_from_user_enum_2(_msg, (char *)(&suffix), (char *)(&service));
}

/*
 * Call is_from_user_enum_2 with given suffix and default service.
 */
int is_from_user_enum_1(struct sip_msg* _msg, char* _suffix, char* _str2)
{
	return is_from_user_enum_2(_msg, _suffix, (char *)(&service));
}

/*
 * Check if from user is a valid enum based user, and check to make sure
 * that the src_ip == an srv record that maps to the enum from user.
 */
int is_from_user_enum_2(struct sip_msg* _msg, char* _suffix, char* _service)
{
	struct ip_addr addr;
	struct hostent* he;
	unsigned short zp;
	char *user_s;
	int user_len, i, j, proto;
	char name[MAX_DOMAIN_SIZE];
	char uri[MAX_URI_SIZE];
	struct to_body* body;
	struct sip_uri muri;
	struct sip_uri luri;
	struct rdata* head;

	str* suffix;
	str* service;

	struct rdata* l;
	struct naptr_rdata* naptr;

	str pattern, replacement, result;
	char string[17];

	body = get_parsed_from_body(_msg);
	if (!body) return -1;

	if (parse_uri(body->uri.s, body->uri.len, &muri) < 0) {
		LOG(L_ERR, "is_from_user_enum(): Error while parsing From uri\n");
		return -1;
	}

	suffix = (str*)_suffix;
	service = (str*)_service;

	if (is_e164(&(muri.user)) == -1) {
		LOG(L_ERR, "is_from_user_enum(): from user is not an E164 number\n");
		return -2;
	}

	/* assert: the from user is a valid formatted e164 string */

	user_s = muri.user.s;
	user_len = muri.user.len;

	j = 0;
	for (i = user_len - 1; i > 0; i--) {
		name[j] = user_s[i];
		name[j + 1] = '.';
		j = j + 2;
	}

	memcpy(name + j, suffix->s, suffix->len + 1);

	head = get_record(name, T_NAPTR);

	if (head == 0) {
		DBG("is_from_user_enum(): No NAPTR record found for %s.\n", name);
		return -3;
	}

	/* we have the naptr records, loop and find an srv record with */
	/* same ip address as source ip address, if we do then true is returned */

	for (l = head; l; l = l->next) {

		if (l->type != T_NAPTR) continue; /*should never happen*/
		naptr = (struct naptr_rdata*)l->rdata;
		if (naptr == 0) {
			LOG(L_CRIT, "is_from_user_enum: BUG: null rdata\n");
			free_rdata_list(head);
			return -4;
		}

		DBG("is_from_user_enum(): order %u, pref %u, flen %u, flags '%.*s', slen %u, "
		    "services '%.*s', rlen %u, regexp '%.*s'\n", naptr->order, naptr->pref,
		    naptr->flags_len, (int)(naptr->flags_len), ZSW(naptr->flags), naptr->services_len,
		    (int)(naptr->services_len), ZSW(naptr->services), naptr->regexp_len,
		    (int)(naptr->regexp_len), ZSW(naptr->regexp));

		if (sip_match(naptr, service) != 0) {
			if (parse_naptr_regexp(&(naptr->regexp[0]), naptr->regexp_len,
					 &pattern, &replacement) < 0) {
				free_rdata_list(head); /*clean up*/
				LOG(L_ERR, "is_from_user_enum(): parsing of NAPTR regexp failed\n");
				return -5;
			}
#ifdef LATER
			if ((pattern.len == 4) && (strncmp(pattern.s, "^.*$", 4) == 0)) {
				DBG("is_from_user_enum(): resulted in replacement: '%.*s'\n",
				    replacement.len, ZSW(replacement.s));				
				retval = set_uri(_msg, replacement.s, replacement.len);
				free_rdata_list(head); /*clean up*/
				return retval;
			}
#endif
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
				LOG(L_ERR, "is_from_user_enum(): regexp replace failed\n");
				free_rdata_list(head); /*clean up*/
				return -6;
			}
			DBG("is_from_user_enum(): resulted in replacement: '%.*s'\n",
			    result.len, ZSW(result.s));

			if(parse_uri(result.s, result.len, &luri) < 0)
			{
				LOG(L_ERR, "is_from_user_enum(): parse uri failed\n");
				free_rdata_list(head); /*clean up*/
				return -7;
			}

			pattern.s[pattern.len] = '!';
			replacement.s[replacement.len] = '!';

			zp = 0;
			proto = PROTO_NONE;
			he = sip_resolvehost(&luri.host, &zp, &proto,
				(luri.type==SIPS_URI_T)?1:0 );

			hostent2ip_addr(&addr, he, 0);

			if(ip_addr_cmp(&addr, &_msg->rcv.src_ip))
			{
				free_rdata_list(head);
				return(1);
			}
		}
	}
	free_rdata_list(head); /*clean up*/
	LOG(L_INFO, "is_from_user_enum(): FAIL\n");

    /* must not have found the record */
    return(-8);
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
static inline int naptr_greater(struct rdata* a, struct rdata* b)
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
static inline void naptr_sort(struct rdata** head)
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
 * Makes enum query on name.  On success, rewrites user part and 
 * replaces Request-URI.
 */
int do_query(struct sip_msg* _msg, char *user, char *name, str *service) {

    char uri[MAX_URI_SIZE];
    char new_uri[MAX_URI_SIZE];
    unsigned int priority, curr_prio, first;
    qvalue_t q;
    struct rdata* head;
    struct rdata* l;
    struct naptr_rdata* naptr;
    str pattern, replacement, result, new_result;

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

	DBG("enum_query(%s): order %u, pref %u, flen %u, flags '%.*s', slen %u, "
	    "services '%.*s', rlen %u, regexp '%.*s'\n", name, naptr->order, naptr->pref,
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
	if (reg_replace(pattern.s, replacement.s, user, &result) < 0) {
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
	    if (rewrite_uri(_msg, &result) == -1) {
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
	    if (append_branch(_msg, &result, 0, 0, q, 0, 0) == -1) {
		goto done;
	    }
	}
    }

done:
    free_rdata_list(head);
    return first ? -1 : 1;
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
	int user_len, i, j;
	char name[MAX_DOMAIN_SIZE];
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

	return do_query(_msg, string, name, service);
}


/*********** INFRASTRUCTURE ENUM ***************/

/*
 * Call enum_query_2 with default suffix and service.
 */
int i_enum_query_0(struct sip_msg* _msg, char* _suffix, char* _service)
{
	return enum_query_2(_msg, (char *)(&i_suffix), (char *)(&service));
}

/*
 * Call enum_query_2 with given suffix and default service.
 */
int i_enum_query_1(struct sip_msg* _msg, char* _suffix, char* _service)
{
	return enum_query_2(_msg, _suffix, (char *)(&service));
}


int i_enum_query_2(struct sip_msg* _msg, char* _suffix, char* _service)
{
	char *user_s;
	int user_len, i, j;
	char name[MAX_DOMAIN_SIZE];
	char apex[MAX_COMPONENT_SIZE + 1];
	char separator[MAX_COMPONENT_SIZE + 1];
	int sdl = 0;    /* subdomain location: infrastructure enum offset */
	int cc_len;
	struct rdata* head;

	char string[17];

	str *suffix, *service;

	suffix = (str*)_suffix;
	service = (str*)_service;

	if (parse_sip_msg_uri(_msg) < 0) {
		LOG(L_ERR, "i_enum_query_2(): uri parsing failed\n");
		return -1;
	}

	if (is_e164(&(_msg->parsed_uri.user)) == -1) {
		LOG(L_ERR, "i_enum_query_2(): uri user is not an E164 number\n");
		return -1;
	}

	user_s = _msg->parsed_uri.user.s;
	user_len = _msg->parsed_uri.user.len;

	/* make sure we don't run out of space in strings */
	if (( 2*user_len + MAX_COMPONENT_SIZE + MAX_COMPONENT_SIZE + 4) > MAX_DOMAIN_SIZE) {
		LOG(L_ERR, "i_enum_query_2(): strings too long\n");
		return -1;
	}
	if ( i_branchlabel.len > MAX_COMPONENT_SIZE ) {
		LOG(L_ERR, "i_enum_query_2(): i_branchlabel too long\n");
		return -1;
	}
	if ( suffix->len > MAX_COMPONENT_SIZE ) {
		LOG(L_ERR, "i_enum_query_2(): suffix too long\n");
		return -1;
	}


	memcpy(&(string[0]), user_s, user_len);
	string[user_len] = (char)0;

	/* Set up parameters as for user-enum */
	memcpy(apex,  suffix->s , suffix->len);
	apex[suffix->len] = (char)0;
	sdl = 0;		/* where to insert i-enum separator */
	separator[0] = 0;	/* don't insert anything */

	cc_len = cclen(string + 1);

	if (!strncasecmp(i_bl_alg.s,"ebl",i_bl_alg.len)) {
		sdl = cc_len; /* default */

		j = 0;
		memcpy(name, i_branchlabel.s, i_branchlabel.len);
		j += i_branchlabel.len;
		name[j++] = '.';

		for (i = cc_len ; i > 0; i--) {
			name[j++] = user_s[i];
			name[j++] = '.';
		}
		memcpy(name + j, suffix->s, suffix->len + 1);

		DBG("enum_query(): Looking for EBL record for %s.\n", name); 
		head = get_record(name, T_EBL);
		if (head == 0) {
			DBG("enum_query(): No EBL found for %s. Defaulting to user ENUM.\n",name);
		} else {
		    	struct ebl_rdata* ebl;
			ebl = (struct ebl_rdata *) head->rdata;

			DBG("enum_query(): EBL record for %s is %d / %.*s / %.*s.\n", name, 
					ebl->position, (int)ebl->separator_len, ebl->separator,
					(int)ebl->apex_len, ebl->apex);

			if ((ebl->apex_len > MAX_COMPONENT_SIZE) || (ebl->separator_len > MAX_COMPONENT_SIZE)) {
				LOG(L_ERR, "enum_query(): EBL strings too long\n"); 
				return -1;
			}

			if (ebl->position > 15)  {
				LOG(L_ERR, "enum_query(): EBL position too large (%d)\n", ebl->position); 
				return -1;
			}

			sdl = ebl->position;

			memcpy(separator, ebl->separator, ebl->separator_len);
			separator[ebl->separator_len] = 0;

			memcpy(apex, ebl->apex, ebl->apex_len);
			apex[ebl->apex_len] = 0;
			free_rdata_list(head);
		}
	} else if (!strncasecmp(i_bl_alg.s,"txt",i_bl_alg.len)) {
		sdl = cc_len; /* default */
		memcpy(separator, i_branchlabel.s, i_branchlabel.len);
		separator[i_branchlabel.len] = 0;
		/* no change to apex */

		j = 0;
		memcpy(name, i_branchlabel.s, i_branchlabel.len);
		j += i_branchlabel.len;
		name[j++] = '.';

		for (i = cc_len ; i > 0; i--) {
			name[j++] = user_s[i];
			name[j++] = '.';
		}
		memcpy(name + j, suffix->s, suffix->len + 1);

		head = get_record(name, T_TXT);
		if (head == 0) {
			DBG("enum_query(): No TXT found for %s. Defaulting to %d\n",name, cc_len);
		} else {
			sdl = atoi(((struct txt_rdata*)head->rdata)->txt);
			DBG("enum_query(): TXT record for %s is %d.\n", name, sdl);

			if ((sdl < 0) || (sdl > 10)) {
				LOG(L_ERR, "enum_query(): sdl %d out of bounds. set back to cc_len.\n",sdl);
				sdl = cc_len;
			}
			free_rdata_list(head);
		}
	} else {	/* defaults to CC */
		sdl = cc_len;
		memcpy(separator, i_branchlabel.s, i_branchlabel.len);
		separator[i_branchlabel.len] = 0;
		/* no change to apex */
	}

	j = 0;
	sdl++; /* to avoid comparing i to (sdl+1) */
	for (i = user_len - 1; i > 0; i--) {
		name[j] = user_s[i];
		name[j + 1] = '.';
		j = j + 2;
		if (separator[0] && (i == sdl)) { /* insert the I-ENUM separator here? */
			strcpy(name + j, separator);  /* we've checked string sizes. */
			j += strlen(separator);
			name[j++] = '.';
		}
	}

	memcpy(name + j, apex, strlen(apex)+1);

	return do_query(_msg, string, name, service);
}



/******************* FQUERY *******************/


/*
 * Call enum_fquery_2 with module parameter suffix and default service.
 */
int enum_fquery_0(struct sip_msg* _msg, char* _str1, char* _str2)
{
	return enum_fquery_2(_msg, (char *)(&suffix), (char *)(&service));
}

/*
 * Call enum_fquery_2 with given suffix and default service.
 */
int enum_fquery_1(struct sip_msg* _msg, char* _suffix, char* _str2)
{
	return enum_fquery_2(_msg, _suffix, (char *)(&service));
}

/*
 * See documentation in README file.
 */

int enum_fquery_2(struct sip_msg* _msg, char* _suffix, char* _service)
{
	char *user_s;
	int user_len, i, j, first;
	char name[MAX_DOMAIN_SIZE];
	char uri[MAX_URI_SIZE];
	char new_uri[MAX_URI_SIZE];
	unsigned int priority, curr_prio;
	struct to_body* body;
	struct sip_uri muri;
	qvalue_t q;
	char tostring[17];
	struct rdata* head;
	struct rdata* l;
	struct naptr_rdata* naptr;

	str pattern, replacement, result, new_result;

	str *suffix, *service;

	char string[17];

	/*
	** all of this to pick up the 'to' string
	*/
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

	memcpy(&(tostring[0]), user_s, user_len);
	tostring[user_len] = (char)0;
	/*
	** to here
	*/


	body = get_parsed_from_body(_msg);
	if (!body) return -1;

	if (parse_uri(body->uri.s, body->uri.len, &muri) < 0) {
		LOG(L_ERR, "is_from_user_e164(): Error while parsing From uri\n");
		return -1;
	}

	suffix = (str*)_suffix;
	service = (str*)_service;

	if (is_e164(&(muri.user)) == -1) {
		LOG(L_ERR, "enum_fquery(): uri user is not an E164 number\n");
		return -1;
	}

	user_s = muri.user.s;
	user_len = muri.user.len;

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
		DBG("enum_fquery(): No NAPTR record found for %s.\n", name);
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
			LOG(L_CRIT, "enum_fquery: BUG: null rdata\n");
			continue;
		}

		DBG("enum_fquery(): order %u, pref %u, flen %u, flags '%.*s', slen %u, "
		    "services '%.*s', rlen %u, regexp '%.*s'\n", naptr->order, naptr->pref,
		    naptr->flags_len, (int)(naptr->flags_len), ZSW(naptr->flags),
		    naptr->services_len,
		    (int)(naptr->services_len), ZSW(naptr->services), naptr->regexp_len,
		    (int)(naptr->regexp_len), ZSW(naptr->regexp));

		if (sip_match(naptr, service) == 0) continue;

		if (parse_naptr_regexp(&(naptr->regexp[0]), naptr->regexp_len,
				       &pattern, &replacement) < 0) {
			LOG(L_ERR, "enum_fquery(): parsing of NAPTR regexp failed\n");
			continue;
		}
		result.s = &(uri[0]);
		result.len = MAX_URI_SIZE;
		/* Avoid making copies of pattern and replacement */
		pattern.s[pattern.len] = (char)0;
		replacement.s[replacement.len] = (char)0;
		if (reg_replace(pattern.s, replacement.s, &(tostring[0]),
				&result) < 0) {
			pattern.s[pattern.len] = '!';
			replacement.s[replacement.len] = '!';
			LOG(L_ERR, "enum_fquery(): regexp replace failed\n");
			continue;
		}
		DBG("enum_fquery(): resulted in replacement: '%.*s'\n",
		    result.len, ZSW(result.s));
		pattern.s[pattern.len] = '!';
		replacement.s[replacement.len] = '!';
		
		if (param.len > 0) {
			if (result.len + param.len > MAX_URI_SIZE - 1) {
				LOG(L_ERR, "ERROR: enum_fquery(): URI is too long\n");
				continue;
			}
			new_result.s = &(new_uri[0]);
			new_result.len = MAX_URI_SIZE;
			if (add_uri_param(&result, &param, &new_result) == 0) {
				LOG(L_ERR, "ERROR: enum_fquery(): Parsing of URI failed\n");
				continue;
			}
			if (new_result.len > 0) {
				result = new_result;
			}
		}

		if (first) {
			if (rewrite_uri(_msg, &result) == -1) {
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
			if (append_branch(_msg, &result, 0, 0, q, 0, 0) == -1) {
				goto done;
			}
		}
	}

done:
	free_rdata_list(head);
	return first ? -1 : 1;
}

