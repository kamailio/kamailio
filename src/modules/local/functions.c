/*
 * Implementation of local functions
 *
 * Copyright (C) 2002-2023 TutPro Inc.
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


#include <arpa/inet.h>
#include "../../core/mem/mem.h"
#include "../../core/action.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/digest/digest.h"
#include "../../core/dprint.h"
#include "../../core/pvar.h"
#include "../../core/usr_avp.h"
#include "../../core/dset.h"
#include "local.h"

/* Database stuff */

#define QUERY_LEN 2048


/*
 * Gets str value of int named AVP
 */
static inline int get_attr(int attr, str **val) 
{
    static int_str name, value;
    
    name.n = attr;
    
    if (search_first_avp(AVP_VAL_STR, name, &value, 0)) {
 	*val = &(value.s);
 	return 1;
    }
    
    return 0;
}


/*
 * Sets str value of int AVP
 */
static inline int set_attr(int attr, str* val) 
{
	int_str name, value;

	name.n = attr;
	value.s = *val;
	if (add_avp(AVP_VAL_STR, name, value) != 0) {
	    LM_ERR("add_avp <%ld> failed\n", name.n);
	    return -1;
	}
	return 1;
}	


/*
 * Check if parameter is an e164 number.
 */
static inline int e164_check(str* _user)
{
    int i;
    char c;
    
    if ((_user->len > 2) && (_user->len < 17) && ((_user->s)[0] == '+')) {
	for (i = 1; i <= _user->len; i++) {
	    c = (_user->s)[i];
	    if (c < '0' && c > '9') return 0;
	}
	return 1;
    }
    return 0;
}

#define ANNOUNCEMENT 1
#define CALL_CENTER 2
#define CONFERENCE 3
#define EARLY_ANNOUNCE 4
#define ECHO 5
#define TRANSFER 6
#define VOICEMAIL 7
#define EXTENSIONS 9

/* 
 * If first sub-domain of domain corresponds to a SEMS service, returns code
 * of that service.  Otherwise, returns 0.
 */
static inline unsigned int sems_service_code(str *domain)
{
    char second;

    if (domain->len < 3) return 0;
    second = *((domain->s) + 1);
    if (*((domain->s) + 2) != '.') return 0;
    switch (*(domain->s)) {
    case 'v':
	  if (second == 'm') return VOICEMAIL;
	  return 0;
    case 'c':
	  if (second == 'f') return CONFERENCE;
	  if (second == 'c') return CALL_CENTER;
	  return 0;
    case 'a':
	  if (second == 's') return ANNOUNCEMENT;
	  return 0;
    case 'e':
	  switch (second) {
	  case 'a': return EARLY_ANNOUNCE;
	  case 'c': return ECHO;
	  case 'x': return EXTENSIONS;
	  default:
		return 0;
	  }
    case 't':
	  if (second == 'r') return TRANSFER;
	  return 0;
    default:
	  return 0;
    }
}


/*
 * Check if domain given by pseudo variable parameter is a SEMS domain.
 */
int is_domain_sems(struct sip_msg* _msg, char* _sp, char* _s2)
{
    pv_spec_t *sp;
    pv_value_t pv_val;
    str domain;
    unsigned int code;

    sp = (pv_spec_t *)_sp;

    if (sp && (pv_get_spec_value(_msg, sp, &pv_val) == 0)) {
	if (pv_val.flags & PV_VAL_STR) {
	    domain = pv_val.rs;
	    if (domain.len == 0 || domain.s == NULL) {
		LM_DBG("missing domain name\n");
		return -1;
	    }
	    code =  sems_service_code(&domain);
	    if (code) {
		domain.s = domain.s + 3;
		domain.len = domain.len - 3;
		if (domain_api.is_domain_local(&domain) == 1) {
		    return code;
		} else {
		    return -1;
		}
	    } else {
		return -1;
	    }
	} else {
	    LM_DBG("pseudo variable value is not string\n");
	    return -1;
	}
    } else {
	LM_DBG("cannot get pseudo variable value\n");
	return -1;
    }
}


/* Stores diversion reason to diversion_reason_avp */	
static inline int set_diversion_reason(char* s, int len)
{
    str reason;
    
    reason.s = s;
    reason.len = len;
    if (set_attr(diversion_reason_avp, &reason) == -1) {
	LM_ERR("failed to set diversion_reason_avp\n");
	return -1;
    } else {
	return 1;
    }
}


/*
 * Replaces Request URI with a possible forwarding URI returned by
 * previous radius_does_uri_exist function call and appends a Diversion
 * header to the request.  The condition of the forwarding URI must match
 * the one given as argument.
 */
int forwarding(struct sip_msg* _m, char* _condition, char* _s2)
{
    condition_t condition;
    str *val;

    condition = (condition_t)_condition;
    switch (condition) {

    case UNC:
	if (get_attr(callee_cfunc_avp, &val)) {
	    if (rewrite_uri(_m, val) == 1) {
		return set_diversion_reason("unconditional", 13);
	    }
	}
	return -1;

    case B:
	if (get_attr(callee_cfb_avp, &val)) {
	    if (km_append_branch(_m, val, 0, 0, 500, 0, 0) == 1) {
		return set_diversion_reason("user-busy", 9);
	    }
	}
	return -1;

    case NA:
	if (get_attr(callee_cfna_avp, &val)) {
	    if (km_append_branch(_m, val, 0, 0, 500, 0, 0) == 1) {
		return set_diversion_reason("no-answer", 9);
	    }
	}
	return -1;

    case UNV:
	if (get_attr(callee_cfunv_avp, &val)) {
	    if (rewrite_uri(_m, val) == 1) {
		return set_diversion_reason("unavailable", 11);
	    }
	}
	return -1;

    default:
	LM_ERR("unknown diversion condition <%d>\n", (int)condition);
	break;
    }

    return -1;
}


/*
 * Checks if the request will be diverted (forwarded or redirected)
 * on a condition.
 */
int diverting_on(struct sip_msg* _m, char* _condition, char* _str2)
{
    condition_t condition;
    str *val;

    condition = (condition_t)_condition;

    switch (condition) {
	
    case UNC:
	return (get_attr(callee_cfunc_avp, &val) ||
		get_attr(callee_crunc_avp, &val)) ? 1 : -1;

    case B:
	return (get_attr(callee_cfb_avp, &val) ||
		get_attr(callee_crb_avp, &val)) ? 1 : -1;

    case NA:
	return (get_attr(callee_cfna_avp, &val) ||
		get_attr(callee_crna_avp, &val)) ? 1 : -1;

    case UNV:
	return (get_attr(callee_cfunv_avp, &val) ||
		get_attr(callee_crunv_avp, &val)) ? 1 : -1;
	
    default:
	LM_ERR("unknown diversion condition <%d>\n", (int)condition);
	break;
    }
    
    return -1;
}


/*
 * Checks if the Request URI has been redirected under the condition given
 * as argument.  Possible conditions are "unconditonal", "unavailable",
 * "busy" and "no-answer".  If so, replaces Request-URI with the redirection
 * URI obtained by preceding successful radius_does_uri_exist call.
 */
int redirecting(struct sip_msg* _m, char* _condition, char* _str2)
{
    condition_t condition;
    str *val;

    condition = (condition_t)_condition;
	
    switch (condition) {

    case UNC:
	if (get_attr(callee_crunc_avp, &val)) {
	    if (rewrite_uri(_m, val) == 1) {
		return set_diversion_reason("unconditional", 13);
	    }
	}
	return -1;

    case B:
	if (get_attr(callee_crb_avp, &val)) {
	    if (rewrite_uri(_m, val) == 1) {
		return set_diversion_reason("user-busy", 9);
	    }
	}
	return -1;

    case NA:
	if (get_attr(callee_crna_avp, &val)) {
	    if (rewrite_uri(_m, val) == 1) {
		return set_diversion_reason("no-answer", 9);
	    }
	}
	return -1;

    case UNV:
	if (get_attr(callee_crunv_avp, &val)) {
	    if (rewrite_uri(_m, val) == 1) {
		return set_diversion_reason("unavailable", 11);
	    }
	}
	return -1;

    default:
	LM_ERR("unknown diversion condition <%d>\n", (int)condition);
	break;
    }

    return -1;
}
