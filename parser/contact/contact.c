/*
 * $Id$
 *
 * Parses one Contact in Contact HF body
 */

#include "contact.h"
#include "../../mem/mem.h" /* pkg_malloc, pkg_free */
#include "../../dprint.h"
#include <string.h>        /* memset */
#include "../../trim.h"    /* trim_leading, trim_trailing */
#include <stdio.h>         /* printf */


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
 * Parse contacts in a Contact HF
 */
int parse_contacts(str* _s, contact_t** _c)
{
	contact_t* c;

	while(1) {
		     /* Allocate and clear contact stucture */
		c = (contact_t*)pkg_malloc(sizeof(contact_t));
		if (c == 0) {
			LOG(L_ERR, "parse_contacts(): No memory left\n");
			goto error;
		}
		memset(c, 0, sizeof(contact_t));
		
		     /* Save beginning of URI */
		c->uri.s = _s->s;
		
		     /* Find the end of the URI */
		if (skip_uri(_s) < 0) {
			LOG(L_ERR, "parse_contacts(): Error while skipping URI\n");
			goto error;
		}
		
		c->uri.len = _s->s - c->uri.s; /* Calculate URI length */
		trim_trailing(&(c->uri));      /* Remove any trailing spaces from URI */

		if (_s->len == 0) goto ok;
		
		if (_s->s[0] == ';') {         /* Contact parameter found */
			_s->s++;
			_s->len--;
			trim_leading(_s);
			
			if (_s->len == 0) {
				LOG(L_ERR, "parse_contacts(): Error while parsing params\n");
				goto error;
			}

			if (parse_cparams(_s, &(c->params), &(c->q), &(c->expires), &(c->method)) < 0) {
				LOG(L_ERR, "parse_contacts(): Error while parsing params\n");
				goto error;
			}

			if (_s->len == 0) goto ok;
		}

		     /* Next character is comma */
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
			free_cparams(&(ptr->params));
		}
		pkg_free(ptr);
	}
}


/*
 * Print list of contacts, just for debugging
 */
void print_contacts(contact_t* _c)
{
	contact_t* ptr;

	ptr = _c;

	while(ptr) {
		printf("---Contact---\n");
		printf("URI    : \'%.*s\'\n", ptr->uri.len, ptr->uri.s);
		printf("q      : %p\n", ptr->q);
		printf("expires: %p\n", ptr->expires);
		printf("method : %p\n", ptr->method);
		if (ptr->params) {
			print_cparams(ptr->params);
		}
		printf("---/Contact---\n");
		ptr = ptr->next;
	}
}
