/*
 * $Id$
 *
 * Event header field body parser.
 * The parser was written for Presence Agent module only.
 * it recognize presence package only, no subpackages, no parameters
 * It should be replaced by a more generic parser if subpackages or
 * parameters should be parsed too.
 */

#include "parse_event.h"
#include "../mem/mem.h"    /* pkg_malloc, pkg_free */
#include "../dprint.h"
#include <string.h>        /* memset */
#include "../trim.h"       /* trim_leading */
#include <stdio.h>         /* printf */


#define PRES_STR "presence"
#define PRES_STR_LEN 8


static inline char* skip_token_nodot(char* _b, int _l)
{
	int i = 0;

	for(i = 0; i < _l; i++) {
		switch(_b[i]) {
		case ' ':
		case '\r':
		case '\n':
		case '\t':
		case ';':
		case '.':
			return _b + i;
		}
	}

	return _b + _l;
}


static inline int event_parser(char* _s, int _l, event_t* _e)
{
	str tmp;
	char* end;

	tmp.s = _s;
	tmp.len = _l;

	trim_leading(&tmp);

	if (tmp.len == 0) {
		LOG(L_ERR, "event_parser(): Empty body\n");
		return -1;
	}

	_e->text.s = tmp.s;

	end = skip_token_nodot(tmp.s, tmp.len);

	tmp.len = end - tmp.s;

	if ((tmp.len == PRES_STR_LEN) && 
	    !strncasecmp(PRES_STR, tmp.s, tmp.len)) {
		_e->parsed = EVENT_PRESENCE;
	} else {
		_e->parsed = EVENT_OTHER;
	}

	return 0;
}


/*
 * Parse Event header field body
 */
int parse_event(struct hdr_field* _h)
{
	event_t* e;

	if (_h->parsed != 0) {
		return 0;
	}

	e = (event_t*)pkg_malloc(sizeof(event_t));
	if (e == 0) {
		LOG(L_ERR, "parse_event(): No memory left\n");
		return -1;
	}

	memset(e, 0, sizeof(event_t));

	if (event_parser(_h->body.s, _h->body.len, e) < 0) {
		LOG(L_ERR, "parse_event(): Error in event_parser\n");
		pkg_free(e);
		return -2;
	}

	_h->parsed = (void*)e;
	return 0;
}


/*
 * Free all memory
 */
void free_event(event_t** _e)
{
	if (*_e) pkg_free(*_e);
	*_e = 0;
}


/*
 * Print structure, for debugging only
 */
void print_event(event_t* _e)
{
	printf("===Event===\n");
	printf("text  : %.*s\n", _e->text.len, _e->text.s);
	printf("parsed: %s\n", 
	       (_e->parsed == EVENT_PRESENCE) ? ("EVENT_PRESENCE") : ("EVENT_OTHER"));
	printf("===/Event===\n");
}
