/*
 * $Id$
 *
 * Route & Record-Route header field parser
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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

#include <stdio.h>
#include <string.h>
#include "parse_rr.h"
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../trim.h"


/*
 * Parse Route and Record-Route header fields
 */
int parse_rr(str* _s, rr_t** _r)
{
	rr_t* r, *res, *last;
	str s;
	param_hooks_t hooks;

	if (!_s || !_r) {
		LOG(L_ERR, "parse_rr(): Invalid parameter value\n");
		return -1;
	}

	     /* Make a temporary copy of the string */
	s.s = _s->s;
	s.len = _s->len;

	res = last = 0;

	while(1) {
		     /* Allocate and clear rr stucture */
		r = (rr_t*)pkg_malloc(sizeof(rr_t));
		if (!r) {
			LOG(L_ERR, "parse_rr(): No memory left\n");
			goto error;
		}

		memset(r, 0, sizeof(rr_t));
		
		     /* Parse name-addr part of the header */
		if (parse_nameaddr(_s, &r->nameaddr) < 0) {
			LOG(L_ERR, "parse_rr(): Error while parsing name-addr\n");
			goto error;
		}
		
		     /* Shift just behind the closing > */
		s.s = r->nameaddr.uri.s + r->nameaddr.uri.len + 1; /* Point just behind > */
		s.len -= r->nameaddr.name.len + r->nameaddr.uri.len + 2;

		     /* Nothing left, finish */
		if (s.len == 0) goto ok;
		
		if (s.s[0] == ';') {         /* Contact parameter found */
			s.s++;
			s.len--;
			trim_leading(&s);
			
			if (s.len == 0) {
				LOG(L_ERR, "parse_rr(): Error while parsing params\n");
				goto error;
			}

			     /* Parse all parameters */
			if (parse_params(&s, CLASS_RR, &hooks, &r->params) < 0) {
				LOG(L_ERR, "parse_rr(): Error while parsing params\n");
				goto error;
			}

			     /* Copy hooks */
			r->lr = hooks.rr.lr;
			r->r2 = hooks.rr.r2;

			if (s.len == 0) goto ok;
		}

		     /* Next character is comma or end of header*/
		s.s++;
		s.len--;
		trim_leading(&s);

		if (s.len == 0) {
			LOG(L_ERR, "parse_rr(): Text after comma missing\n");
			goto error;
		}

		     /* Append the structure as last parameter of the linked list */
		if (!res) res = r;
		if (last) last->next = r;
		last = r;
	}

 error:
	if (r) pkg_free(r);
	free_rr(res); /* Free any contacts created so far */
	return -1;

 ok:
	if (last) last->next = r;
	*_r = res;
	return 0;
}


/*
 * Free list of rrs
 * _r is head of the list
 */
void free_rr(rr_t* _r)
{
	rr_t* ptr;

	while(_r) {
		ptr = _r;
		_r = _r->next;
		if (ptr->params) {
			free_params(ptr->params);
		}
		pkg_free(ptr);
	}
}


/*
 * Print list of RRs, just for debugging
 */
void print_rr(rr_t* _r)
{
	rr_t* ptr;

	ptr = _r;

	while(ptr) {
		printf("---RR---\n");
		print_nameaddr(&ptr->nameaddr);
		printf("lr : %p\n", ptr->lr);
		printf("r2 : %p\n", ptr->r2);
		if (ptr->params) {
			print_params(ptr->params);
		}
		printf("---/RR---\n");
		ptr = ptr->next;
	}
}
