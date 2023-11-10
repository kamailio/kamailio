/*
 * lost module naptr functions
 * thankfully taken over from the enum module
 *
 * Copyright (C) 2002-2010 Juha Heinanen
 * 
 * Copyright (C) 2021 Wolfgang Kampichler
 * DEC112, FREQUENTIS AG
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
 * \brief Kamailio lost :: naptr
 * \ingroup lost
 * Module: \ref lost
 */

#include "../../core/resolve.h"
#include "../../core/strutils.h"
#include "../../core/qvalue.h"

/* Checks if NAPTR record has flag u and its services field
 * LoST:https or LOST:http
 * LIS:HELD
 */
static inline int service_match(struct naptr_rdata *naptr, str *service)
{
	if(service->len == 0) {
		return 0;
	}

	/* LoST:https or LOST:http or LIS:HELD */
	if(naptr->flags_len == 1) {
		return (((naptr->flags[0] == 'u') || (naptr->flags[0] == 'U'))
				&& (naptr->services_len == service->len)
				&& (strncasecmp(naptr->services, service->s, service->len)
						== 0));
	}

	/* LIS:HELD domain */
	if(naptr->flags_len == 0) {
		return ((naptr->services_len == service->len)
				&& (strncasecmp(naptr->services, service->s, service->len)
						== 0));
	}

	/* no matching service found */
	return 0;
}

/* Parse NAPTR regexp field of the form !pattern!replacement! and return its
 * components in pattern and replacement parameters.  Regexp field starts at
 * address first and is len characters long.
 */
static inline int parse_naptr_regexp(
		char *first, int len, str *pattern, str *replacement)
{
	char *second, *third;

	if(len > 0) {
		if(*first == '!') {
			second = (char *)memchr((void *)(first + 1), '!', len - 1);
			if(second) {
				len = len - (second - first + 1);
				if(len > 0) {
					third = memchr(second + 1, '!', len);
					if(third) {
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

/*
 * Tests if one result record is "greater" that the other.  Non-NAPTR records
 * greater that NAPTR record.  An invalid NAPTR record is greater than a 
 * valid one.  Valid NAPTR records are compared based on their
 * (order,preference).
 */
static inline int naptr_greater(struct rdata *a, struct rdata *b)
{
	struct naptr_rdata *na, *nb;

	if(a->type != T_NAPTR)
		return 1;
	if(b->type != T_NAPTR)
		return 0;

	na = (struct naptr_rdata *)a->rdata;
	if(na == 0)
		return 1;

	nb = (struct naptr_rdata *)b->rdata;
	if(nb == 0)
		return 0;

	return (((na->order) << 16) + na->pref) > (((nb->order) << 16) + nb->pref);
}

/*
 * Bubble sorts result record list according to naptr (order,preference).
 */
static inline void naptr_sort(struct rdata **head)
{
	struct rdata *p, *q, *r, *s, *temp, *start;

	/* r precedes p and s points to the node up to which comparisons
         are to be made */

	s = NULL;
	start = *head;
	while(s != start->next) {
		r = p = start;
		q = p->next;
		while(p != s) {
			if(naptr_greater(p, q)) {
				if(p == start) {
					temp = q->next;
					q->next = p;
					p->next = temp;
					start = q;
					r = q;
				} else {
					temp = q->next;
					q->next = p;
					p->next = temp;
					r->next = q;
					r = q;
				}
			} else {
				r = p;
				p = p->next;
			}
			q = p->next;
			if(q == s)
				s = p;
		}
	}
	*head = start;
}

/*
 * NAPTR lookup on hostname & service, returns result as string
 */
int lost_naptr_lookup(str hostname, str *service, str *result)
{
	struct rdata *head;
	struct rdata *l;
	struct naptr_rdata *naptr;

	str pattern, replacement;

	head = get_record(hostname.s, T_NAPTR, RES_ONLY_TYPE);

	if(head == 0) {
		LM_DBG("no NAPTR record found for [%.*s]\n", hostname.len, hostname.s);
		return 0;
	}

	naptr_sort(&head);

	/* we have the naptr records, loop and find an srv record with */
	/* same ip address as source ip address, if we do then true is returned */

	for(l = head; l; l = l->next) {

		if(l->type != T_NAPTR)
			continue; /*should never happen*/
		naptr = (struct naptr_rdata *)l->rdata;

		if(naptr == 0) {
			LM_ERR("no rdata in DNS response\n");
			free_rdata_list(head);
			return 0;
		}

		LM_DBG("NAPTR query on %.*s: order %u, pref %u, flen %u, flags "
			   "'%.*s', slen %u, services '%.*s', rlen %u, "
			   "regexp '%.*s'\n",
				hostname.len, hostname.s, naptr->order, naptr->pref,
				naptr->flags_len, (int)(naptr->flags_len), ZSW(naptr->flags),
				naptr->services_len, (int)(naptr->services_len),
				ZSW(naptr->services), naptr->regexp_len,
				(int)(naptr->regexp_len), ZSW(naptr->regexp));

		if(service_match(naptr, service) == 0) {
			continue;
		}

		if(parse_naptr_regexp(&(naptr->regexp[0]), naptr->regexp_len, &pattern,
				   &replacement)
				< 0) {
			free_rdata_list(head); /*clean up*/
			LM_ERR("parsing of NAPTR regexp failed\n");
			return 0;
		}
		/* Avoid making copies of pattern and replacement */
		pattern.s[pattern.len] = (char)0;
		replacement.s[replacement.len] = (char)0;
		/* replace hostname */
		if(reg_replace(pattern.s, replacement.s, &(hostname.s[0]), result)
				< 0) {
			pattern.s[pattern.len] = '!';
			replacement.s[replacement.len] = '!';
			LM_ERR("regexp replace failed\n");
			free_rdata_list(head); /*clean up*/
			return 0;
		} else {

			LM_DBG("resulted in replacement: '%.*s'\n", result->len,
					ZSW(result->s));

			free_rdata_list(head); /*clean up*/
			return 1;
		}
	}

	/* must not have found the record */
	return 0;
}