/*
 * $Id$
 *
 * Expires header field body parser
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
				return 0;

			default:
				LOG(L_ERR, "expires_parser(): Invalid character\n");
				return -2;
			}
		}
	}

	_e->text.len = _l;
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
