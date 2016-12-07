/* 
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
 * \brief Parser :: Parse name-addr part
 *
 * \ingroup parser
 */

#include <string.h>
#include "../dprint.h"
#include "parse_nameaddr.h"
#include "parser_f.h"
#include "../ut.h"


/*! \brief
 * Parse name-addr part, the given string can be longer,
 * parsing will stop when closing > is found
 */
int parse_nameaddr(str* _s, name_addr_t* _a)
{
	char* uri_end;

	if (!_s || !_a) {
		return -1;
	}

	_a->name.s = _s->s;

	_a->uri.s = find_not_quoted(_s, '<'); 
	if (_a->uri.s) {
		_a->name.len = _a->uri.s - _a->name.s;
		_a->uri.s++; /* We will skip < character */
	} else {
		return -3;
	}
	
	_a->uri.len = _s->len - _a->name.len - 1;
	uri_end = find_not_quoted(&_a->uri, '>');
	
	if (!uri_end) {
		return -4;
	}
	/* Total length of the field including <> */
	_a->len = uri_end - _a->name.s + 1;
	
	_a->uri.len = uri_end - _a->uri.s;
	return 0;
}


/*! \brief
 * Print a name-addr structure, just for debugging
 */
void print_nameaddr(FILE* _o, name_addr_t* _a)
{
	fprintf(_o, "---name-addr---\n");
	fprintf(_o, "name: '%.*s'\n", _a->name.len, ZSW(_a->name.s));
	fprintf(_o, "uri : '%.*s'\n", _a->uri.len, ZSW(_a->uri.s));
	fprintf(_o, "len : %d\n", _a->len);
	fprintf(_o, "---/name-addr---\n");
}
