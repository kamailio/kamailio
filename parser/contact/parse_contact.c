/*
 * $Id$
 *
 * Contact header field body parser
 */

#include "parse_contact.h"
#include "../hf.h"     
#include "../../mem/mem.h"   /* pkg_malloc, pkg_free */
#include "../../dprint.h"
#include <stdio.h>           /* printf */
#include "../../trim.h"      /* trim_leading */
#include <string.h>          /* memset */


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
	if ((*_c)->contacts) {
		free_contacts(&((*_c)->contacts));
	}
	
	pkg_free(*_c);
	*_c = 0;
}


/*
 * Print structure, for debugging only
 */
void print_contact(contact_body_t* _c)
{
	printf("===Contact body===\n");
	printf("star: %d\n", _c->star);
	print_contacts(_c->contacts);
	printf("===/Contact body===\n");
}
