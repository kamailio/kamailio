/*
 * Contact header field body parser
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

#include <string.h>          /* memset */
#include "../hf.h"     
#include "../../mem/mem.h"   /* pkg_malloc, pkg_free */
#include "../../dprint.h"
#include "../../trim.h"      /* trim_leading */
#include "parse_contact.h"



static inline int contact_parser(char* _s, int _l, contact_body_t* _c)
{
	str tmp;

	tmp.s = _s;
	tmp.len = _l;

	trim_leading(&tmp);

	if (tmp.len == 0) {
		LOG(L_ERR, "contact_parser(): Empty body\n");
		return -1;
	}

	if (tmp.s[0] == '*') {
		_c->star = 1;
	} else {
		if (parse_contacts(&tmp, &(_c->contacts)) < 0) {
			LOG(L_ERR, "contact_parser(): Error while parsing contacts\n");
			return -2;
		}
	}

	return 0;
}


/*
 * Parse contact header field body
 */
int parse_contact(struct hdr_field* _h)
{
	contact_body_t* b;

	if (_h->parsed != 0) {
		return 0;  /* Already parsed */
	}

	b = (contact_body_t*)pkg_malloc(sizeof(contact_body_t));
	if (b == 0) {
		LOG(L_ERR, "parse_contact(): No memory left\n");
		return -1;
	}

	memset(b, 0, sizeof(contact_body_t));

	if (contact_parser(_h->body.s, _h->body.len, b) < 0) {
		LOG(L_ERR, "parse_contact(): Error while parsing\n");
		pkg_free(b);
		return -2;
	}

	_h->parsed = (void*)b;
	return 0;
}


/*
 * Free all memory
 */
void free_contact(contact_body_t** _c)
{
	if(*_c==NULL)
		return;
	if ((*_c)->contacts) {
		free_contacts(&((*_c)->contacts));
	}
	
	pkg_free(*_c);
	*_c = 0;
}


/*
 * Print structure, for debugging only
 */
void print_contact(FILE* _o, contact_body_t* _c)
{
	fprintf(_o, "===Contact body===\n");
	fprintf(_o, "star: %d\n", _c->star);
	print_contacts(_o, _c->contacts);
	fprintf(_o, "===/Contact body===\n");
}


/*
 * Contact header field iterator, returns next contact if any, it doesn't
 * parse message header if not absolutely necessary
 */
int contact_iterator(contact_t** c, struct sip_msg* msg, contact_t* prev)
{
	static struct hdr_field* hdr = 0;
	struct hdr_field* last;
	contact_body_t* cb;

	if (!msg) {
		LOG(L_ERR, "Invalid parameter value\n");
		return -1;
	}

	if (!prev) {
		     /* No pointer to previous contact given, find topmost
		      * contact and return pointer to the first contact
		      * inside that header field
		      */
		hdr = msg->contact;
		if (!hdr) {
			if (parse_headers(msg, HDR_CONTACT_F, 0) == -1) {
				LOG(L_ERR, "Error while parsing headers\n");
				return -1;
			}

			hdr = msg->contact;
		}

		if (hdr) {
			if (parse_contact(hdr) < 0) {
				LOG(L_ERR, "Error while parsing Contact\n");
				return -1;
			}
		} else {
			*c = 0;
			return 1;
		}

		cb = (contact_body_t*)hdr->parsed;
		*c = cb->contacts;
		return 0;
	} else {
		     /* Check if there is another contact in the
		      * same header field and if so then return it
		      */
		if (prev->next) {
			*c = prev->next;
			return 0;
		}

		if(hdr==NULL) {
			LOG(L_ERR, "contact iterator not initialized\n");
			return -1;
		}

		     /* Try to find and parse another Contact
		      * header field
		      */
		last = hdr;
		hdr = hdr->next;

		     /* Search another already parsed Contact
		      * header field
		      */
		while(hdr && hdr->type != HDR_CONTACT_T) {
			hdr = hdr->next;
		}

		if (!hdr) {
			     /* Look for another Contact HF in unparsed
			      * part of the message header
			      */
			if (parse_headers(msg, HDR_CONTACT_F, 1) == -1) {
				LOG(L_ERR, "Error while parsing message header\n");
				return -1;
			}
			
			     /* Check if last found header field is Contact
			      * and if it is not the same header field as the
			      * previous Contact HF (that indicates that the previous 
			      * one was the last header field in the header)
			      */
			if ((msg->last_header->type == HDR_CONTACT_T) &&
			    (msg->last_header != last)) {
				hdr = msg->last_header;
			} else {
				*c = 0;
				return 1;
			}
		}
		
		if (parse_contact(hdr) < 0) {
			LOG(L_ERR, "Error while parsing Contact HF body\n");
			return -1;
		}
		
		     /* And return first contact within that
		      * header field
		      */
		cb = (contact_body_t*)hdr->parsed;
		*c = cb->contacts;
		return 0;
	}
}
