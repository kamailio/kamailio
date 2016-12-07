/*
 * Expires header field body parser
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

/*! \file
 * \brief Parser :: Expires header field body parser.
 *
 *
 * \ingroup parser
 */



#include "parse_expires.h"
#include <stdio.h>          /* printf */
#include "../mem/mem.h"     /* pkg_malloc, pkg_free */
#include "../dprint.h"
#include "../trim.h"        /* trim_leading */
#include <string.h>         /* memset */
#include "../ut.h"


static inline int expires_parser(char* _s, int _l, exp_body_t* _e)
{
	int i;
	str tmp;
	
	tmp.s = _s;
	tmp.len = _l;

	trim(&tmp);

	if (tmp.len == 0) {
		LOG(L_ERR, "expires_parser(): Empty body\n");
		_e->valid = 0;
		return -1;
	}

	_e->text.s = tmp.s;
	_e->text.len = tmp.len;

	/* more then 32bit/maxuint cant be valid */
	if (tmp.len > 10) {
		_e->valid = 0;
		return 0;
	}

	for(i = 0; i < tmp.len; i++) {
		if ((tmp.s[i] >= '0') && (tmp.s[i] <= '9')) {
			_e->val *= 10;
			_e->val += tmp.s[i] - '0';
		} else {
			switch(tmp.s[i]) {
			case ' ':
			case '\t':
			case '\r':
			case '\n':
				_e->text.len = i;
				_e->valid = 1;
				return 0;

			default:
				     /* Exit normally here, we want to be backwards compatible with
				      * RFC2543 entities that can put absolute time here
				      */
				     /*
				LOG(L_ERR, "expires_parser(): Invalid character\n");
				return -2;
				     */
				_e->valid = 0;
				return 0;
			}
		}
	}

	_e->valid = 1;
	return 0;
}


/*! \brief
 * Parse expires header field body
 */
int parse_expires(struct hdr_field* _h)
{
	exp_body_t* e;

	if (_h->parsed) {
		return 0;  /* Already parsed */
	}

	e = (exp_body_t*)pkg_malloc(sizeof(exp_body_t));
	if (e == 0) {
		LOG(L_ERR, "parse_expires(): No memory left\n");
		return -1;
	}
	
	memset(e, 0, sizeof(exp_body_t));

	if (expires_parser(_h->body.s, _h->body.len, e) < 0) {
		LOG(L_ERR, "parse_expires(): Error while parsing\n");
		pkg_free(e);
		return -2;
	}
	
	_h->parsed = (void*)e;
	return 0;
}


/*! \brief
 * Free all memory associated with exp_body_t
 */
void free_expires(exp_body_t** _e)
{
	pkg_free(*_e);
	*_e = 0;
}


/*! \brief
 * Print exp_body_t content, for debugging only
 */
void print_expires(exp_body_t* _e)
{
	printf("===Expires===\n");
	printf("text: \'%.*s\'\n", _e->text.len, ZSW(_e->text.s));
	printf("val : %d\n", _e->val);
	printf("===/Expires===\n");
}
