/* 
 * $Id$
 *
 * Copyright (C) 2006 Otmar Lendl & Klaus Darilion
 *
 * Based on the ENUM and domain module.
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
 * History:
 * --------
 *  2006-04-20  Initial Version
 *  2006-09-08  Updated to -02 version, added support for D2P+SIP:std
 */


/*!
 * \file
 * \brief Domain Policy related functions
 */


#include "domainpolicy_mod.h"
#include "domainpolicy.h"
#include "../../lib/srdb1/db.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../ut.h"
#include "../../dset.h"
#include "../../route.h"
#include "../../ip_addr.h"
#include "../../socket_info.h"

#include "../../resolve.h"
#include "../../lib/kcore/regexp.h"

#define IS_D2PNAPTR(naptr) ((naptr->services_len >= 7) && (!strncasecmp("D2P+SIP", naptr->services, 7)))

static db1_con_t* db_handle=0;
static db_func_t domainpolicy_dbf;

/*
 * some helper structs + functions to help build up the AVPs.
 * We can't immediately store them in AVPs as a later non-matched
 * rule can result in junking all the AVPs added up to that moment.
 *
 * Thus we store them temporarily in an avp_stack.
 */
#define AVPMAXSIZE 120
#define AVPSTACKSIZE 32

struct avp {
    char att[AVPMAXSIZE];
    char val[AVPMAXSIZE];
};

struct avp_stack {
    int succeeded;
    int i;
    struct avp avp[AVPSTACKSIZE];
};

/*
 * Push avp-pair on stack.
 *
 * return 0 on failure.
 */
static int stack_push(struct avp_stack *stack, char *att, char *val) {
    int i;
    if (stack->i >= (AVPSTACKSIZE-1)) {
	LM_ERR("exceeded stack size.!\n");
	return(0);
    }

    i = (stack->i)++;
    strncpy(stack->avp[i].att, att, AVPMAXSIZE - 1);
    strncpy(stack->avp[i].val, val, AVPMAXSIZE - 1);

    stack->succeeded = 1;

    return(1);
}


static void stack_reset(struct avp_stack *stack) {
    stack->i 		= 0;
    stack->succeeded	= 0;
}

static int stack_succeeded(struct avp_stack *stack) {
    return(stack->succeeded);
}

static void stack_to_avp(struct avp_stack *stack) {
	int j;
	int_str  avp_att;
	int_str  avp_val;
	unsigned int intval;

	intval=2;

	for(j=0; j< stack->i; j++) {
		/* AVP names can be integer or string based */
		LM_DBG("process AVP: name='%s' value='%s'\n", 
					stack->avp[j].att, stack->avp[j].val);

		/* if the second character is a ':', ignore the prefix
		 * this allows specifying the name with i:... or s:... too
		 * Note: the first character is ignored!!!
		 */
		if ( stack->avp[j].att[0] && stack->avp[j].att[1]==':' ) { 

			switch (stack->avp[j].att[0]) {
			case 'i':
			case 'I':
				intval = 1;
				break;
			case 's':
			case 'S':
				intval = 0;
				break;
			default:
				LM_ERR("invalid type '%c'\n",stack->avp[j].att[0]);
				continue;
			}
			avp_att.s.s = (char *) &(stack->avp[j].att[2]); 
		} else {
			avp_att.s.s = stack->avp[j].att;
		}
		avp_att.s.len = strlen(avp_att.s.s);
		if (!avp_att.s.len) {
			LM_ERR("empty AVP name string!\n");
			continue;
		}

		avp_val.s.s = stack->avp[j].val; 
		avp_val.s.len = strlen(avp_val.s.s);

		if (intval==1) {
			/* integer type explicitely forced with i: */
			if (str2int(&(avp_att.s), &intval) == 0) {
				/* integer named AVP */
				if (!intval) {
					LM_ERR("nameless integer AVP!\n");
					continue;
				}
				avp_att.n = intval;
				LM_DBG("create integer named AVP <i:%d>\n", avp_att.n);
				add_avp(AVP_VAL_STR, avp_att, avp_val);
				continue;
			} else { 
				LM_ERR("integer AVP is not an integer!\n");
				continue;
			}
		} 

		if (intval==2) {
			/* string type undefined */
			/* convert name into integer. if it succeeds then it is
			 * an integer named AVP. If it fails, then it is a string
			 * named AVP
			 */
			if (str2int(&(avp_att.s), &intval) == 0) {
				/* integer named AVP */
				if (!intval) {
					LM_ERR("nameless integer AVP!\n");
					continue;
				}
				avp_att.n = intval;
				LM_DBG("create integer named AVP <i:%d>\n", avp_att.n);
				add_avp(AVP_VAL_STR, avp_att, avp_val);
				continue;
			} else { 
				LM_DBG("create string named AVP <s:%.*s>\n", 
						avp_att.s.len, ZSW(avp_att.s.s));
				add_avp(AVP_NAME_STR | AVP_VAL_STR, avp_att, avp_val);
				continue;
			}
		} 

		/* intval==0, string type explicitely forced with s: */
		LM_DBG("create string named AVP <s:%.*s>\n", 
				avp_att.s.len, ZSW(avp_att.s.s));
		add_avp(AVP_NAME_STR | AVP_VAL_STR, avp_att, avp_val);
	}
}

/* helper db functions*/

/*!
 * \brief Bind the database interface
 * \param db_url database url
 * \return -1 on failure, 0 on success
 */
int domainpolicy_db_bind(const str* db_url)
{
	if (db_bind_mod(db_url, &domainpolicy_dbf )) {
		LM_CRIT("cannot bind to database module! "
		"Did you forget to load a database module ?\n");
		return -1;
	}
	return 0;
}


/*!
 * \brief Initialize the database connection
 * \param db_url database url
 * \return -1 on failure, 0 on success
 */
int domainpolicy_db_init(const str* db_url)
{
	if (domainpolicy_dbf.init==0){
		LM_CRIT("unbound database module\n");
		goto error;
	}
	db_handle=domainpolicy_dbf.init(db_url);
	if (db_handle==0){
		LM_CRIT("cannot initialize database connection\n");
		goto error;
	}
	return 0;
error:
	return -1;
}


/*!
 * \brief Close the database connection
 */
void domainpolicy_db_close(void)
{
	if (db_handle && domainpolicy_dbf.close){
		domainpolicy_dbf.close(db_handle);
		db_handle=0;
	}
}


/*!
 * \brief Check the database table version
 * \param db_url database URL
 * \param name table name
 * \return -1 on failure, positive database version on success
 */
int domainpolicy_db_ver(const str* db_url, const str* name)
{
	int ver;
	db1_con_t* dbh;

	if (domainpolicy_dbf.init==0){
		LM_CRIT("unbound database\n");
		return -1;
	}
	dbh=domainpolicy_dbf.init(db_url);
	if (dbh==0){
		LM_CRIT("null database handler\n");
		return -1;
	}
	ver=db_table_version(&domainpolicy_dbf, dbh, name);
	domainpolicy_dbf.close(dbh);
	return ver;
}

/***************************/
/*
 *
 * code from enum.c
 *
 * should be moved to some DDDS support module instead of code-duplication
 *
 *
 */

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
						LM_ERR("third ! missing from regexp\n");
						return -1;
					}
				} else {
					LM_ERR("third ! missing from regexp\n");
					return -2;
				}
			} else {
				LM_ERR("second ! missing from regexp\n");
				return -3;
			}
		} else {
			LM_ERR("first ! missing from regexp\n");
			return -4;
		}
	} else {
		LM_ERR("regexp missing\n");
		return -5;
	}
}


/*
 * Tests if one result record is "greater" that the other.  Non-NAPTR records
 * greater that NAPTR record.  An invalid NAPTR record is greater than a 
 * valid one.  Valid NAPTR records are compared based on their
 * (order,preference).
 *
 * Naptrs without D2P+SIP service field are greater.
 *
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

	if (!IS_D2PNAPTR(na))
	    	return 1;
	
	if (!IS_D2PNAPTR(nb))
	    	return 0;

	
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
 * input: rule straight from the DDDS + avp-stack.
 *
 * output: adds found rules to the stack and return
 * 	1 on success
 * 	0 on failure
 */
static int check_rule(str *rule, char *service, int service_len, struct avp_stack *stack) {

    /* for the select */
    db_key_t keys[2];
    db_val_t vals[2];
    db_key_t cols[4]; 
    db1_res_t* res;
    db_row_t* row;
    db_val_t* val;
    int	i;
    char *type;
    int type_len;

    LM_INFO("checking for '%.*s'.\n", rule->len, ZSW(rule->s));

    if ((service_len != 11) || (strncasecmp("d2p+sip:fed", service, 11) && 
	    strncasecmp("d2p+sip:std", service, 11)  && strncasecmp("d2p+sip:dom", service, 11))) {
    	LM_ERR("can only cope with d2p+sip:fed, d2p+sip:std,and d2p+sip:dom "
				"for now (and not %.*s).\n", service_len, service);
	return(0);
    }

    type = service + 8;
    type_len = service_len - 8;

    if (domainpolicy_dbf.use_table(db_handle, &domainpolicy_table) < 0) {
	    LM_ERR("failed to domainpolicy table\n");
	    return -1;
    }

    keys[0]=&domainpolicy_col_rule;
    keys[1]=&domainpolicy_col_type;
    cols[0]=&domainpolicy_col_rule;
    cols[1]=&domainpolicy_col_type;
    cols[2]=&domainpolicy_col_att;
    cols[3]=&domainpolicy_col_val;

    VAL_TYPE(&vals[0]) = DB1_STR;
    VAL_NULL(&vals[0]) = 0;
    VAL_STR(&vals[0]).s = rule->s;
    VAL_STR(&vals[0]).len = rule->len;

    VAL_TYPE(&vals[1]) = DB1_STR;
    VAL_NULL(&vals[1]) = 0;
    VAL_STR(&vals[1]).s = type;
    VAL_STR(&vals[1]).len = type_len;

    /*
     * SELECT rule, att, val from domainpolicy where rule = "..."
     */

    if (domainpolicy_dbf.query(db_handle, keys, 0, vals, cols, 2, 4, 0, &res) < 0
		    ) {
	    LM_ERR("querying database\n");
	    return -1;
    }
    
    LM_INFO("querying database OK\n");

    if (RES_ROW_N(res) == 0) {
	    LM_DBG("rule '%.*s' is not know.\n", 
		rule->len, ZSW(rule->s));
	    domainpolicy_dbf.free_result(db_handle, res);
	    return 0;
    } else {
	    LM_DBG("rule '%.*s' is known\n", rule->len, ZSW(rule->s));

	    row = RES_ROWS(res);

	    for(i = 0; i < RES_ROW_N(res); i++) {
			if (ROW_N(row + i) != 4) {
	    	    LM_ERR("unexpected cell count\n");
				return(-1);
			}

			val = ROW_VALUES(row + i);

			if ((VAL_TYPE(val) != DB1_STRING) || 
				(VAL_TYPE(val+1) != DB1_STRING) ||
				(VAL_TYPE(val+2) != DB1_STRING) ||
				(VAL_TYPE(val+3) != DB1_STRING)) {
					LM_ERR("unexpected cell types\n");
			    return(-1);
			}

			if (VAL_NULL(val+2) || VAL_NULL(val+3)) {
				LM_INFO("db returned NULL values. Fine with us.\n");
				continue;
			}

			LM_INFO("DB returned %s/%s \n",VAL_STRING(val+2),VAL_STRING(val+3));


			if (!stack_push(stack, (char *) VAL_STRING(val+2), 
					(char *) VAL_STRING(val+3))) {
			    return(-1);
			}
	    }
	    domainpolicy_dbf.free_result(db_handle, res);
	    return 1;
    }
}

int dp_can_connect_str(str *domain, int rec_level) {
    struct rdata* head;
    struct rdata* l;
    struct naptr_rdata* naptr;
    struct naptr_rdata* next_naptr;
    int	   ret;
    str	   newdomain;
    char   uri[MAX_URI_SIZE];
    struct avp_stack stack;
    int    last_order = -1;
    int    failed = 0;
    int    found_anything = 0;

    str pattern, replacement, result;

    stack_reset(&stack);
    /* If we're in a recursive call, set the domain-replacement */
    if ( rec_level > 0 ) {
	stack_push(&stack, domain_replacement_name.s.s, domain->s);
	stack.succeeded = 0;
    }

    if (rec_level > MAX_DDDS_RECURSIONS) {
    	LM_ERR("too many indirect NAPTRs. Aborting at %.*s.\n", domain->len,
				ZSW(domain->s));
		return(DP_DDDS_RET_DNSERROR);
    }

    LM_INFO("looking up Domain itself: %.*s\n",domain->len, ZSW(domain->s));
    ret = check_rule(domain,"D2P+sip:dom", 11, &stack);

    if (ret == 1) {
	LM_INFO("found a match on domain itself\n");
	stack_to_avp(&stack);
	return(DP_DDDS_RET_POSITIVE);
    } else if (ret == 0) {
	LM_INFO("no match on domain itself.\n");
	stack_reset(&stack);
	/* If we're in a recursive call, set the domain-replacement */
	if ( rec_level > 0 ) {
	    stack_push(&stack, domain_replacement_name.s.s, (char *) domain->s);
	    stack.succeeded = 0;
	}
    } else {
	return(DP_DDDS_RET_DNSERROR);	/* actually: DB error */
    }

    LM_INFO("doing DDDS with %.*s\n",domain->len, ZSW(domain->s));
    head = get_record(domain->s, T_NAPTR, RES_ONLY_TYPE);
    if (head == 0) {
    	LM_NOTICE("no NAPTR record found for %.*s.\n", 
				domain->len, ZSW(domain->s));
    	return(DP_DDDS_RET_NOTFOUND);
    }

    LM_DBG("found the following NAPTRs: \n");
    for (l = head; l; l = l->next) {
	if (l->type != T_NAPTR) {
	    LM_DBG("found non-NAPTR record.\n");
	    continue; /*should never happen*/
	}
	naptr = (struct naptr_rdata*)l->rdata;
	if (naptr == 0) {
		LM_CRIT("null rdata\n");
		continue;
	}
	LM_DBG("order %u, pref %u, flen %u, flags '%.*s', slen %u, "
	    "services '%.*s', rlen %u, regexp '%.*s', repl '%s'\n", 
		naptr->order, naptr->pref,
	    naptr->flags_len, (int)(naptr->flags_len), ZSW(naptr->flags),
	    naptr->services_len,
	    (int)(naptr->services_len), ZSW(naptr->services), naptr->regexp_len,
	    (int)(naptr->regexp_len), ZSW(naptr->regexp),
	    ZSW(naptr->repl)
	    );
    }


    LM_DBG("sorting...\n");
    naptr_sort(&head);

    for (l = head; l; l = l->next) {

	if (l->type != T_NAPTR) continue; /*should never happen*/
	naptr = (struct naptr_rdata*)l->rdata;
	if (naptr == 0) {
		LM_CRIT("null rdata\n");
		continue;
	}

	LM_DBG("considering order %u, pref %u, flen %u, flags '%.*s', slen %u, "
	    "services '%.*s', rlen %u, regexp '%.*s', repl '%s'\n", 
		naptr->order, naptr->pref,
	    naptr->flags_len, (int)(naptr->flags_len), ZSW(naptr->flags),
	    naptr->services_len,
	    (int)(naptr->services_len), ZSW(naptr->services), naptr->regexp_len,
	    (int)(naptr->regexp_len), ZSW(naptr->regexp),
	    ZSW(naptr->repl)
	    );

	/*
	 * New order? then we check whether the had success during the last one.
	 * If yes, we can leave the loop.
	 */
	if (last_order != naptr->order) {
	    	last_order = naptr->order;
		failed = 0;

		if (stack_succeeded(&stack)) {
    		LM_INFO("we don't need to consider further orders "
						"(starting with %d).\n",last_order);
		    break;
		}
	} else if (failed) {
	    LM_INFO("order %d has already failed.\n",last_order);
	    continue;
	}


	/*
	 * NAPTRs we don't care about
	 */
	if (!IS_D2PNAPTR(naptr)) 
	    continue;

	/*
	 * once we've been here, don't return DP_DDDS_RET_NOTFOUND
	 */
	found_anything = 1;

	next_naptr = NULL;
	if (l->next && (l->next->type == T_NAPTR)) {
	     next_naptr = (struct naptr_rdata*)l->next->rdata;
	}

	/*
	 * Non-terminal?
	 */
	if ((naptr->services_len == 7) && !strncasecmp("D2P+SIP", naptr->services,7) && (naptr->flags_len == 0)){
	    LM_INFO("found non-terminal NAPTR\n");

	    /*
	     * This needs to be the only record with this order.
	     */
	    if (next_naptr && (next_naptr->order == naptr->order) && IS_D2PNAPTR(next_naptr)) {
	    	LM_ERR("non-terminal NAPTR needs to be the only one "
					"with this order %.*s.\n", domain->len, ZSW(domain->s));

		return(DP_DDDS_RET_DNSERROR);
	    }

	    newdomain.s = naptr->repl;
	    newdomain.len = strlen(naptr->repl);

	    ret = dp_can_connect_str(&newdomain, rec_level + 1);

	    if (ret == DP_DDDS_RET_POSITIVE)	/* succeeded, we're done. */
		return(ret);

	    if (ret == DP_DDDS_RET_NEGATIVE)	/* found rules, did not work */
		continue;			/* look for more rules */

	    if (ret == DP_DDDS_RET_DNSERROR)	/* errors during lookup */
		return(ret);			/* report them */

	    if (ret == DP_DDDS_RET_NOTFOUND)	/* no entries in linked domain? */
		return(ret);			/* ok, fine. go with that */

	    continue; /* not reached */
	}

	/*
	 * wrong kind of terminal
	 */
	if ((naptr->flags_len != 1) || (tolower(naptr->flags[0]) != 'u')) {
	    LM_ERR("terminal NAPTR needs flag = 'u' and not '%.*s'.\n",
					(int)naptr->flags_len, ZSW(naptr->flags));
		/*
		 * It's not that clear what we should do now: Ignore this records or regard it as failed.
		 * We go with "ignore" for now.
		 */
		continue;
	}

	if (parse_naptr_regexp(&(naptr->regexp[0]), naptr->regexp_len,
			       &pattern, &replacement) < 0) {
		LM_ERR("parsing of NAPTR regexp failed\n");
		continue;
	}
	result.s = &(uri[0]);
	result.len = MAX_URI_SIZE;

	/* Avoid making copies of pattern and replacement */
	pattern.s[pattern.len] = (char)0;
	replacement.s[replacement.len] = (char)0;
	if (reg_replace(pattern.s, replacement.s, domain->s,
			&result) < 0) {
		pattern.s[pattern.len] = '!';
		replacement.s[replacement.len] = '!';
		LM_ERR("regexp replace failed\n");
		continue;
	}
	LM_INFO("resulted in replacement: '%.*s'\n", result.len, ZSW(result.s));
	pattern.s[pattern.len] = '!';
	replacement.s[replacement.len] = '!';

	ret = check_rule(&result,naptr->services,naptr->services_len, &stack);

	if (ret == 1) {
	    LM_INFO("positive return\n");
	} else if (ret == 0) {
	    LM_INFO("check_rule failed.\n");
	    stack_reset(&stack);
	    /* If we're in a recursive call, set the domain-replacement */
	    if ( rec_level > 0 ) {
		stack_push(&stack, domain_replacement_name.s.s, (char *) domain->s);
		stack.succeeded = 0;
	    }
	    failed = 1;
	} else {
	    return(DP_DDDS_RET_DNSERROR);
    	}
    }

    if (stack_succeeded(&stack)) {
        LM_INFO("calling stack_to_avp.\n");
		stack_to_avp(&stack);
		return(DP_DDDS_RET_POSITIVE);
    }

    LM_INFO("returning %d.\n", 
	    (found_anything ? DP_DDDS_RET_NEGATIVE : DP_DDDS_RET_NOTFOUND));
    return(  found_anything ? DP_DDDS_RET_NEGATIVE : DP_DDDS_RET_NOTFOUND );
}


/*!
 * \brief Check if host in Request URI has DP-DDDS NAPTRs and if we can connect to them
 * \param _msg SIP message
 * \param _s1 unused
 * \param _s2 unused
 * \return negative on failure, positive on success
 */
int dp_can_connect(struct sip_msg* _msg, char* _s1, char* _s2) {

	static char domainname[MAX_DOMAIN_SIZE];
	str domain;
	int ret;

	if (!is_route_type(REQUEST_ROUTE)) {
		LM_ERR("unsupported route type\n");
		return -1;
	}

	if (parse_sip_msg_uri(_msg) < 0) {
		LM_ERR("failed to parse R-URI\n");
		return -1;
	}

	if (_msg->parsed_uri.host.len >= MAX_DOMAIN_SIZE) {
		LM_ERR("domain buffer to small\n");
		return -1;
	}

	/* copy domain into static buffer as later we sometimes need \0
	 * terminated strings
	 */
	domain.s = (char *) &(domainname[0]);
	domain.len = _msg->parsed_uri.host.len;
	memcpy(domain.s, _msg->parsed_uri.host.s, domain.len);
	domainname[domain.len] = '\0';

	LM_DBG("domain is %.*s.\n", domain.len, ZSW(domain.s));

	ret = dp_can_connect_str(&domain,0);
	LM_DBG("returning %d.\n", ret);
	return(ret);
}


/*!
 * \brief Apply DP-DDDS policy to current SIP message
 *
 * Apply DP-DDDS policy to current SIP message. This means
 * build a new destination URI from the policy AVP and export it
 * as AVP. Then in kamailio.cfg this new target AVP can be pushed
 * into the destination URI $duri
 * \param _msg SIP message
 * \param _s1 unused
 * \param _s2 unused
 * \return negative on failure, positive on succes
 */
int dp_apply_policy(struct sip_msg* _msg, char* _s1, char* _s2) {

	str *domain;
	int_str val;
	struct usr_avp *avp;

	char duri[MAX_URI_SIZE];
	str duri_str;
	int len, didsomething;
	char *at; /* pointer to current location inside duri */

	str host;
	int port, proto;
	struct socket_info* si;

	if (!is_route_type(REQUEST_ROUTE)) {
		LM_ERR("unsupported route type\n");
		return -1;
	}

	/*
	 * set the send_socket
	 */

	/* search for send_socket AVP */
	avp = search_first_avp(send_socket_avp_name_str, send_socket_name, &val, 0);
	if (avp) {
		if ( !(avp->flags&AVP_VAL_STR) ||  !val.s.s || !val.s.len) {
			LM_ERR("empty or non-string send_socket_avp, "
					"return with error ...\n");
			return -1;
		}
		LM_DBG("send_socket_avp found = '%.*s'\n", val.s.len, ZSW(val.s.s));
		/* parse phostport - AVP str val is asciiz */
		/* FIXME: This code relies on the fact that the string value of an AVP
		 * is zero terminated, which may or may not be true in the future */
		if (parse_phostport(val.s.s, &(host.s), &(host.len), &port, &proto)) {
			LM_ERR("could not parse send_socket, return with error ...\n");
			return -1;
		}
		si = grep_sock_info( &host, (unsigned short) port, (unsigned short) proto);
		if (si) {
			set_force_socket(_msg, si);
		} else {
			LM_WARN("could not find socket for"
					"send_socket '%.*s'\n", val.s.len, ZSW(val.s.s));
		}
	} else {
		LM_DBG("send_socket_avp not found\n");
	}

	/*
	 * set the destination URI
	 */

	didsomething = 0; /* if no AVP is set, there is no need to set the DURI in the end */
	
	if (parse_sip_msg_uri(_msg) < 0) {
		LM_ERR("failed to parse R-URI\n");
		return -1;
	}

	at = (char *)&(duri[0]);
	len = 0;
	if ( (len + 4) >  MAX_URI_SIZE) {
		LM_ERR("duri buffer to small to add uri schema\n");
		return -1;
	}
	memcpy(at, "sip:", 4); at = at + 4; len = len + 4;

	domain = &(_msg->parsed_uri.host);
	LM_DBG("domain is %.*s.\n", domain->len, ZSW(domain->s));

	/* search for prefix and add it to duri buffer */
	avp = search_first_avp(domain_prefix_avp_name_str, domain_prefix_name, &val, 0);
	if (avp) {
		if ( !(avp->flags&AVP_VAL_STR) ||  !val.s.s || !val.s.len) {
			LM_ERR("empty or non-string domain_prefix_avp, return with error ...\n");
			return -1;
		}
		LM_DBG("domain_prefix_avp found = '%.*s'\n", val.s.len, ZSW(val.s.s));
		if ( (len + val.s.len +1) >  MAX_URI_SIZE) {
			LM_ERR("duri buffer to small to add domain prefix\n");
			return -1;
		}
		memcpy(at, val.s.s, val.s.len); at = at + val.s.len;
		*at = '.'; at = at + 1;	/* add . as delimiter between prefix and domain */
		didsomething = 1;
	} else {
		LM_DBG("domain_prefix_avp not found\n");
	}


	/* add domain to duri buffer */
	avp = search_first_avp(domain_replacement_avp_name_str, domain_replacement_name, &val, 0);
	if (avp) {
		if ( !(avp->flags&AVP_VAL_STR) ||  !val.s.s || !val.s.len) {
			LM_ERR("empty or non-string domain_replacement_avp, return with"
					"error ...\n");
			return -1;
		}
		LM_DBG("domain_replacement_avp found='%.*s'\n",val.s.len, ZSW(val.s.s));
		if ( (len + val.s.len +1) >  MAX_URI_SIZE) {
			LM_ERR("duri buffer to small to add domain replacement\n");
			return -1;
		}
		memcpy(at, val.s.s, val.s.len); at = at + val.s.len;
		didsomething = 1;
	} else {
	    LM_DBG("domain_replacement_avp not found, using original domain '"
				"%.*s'\n",domain->len, domain->s);
	    if ( (len + domain->len) >  MAX_URI_SIZE) {
		LM_ERR("duri buffer to small to add domain\n");
		return -1;
	    }
	    memcpy(at, domain->s, domain->len); at = at + domain->len;
	}
	
	/* search for suffix and add it to duri buffer */
	avp = search_first_avp(domain_suffix_avp_name_str, domain_suffix_name, &val, 0);
	if (avp) {
		if ( !(avp->flags&AVP_VAL_STR) ||  !val.s.s || !val.s.len) {
			LM_ERR("empty or non-string domain_suffix_avp,return with error .."
					"\n");
			return -1;
		}
		LM_DBG("domain_suffix_avp found = '%.*s'\n", val.s.len, ZSW(val.s.s));
		if ( (len + val.s.len + 1) >  MAX_URI_SIZE) {
			LM_ERR("duri buffer to small to add domain suffix\n");
			return -1;
		}
		*at = '.'; at = at + 1;	/* add . as delimiter between domain and suffix */
		memcpy(at, val.s.s, val.s.len); at = at + val.s.len;
		didsomething = 1;
	} else {
		LM_DBG("domain_suffix_avp not found\n");
	}

	/* search for port override and add it to duri buffer */
	avp = search_first_avp(port_override_avp_name_str, port_override_name, &val, 0);
	if (avp) {
		if ( !(avp->flags&AVP_VAL_STR) ||  !val.s.s || !val.s.len) {
			LM_ERR("empty or non-string port_override_avp, return with error ...\n");
			return -1;
		}
		LM_DBG("port_override_avp found = '%.*s'\n", val.s.len, ZSW(val.s.s));
		/* We do not check if the port is valid */
		if ( (len + val.s.len + 1) >  MAX_URI_SIZE) {
			LM_ERR("duri buffer to small to add domain suffix\n");
			return -1;
		}
		*at = ':'; at = at + 1;	/* add : as delimiter between domain and port */
		memcpy(at, val.s.s, val.s.len); at = at + val.s.len;
		didsomething = 1;
	} else {
		LM_DBG("port_override_avp not found, using original port\n");
		if (_msg->parsed_uri.port.len) {
			LM_DBG("port found in RURI, reusing it for DURI\n");
			if ( (len + _msg->parsed_uri.port.len + 1) >  MAX_URI_SIZE) {
				LM_ERR("duri buffer to small to copy port\n");
				return -1;
			}
			*at = ':'; at = at + 1;	
			/* add : as delimiter between domain and port */
			memcpy(at, _msg->parsed_uri.port.s, _msg->parsed_uri.port.len); 
			at = at + _msg->parsed_uri.port.len;
		} else {
			LM_DBG("port not found in RURI, no need to copy it to DURI\n");
		}
	}

	/* search for transport override and add it to duri buffer */
	avp = search_first_avp(transport_override_avp_name_str, transport_override_name, &val, 0);
	if (avp) {
		if ( !(avp->flags&AVP_VAL_STR) ||  !val.s.s || !val.s.len) {
			LM_ERR("empty or non-string transport_override_avp, "
					"return with error ...\n");
			return -1;
		}
		LM_DBG("transport_override_avp found='%.*s'\n",val.s.len, ZSW(val.s.s));

		if ( (len + val.s.len + 11) >  MAX_URI_SIZE) {
			LM_ERR("duri buffer to small to add transport override\n");
			return -1;
		}
		/* add : as transport parameter to duri; NOTE: no checks if transport parameter is valid  */
		memcpy(at, ";transport=", 11); at = at + 11;
		memcpy(at, val.s.s, val.s.len); at = at + val.s.len;
		didsomething = 1;
	} else {
		LM_DBG("transport_override_avp not found, using original transport\n");
		if (_msg->parsed_uri.transport.len) {
			LM_DBG("transport found in RURI, reusing it for DURI\n");
			if ( (len + _msg->parsed_uri.transport.len + 1) >  MAX_URI_SIZE) {
				LM_ERR("duri buffer to small to copy transport\n");
				return -1;
			}
			*at = ';'; at = at + 1; /* add : as delimiter between domain and port */
			memcpy(at, _msg->parsed_uri.transport.s, _msg->parsed_uri.transport.len); at = at + _msg->parsed_uri.transport.len;
		} else {
			LM_DBG("transport not found in RURI, no need to copy it to DURI\n");
		}
	}

	/* write new target DURI into DURI */
	if (didsomething == 0) {
		LM_DBG("no domainpolicy AVP set, no need to push new DURI\n");
		return 2;
	}
	duri_str.s = (char *)&(duri[0]);
	duri_str.len = at - duri_str.s;
	LM_DBG("new DURI is '%.*s'\n",duri_str.len, ZSW(duri_str.s));
	set_dst_uri(_msg, &duri_str);
	/* dst_uri changes, so it makes sense to re-use the current uri for
		forking */
	ruri_mark_new(); /* re-use uri for serial forking */

	return 1;
}

