/*
 * $Id$
 *
 * Expires header field body parser
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


#include "parse_expires.h"
#include <stdio.h>          /* printf */
#include "../mem/mem.h"     /* pkg_malloc, pkg_free */
#include "../dprint.h"
#include "../trim.h"        /* trim_leading */
#include <string.h>         /* memset */


static inline int expires_parser(char* _s, int _l, exp_body_t* _e)
{
	int i;
	str tmp;
	
	tmp.s = _s;
	tmp.len = _l;

	trim_leading(&tmp);

	if (tmp.len == 0) {
		LOG(L_ERR, "expires_parser(): Empty body\n");
		_e->valid = 0;
		return -1;
	}

	_e->text.s = tmp.s;

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

	_e->text.len = _l;
	_e->valid = 1;
	return 0;
}


/*
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


/*
 * Free all memory associated with exp_body_t
 */
void free_expires(exp_body_t** _e)
{
	pkg_free(*_e);
	*_e = 0;
}


/*
 * Print exp_body_t content, for debugging only
 */
void print_expires(exp_body_t* _e)
{
	printf("===Expires===\n");
	printf("text: \'%.*s\'\n", _e->text.len, _e->text.s);
	printf("val : %d\n", _e->val);
	printf("===/Expires===\n");
}
