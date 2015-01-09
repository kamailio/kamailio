/*
 * Enum and E164 related functions
 *
 * Copyright (C) 2002-2010 Juha Heinanen
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

/*!
 * \file
 * \brief SIP-router enum :: Enum and E164 related functions (module interface)
 * \ingroup enum
 * Module: \ref enum
 */

/*! \defgroup enum Enum and E.164-related functions

 *  Enum module implements [i_]enum_query functions that make an enum query
 * based on the user part of the current Request-URI. These functions
 * assume that the Request URI user part consists of an international
 * phone number of the form +decimal-digits, where the number of digits is
 * at least 2 and at most 15. Out of this number enum_query forms a domain
 * name, where the digits are in reverse order and separated by dots
 * followed by domain suffix that by default is "e164.arpa.". For example,
 * if the user part is +35831234567, the domain name will be
 * "7.6.5.4.3.2.1.3.8.5.3.e164.arpa.". i_enum_query operates in a similar
 * fashion. The only difference is that it adds a label (default "i") to
 * branch off from the default, user-ENUM tree to an infrastructure ENUM
 * tree.
 */



#include <stdlib.h>

#include "enum.h"
#include "../../sr_module.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../ut.h"
#include "../../resolve.h"
#include "../../mem/mem.h"
#include "../../dset.h"
#include "../../qvalue.h"
#include "enum_mod.h"
#include "../../lib/kcore/regexp.h"
#include "../../pvar.h"

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
						LM_ERR("Third ! missing from regexp\n");
						return -1;
					}
				} else {
					LM_ERR("Third ! missing from regexp\n");
					return -2;
				}
			} else {
				LM_ERR("Second ! missing from regexp\n");
				return -3;
			}
		} else {
			LM_ERR("First ! missing from regexp\n");
			return -4;
		}
	} else {
		LM_ERR("Regexp missing\n");
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
 * Checks if argument is an e164 number starting with +
 */
static inline int is_e164(str* _user)
{
	int i;
	char c;
	
	if ((_user->len > 2) && (_user->len < MAX_NUM_LEN) && ((_user->s)[0] == '+')) {
		for (i = 1; i < _user->len; i++) {
			c = (_user->s)[i];
			if ((c < '0') || (c > '9')) return -1;
		}
		return 1;
	} else {
	    return -1;
	}
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
	char proto;
	char *user_s;
	int user_len, i, j;
	char name[MAX_DOMAIN_SIZE];
	char uri[MAX_URI_SIZE];
	struct sip_uri *furi;
	struct sip_uri luri;
	struct rdata* head;

	str* suffix;
	str* service;

	struct rdata* l;
	struct naptr_rdata* naptr;

	str pattern, replacement, result;
	char string[MAX_NUM_LEN];

	if (parse_from_header(_msg) < 0) {
	    LM_ERR("Failed to parse From header\n");
	    return -1;
	}
	
	if(_msg->from==NULL || get_from(_msg)==NULL) {
	    LM_DBG("No From header\n");
	    return -1;
	}

	if ((furi = parse_from_uri(_msg)) == NULL) {
	    LM_ERR("Failed to parse From URI\n");
	    return -1;
	}

	suffix = (str*)_suffix;
	service = (str*)_service;

	if (is_e164(&(furi->user)) == -1) {
	    LM_ERR("From URI user is not an E164 number\n");
	    return -1;
	}

	/* assert: the from user is a valid formatted e164 string */

	user_s = furi->user.s;
	user_len = furi->user.len;

	j = 0;
	for (i = user_len - 1; i > 0; i--) {
		name[j] = user_s[i];
		name[j + 1] = '.';
		j = j + 2;
	}

	memcpy(name + j, suffix->s, suffix->len + 1);

	head = get_record(name, T_NAPTR, RES_ONLY_TYPE);

	if (head == 0) {
		LM_DBG("No NAPTR record found for %s.\n", name);
		return -3;
	}

	/* we have the naptr records, loop and find an srv record with */
	/* same ip address as source ip address, if we do then true is returned */

	for (l = head; l; l = l->next) {

		if (l->type != T_NAPTR) continue; /*should never happen*/
		naptr = (struct naptr_rdata*)l->rdata;
		if (naptr == 0) {
			LM_ERR("Null rdata in DNS response\n");
			free_rdata_list(head);
			return -4;
		}

		LM_DBG("ENUM query on %s: order %u, pref %u, flen %u, flags "
		       "'%.*s', slen %u, services '%.*s', rlen %u, "
		       "regexp '%.*s'\n",
		       name, naptr->order, naptr->pref,
		    naptr->flags_len, (int)(naptr->flags_len), ZSW(naptr->flags), naptr->services_len,
		    (int)(naptr->services_len), ZSW(naptr->services), naptr->regexp_len,
		    (int)(naptr->regexp_len), ZSW(naptr->regexp));

		if (sip_match(naptr, service) != 0) {
			if (parse_naptr_regexp(&(naptr->regexp[0]), naptr->regexp_len,
					 &pattern, &replacement) < 0) {
				free_rdata_list(head); /*clean up*/
				LM_ERR("Parsing of NAPTR regexp failed\n");
				return -5;
			}
#ifdef LATER
			if ((pattern.len == 4) && (strncmp(pattern.s, "^.*$", 4) == 0)) {
				LM_DBG("Resulted in replacement: '%.*s'\n",
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
				LM_ERR("Regexp replace failed\n");
				free_rdata_list(head); /*clean up*/
				return -6;
			}
			LM_DBG("Resulted in replacement: '%.*s'\n",
			    result.len, ZSW(result.s));

			if(parse_uri(result.s, result.len, &luri) < 0)
			{
				LM_ERR("Parsing of URI <%.*s> failed\n",
				       result.len, result.s);
				free_rdata_list(head); /*clean up*/
				return -7;
			}

			pattern.s[pattern.len] = '!';
			replacement.s[replacement.len] = '!';

			zp = 0;
			proto = PROTO_NONE;
			he = sip_resolvehost(&luri.host, &zp, &proto);

			hostent2ip_addr(&addr, he, 0);

			if(ip_addr_cmp(&addr, &_msg->rcv.src_ip))
			{
				free_rdata_list(head);
				return(1);
			}
		}
	}
	free_rdata_list(head); /*clean up*/
	LM_DBG("FAIL\n");

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
	switch (puri.type) {
	case SIP_URI_T:
	    memcpy(at, "sip:", 4);
	    at = at + 4;
	    break;
	case SIPS_URI_T:
	    memcpy(at, "sips:", 5);
	    at = at + 5;
	    break;
	case TEL_URI_T:
	    memcpy(at, "tel:", 4);
	    at = at + 4;
	    break;
	case TELS_URI_T:
	    memcpy(at, "tels:", 5);
	    at = at + 5;
	    break;
	default:
	    LM_ERR("Unknown URI scheme <%d>\n", puri.type);
	    return 0;
	}
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

    head = get_record(name, T_NAPTR, RES_ONLY_TYPE);
    
    if (head == 0) {
	LM_DBG("No NAPTR record found for %s.\n", name);
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
	    LM_ERR("Null rdata in DNS response\n");
	    continue;
	}

	LM_DBG("ENUM query on %s: order %u, pref %u, flen %u, flags '%.*s', "
	       "slen %u, services '%.*s', rlen %u, regexp '%.*s'\n",
	       name, naptr->order, naptr->pref,
	    naptr->flags_len, (int)(naptr->flags_len), ZSW(naptr->flags),
	    naptr->services_len,
	    (int)(naptr->services_len), ZSW(naptr->services), naptr->regexp_len,
	    (int)(naptr->regexp_len), ZSW(naptr->regexp));
	
	if (sip_match(naptr, service) == 0) continue;
	
	if (parse_naptr_regexp(&(naptr->regexp[0]), naptr->regexp_len,
			       &pattern, &replacement) < 0) {
	    LM_ERR("Parsing of NAPTR regexp failed\n");
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
	    LM_ERR("Regexp replace failed\n");
	    continue;
	}
	LM_DBG("Resulted in replacement: '%.*s'\n", result.len, ZSW(result.s));
	pattern.s[pattern.len] = '!';
	replacement.s[replacement.len] = '!';
	
	if (param.len > 0) {
	    if (result.len + param.len > MAX_URI_SIZE - 1) {
		LM_ERR("URI is too long\n");
		continue;
	    }
	    new_result.s = &(new_uri[0]);
	    new_result.len = MAX_URI_SIZE;
	    if (add_uri_param(&result, &param, &new_result) == 0) {
		LM_ERR("Parsing of URI <%.*s> failed\n",
		       result.len, result.s);
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
	    if (append_branch(_msg, &result, 0, 0, q, 0, 0, 0, 0, 0, 0) == -1) {
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
	return enum_query(_msg, &suffix, &service);
}


/*
 * Call enum_query_2 with given suffix and default service.
 */
int enum_query_1(struct sip_msg* _msg, char* _suffix, char* _str2)
{
    str suffix;
  
    if (get_str_fparam(&suffix, _msg, (fparam_t*)_suffix) != 0) {
	LM_ERR("unable to get suffix\n");
	return -1;
    }

    return enum_query(_msg, &suffix, &service);
}


/*
 * Call enum_query_2 with given suffix and service.
 */
int enum_query_2(struct sip_msg* _msg, char* _suffix, char* _service)
{
    str suffix, *service;
  
    if (get_str_fparam(&suffix, _msg, (fparam_t*)_suffix) != 0) {
	LM_ERR("unable to get suffix\n");
	return -1;
    }

    service = (str*)_service;
    if ((service == NULL) || (service->len == 0)) {
	LM_ERR("invalid service parameter");
	return -1;
    }

    return enum_query(_msg, &suffix, service);
}


/*
 * See documentation in README file.
 */
int enum_query(struct sip_msg* _msg, str* suffix, str* service)
{
	char *user_s;
	int user_len, i, j;
	char name[MAX_DOMAIN_SIZE];
	char string[MAX_NUM_LEN];

	LM_DBG("enum_query on suffix <%.*s> service <%.*s>\n",
	       suffix->len, suffix->s, service->len, service->s);

	if (parse_sip_msg_uri(_msg) < 0) {
		LM_ERR("Parsing of R-URI failed\n");
		return -1;
	}

	if (is_e164(&(_msg->parsed_uri.user)) == -1) {
		LM_ERR("R-URI user is not an E164 number\n");
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
	return i_enum_query_2(_msg, (char *)(&i_suffix), (char *)(&service));
}

/*
 * Call enum_query_2 with given suffix and default service.
 */
int i_enum_query_1(struct sip_msg* _msg, char* _suffix, char* _service)
{
	return i_enum_query_2(_msg, _suffix, (char *)(&service));
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

	char string[MAX_NUM_LEN];

	str *suffix, *service;

	suffix = (str*)_suffix;
	service = (str*)_service;

	if (parse_sip_msg_uri(_msg) < 0) {
		LM_ERR("Parsing of R-URI failed\n");
		return -1;
	}

	if (is_e164(&(_msg->parsed_uri.user)) == -1) {
		LM_ERR("R-URI user is not an E164 number\n");
		return -1;
	}

	user_s = _msg->parsed_uri.user.s;
	user_len = _msg->parsed_uri.user.len;

	/* make sure we don't run out of space in strings */
	if (( 2*user_len + MAX_COMPONENT_SIZE + MAX_COMPONENT_SIZE + 4) > MAX_DOMAIN_SIZE) {
		LM_ERR("Strings too long\n");
		return -1;
	}
	if ( i_branchlabel.len > MAX_COMPONENT_SIZE ) {
		LM_ERR("i_branchlabel too long\n");
		return -1;
	}
	if ( suffix->len > MAX_COMPONENT_SIZE ) {
		LM_ERR("Suffix too long\n");
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

		LM_DBG("Looking for EBL record for %s.\n", name); 
		head = get_record(name, T_EBL, RES_ONLY_TYPE);
		if (head == 0) {
			LM_DBG("No EBL found for %s. Defaulting to user ENUM.\n",name);
		} else {
		    	struct ebl_rdata* ebl;
			ebl = (struct ebl_rdata *) head->rdata;

			LM_DBG("EBL record for %s is %d / %.*s / %.*s.\n",
			       name, ebl->position, (int)ebl->separator_len,
			       ebl->separator,(int)ebl->apex_len, ebl->apex);

			if ((ebl->apex_len > MAX_COMPONENT_SIZE) || (ebl->separator_len > MAX_COMPONENT_SIZE)) {
				LM_ERR("EBL strings too long\n"); 
				return -1;
			}

			if (ebl->position > 15)  {
				LM_ERR("EBL position too large (%d)\n",
				       ebl->position); 
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

		head = get_record(name, T_TXT, RES_ONLY_TYPE);
		if (head == 0) {
			LM_DBG("TXT found for %s. Defaulting to %d\n",
			       name, cc_len);
		} else {
			sdl = atoi(((struct txt_rdata*)head->rdata)->txt[0].cstr);
			LM_DBG("TXT record for %s is %d.\n", name, sdl);

			if ((sdl < 0) || (sdl > 10)) {
				LM_ERR("Sdl %d out of bounds. Set back to cc_len.\n", sdl);
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
 * Call enum_pv_query_3 with pv arg, module parameter suffix,
 * and default service.
 */
int enum_pv_query_1(struct sip_msg* _msg, char* _sp)
{
    return enum_pv_query_3(_msg, _sp, (char *)(&suffix), (char *)(&service));
}

/*
 * Call enum_pv_query_3 with pv and suffix args and default service.
 */
int enum_pv_query_2(struct sip_msg* _msg, char* _sp, char* _suffix)
{
    return enum_pv_query_3(_msg, _sp, _suffix, (char *)(&service));
}

/*
 * See documentation in README file.
 */

int enum_pv_query_3(struct sip_msg* _msg, char* _sp, char* _suffix,
		    char* _service)
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
	str *suffix, *service;
	char string[17];
	pv_spec_t *sp;
	pv_value_t pv_val;

	sp = (pv_spec_t *)_sp;
	suffix = (str*)_suffix;
	service = (str*)_service;

	/*
	 * Get E.164 number from pseudo variable
         */
	if (sp && (pv_get_spec_value(_msg, sp, &pv_val) == 0)) {
	    if (pv_val.flags & PV_VAL_STR) {
		if (pv_val.rs.len == 0 || pv_val.rs.s == NULL) {
		    LM_DBG("Missing E.164 number\n");
		    return -1;
		}
	    } else {
		LM_DBG("Pseudo variable value is not string\n");
		return -1;
	}
	} else {
	    LM_DBG("Cannot get pseudo variable value\n");
	    return -1;
	}
	if (is_e164(&(pv_val.rs)) == -1) {
	    LM_ERR("pseudo variable does not contain an E164 number\n");
	    return -1;
	}

	user_s = pv_val.rs.s;
	user_len = pv_val.rs.len;

	memcpy(&(string[0]), user_s, user_len);
	string[user_len] = (char)0;

	j = 0;
	for (i = user_len - 1; i > 0; i--) {
		name[j] = user_s[i];
		name[j + 1] = '.';
		j = j + 2;
	}

	memcpy(name + j, suffix->s, suffix->len + 1);

	head = get_record(name, T_NAPTR, RES_ONLY_TYPE);

	if (head == 0) {
		LM_DBG("No NAPTR record found for %s.\n", name);
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
			LM_ERR("Null rdata in DNS response\n");
			continue;
		}

		LM_DBG("ENUM query on %s: order %u, pref %u, flen %u, flags "
		       "'%.*s', slen %u, services '%.*s', rlen %u, "
		       "regexp '%.*s'\n",
		       name, naptr->order, naptr->pref,
		    naptr->flags_len, (int)(naptr->flags_len), ZSW(naptr->flags),
		    naptr->services_len,
		    (int)(naptr->services_len), ZSW(naptr->services), naptr->regexp_len,
		    (int)(naptr->regexp_len), ZSW(naptr->regexp));

		if (sip_match(naptr, service) == 0) continue;

		if (parse_naptr_regexp(&(naptr->regexp[0]), naptr->regexp_len,
				       &pattern, &replacement) < 0) {
			LM_ERR("Parsing of NAPTR regexp failed\n");
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
			LM_ERR("Regexp replace failed\n");
			continue;
		}
		LM_DBG("Resulted in replacement: '%.*s'\n",
		       result.len, ZSW(result.s));
		pattern.s[pattern.len] = '!';
		replacement.s[replacement.len] = '!';
		
		if (param.len > 0) {
			if (result.len + param.len > MAX_URI_SIZE - 1) {
				LM_ERR("URI is too long\n");
				continue;
			}
			new_result.s = &(new_uri[0]);
			new_result.len = MAX_URI_SIZE;
			if (add_uri_param(&result, &param, &new_result) == 0) {
				LM_ERR("Parsing of URI <%.*s> failed\n",
				       result.len, result.s);
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
			if (append_branch(_msg, &result, 0, 0, q, 0, 0, 0, 0, 0, 0)
			    == -1) {
				goto done;
			}
		}
	}

done:
	free_rdata_list(head);
	return first ? -1 : 1;
}

