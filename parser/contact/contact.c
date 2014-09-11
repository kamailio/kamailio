/*
 * Parses one Contact in Contact HF body
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * -------
 *  2003-03-25 Adapted to use new parameter parser (janakj)
 */

#include <string.h>        /* memset */
#include "../../mem/mem.h" /* pkg_malloc, pkg_free */
#include "../../dprint.h"
#include "../../trim.h"    /* trim_leading, trim_trailing */
#include "contact.h"


#define ST1 1 /* Basic state */
#define ST2 2 /* Quoted */
#define ST3 3 /* Angle quoted */
#define ST4 4 /* Angle quoted and quoted */
#define ST5 5 /* Escape in quoted */
#define ST6 6 /* Escape in angle quoted and quoted */


/*
 * Skip URI, stops when , (next contact)
 * or ; (parameter) is found
 */
static inline int skip_uri(str* _s)
{
	register int st = ST1;

	while(_s->len) {
		switch(*(_s->s)) {
		case ',':
		case ';':
			if (st == ST1) return 0;
			break;

		case '\"':
			switch(st) {
			case ST1: st = ST2; break;
			case ST2: st = ST1; break;
			case ST3: st = ST4; break;
			case ST4: st = ST3; break;
			case ST5: st = ST2; break;
			case ST6: st = ST4; break;
			}
			break;

		case '<':
			switch(st) {
			case ST1: st = ST3; break;
			case ST3: 
				LOG(L_ERR, "skip_uri(): Second < found\n");
				return -1;
			case ST5: st = ST2; break;
			case ST6: st = ST4; break;
			}
			break;
			
		case '>':
			switch(st) {
			case ST1: 
				LOG(L_ERR, "skip_uri(): > is first\n");
				return -2;

			case ST3: st = ST1; break;
			case ST5: st = ST2; break;
			case ST6: st = ST4; break;
			}
			break;

		case '\\':
			switch(st) {
			case ST2: st = ST5; break;
			case ST4: st = ST6; break;
			case ST5: st = ST2; break;
			case ST6: st = ST4; break;
			}
			break;

		default: break;

		}

		_s->s++;
		_s->len--;
	}

	if (st != ST1) {
		LOG(L_ERR, "skip_uri(): < or \" not closed\n");
		return -3;
	}

	return 0;
}


/*
 * Skip name part
 *
 * _s will be adjusted to point at the beginning
 * of URI
 */
static inline int skip_name(str* _s)
{
	char* last_wsp, *p;
	int i, quoted = 0;
	

	if (!_s) {
		LOG(L_ERR, "skip_name(): Invalid parameter value\n");
		return -1;
	}

	p = _s->s;

	last_wsp = 0;

	for(i = 0; i < _s->len; i++) {
		if (!quoted) {
			if ((*p == ' ') || (*p == '\t')) {
				last_wsp = p;
			} else {
				if (*p == '<') {
					_s->s = p;
					_s->len -= i;
					return 0;
				}
				
				if (*p == ':') {
					if (last_wsp) {
						_s->s = last_wsp;
						_s->len -= last_wsp - _s->s + 1;
					}
					return 0;
				}

				if (*p == '\"') {
					quoted = 1;
				}
			}
		} else {
			if ((*p == '\"') && (*(p-1) != '\\')) quoted = 0;
		}
		p++;
	}

	if (quoted) {
		LOG(L_ERR, "skip_name(): Closing quote missing in name part of Contact\n");
	} else {
		LOG(L_ERR, "skip_name(): Error in contact, scheme separator not found\n");
	}

	return -1;
}


/*
 * Parse contacts in a Contact HF
 */
int parse_contacts(str* _s, contact_t** _c)
{
	contact_t* c;
	param_hooks_t hooks;

	while(1) {
		     /* Allocate and clear contact structure */
		c = (contact_t*)pkg_malloc(sizeof(contact_t));
		if (c == 0) {
			LOG(L_ERR, "parse_contacts(): No memory left\n");
			goto error;
		}
		memset(c, 0, sizeof(contact_t));
		
		c->name.s = _s->s;

		if (skip_name(_s) < 0) {
			LOG(L_ERR, "parse_contacts(): Error while skipping name part\n");
			goto error;
		}

		c->uri.s = _s->s;
		c->name.len = _s->s - c->name.s;
		trim_trailing(&c->name);
		
		     /* Find the end of the URI */
		if (skip_uri(_s) < 0) {
			LOG(L_ERR, "parse_contacts(): Error while skipping URI\n");
			goto error;
		}
		
		c->uri.len = _s->s - c->uri.s; /* Calculate URI length */
		trim_trailing(&(c->uri));      /* Remove any trailing spaces from URI */

		     /* Remove <> if any */
		if ((c->uri.len >= 2) && (c->uri.s[0] == '<') && (c->uri.s[c->uri.len - 1] == '>')) {
			c->uri.s++;
			c->uri.len -= 2;
		}

		trim(&c->uri);
		
		if (_s->len == 0) goto ok;
		
		if (_s->s[0] == ';') {         /* Contact parameter found */
			_s->s++;
			_s->len--;
			trim_leading(_s);
			
			if (_s->len == 0) {
				LOG(L_ERR, "parse_contacts(): Error while parsing params\n");
				goto error;
			}

			if (parse_params(_s, CLASS_CONTACT, &hooks, &c->params) < 0) {
				LOG(L_ERR, "parse_contacts(): Error while parsing parameters\n");
				goto error;
			}

			c->q = hooks.contact.q;
			c->expires = hooks.contact.expires;
			c->received = hooks.contact.received;
			c->methods = hooks.contact.methods;
			c->instance = hooks.contact.instance;
			c->reg_id = hooks.contact.reg_id;

			if (_s->len == 0) goto ok;
		}

		     /* Next character is comma */
		c->len = _s->s - c->name.s;
		_s->s++;
		_s->len--;
		trim_leading(_s);

		if (_s->len == 0) {
			LOG(L_ERR, "parse_contacts(): Text after comma missing\n");
			goto error;
		}

		c->next = *_c;
		*_c = c;
	}

 error:
	if (c) pkg_free(c);
	free_contacts(_c); /* Free any contacts created so far */
	return -1;

 ok:
	c->len = _s->s - c->name.s;
	c->next = *_c;
	*_c = c;
	return 0;
}


/*
 * Free list of contacts
 * _c is head of the list
 */
void free_contacts(contact_t** _c)
{
	contact_t* ptr;

	while(*_c) {
		ptr = *_c;
		*_c = (*_c)->next;
		if (ptr->params) {
			free_params(ptr->params);
		}
		pkg_free(ptr);
	}
}


/*
 * Print list of contacts, just for debugging
 */
void print_contacts(FILE* _o, contact_t* _c)
{
	contact_t* ptr;

	ptr = _c;

	while(ptr) {
		fprintf(_o, "---Contact---\n");
		fprintf(_o, "name    : '%.*s'\n", ptr->name.len, ptr->name.s);
		fprintf(_o, "URI     : '%.*s'\n", ptr->uri.len, ptr->uri.s);
		fprintf(_o, "q       : %p\n", ptr->q);
		fprintf(_o, "expires : %p\n", ptr->expires);
		fprintf(_o, "received: %p\n", ptr->received);
		fprintf(_o, "methods : %p\n", ptr->methods);
		fprintf(_o, "instance: %p\n", ptr->instance);
		fprintf(_o, "reg-id  : %p\n", ptr->reg_id);
		fprintf(_o, "len     : %d\n", ptr->len);
		if (ptr->params) {
			print_params(_o, ptr->params);
		}
		fprintf(_o, "---/Contact---\n");
		ptr = ptr->next;
	}
}
